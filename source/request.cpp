
class request
{

public:
    int _n_triples = 0, _n_prefixes = 0, _n_orders = 0, _n_filters = 0, _n_filter_groups = 0, modifier = 0, current_filter_group[32];
    local_triple _triples[1024];
    prefix _prefixes[1024];
    order _orders[32];
    filter _filters[32];
    filter_group _filter_groups[32];
    unsigned long *_to_delete = 0, _n_delete = 0;
    unsigned long *_to_send = 0, _n_send = 0;
    char type[256];
    char request_id[1024];

    // Variables set for chain request
    chain_variable var[32];
    int n_var = 0, depth = 0;

    // Current combination during combinations building
    int n_comb;
    int comb_var[32];
    char *comb_value[32], *comb_value_type[32], *comb_value_lang[32];

    // Set of candidate combinations
    char ***pre_comb_value, ***pre_comb_value_type, ***pre_comb_value_lang;
    int n_pcv = 0;

    bool parse_request(char *buffer, char **message, int fd, int operation, int endpoint, int modifier, int *pip) {
        int r = 0, pos = 0, status = 0, nested_array_items[32], current_level = 0, in_block = 0;
        char *t, *subject = NULL, *predicate = NULL, *object = NULL, *datatype = NULL, lang[8];
        bool in_method = false, in_id = false;

        memset(nested_array_items, 0, 32*sizeof(int));
        memset(this->current_filter_group, 0, 32*sizeof(int));
        init_globals(O_RDONLY);
        unicode_to_utf8(buffer);
        t = (char *)malloc(strlen(buffer) + 1);
        if(!t) out_of_memory(fd);
        t[0] = 0;
        this->type[0] = 0;
        this->request_id[0] = 0;
        lang[0] = 0;

        // JSON
        if (buffer[0] != '{') {
            *message = (char *)malloc(128);
            if(!message) out_of_memory(fd);
            strcpy(*message, "Unknown request format (expecting: JSON)");
            this->free_all();
            goto send_parse_error;
        }
        jsmn_parser parser;
        jsmntok_t tokens[MAX_JSON_TOKENS];
        jsmn_init(&parser);
        r = jsmn_parse(&parser, buffer, strlen(buffer), tokens, MAX_JSON_TOKENS);
        if(r <= 0) {
            *message = (char *)malloc(128);
            if(!message) out_of_memory(fd);
            strcpy(*message, "Cannot parse JSON");
            this->free_all();
            goto send_parse_error;
        }
//printf("%i tokens\n",r);
        for (int i = 0; i < r; i++) {
            strncpy(t, (char *)((long)buffer + tokens[i].start), tokens[i].end - tokens[i].start);
            t[tokens[i].end - tokens[i].start] = 0;
//printf("token %s: type: %i, start: %i, end: %i, size: %i, in_block: %i\n",t,tokens[i].type,tokens[i].start,tokens[i].end,tokens[i].size,in_block);
            // This item is a collection or an array
            if(tokens[i].type == 1 || tokens[i].type == 2) {
                *message = this->save_parsed_item(&subject, &predicate, &object, &datatype, lang, operation, in_block, current_level, &pos, &status);
                lang[0] = 0;
                if (status == -1)
                    goto send_parse_error;
                if (tokens[i].type == 2 && (in_block == B_CHAIN || in_block == B_PATTERN || in_block == B_ORDER || in_block == B_FILTER)) {
                    if(nested_array_items[current_level] > 0)
                        nested_array_items[++current_level] = tokens[i].size;
                    else
                        nested_array_items[current_level] = tokens[i].size;
//printf("  level = %i, nested items at this level: %i\n", current_level, nested_array_items[current_level]);
                }
                pos = 0;
            }

            // This item is a string
            if (tokens[i].type == 3 ) // && t[0] != '{') // Uncommenting this will disallow nested JSON in triples
            {
                if (in_block != B_PATTERN && in_block != B_CHAIN && in_block != B_PREFIX && in_block != B_ORDER && in_block != B_FILTER)
                    strtolower(t);
                else
                    descreen(t);
                if (strcmp(t, "method") == 0)
                {
                    in_method = true;
                    continue;
                }
                if (strcmp(t, "requestid") == 0)
                {
                    in_id = true;
                    continue;
                }
                if (strcmp(t, "pattern") == 0 || strcmp(t, "triple") == 0)
                {
                    if (!endpoint)
                        endpoint = EP_TRIPLE;
                    if (endpoint != EP_TRIPLE)
                        goto wrong_parameter;
                    if (strcmp(t, "pattern") == 0)
                        in_block = B_PATTERN;
                    continue;
                }
                if (strcmp(t, "chain") == 0)
                {
                    if (!endpoint)
                        endpoint = EP_CHAIN;
                    if (endpoint != EP_CHAIN)
                        goto wrong_parameter;
                    in_block = B_CHAIN;
                    continue;
                }
                if (strcmp(t, "order") == 0)
                {
                    if (endpoint != EP_TRIPLE && endpoint != EP_CHAIN)
                        goto wrong_parameter;
                    in_block = B_ORDER;
                    if(subject) { free(subject); subject = NULL; }
                    if(object) { free(object); object = NULL; }
                    pos = 0;
                    continue;
                }
                if (strcmp(t, "filter") == 0)
                {
                    if (endpoint != EP_CHAIN)
                        goto wrong_parameter;
                    in_block = B_FILTER;
                    if(subject) { free(subject); subject = NULL; }
                    if(predicate) { free(predicate); predicate = NULL; }
                    if(object) { free(object); object = NULL; }
                    pos = 0;
                    continue;
                }
                if (strcmp(t, "count") == 0)
                {
                    if (endpoint != EP_TRIPLE && endpoint != EP_CHAIN)
                        goto wrong_parameter;
                    modifier |= MOD_COUNT;
                    continue;
                }
                if (strcmp(t, "prefix") == 0)
                {
                    if (!endpoint)
                        endpoint = EP_PREFIX;
                    if (endpoint != EP_PREFIX)
                        goto wrong_parameter;
                    in_block = B_PREFIX;
                    continue;
                }
                if (in_id)
                {
                    strcpy(this->request_id, t);
                    in_id = false;
                }
                else if (in_method)
                {
                    if (strcmp(t, "get") == 0)
                        operation = OP_GET;
                    else if (strcmp(t, "put") == 0)
                        operation = OP_PUT;
                    else if (strcmp(t, "post") == 0)
                        operation = OP_POST;
                    else if (strcmp(t, "delete") == 0)
                        operation = OP_DELETE;
                    if (!operation)
                    {
                        *message = (char *)malloc(128 + strlen(t));
                        if(!message) out_of_memory(fd);
                        sprintf(*message, "Unknown method: %s", t);
                        goto send_parse_error;
                    }
                    strcpy(this->type, t);
                    in_method = false;
                }
                else if (in_block == B_PATTERN || in_block == B_CHAIN || in_block == B_FILTER || in_block == B_PREFIX || in_block == B_ORDER)
                {
                    if (tokens[i].end - tokens[i].start > 65535) {
                        *message = (char *)malloc(128 + tokens[i].end - tokens[i].start);
                        if(!message) out_of_memory(fd);
                        sprintf(*message, "Token is too long: %s", t);
                        goto send_parse_error;
                    }
                    switch (pos) {
                    case 0:
                        if(subject) free(subject);
                        subject = (char*)malloc(strlen(t)+1);
                        if(!subject) out_of_memory();
                        strcpy(subject, t);
                        break;
                    case 1:
                        if(predicate) free(predicate);
                        predicate = (char*)malloc(strlen(t)+1);
                        if(!predicate) out_of_memory();
                        strcpy(predicate, t);
                        break;
                    case 2:
                        if(object) free(object);
                        object = (char*)malloc(strlen(t)+1);
                        if(!object) out_of_memory();
                        strcpy(object, t);
                        break;
                    case 3:
                        if(datatype) free(datatype);
                        datatype = (char*)malloc(strlen(t)+1);
                        if(!datatype) out_of_memory();
                        strcpy(datatype, t);
                        break;
                    case 4:
                        if (strlen(t) < 7)
                            strcpy(lang, t);
                        break;
                    }
                    pos++;
                }
                if(nested_array_items[current_level] > 0) {
                    bool end_of_block = false;
                    int prev_level = current_level;
                    do {
                        if(nested_array_items[current_level] > 0)
                            nested_array_items[current_level]--;
                        if(nested_array_items[current_level] == 0 && current_level > 0) {
                            end_of_block = true;
                            current_level--;
                            if(nested_array_items[current_level] > 0)
                                nested_array_items[current_level]--;
                        }
                    } while(current_level > 0 && nested_array_items[current_level] == 0);
//printf("  set current_level to %i (%i items at this level)\n", current_level, nested_array_items[current_level]);
                    if(end_of_block) {
                        *message = this->save_parsed_item(&subject, &predicate, &object, &datatype, lang, operation, in_block, prev_level, &pos, &status);
                        lang[0] = 0;
                        if (status == -1)
                            goto send_parse_error;
                    }
                    if(current_level == 0 && nested_array_items[current_level] == 0)
                        in_block = 0;
                }
            }
        }

        // Write the last block of the array
        *message = this->save_parsed_item(&subject, &predicate, &object, &datatype, lang, operation, in_block, current_level, &pos, &status);
        if (status == -1)
            goto send_parse_error;
        free(buffer);
        if(subject) free(subject);
        if(predicate) free(predicate);
        if(object) free(object);
        if(datatype) free(datatype);
        buffer = NULL;
        free(t);
        t = NULL;

        // Finished JSON processing, start performing actions
        this->modifier = modifier;
        if (endpoint == EP_PREFIX && this->_n_prefixes && (operation == OP_PUT || operation == OP_POST))
        {
            logger(LOG, "PUT prefix request", this->request_id, 0);
            if (*message = this->commit_prefixes())
                goto send_parse_error;
        }
        else if (endpoint == EP_PREFIX && operation == OP_GET)
        {
            logger(LOG, "GET prefix request", this->request_id, 0);
            *message = this->return_prefixes();
            return true;
        }
        else if (endpoint == EP_PREFIX && operation == OP_DELETE)
        {
            logger(LOG, "DELETE prefix request", this->request_id, 0);
            if (*message = this->delete_prefixes())
                goto send_parse_error;
        }
        else if (endpoint == EP_TRIPLE && this->_n_triples && operation == OP_PUT)
        {
            logger(LOG, "PUT triples request", this->request_id, 0);
            if (*message = this->commit_triples(pip))
                goto send_parse_error;
        }
        else if (endpoint == EP_TRIPLE && this->_n_send && operation == OP_GET)
        {
            logger(LOG, "GET triples request", this->request_id, 0);
            *message = this->return_triples();
            return true;
        }
        else if (endpoint == EP_CHAIN && this->_n_triples && this->n_var && operation == OP_GET)
        {
            logger(LOG, "GET triples request", this->request_id, 0);
            *message = this->return_triples_chain(this->request_id);
            return true;
        }
        else if (endpoint == EP_TRIPLE && this->_n_delete && operation == OP_DELETE)
        {
            logger(LOG, "DELETE triples request", this->request_id, 0);
            if (*message = this->commit_delete(pip))
                goto send_parse_error;
        }
        return true;
    wrong_parameter:
        *message = (char *)malloc(128);
        if(!message) out_of_memory(fd);
        sprintf(*message, "%s parameter cannot be specified in the request to this endpoint", t);
        this->free_all();
    send_parse_error:
        if (buffer)
            free(buffer);
        if (t)
            free(t);
        if (!*message)
        {
            *message = (char *)malloc(128);
            if(!message) out_of_memory(fd);
            strcpy(*message, "Parsing error");
        }
        char *inv = (char *)malloc(256 + strlen(*message));
        if(!inv) out_of_memory(fd);
        sprintf(inv, "{\"Status\":\"Error\", \"RequestId\":\"%s\", \"Message\":\"%s\"}", this->request_id, *message);
        if (*message)
            free(*message);
        *message = inv;
        return false;
    }

    char *save_parsed_item(char **subject, char **predicate, char **object, char **datatype, char *lang, int operation, int in_block, int current_level, int *pos, int *status) {
        if(subject == NULL || *subject == NULL) return NULL;
//printf("Save parsed item %s - %s\n", *subject, *predicate);
        char *message = NULL;
        bool done = false;
        if (in_block == B_PATTERN && *subject[0] && operation == OP_PUT) {
            message = this->make_triple(subject, predicate, object, datatype, lang, status);
            done = true;
        }
        else if (in_block == B_PATTERN && *subject[0] && operation == OP_DELETE) {
            message = this->delete_triple(subject, predicate, object, datatype, lang, status);
            done = true;
        }
        else if (in_block == B_PATTERN && *subject[0] && operation == OP_GET) {
            message = this->get_triple(subject, predicate, object, datatype, lang, status);
            done = true;
        }
        else if (in_block == B_CHAIN && *subject[0] && operation == OP_GET) {
            message = this->make_triple(subject, predicate, object, datatype, lang, status);
            done = true;
        }
        else if (in_block == B_ORDER && *subject[0] && operation == OP_GET) {
            message = this->save_order(*subject, *predicate, status);
            done = true;
        }
        else if (in_block == B_FILTER && *subject[0] && operation == OP_GET) {
            message = this->save_filter(*subject, *predicate, *object, status, current_level);
            done = true;
        }
        else if (in_block == B_PREFIX && (*subject[0] || *predicate[0])) {
            message = this->make_prefix(*subject, *predicate, status);
            done = true;
        }
        if(*subject) { free(*subject); *subject = NULL; }
        if(*predicate) { free(*predicate); *predicate = NULL; }
        if(*object) { free(*object); *object = NULL; }
        if(*datatype) { free(*datatype); *datatype = NULL; }
        return message;
    }

    char *make_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
    {
        char *message;
//printf("Make triple %s - %s - %s - %s - %s\n", *subject, *predicate, *object, *datatype, lang);
        if( strcmp(*subject, "*") == 0 || strcmp(*subject, "*") == 0 ) {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "You cannot use * as a subject or predicate");
            *status = -1;
            return message;
        }
        if (resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1) {
            *status = -1;
            return message;
        }
        if (this->_n_triples >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        this->_triples[this->_n_triples].s = (char *)malloc(strlen(*subject) + 1);
        if(!this->_triples[this->_n_triples].s) out_of_memory();
        strcpy(this->_triples[this->_n_triples].s, *subject);
        this->_triples[this->_n_triples].p = (char *)malloc(strlen(*predicate) + 1);
        if(!this->_triples[this->_n_triples].p) out_of_memory();
        strcpy(this->_triples[this->_n_triples].p, *predicate);
        this->_triples[this->_n_triples].o = (char *)malloc(strlen(*object) + 1);
        if(!this->_triples[this->_n_triples].o) out_of_memory();
        strcpy(this->_triples[this->_n_triples].o, *object);
        int datatype_len = 0;
        if(datatype != NULL) {
            if(*datatype != NULL) {
                if(*datatype[0] != 0) {
                    this->_triples[this->_n_triples].d = (char *)malloc(strlen(*datatype) + 1);
                    if(!this->_triples[this->_n_triples].d) out_of_memory();
                    strcpy(this->_triples[this->_n_triples].d, *datatype);
                    datatype_len = strlen(*datatype);
                }
            }
        }
        if(!datatype_len) this->_triples[this->_n_triples].d = NULL;
        strcpy(this->_triples[this->_n_triples].l, lang);
        char *data = (char*)malloc(strlen(*subject) + strlen(*predicate) + strlen(*object) + datatype_len + 128);
        if(!data) out_of_memory();
        sprintf(data, "%s | %s | %s | %s", *subject, *predicate, *object, lang);
        if(datatype_len)
            strcat(data, *datatype);
        SHA1((unsigned char *)data, strlen(data), this->_triples[this->_n_triples].hash);
        free(data);
        this->_triples[this->_n_triples].mini_hash = get_mini_hash(this->_triples[this->_n_triples].hash);
        this->_n_triples++;
        return NULL;
    }

    char *delete_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
    {
        char *message;
        if (resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1) {
            *status = -1;
            return message;
        }
        if (this->_n_triples >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        unsigned long n = 0;
        unsigned long *res = find_matching_triples(*subject, *predicate, *object, (datatype ? *datatype : NULL), lang, &n);
        if (n > 0)
        {
            if (this->_to_delete)
                this->_to_delete = (unsigned long *)realloc(this->_to_delete, sizeof(unsigned long) * (this->_n_delete + n));
            else
                this->_to_delete = (unsigned long *)malloc(sizeof(unsigned long) * (this->_n_delete + n));
            if(!this->_to_delete) out_of_memory();
            for (unsigned long i = 0; i < n; i++)
                this->_to_delete[this->_n_delete + i] = res[i];
            this->_n_delete += n;
        }
        free(res);
        return NULL;
    }

    char *get_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
    {
        char *message;
        if (resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1)
        {
            *status = -1;
            return message;
        }
        if (this->_n_triples >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        if (this->n_var > 0)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Variables are not allowed in patterns; use Chain parameter");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(local_triple));
        unsigned long n = 0;
        unsigned long *res = find_matching_triples(*subject, *predicate, *object, (datatype ? *datatype : NULL), lang, &n);
        if (n > 0)
        {
            if (this->_to_send)
                this->_to_send = (unsigned long *)realloc(this->_to_send, sizeof(unsigned long) * (this->_n_send + n));
            else
                this->_to_send = (unsigned long *)malloc(sizeof(unsigned long) * (this->_n_send + n));
            if(!this->_to_send) out_of_memory();
            for (unsigned long i = 0; i < n; i++)
                this->_to_send[this->_n_send + i] = res[i];
            this->_n_send += n;
        }
        free(res);
        *status = 0;
        return NULL;
    }

    char *save_order(char *subject, char *object, int *status)
    {
        char *message;
        if (this->_n_prefixes >= 32)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many order sequences");
            *status = -1;
            return message;
        }
        strcpy(this->_orders[this->_n_orders].variable, subject);
        strtolower(object);
        strcpy(this->_orders[this->_n_orders].direction, object);
        this->_n_orders++;
        return NULL;
    }

    char *save_filter(char *subject, char *predicate, char *object, int *status, int current_level)
    {
        char *message;
        if (this->_n_filters >= 32)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many filter sequences");
            *status = -1;
            return message;
        }
//printf("%i %s - %s - %s\n", current_level, subject, predicate, object);
        // Filters grouping
        if(subject[0] && predicate == NULL && object == NULL) {
            strtolower(subject);
            this->_filter_groups[this->_n_filter_groups].logic = ((strcmp(subject, "or") == 0) ? LOGIC_OR : LOGIC_AND);
            this->_filter_groups[this->_n_filter_groups].group = (current_level > 0 ? this->current_filter_group[current_level - 1] : -1);
            this->current_filter_group[current_level] = this->_n_filter_groups;
            this->_n_filter_groups++;
            return NULL;
        }
        strcpy(this->_filters[this->_n_filters].variable, subject);
        strtolower(predicate);
        if(strcmp(predicate, "contains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_CONTAINS;
        else if(strcmp(predicate, "notcontains") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTCONTAINS;
        else if(strcmp(predicate, "equal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_EQUAL;
        else if(strcmp(predicate, "notequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTEQUAL;
        else if(strcmp(predicate, "more") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_MORE;
        else if(strcmp(predicate, "less") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_LESS;
        else if(strcmp(predicate, "moreorequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_MOREOREQUAL;
        else if(strcmp(predicate, "lessorequal") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_LESSOREQUAL;
        else if(strcmp(predicate, "exists") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_EXISTS;
        else if(strcmp(predicate, "notexists") == 0)
            this->_filters[this->_n_filters].operation = COMPARE_NOTEXISTS;
        else {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Unknown filter operation: %s", predicate);
            *status = -1;
            return message;
        }
        strcpy(this->_filters[this->_n_filters].value, object);
        this->_filters[this->_n_filters].group = (current_level > 0 ? this->current_filter_group[current_level - 1] : -1);
        this->_n_filters++;
        return NULL;
    }

    char *make_prefix(char *subject, char *object, int *status)
    {
        char *message;
        if (this->_n_prefixes >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) out_of_memory();
            sprintf(message, "Too many prefixes");
            *status = -1;
            return message;
        }
        strcpy(this->_prefixes[this->_n_prefixes].shortcut, subject);
        strcpy(this->_prefixes[this->_n_prefixes].value, object);
        this->_prefixes[this->_n_prefixes].len = strlen(this->_prefixes[this->_n_prefixes].shortcut);
        this->_n_prefixes++;
        return NULL;
    }

    char *resolve_prefix(char **s)
    {
        if (strncmp(*s, "http", 4) == 0 || *s[0] == '*')
            return NULL;
        if (*s[0] == '?')
        {
            bool found = false;
            for (int i = 0; i < this->n_var; i++)
            {
                if (strcmp(this->var[i].obj, *s) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found && this->n_var < 32)
                strcpy(this->var[this->n_var++].obj, *s);
            return NULL;
        }
        for (int i = 0; i < *n_prefixes; i++)
        {
            if (strncmp(*s, prefixes[i].shortcut, prefixes[i].len) == 0 && ((char)*(char*)((unsigned long)*s + prefixes[i].len) == ':'))
            {
                char *st = (char*)malloc(strlen(*s) + strlen(prefixes[i].value) + 1);
                if(!st) out_of_memory();
                strcpy(st, prefixes[i].value);
                strcat(st, (char *)((unsigned long)(*s) + prefixes[i].len + 1));
                *s = (char*)realloc(*s, strlen(st) + 1);
                if(!*s) out_of_memory();
                strcpy(*s, st);
                free(st);
                return NULL;
            }
        }
        // Default prefix
        for (int i = 0; i < *n_prefixes; i++)
        {
            if (prefixes[i].shortcut[0] != 0)
                continue;
            char *st = (char*)malloc(strlen(*s) + strlen(prefixes[i].value) + 1024);
            if(!st) out_of_memory();
            strcpy(st, prefixes[i].value);
            strcat(st, *s);
            *s = (char*)realloc(*s, strlen(st) + 1024);
            if(!*s) out_of_memory();
            strcpy(*s, st);
            free(st);
            break;
        }
        return NULL;
    }

    int resolve_prefixes_in_triple(char **subject, char **predicate, char **object, char **datatype, char **message)
    {
        if (*subject[0])
        {
            if (*message = this->resolve_prefix(subject))
                return -1;
        }
        if (*predicate[0])
        {
            if (*message = this->resolve_prefix(predicate))
                return -1;
        }
        // If the object is not a literal
        bool is_literal = false;
        if(datatype != NULL) {
            if(*datatype != NULL) {
                if (*message = this->resolve_prefix(datatype))
                    return -1;
                if (*datatype[0] != 0)
                    is_literal = true;
            }
        }
        if(!is_literal) {
            if (*message = this->resolve_prefix(object))
                return -1;
        }
        return 0;
    }

    char *commit_prefixes()
    {
        //logger(LOG, "Commit prefixes", "", this->_n_prefixes);
        bool added = false;
        sem_wait(psem);
        for (int i = 0; i < this->_n_prefixes; i++)
        {
            bool found = false;
            for (int j = 0; j < (*n_prefixes); j++)
            {
                if (strcmp(prefixes[j].shortcut, this->_prefixes[i].shortcut) == 0)
                {
                    found = true;
                    if (strcmp(prefixes[j].value, this->_prefixes[i].value) != 0)
                    {
                        strcpy(prefixes[j].value, this->_prefixes[i].value);
                        added = true;
                    }
                }
            }
            if (!found)
            {
                memcpy(&prefixes[(*n_prefixes)++], &this->_prefixes[i], sizeof(prefix));
                added = true;
            }
        }
        sem_post(psem);
        save_globals();
        //logger(LOG, "Prefixes committed", "", *n_prefixes);
        return NULL;
    }

    char *return_prefixes(void)
    {
        char *result = (char*)malloc((*n_prefixes)*1600);
        if(!result) out_of_memory();
        sprintf(result, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Prefixes\": [", this->request_id);
        //logger(LOG, "Get prefixes", "", this->_n_prefixes);
        for (int j = 0; j < (*n_prefixes); j++)
        {
            if(j>0) strcat(result, ", ");
            sprintf((char*)((unsigned long)result+strlen(result)), "{\"%s\": \"%s\"}", prefixes[j].shortcut, prefixes[j].value);
        }
        strcat(result,"]}");
        return result;
    }

    char *delete_prefixes(void)
    {
        //logger(LOG, "Delete prefixes", "", this->_n_prefixes);
        sem_wait(psem);
        for (int i = 0; i < this->_n_prefixes; i++)
        {
            bool found = false;
            for (int j = 0; j < (*n_prefixes); j++)
            {
                if (strcmp(prefixes[j].shortcut, this->_prefixes[i].shortcut) == 0)
                {
                    if((*n_prefixes) - 1 > j)
                        memmove(&prefixes[j], &prefixes[j+1], sizeof(prefix)*((*n_prefixes)-j-1));
                    memset(&prefixes[(*n_prefixes)-1], 0, sizeof(prefix));
                    (*n_prefixes)--;
                }
            }
        }
        sem_post(psem);
        save_globals();
        //logger(LOG, "Prefixes deleted", "", *n_prefixes);
        return NULL;
    }

    char *commit_triples(int *pip)
    {
        if (pip[1] == -1)
            return NULL;
        char *message = (char *)malloc(1024 * 10);
        if(!message) out_of_memory();
        message[0] = 0;
        sem_wait(wsem);
        //logger(LOG, "(child) Commit triples", "", this->_n_triples);
        for (int i = 0; i < this->_n_triples; i++)
        {
//logger(LOG, "Commit triple", "", i);
            // Send to the parent process for commit
            int command = COMMIT_TRIPLE;
            if (!write_to_pipe(pip[1], command, &this->_triples[i]))
            {
                strcpy(message, "Pipe write failed");
                sem_post(wsem);
                return message;
            }
            int ret = 0;
            if (!read(pip[2], &ret, sizeof(int)))
                logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
            if (ret == RESULT_ERROR)
                strcat(message, "Transaction commit error. ");
            if (ret & RESULT_CHUNKS_REBUILT)
                chunks_rebuilt = true;
            if (ret & RESULT_TRIPLES_REALLOCATED)
                triples_reallocated = true;
        }
        sem_post(wsem);
        if (message[0])
            return message;
        free(message);
        return NULL;
    }

    char *commit_delete(int *pip)
    {
        //logger(LOG, "(child) Commit delete", "", this->_n_delete);
        if (pip[1] == -1)
            return NULL;
        char *message = (char *)malloc(1024 * 10);
        if(!message) out_of_memory();
        message[0] = 0;
        int command = DELETE_TRIPLE;
        sem_wait(wsem);
        for (int i = 0; i < this->_n_delete; i++)
        {
            if (!write(pip[1], &command, sizeof(int)))
            {
                char *message = (char *)malloc(128);
                if(!message) out_of_memory();
                strcpy(message, "Error writing to pipe");
                sem_post(wsem);
                return message;
            }
            if (!write(pip[1], &this->_to_delete[i], sizeof(unsigned long)))
            {
                char *message = (char *)malloc(128);
                if(!message) out_of_memory();
                strcpy(message, "Error writing to pipe");
                sem_post(wsem);
                return message;
            }
            int ret = 0;
            if (!read(pip[2], &ret, sizeof(int)))
                logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
            if (ret == RESULT_ERROR)
                strcat(message, "Transaction commit error. ");
        }
        sem_post(wsem);
        free(this->_to_delete);
        if (message[0])
            return message;
        free(message);
        return NULL;
    }

    void free_all(void)
    {
        for (int i = 0; i < this->_n_triples; i++)
        {
            if (this->_triples[i].s) free(this->_triples[i].s);
            if (this->_triples[i].p) free(this->_triples[i].p);
            if (this->_triples[i].o) free(this->_triples[i].o);
            if (this->_triples[i].d) free(this->_triples[i].d);
        }
        if (prefixes)
            munmap(prefixes, N_MAX_PREFIX * sizeof(prefix));
        if (triples && !triples_reallocated)
            munmap(triples, (*allocated) * sizeof(triple));
        if (chunks_size)
            munmap(chunks_size, MAX_CHUNKS * sizeof(unsigned long));
        if (s_chunks_size)
            munmap(s_chunks_size, MAX_CHUNKS * sizeof(unsigned long));
        if (p_chunks_size)
            munmap(p_chunks_size, MAX_CHUNKS * sizeof(unsigned long));
        if (o_chunks_size)
            munmap(o_chunks_size, MAX_CHUNKS * sizeof(unsigned long));
        if (!chunks_rebuilt)
        {
            if (full_index)
                munmap(full_index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
            if (s_index)
                munmap(s_index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
            if (p_index)
                munmap(p_index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
            if (o_index)
                munmap(o_index, (*n_chunks) * CHUNK_SIZE * sizeof(mini_index));
        }
        if (stringtable)
            munmap(stringtable, *string_allocated);
        if (global_block_ul)
            munmap(global_block_ul, sizeof(unsigned long) * N_GLOBAL_VARS);
        triples = NULL;
        full_index = NULL;
        s_index = NULL;
        p_index = NULL;
        o_index = NULL;
        stringtable = NULL;
        global_block_ul = NULL;
        if (this->n_pcv != 0) {
            for (int i = 0; i < this->n_pcv; i++) {
                free(this->pre_comb_value[i]);
                if(this->pre_comb_value_type[i]) free(this->pre_comb_value_type[i]);
                if(this->pre_comb_value_lang[i]) free(this->pre_comb_value_lang[i]);
            }
            free(this->pre_comb_value); free(this->pre_comb_value_type); free(this->pre_comb_value_lang);
        }
    }

    char *return_triples(void) {
        //logger(LOG, "(child) Return triples", "", this->_n_send);
        char *answer = (char *)malloc(this->_n_send * 1024 * 10);
        if(!answer) out_of_memory();
        if(this->modifier & MOD_COUNT) {
            sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Count\":\"%i\"}", this->request_id, this->_n_send);
            return answer;
        }
        sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Triples\":[", this->request_id);
        // Sort combinations. Coarse, temporary implementation, could be optimized
        for(int j=this->_n_orders-1; j>=0; j--) {
            if(strcmp(this->_orders[j].direction, "desc") == 0) sort_order = 1;
            else sort_order = 0;
            char **unique = (char**)malloc(this->_n_send*sizeof(char*));
            if(!unique) out_of_memory();
            unsigned long *orders = (unsigned long*)malloc(this->_n_send*sizeof(unsigned long));
            if(!orders) out_of_memory();
            int sort_var_index = 0;
            if(strcmp(this->_orders[j].variable, "predicate") == 0) sort_var_index = 1;
            else if(strcmp(this->_orders[j].variable, "object") == 0) sort_var_index = 2;
            for (int i = 0; i < this->_n_send; i++) {
                int pos = (sort_var_index == 1 ? triples[this->_to_send[i]].p_pos : (sort_var_index == 2 ? triples[this->_to_send[i]].o_pos : triples[this->_to_send[i]].s_pos ));
                unique[i] = get_string(pos);
            }
            qsort(unique, this->_n_send, sizeof(char*), cstring_cmp);
            // Cycle through sorted unique items
            for (int y = 0; y < this->_n_send; y++) {
                // Cycle through all triples to find the next unique item
                for (int z = 0; z < this->_n_send; z++) {
                    bool placed = false;
                    char *obj = get_string((sort_var_index == 1 ? triples[this->_to_send[z]].p_pos : (sort_var_index == 2 ? triples[this->_to_send[z]].o_pos : triples[this->_to_send[z]].s_pos )));
                    if(strcmp(obj, unique[y]) == 0) {
                        bool found = false;
                        for(int v=0; v<y; v++) {
                            if(orders[v] == this->_to_send[z]) {
                                found = true;
                                break;
                            }
                        }
                        if(!found) {
                            orders[y] = this->_to_send[z];
                            placed = true;
                        }
                    }
                    if(placed) break;
                }
            }
            memcpy(this->_to_send, orders, this->_n_send*sizeof(unsigned long));
            free(unique); free(orders);
        }
        for (int i = 0; i < this->_n_send; i++) {
            char ans[1024 * 10];
            sprintf(ans, "[\"%s\", \"%s\", \"%s\"", get_string(triples[this->_to_send[i]].s_pos), get_string(triples[this->_to_send[i]].p_pos), get_string(triples[this->_to_send[i]].o_pos));
            if (triples[this->_to_send[i]].d_len) {
                char *dt = get_string(triples[this->_to_send[i]].d_pos);
                if (dt)
                    sprintf((char *)((unsigned long)ans + strlen(ans)), ", \"%s\"", dt);
                else
                    strcat(ans, ", \"\"");
                if (triples[this->_to_send[i]].l[0])
                    sprintf((char *)((unsigned long)ans + strlen(ans)), ", \"%s\"", triples[this->_to_send[i]].l);
            }
            strcat(ans, "]");
            if (i > 0)
                strcat(answer, ", ");
            strcat(answer, ans);
        }
        strcat(answer, "]}");
        return answer;
    }

    // Triples chain request processing functions

    // Find index in the conditions array for the given variable
    int find_cond(char *obj)
    {
        for (int j = 0; j < this->n_var; j++)
        {
            if (strcmp(obj, this->var[j].obj) == 0)
                return j;
        }
        return -1;
    }

    // Add condition for the new variable
    int add_cond(char *obj)
    {
        if (this->n_var >= 32)
            return -1;
        strcpy(this->var[this->n_var].obj, obj);
        return this->n_var++;
    }

    // Recursively propagates variables dependence
    void propagate_dependent(int i)
    {
        this->depth++;
        if (this->depth > 10)
            return;
        this->var[i].dep++;
        for (int j = 0; j < this->var[i].n; j++)
        {
            if (this->var[this->var[i].cond_p[j]].obj[0] == '?')
                propagate_dependent(this->var[i].cond_p[j]);
            if (this->var[this->var[i].cond_o[j]].obj[0] == '?')
                propagate_dependent(this->var[i].cond_o[j]);
        }
    }

    // Add a new candidate object (value) for some variable
    // i is the variable index in this->var, s is the variable value, subject is the subject to which the value belongs (if any), linkage is the predicate between subject and the variable, datatype and lang are the value's metadata
    void add_candidate(int i, char *s, char *subject, char *linkage, char *datatype, char *lang)
    {
//printf("add candidate %s (subject = %s, linkage = %s, datatype = %s, lang = %s)\n", s, subject, linkage, datatype, lang);
        bool found = false;
        for (int j = 0; j < this->var[i].n_cand; j++)
        {
            if (strcmp(this->var[i].cand[j], s) == 0 && strcmp(this->var[i].cand_s[j], subject) == 0 && strcmp(this->var[i].cand_p[j], linkage) == 0)
            {
                found = true;
                break;
            }
        }
        if (found)
            return;
        if (!this->var[i].cand)
        {
            this->var[i].cand = (char **)malloc(1024 * sizeof(char *));
            this->var[i].cand_s = (char **)malloc(1024 * sizeof(char *));
            this->var[i].cand_p = (char **)malloc(1024 * sizeof(char *));
            this->var[i].cand_d = (char **)malloc(1024 * sizeof(char *));
            this->var[i].cand_l = (char **)malloc(1024 * sizeof(char *));
            if(!this->var[i].cand || !this->var[i].cand_s || !this->var[i].cand_p || !this->var[i].cand_d || !this->var[i].cand_l) out_of_memory();
        }
        else if (this->var[i].n_cand % 1024 == 0)
        {
            this->var[i].cand = (char **)realloc(this->var[i].cand, (this->var[i].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[i].cand_s = (char **)realloc(this->var[i].cand_s, (this->var[i].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[i].cand_p = (char **)realloc(this->var[i].cand_p, (this->var[i].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[i].cand_d = (char **)realloc(this->var[i].cand, (this->var[i].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[i].cand_l = (char **)realloc(this->var[i].cand, (this->var[i].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            if(!this->var[i].cand || !this->var[i].cand_s || !this->var[i].cand_p || !this->var[i].cand_d || !this->var[i].cand_l) out_of_memory();
        }
        this->var[i].cand[this->var[i].n_cand] = s;
        this->var[i].cand_s[this->var[i].n_cand] = subject;
        this->var[i].cand_p[this->var[i].n_cand] = linkage;
        this->var[i].cand_d[this->var[i].n_cand] = datatype;
        this->var[i].cand_l[this->var[i].n_cand] = lang;
        this->var[i].n_cand++;
    }

    // Fill candidate objects (values) array for j-th variable using its linkage from i-th variable
    bool get_candidates(int i, int j, char *subject, char *predicate, char *object, bool negative)
    {
        unsigned long n = 0;
//printf("Find %s - %s - %s\n", subject, predicate, object);
        unsigned long *res = find_matching_triples(subject, predicate, object, NULL, NULL, &n);
//printf("found %i items\n", n);
        if( negative ) {
            free(res);
            if( n > 0 )
                return false;
            return true;
        }
        for (unsigned long k = 0; k < n; k++)
        {
            if (subject[0] == '*')
                add_candidate(i, get_string(triples[res[k]].s_pos), NULL, NULL, NULL, NULL);
            if (predicate[0] == '*')
                add_candidate(this->var[i].cond_p[j], get_string(triples[res[k]].p_pos), subject, object, NULL, NULL);
            if (object[0] == '*')
                add_candidate(this->var[i].cond_o[j], get_string(triples[res[k]].o_pos), subject, predicate, triples[res[k]].d_pos ? get_string(triples[res[k]].d_pos) : NULL, triples[res[k]].l);
        }
        free(res);
        if( n > 0 ) return true;
        return false;
    }

    // Recursively build single combination (single row of response)
    void build_combination(int i, int ref_by_p, char *ref_by_s)
    {
//printf("\nEnter build_combination var = %s, ref_by_p = %s, ref_by_s = %s\n", this->var[i].obj, this->var[ref_by_p].obj, ref_by_s);
        for (int j = 0; j < n_comb; j++)
        {
            // Error: this variable is already present in the current combination
            if (comb_var[j] == i)
                return;
        }
        comb_var[n_comb++] = i;
        // Cycle through all candidate objects
        for (int j = 0; j < this->var[i].n_cand; j++)
        {
            if (!this->var[i].cand[j]) continue;
            if (ref_by_p != -1)
            {
                if (strcmp(this->var[ref_by_p].obj, this->var[i].cand_p[j]) != 0)
                    continue;
            }
            if (ref_by_s != NULL)
            {
                if (strcmp(ref_by_s, this->var[i].cand_s[j]) != 0)
                    continue;
            }
            comb_value[n_comb - 1] = this->var[i].cand[j];
            comb_value_type[n_comb - 1] = this->var[i].cand_d[j];
            comb_value_lang[n_comb - 1] = this->var[i].cand_l[j];
            // Cycle through all references from this variable to others
            int nref = 0;
            for (int k = 0; k < this->var[i].n; k++)
            {
                // Predicate is a variable
                if (this->var[this->var[i].cond_p[k]].obj[0] == '?')
                    continue;
                if (this->var[this->var[i].cond_o[k]].obj[0] == '?') {
                    nref++;
                    build_combination(this->var[i].cond_o[k], this->var[i].cond_p[k], this->var[i].cand[j]);
                }
            }
            // If there is no further conditions on this variable, i.e. it is the end of branch
            if (nref == 0)
            {
                if (this->n_pcv == 0) {
                    this->pre_comb_value = (char ***)malloc(1024 * sizeof(char **));
                    this->pre_comb_value_type = (char ***)malloc(1024 * sizeof(char **));
                    this->pre_comb_value_lang = (char ***)malloc(1024 * sizeof(char **));
                    if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) out_of_memory();
                }
                else if (this->n_pcv % 1024 == 0) {
                    this->pre_comb_value = (char ***)realloc(this->pre_comb_value, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                    this->pre_comb_value_type = (char ***)realloc(this->pre_comb_value_type, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                    this->pre_comb_value_lang = (char ***)realloc(this->pre_comb_value_lang, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                    if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) out_of_memory();
                }
                this->pre_comb_value[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                if(!this->pre_comb_value[this->n_pcv]) out_of_memory();
                memset(this->pre_comb_value[this->n_pcv], 0, 32 * sizeof(char *));
                this->pre_comb_value_type[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                if(!this->pre_comb_value_type[this->n_pcv]) out_of_memory();
                memset(this->pre_comb_value_type[this->n_pcv], 0, 32 * sizeof(char *));
                this->pre_comb_value_lang[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                if(!this->pre_comb_value_lang[this->n_pcv]) out_of_memory();
                memset(this->pre_comb_value_lang[this->n_pcv], 0, 32 * sizeof(char *));
                for (int l = 0; l < n_comb; l++) {
                    this->pre_comb_value[this->n_pcv][comb_var[l]] = comb_value[l];
                    if(comb_value_type[l])
                        this->pre_comb_value_type[this->n_pcv][comb_var[l]] = comb_value_type[l];
                    if(comb_value_lang[l]) {
                        if(comb_value_lang[l][0])
                            this->pre_comb_value_lang[this->n_pcv][comb_var[l]] = comb_value_lang[l];
                    }
                }
                this->n_pcv++;
            }
        }
        n_comb--;
    }

    // Entry point for triples chain request
    char *return_triples_chain(char *reqid)
    {
        int order[32], max_dep = 0, n = 0;
        char star[2] = "*";
        memset(order, 0, sizeof(int) * 32);
        // Fill the conditions
//printf("Patterns: %i\n", this->_n_triples);
        for (int i = 0; i < this->_n_triples; i++)
        {
            int ics = find_cond(this->_triples[i].s);
//printf("%s\n", this->_triples[i].s);
            if (ics == -1)
                ics = add_cond(this->_triples[i].s);
            int icp = find_cond(this->_triples[i].p);
            if (icp == -1)
                icp = add_cond(this->_triples[i].p);
            int ico = find_cond(this->_triples[i].o);
            if (ico == -1)
                ico = add_cond(this->_triples[i].o);
            if (ics == -1 || icp == -1 || ico == -1)
                continue;
            this->var[ics].cond_p[this->var[ics].n] = icp;
            this->var[ics].cond_o[this->var[ics].n++] = ico;
            if (this->var[icp].obj[0] == '?')
                this->var[icp].dep++;
            if (this->var[ico].obj[0] == '?')
                this->var[ico].dep++;
        }
        // Find variables dependence to define the variables resolution order
        for (int i = 0; i < this->n_var; i++)
        {
            for (int j = 0; j < this->var[i].n; j++)
            {
                if (this->var[this->var[i].cond_p[j]].obj[0] == '?')
                    propagate_dependent(this->var[i].cond_p[j]);
                if (this->var[this->var[i].cond_o[j]].obj[0] == '?')
                    propagate_dependent(this->var[i].cond_o[j]);
            }
        }
        // Search for maximal dependence
        for (int i = 0; i < this->n_var; i++)
        {
            if (this->var[i].dep > max_dep)
                max_dep = this->var[i].dep;
                // Along with that, mark variables that must not exist
                for(int j = 0; j < this->_n_filters; j++) {
                    if(this->_filters[j].operation == COMPARE_NOTEXISTS && strcmp(this->_filters[j].variable, this->var[i].obj) == 0)
                        this->var[i].notexists = true;
                }
        }
        // Build an array with the variables processing order
        for (int i = 0; i <= max_dep; i++)
        {
            for (int j = 0; j < this->n_var; j++)
            {
                if (this->var[j].dep == i)
                    order[n++] = j;
            }
        }
        // Consequently find variable values
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            char *subject = this->var[i].obj;
            // Cycle the conditions on some subject
            for (int j = 0; j < this->var[i].n; j++)
            {
                char *predicate = this->var[this->var[i].cond_p[j]].obj;
                char *object = this->var[this->var[i].cond_o[j]].obj;
                if (subject[0] == '?' && this->var[i].n_cand)
                {
                    for (int l = 0; l < this->var[i].n_cand; l++) {
//printf("case 1: %s - %s - %s\n", this->var[i].cand[l], predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object);
                        bool match = get_candidates(i, j, this->var[i].cand[l], predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[this->var[i].cond_o[j]].notexists);
//printf("got %d for %s, notex is %d\n", match, this->var[this->var[i].cond_o[j]].obj, this->var[this->var[i].cond_o[j]].notexists);
                        if( this->var[this->var[i].cond_o[j]].notexists && !match ) {
                            this->var[i].cand[l] = NULL;
                        }
                    }
                }
                else {
//printf("case 2: %s - %s - %s\n", subject[0] == '?' ? star : subject, predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object);
                    bool match = get_candidates(i, j, subject[0] == '?' ? star : subject, predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[i].notexists);
                }
            }
        }
    // Remove references to non-existant variables
    for (int i = 0; i < n; i++) {
        for(int j=0; j<this->var[i].n; j++) {
            if (this->var[this->var[i].cond_p[j]].notexists || this->var[this->var[i].cond_o[j]].notexists) {
                if (j < this->var[i].n-1) {
                    memmove(&this->var[i].cond_p[j], &this->var[i].cond_p[j+1], sizeof(int)*(this->var[i].n - j));
                    memmove(&this->var[i].cond_o[j], &this->var[i].cond_o[j+1], sizeof(int)*(this->var[i].n - j));
                }
                this->var[i].n --;
            }
        }
    }
/*
printf("Variables:\n");
for(int x=0; x<n; x++) {
    int i = order[x];
    if(this->var[i].obj[0] != '?' && this->var[i].n == 0)
        continue;
    printf("%i:\t%s, dep = %i\n", i, this->var[i].obj, this->var[i].dep);
    for(int j=0; j<this->var[i].n; j++)
        printf("\t\t%s => %s\n", this->var[this->var[i].cond_p[j]].obj, this->var[this->var[i].cond_o[j]].obj);
    if(!this->var[i].n_cand) continue;
    printf("\tCandidates:\n");
    for(int j=0; j<this->var[i].n_cand; j++) {
        if(!this->var[i].cand[j]) continue;
        printf("\t\t%s", this->var[i].cand[j]);
        if(this->var[i].cand_s[j])
            printf("\t(%s -> %s)", this->var[i].cand_s[j], this->var[i].cand_p[j]);
        printf("\n");
    }
}

printf("Filter groups: %i\n", this->_n_filter_groups);
for(int i=0; i<this->_n_filter_groups; i++) {
    printf("%i: %i, %i\n", i, this->_filter_groups[i].group, this->_filter_groups[i].logic);
}

printf("Filters: %i\n", this->_n_filters);
for(int i=0; i<this->_n_filters; i++) {
    printf("%i (%i): %s - %i - %s\n", i, this->_filters[i].group, this->_filters[i].variable, this->_filters[i].operation, this->_filters[i].value);
}
*/
        // Search for the first variable having candidates
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            if (this->var[i].obj[0] != '?' || this->var[i].n_cand == 0 || this->var[i].notexists)
                continue;
            // Cycle through candidates and build combination starting with each of them
            n_comb = 0;
            build_combination(i, -1, NULL);
            break;
        }
        for (int i = 0; i < n; i++) {
            free(this->var[i].cand);
            free(this->var[i].cand_s);
            free(this->var[i].cand_p);
            free(this->var[i].cand_d);
            free(this->var[i].cand_l);
        }

        // Cycle through incomplete combinations to make complete ones
        for (int z = 0; z < this->n_pcv; z++)
        {
            // Cycle through all remaining combinations
            int npcv = this->n_pcv;
            for (int x = z + 1; x < npcv; x++)
            {
                bool compat = true, inequal = false;
                // Every non-empty element of the compatible remaining combination must be present in the source combination, if the value of the same variable is set in the source combination
                for (int v = 0; v < 32; v++)
                {
                    if (this->pre_comb_value[z][v] == NULL)
                    {
                        if (this->pre_comb_value[x][v] != NULL)
                            inequal = true;
                        continue;
                    }
                    if (this->pre_comb_value[x][v] == NULL)
                        continue;
                    if (strcmp(this->pre_comb_value[z][v], this->pre_comb_value[x][v]) != 0)
                    {
                        compat = false;
                        break;
                    }
                }
                // Sub-combinations are compatible and not equal, let us join them
                if (compat && inequal)
                {
                    if (this->n_pcv == 0) {
                        this->pre_comb_value = (char ***)malloc(1024 * sizeof(char **));
                        this->pre_comb_value_type = (char ***)malloc(1024 * sizeof(char **));
                        this->pre_comb_value_lang = (char ***)malloc(1024 * sizeof(char **));
                        if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) out_of_memory();
                    }
                    else if (this->n_pcv % 1024 == 0) {
                        this->pre_comb_value = (char ***)realloc(this->pre_comb_value, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        this->pre_comb_value_type = (char ***)realloc(this->pre_comb_value_type, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        this->pre_comb_value_lang = (char ***)realloc(this->pre_comb_value_lang, (this->n_pcv / 1024 + 1) * 1024 * sizeof(char **));
                        if(!this->pre_comb_value || !this->pre_comb_value_type || !this->pre_comb_value_lang) out_of_memory();
                    }
                    this->pre_comb_value[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                    if(!this->pre_comb_value[this->n_pcv]) out_of_memory();
                    memset(this->pre_comb_value[this->n_pcv], 0, 32 * sizeof(char *));
                    this->pre_comb_value_type[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                    if(!this->pre_comb_value_type[this->n_pcv]) out_of_memory();
                    memset(this->pre_comb_value_type[this->n_pcv], 0, 32 * sizeof(char *));
                    this->pre_comb_value_lang[this->n_pcv] = (char **)malloc(32 * sizeof(char *));
                    if(!this->pre_comb_value_lang[this->n_pcv]) out_of_memory();
                    memset(this->pre_comb_value_lang[this->n_pcv], 0, 32 * sizeof(char *));
                    for (int v = 0; v < 32; v++)
                    {
                        if (this->pre_comb_value[z][v] != NULL) {
                            this->pre_comb_value[this->n_pcv][v] = this->pre_comb_value[z][v];
                            this->pre_comb_value_type[this->n_pcv][v] = this->pre_comb_value_type[z][v];
                            this->pre_comb_value_lang[this->n_pcv][v] = this->pre_comb_value_lang[z][v];
                        }
                        if (this->pre_comb_value[x][v] != NULL) {
                            this->pre_comb_value[this->n_pcv][v] = this->pre_comb_value[x][v];
                            this->pre_comb_value_type[this->n_pcv][v] = this->pre_comb_value_type[x][v];
                            this->pre_comb_value_lang[this->n_pcv][v] = this->pre_comb_value_lang[x][v];
                        }
                    }
                    this->n_pcv++;
                }
            }
        }

        // Apply filters
        for (int z = 0; z < this->n_pcv; z++) {
            bool filter_match[32], incomplete = false, fixed_lvalue = false, fixed_rvalue = false;
            int group_match[32], res;
            memset(filter_match, 0, 32*sizeof(bool)); memset(group_match, 0, 32*sizeof(int));
            // First, let us check each individual filter
            for(int i=0; i<this->_n_filters; i++) {
                char *lvalue = NULL, *rvalue = NULL;
                if(this->_filters[i].variable[0] == '?') {
                    for (int x = 0; x < n; x++) {
                        if (strcmp(this->var[x].obj, this->_filters[i].variable) == 0) {
                            lvalue = this->pre_comb_value[z][x];
                            break;
                        }
                    }
                }
                else {
                    lvalue = this->_filters[i].variable;
                    fixed_lvalue = true;
                }
                if(this->_filters[i].value[0] == '?') {
                    for (int x = 0; x < n; x++) {
                        if (strcmp(this->var[x].obj, this->_filters[i].value) == 0) {
                            rvalue = this->pre_comb_value[z][x];
                            break;
                        }
                    }
                }
                else {
                    rvalue = this->_filters[i].value;
                    fixed_rvalue = true;
                }
//printf("filter %i, lvalue: %s, rvalue: %s\n", i, lvalue, rvalue);
                if(!lvalue || !rvalue) {
                    incomplete = true;
                    break;
                }
                bool result = false;
                switch(this->_filters[i].operation) {
                    case COMPARE_CONTAINS:
                        if(strstr(lvalue, rvalue))
                            result = true;
                        break;
                    case COMPARE_NOTCONTAINS:
                        if(!strstr(lvalue, rvalue))
                            result = true;
                        break;
                    case COMPARE_EXISTS:
                        if(lvalue[0])
                            result = true;
                        break;
                    case COMPARE_NOTEXISTS:
                        result = true;
                        break;
                    case COMPARE_EQUAL:
                    case COMPARE_NOTEQUAL:
                        res = strcmp(lvalue, rvalue);
                        if((res == 0 && this->_filters[i].operation == COMPARE_EQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                            result = true;
                        if(fixed_lvalue) {
                            char *tmp = (char*)malloc(1024+strlen(lvalue));
                            if(!tmp) out_of_memory();
                            strcpy(tmp, lvalue);
                            this->resolve_prefix(&tmp);
                            res = strcmp(tmp, rvalue);
                            if((res == 0 && this->_filters[i].operation == COMPARE_EQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        if(fixed_rvalue) {
                            char *tmp = (char*)malloc(1024+strlen(rvalue));
                            if(!tmp) out_of_memory();
                            strcpy(tmp, rvalue);
                            this->resolve_prefix(&tmp);
                            res = strcmp(lvalue, tmp);
                            if((res == 0 && this->_filters[i].operation == COMPARE_EQUAL) || (res != 0 && this->_filters[i].operation == COMPARE_NOTEQUAL))
                                result = true;
                            free(tmp);
                        }
                        break;
                    case COMPARE_MORE:
                        if(atof(lvalue) > atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_MOREOREQUAL:
                        if(atof(lvalue) >= atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_LESS:
                        if(atof(lvalue) < atof(rvalue))
                            result = true;
                        break;
                    case COMPARE_LESSOREQUAL:
                        if(atof(lvalue) <= atof(rvalue))
                            result = true;
                        break;
                }
            filter_match[i] = result;
//printf("Result of filter %i: %s\n", i, (result?"true":"false"));
            }
            // Now let us check filters combinations
            if(!incomplete && this->_n_filter_groups > 0) {
                bool resolved;
                int iterations = 1;
                do {
//printf("Iteration %i\n", iterations);
                    resolved = true;
                    for(int i=0; i<this->_n_filter_groups; i++) {
                        if(group_match[i] != 0) continue;
                        // Check ordinary filters included in this group
                        bool one_match = false, one_not_match = false, has_filters = false;
                        for(int j=0; j<this->_n_filters; j++) {
                            if(this->_filters[j].group == i) {
                                if(filter_match[j]) one_match = true;
                                else one_not_match = true;
                                has_filters = true;
                            }
                        }
                        if(has_filters) {
                            if(this->_filter_groups[i].logic == LOGIC_OR) {
                                if(one_match)
                                    group_match[i] = 1;
                                else
                                    group_match[i] = 2;
                            }
                            else if(this->_filter_groups[i].logic == LOGIC_AND) {
                                if(one_not_match)
                                    group_match[i] = 2;
                                else if(one_match)
                                    group_match[i] = 1;
                            }
                        }
                        // Check nested filter groups
                        bool has_member_groups = false, has_undefined_member = false;
                        one_match = one_not_match = false;
                        for(int k=0; k<this->_n_filter_groups; k++) {
                            if(this->_filter_groups[k].group == i) {
                                if(group_match[k] == 1) one_match = true;
                                else if(group_match[k] == 2) one_not_match = true;
                                else has_undefined_member = true;
                                has_member_groups = true;
                            }
                        }
                        if(has_member_groups) {
                            if(has_undefined_member) group_match[i] = 0;
                            else if(this->_filter_groups[i].logic == LOGIC_OR) {
                                if(one_match || group_match[i] == 1)
                                    group_match[i] = 1;
                                else
                                    group_match[i] = 2;
                            }
                            else if(this->_filter_groups[i].logic == LOGIC_AND) {
                                if(one_not_match)
                                    group_match[i] = 2;
                                else if(one_match || group_match[i] == 1)
                                    group_match[i] = 1;
                            }
                        }
                        if(group_match[i] == 0) resolved = false;
//printf("  group %i: %i (has member groups: %i, has_undefined_member: %i)\n", i, group_match[i], has_member_groups, has_undefined_member);
                    }
                    iterations++;
                } while( !resolved && iterations < 32 );
                if(iterations == 32) {
                    char *answer = (char *)malloc(1024);
                    if(!answer) out_of_memory();
                    sprintf(answer, "{\"Status\":\"Error\", \"Message\":\"Cannot resolve filters\"}", this->request_id);
                    return answer;
                }
                resolved = true;
                for(int i=0; i<this->_n_filter_groups; i++) {
                    if(this->_filter_groups[i].group == -1 && group_match[i] == 2)
                        resolved = false;
                }
                // Combination have not passed, make it incompleteto throw out on the next step
                if(!resolved)
                    this->pre_comb_value[z][0] = NULL;
            }
        }

        // Output complete combinations
        char vars[1024 * 32], *result;
        bool first = true, first_val = true;
        result = (char *)malloc(1024 * 3 * this->n_pcv);
        if(!result) out_of_memory();
        // Get rid of incomplete combinations (could be optimized)
        for (int z = 0; z < this->n_pcv; z++) {
            bool complete = true;
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (this->var[i].notexists)
                    continue;
                if (this->var[i].obj[0] != '?')
                    continue;
                if (this->pre_comb_value[z][i] == NULL) {
                    complete = false;
                    break;
                }
            }
            if (!complete) {
                free(this->pre_comb_value[z]);
                free(this->pre_comb_value_type[z]);
                free(this->pre_comb_value_lang[z]);
                if(this->n_pcv - z - 1 > 0) {
                    memmove((void*)((long)this->pre_comb_value+z*sizeof(char***)), (void*)((long)this->pre_comb_value+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                    memmove((void*)((long)this->pre_comb_value_type+z*sizeof(char***)), (void*)((long)this->pre_comb_value_type+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                    memmove((void*)((long)this->pre_comb_value_lang+z*sizeof(char***)), (void*)((long)this->pre_comb_value_lang+(z+1)*sizeof(char***)), (this->n_pcv - z - 1)*sizeof(char***) );
                }
                this->n_pcv--;
                z--;
            }
        }
        if(this->modifier & MOD_COUNT) {
            char *answer = (char *)malloc(1024);
            if(!answer) out_of_memory();
            sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Count\":\"%i\"}", this->request_id, this->n_pcv);
            return answer;
        }
        // Sort combinations. Coarse, temporary implementation, could be optimized
        for(int j=this->_n_orders-1; j>=0; j--) {
            if(strcmp(this->_orders[j].direction, "desc") == 0) sort_order = 1;
            else sort_order = 0;
            char **unique = (char**)malloc(this->n_pcv*sizeof(char*));
            if(!unique) out_of_memory();
            int *orders = (int*)malloc(this->n_pcv*sizeof(int));
            if(!orders) out_of_memory();
            int sort_var_index = 0;
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (strcmp(this->var[i].obj, this->_orders[j].variable) != 0)
                    continue;
                sort_var_index = i;
            }
            for (int z = 0; z < this->n_pcv; z++) {
                if(!this->pre_comb_value[z]) continue;
                unique[z] = this->pre_comb_value[z][sort_var_index];
            }
            qsort(unique, this->n_pcv, sizeof(char*), cstring_cmp);
            // Cycle through sorted unique items
            for (int y = 0; y < this->n_pcv; y++) {
                // Cycle through all combinations to find the next unique item
                for (int z = 0; z < this->n_pcv; z++) {
                    bool placed = false;
                    if (strcmp(this->var[sort_var_index].obj, this->_orders[j].variable) != 0)
                        continue;
                    if(strcmp(this->pre_comb_value[z][sort_var_index], unique[y]) == 0) {
                        bool found = false;
                        for(int v=0; v<y; v++) {
                            if(orders[v] == z) {
                                found = true;
                                break;
                            }
                        }
                        if(!found) {
                            orders[y] = z;
                            placed = true;
                        }
                    }
                    if(placed) break;
                }
            }
            char ***new_pre_comb_value = (char ***)malloc(this->n_pcv * sizeof(char **));
            char ***new_pre_comb_value_type = (char ***)malloc(this->n_pcv * sizeof(char **));
            char ***new_pre_comb_value_lang = (char ***)malloc(this->n_pcv * sizeof(char **));
            if(!new_pre_comb_value) out_of_memory();
            if(!new_pre_comb_value_type) out_of_memory();
            if(!new_pre_comb_value_lang) out_of_memory();
            for (int y = 0; y < this->n_pcv; y++) {
                new_pre_comb_value[y] = this->pre_comb_value[orders[y]];
                new_pre_comb_value_type[y] = this->pre_comb_value_type[orders[y]];
                new_pre_comb_value_lang[y] = this->pre_comb_value_lang[orders[y]];
            }
            memcpy(this->pre_comb_value, new_pre_comb_value, this->n_pcv * sizeof(char **));
            memcpy(this->pre_comb_value_type, new_pre_comb_value_type, this->n_pcv * sizeof(char **));
            memcpy(this->pre_comb_value_lang, new_pre_comb_value_lang, this->n_pcv * sizeof(char **));
            free(new_pre_comb_value); free(new_pre_comb_value_type); free(new_pre_comb_value_lang);
            free(unique); free(orders);
        }

        // Response output
        sprintf(vars, "\"Vars\": [");
        sprintf(result, "\"Result\": [");
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            if (this->var[i].notexists)
                continue;
            if (this->var[i].obj[0] != '?')
                continue;
            if (!first)
                strcat(vars, ", ");
            first = false;
//printf("%s\t", this->var[i].obj);
            sprintf((char *)((unsigned long)vars + strlen(vars)), "\"%s\"", this->var[i].obj);
        }
        strcat(vars, "] ");
//printf("\n");
        first = true;
        for (int z = 0; z < this->n_pcv; z++) {
            if (!first)
                strcat(result, ", ");
            first = false;
            first_val = true;
            strcat(result, " [");
            for (int x = 0; x < n; x++) {
                int i = order[x];
                if (this->var[i].notexists)
                    continue;
                if (this->var[i].obj[0] != '?')
                    continue;
//printf("%s\t", this->pre_comb_value[z][i]);
                if (!first_val)
                    strcat(result, ", ");
                first_val = false;
                sprintf((char *)((unsigned long)result + strlen(result)), "[\"%s\", \"%s\", \"%s\"]", this->pre_comb_value[z][i], this->pre_comb_value_type[z][i] ? this->pre_comb_value_type[z][i] : "", this->pre_comb_value_lang[z][i] ? this->pre_comb_value_lang[z][i] : "");
            }
//printf("\n");
            strcat(result, "]");
        }
        strcat(result, "] ");
        logger(LOG, "(child) Return triples chain", "", this->n_pcv);

        char *answer = (char *)malloc(1024 + strlen(vars) + strlen(result));
        if(!answer) out_of_memory();
        sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", %s, %s}", reqid, vars, result);
//printf(answer);
        free(result);
        return answer;
    }

};

// Instantiate Request class, process request and free memory
char *process_request(char *buffer, int operation, int endpoint, int modifier, int *pip)
{
    request req;
    char *answer = NULL;
#ifdef PROFILE
    struct timeval start, end;
    gettimeofday(&start, NULL);
#endif
    char *pmessage = NULL;
    if (!req.parse_request(buffer, &pmessage, 0, operation, endpoint, modifier, pip))
    {
logger(LOG, "Goint to free_all", "", 0);
        req.free_all();
        return pmessage;
    }
    else
    {
#ifdef PROFILE
        gettimeofday(&end, NULL);
        printf("Total request processing %f\n", (float)((float)end.tv_sec - (float)start.tv_sec + (float)(end.tv_usec - start.tv_usec) / 1000000));
#endif
        // If valid answer returned
        if (pmessage)
        {
            req.free_all();
            return pmessage;
        }
        req.free_all();
        answer = (char *)malloc(1200);
        if(!answer) out_of_memory();
        sprintf(answer, "{\"RequestId\":\"%s\", \"Status\":\"Ok\"}", req.request_id);
        return answer;
    }
}
