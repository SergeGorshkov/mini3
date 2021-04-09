
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
    int n_var = 0, n_optional = 0, depth = 0;
    char *optional[32];

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
                if (strcmp(t, "optional") == 0)
                {
                    if (endpoint != EP_CHAIN)
                        goto wrong_parameter;
                    in_block = B_OPTIONAL;
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
                else if (in_block == B_OPTIONAL) {
                    this->optional[this->n_optional] = (char*)malloc(strlen(t) + 1);
                    strcpy(this->optional[this->n_optional++], t);
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
        sprintf(*message, "%s parameter cannot be specified in the request to this endpoint (%i)", t, endpoint);
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
        sprintf(data, "%s | %s | %s", *subject, *predicate, *object);
        memset(this->_triples[this->_n_triples].hash, 0, SHA_DIGEST_LENGTH);
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
            }
            free(this->pre_comb_value);
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
    void propagate_dependent(int i, int dep)
    {
        this->depth++;
        if (this->depth > 10)
            return;
        this->var[i].dep += dep;
        for (int j = 0; j < this->var[i].n; j++)
        {
            if (this->var[ this->var[i].cond_o[j] ].obj[0] == '?') {
                this->var[ this->var[i].cond_o[j] ].dependent_of[ this->var[ this->var[i].cond_o[j] ].n_dep_of++ ] = i;
                propagate_dependent(this->var[i].cond_o[j], this->var[i].dep + 1);
            }
        }
    }

    // Check if possible candidate s has a triple with the given predicate and object
    bool check_candidate_match(char *s, char *predicate, int cond_o) {
        bool match = false;
        unsigned long n = 0, *cand;
        if (this->var[ cond_o ].obj[0] == '?') {
            if (!this->var[ cond_o ].n_cand) return true; // Doubtful
            for (int k = 0; k < this->var[ cond_o ].n_cand; k++) {
//printf("\tCheck (1) %s - %s - %s\n", s, predicate, this->var[ cond_o ].cand[k]);
                cand = find_matching_triples(s, predicate, this->var[ cond_o ].cand[k], NULL, NULL, &n);
                if (cand != NULL) {
                    free(cand);
                    match = true;
                    break;
                }
            }
        }
        else {
            cand = find_matching_triples(s, predicate, this->var[ cond_o ].obj, NULL, NULL, &n);
//printf("\tCheck (2) %s - %s - %s, %i, %x\n", s, predicate, this->var[ cond_o ].obj, n, cand);
            if (cand != NULL) {
                free(cand);
                match = true;
            }
        }
        return match;
    }

    // Add a new candidate object (value) for some variable
    // j is the variable index in this->var, s is the variable value, datatype and lang are the value's metadata
    int add_candidate(int j, char *s, char *datatype, char *lang)
    {
        bool found = false;
        for (int k = 0; k < this->var[j].n_cand; k++)
        {
            if (strcmp(this->var[j].cand[k], s) == 0)
                return k;
        }
//printf("add candidate (%i) %s (datatype = %s, lang = %s)\n", j, s, datatype, lang);

        // Check that candidate match other conditions
        bool match = true;
        for (int i = 0; i < this->var[j].n; i++) {
            // For a normal predicate
            if (this->var[j].cond_p[i] >= 0) {
                match = check_candidate_match(s, this->var[ this->var[j].cond_p[i] ].obj, this->var[j].cond_o[i]);
                if(!match) break;
            }
            // If predicate is a variable
            else if (this->var[j].cond_p[0] == -1) {
                for (int l = 0; l < this->var[ this->var[j].cond_o[i] ].n; l++) {
                    if (this->var[ this->var[j].cond_o[i] ].cond_p[l] == -2) {
                        match = check_candidate_match(s, this->var[ this->var[j].cond_o[i] ].obj, this->var[ this->var[j].cond_o[i] ].cond_o[l]);
                        if(!match) break;
                    }
                }
            }
        }
        if(!match) {
//printf("not match\n");
            return -1;
        }

        if (!this->var[j].cand)
        {
            this->var[j].cand = (char **)malloc(1024 * sizeof(char *));
            this->var[j].cand_d = (char **)malloc(1024 * sizeof(char *));
            this->var[j].cand_l = (char **)malloc(1024 * sizeof(char *));
            if(!this->var[j].cand || !this->var[j].cand_d || !this->var[j].cand_l) out_of_memory();
        }
        else if (this->var[j].n_cand % 1024 == 0)
        {
            this->var[j].cand = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[j].cand_d = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            this->var[j].cand_l = (char **)realloc(this->var[j].cand, (this->var[j].n_cand / 1024 + 1) * 1024 * sizeof(char *));
            if(!this->var[j].cand || !this->var[j].cand_d || !this->var[j].cand_l) out_of_memory();
        }
        // Add candidate for the j-th variable
        this->var[j].cand[this->var[j].n_cand] = s;
        this->var[j].cand_d[this->var[j].n_cand] = datatype;
        this->var[j].cand_l[this->var[j].n_cand] = lang;
//printf("added %i to %i\n", this->var[j].n_cand, j);
        this->var[j].n_cand++;
        return this->var[j].n_cand - 1;
    }

    // Add solution from the i-th variable to the j-th. Cand (source) is the i'th variable candidate, sol (target) is j'th variable candidate
    void add_solution(int i, int j, int cand, int sol, int bearer) {
//printf("\tadd solution: from var %i cand %i = %s to var %i cand %i = %s, bearer = %i\n", i, cand, this->var[i].cand[cand], j, sol, this->var[j].cand[sol], bearer);
        if (!this->var[i].solution[j]) {
            this->var[i].solution[j] = (int*)malloc(1024 * sizeof(int));
            this->var[i].solution_cand[j] = (int*)malloc(1024 * sizeof(int));
            if (!this->var[i].solution[j] || !this->var[i].solution_cand[j]) out_of_memory();
            if (bearer != -1) {
                this->var[i].bearer[j] = (int*)malloc(1024 * sizeof(int));
                if(!this->var[i].bearer[j]) out_of_memory();
            }
        }
        else if (this->var[i].n_sol[j] % 1024 == 0) {
            this->var[i].solution[j] = (int *)realloc(this->var[i].solution[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
            this->var[i].solution_cand[j] = (int *)realloc(this->var[i].solution_cand[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
            if(!this->var[i].solution[j] || !this->var[i].solution_cand[j]) out_of_memory();
            if(bearer != -1) {
                this->var[i].bearer[j] = (int *)realloc(this->var[i].bearer[j], (this->var[i].n_sol[j] / 1024 + 1) * 1024 * sizeof(int));
                if(!this->var[i].bearer[j]) out_of_memory();
            }
        }
        bool found = false;
        for (int k=0; k<this->var[i].n_sol[j]; k++) {
            if (this->var[i].solution_cand[j][k] == cand && this->var[i].solution[j][k] == sol) {
                if (bearer != -1 && this->var[i].bearer) {
                    if (this->var[i].bearer[j][k] == bearer)
                        found = true;
                }
                else
                    found = true;
            }
        }
        if(found) return;
        this->var[i].solution_cand[j][ this->var[i].n_sol[j] ] = cand;
        this->var[i].solution[j][ this->var[i].n_sol[j] ] = sol;
        if(bearer != -1)
            this->var[i].bearer[j][ this->var[i].n_sol[j] ] = bearer;
        this->var[i].n_sol[j]++;
//printf("\tadded solution_%i to variable %i, bearer = %i, n_sol = %i\n", j, i, bearer, this->var[i].n_sol[j]);
    }

    // Fill candidate objects (values) array for j-th variable using its linkage from i-th variable
    bool get_candidates(int i, int j, int cand_subject, char *subject, char *predicate, char *object, bool negative, int bearer)
    {
        unsigned long n = 0;
//printf("\nFind %s - %s - %s for i = %i, j = %i, cand = %i, bearer = %i\n", subject, predicate, object, i, j, cand_subject, bearer);
        unsigned long *res = find_matching_triples(subject, predicate, object, NULL, NULL, &n);
//printf("found %i items\n", n);
        if (negative) {
            free(res);
            if( n > 0 )
                return false;
            return true;
        }
        for (unsigned long k = 0; k < n; k++)
        {
            if (subject[0] == '*')
                add_candidate(i, get_string(triples[res[k]].s_pos), NULL, NULL);
            else if (predicate[0] == '*') {
                int ind_cand = add_candidate(j, get_string(triples[res[k]].p_pos), NULL, NULL);
//printf("predicate = %s, i = %i\n", get_string(triples[res[k]].p_pos), i);
                if (i >= 0 && ind_cand > -1)
                    add_solution(i, j, cand_subject, ind_cand, bearer);
            }
            else if (object[0] == '*') {
                int ind_cand = add_candidate(j, get_string(triples[res[k]].o_pos), triples[res[k]].d_pos ? get_string(triples[res[k]].d_pos) : NULL, triples[res[k]].l);
//printf("add candidate %s to variable %s, ind_cand = %i\n", get_string(triples[res[k]].o_pos), this->var[j].obj, ind_cand);
                if (i >= 0 && ind_cand > -1)
                    add_solution(i, j, cand_subject, ind_cand, bearer);
            }
        }
        free(res);
        if( n > 0 ) return true;
        return false;
    }

    // Add new combination to the i-th variable combinations
    void push_combination(int i, int *comb) {
        if (this->var[i].n_comb == 0) {
            this->var[i].comb_value = (int **)malloc(1024 * sizeof(int*));
            if(!this->var[i].comb_value) out_of_memory();
        }
        else if (this->var[i].n_comb % 1024 == 0) {
            this->var[i].comb_value = (int **)realloc(this->var[i].comb_value, (this->var[i].n_comb / 1024 + 1) * 1024 * sizeof(int *));
            if(!this->var[i].comb_value) out_of_memory();
        }
        /* int size = 0;
        for (int k = 0; k < 32; k++) {
            if (comb[k] != -1)
                size++;
        }
        if (size < 2) return; */
        // Check if this combination already exists
        for (int j = 0; j < this->var[i].n_comb; j++) {
            bool identical = true;
            for (int k = 0; k < 32; k++) {
                if (comb[k] != this->var[i].comb_value[j][k]) {
                    identical = false;
                    break;
                }
            }
            if (identical) return;
        }
        this->var[i].comb_value[this->var[i].n_comb] = (int *)malloc(32 * sizeof(int));
        if (!this->var[i].comb_value[this->var[i].n_comb]) out_of_memory();
        memcpy(this->var[i].comb_value[this->var[i].n_comb], comb, sizeof(int)*32);
        this->var[i].n_comb++;
    }

    // Recursively build combinations for the i-th variable, id candidate
    void build_combination(int i, int id, int *comb, int ind_cond, int bearer) {
        if(this->var[ this->var[i].cond_o[ind_cond] ].obj[0] != '?') {
            if(ind_cond > this->var[i].n - 1)
                this->push_combination(i, comb);
            else
                this->build_combination(i, id, comb, ind_cond+1, bearer);
        }
        bool relation = false;
        int dependent_cond = 0;
        for (int y = 0; y < this->var[i].n; y++) {
            if (this->var[i].cond_p[y] == -2) {
                relation = true;
                for (int z = 0; z < this->var[ this->var[i].dependent_of[0] ].n; z++) {
                    if (this->var[ this->var[i].dependent_of[0] ].cond_p[z] == -1 && this->var[ this->var[i].dependent_of[0] ].cond_o[z] == i)
                        dependent_cond = z;
                }
            }
        }
        int value_var = this->var[i].cond_o[ind_cond];
        // For the predicate variables
        if (relation) {
            if (this->var[i].n_sol[value_var] > 0) {
//printf("value_var = %i, dependent of = %i, dependent_cond = %i\n", value_var, this->var[i].dependent_of[0], dependent_cond);
                for (int j = 0; j < this->var[i].n_sol[value_var]; j++) {
//printf("Build for solution (rel) %s = %s, id = %i of variable %i, bearer is %i\n", this->var[value_var].obj, this->var[value_var].cand[ this->var[i].solution[value_var][j] ], id, i, bearer);
                    if (this->var[i].solution[value_var][j] >= 0 && this->var[value_var].n_cand > this->var[i].solution[value_var][j]) {
                        if (bearer != -1 && this->var[i].bearer) {
                            if (this->var[i].bearer[ value_var ][j] == bearer) {
//printf("Bearer this->var[%i].bearer[ %i ][%i] = %i, bearer match: %s = %s, %s = %s\n", i, value_var, j, this->var[i].bearer[ value_var ][j], this->var[i].obj, this->var[i].cand[this->var[i].solution_cand[value_var][j]], this->var[value_var].obj, this->var[value_var].cand[this->var[i].solution[value_var][j]]);
                                comb[i] = this->var[i].solution_cand[value_var][j];
                                comb[value_var] = this->var[i].solution[value_var][j];
                            }
                        }
                    }
                    if(ind_cond > this->var[i].n - 1)
                        this->push_combination(i, comb);
                    else
                        this->build_combination(i, id, comb, ind_cond+1, bearer);
                }
            }
            else {
                if(ind_cond > this->var[i].n - 1)
                    this->push_combination(i, comb);
                else
                    this->build_combination(i, id, comb, ind_cond+1, bearer);
            }
        }
        // For the ordinary variables
        else {
            if (this->var[i].n_sol[value_var] > 0) {
                for (int j = 0; j < this->var[i].n_sol[value_var]; j++) {
                    if (this->var[i].solution_cand[value_var][j] != id) continue;
                    comb[value_var] = this->var[i].solution[value_var][j];
                    if(ind_cond > this->var[i].n - 1)
                        this->push_combination(i, comb);
                    else
                        this->build_combination(i, id, comb, ind_cond+1, bearer);
                }
            }
            else {
                if(ind_cond > this->var[i].n - 1)
                    this->push_combination(i, comb);
                else
                    this->build_combination(i, id, comb, ind_cond+1, bearer);
            }
        }
    }

    void global_push_combination(int i, int j) {
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
        for (int l = 0; l < this->n_var; l++) {
            if (this->var[i].comb_value[j][l] < 0) continue;
            this->pre_comb_value[this->n_pcv][l] = this->var[l].cand[ this->var[i].comb_value[j][l] ];
            if (this->var[l].cand_d[ this->var[i].comb_value[j][l] ]) {
                this->pre_comb_value_type[this->n_pcv][l] = this->var[l].cand_l[ this->var[i].comb_value[j][l] ];
            }
            if (this->var[l].cand_l[ this->var[i].comb_value[j][l] ]) {
                if(this->var[l].cand_l[ this->var[i].comb_value[j][l] ][0]) {
                    this->pre_comb_value_lang[this->n_pcv][l] = this->var[l].cand_d[ this->var[i].comb_value[j][l] ];
                }
            }
        }
        this->n_pcv++;
    }

    // Entry point for triples chain request
    char *return_triples_chain(char *reqid)
    {
        int order[32], max_dep = 0, n = 0;
        char star[2] = "*", relation[9] = "relation", relation_value[15] = "relation_value";
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
            if (this->_triples[i].p[0] == '?') {
                this->var[ics].cond_p[this->var[ics].n] = -1;
                this->var[ics].cond_o[this->var[ics].n++] = icp;
                this->var[icp].dep++;
                this->var[icp].cond_p[this->var[icp].n] = -2;
                this->var[icp].cond_o[this->var[icp].n++] = ico;
                this->var[ico].dep++;
            }
            else {
                this->var[ics].cond_p[this->var[ics].n] = icp;
                this->var[ics].cond_o[this->var[ics].n++] = ico;
                if (this->var[icp].obj[0] == '?')
                    this->var[icp].dep++;
                if (this->var[ico].obj[0] == '?')
                    this->var[ico].dep++;
            }
        }
        // Mark optional variables
        for (int i = 0; i < this->n_optional; i++) {
            for (int j = 0; j < this->n_var; j++) {
                if (strcmp(this->optional[i], this->var[j].obj) == 0)
                    this->var[j].optional = true;
            }
        }
        // Find variables dependence to define the variables resolution order
        for (int i = 0; i < this->n_var; i++)
        {
            for (int j = 0; j < this->var[i].n; j++)
            {
                if (this->var[i].dep == 0 && this->var[ this->var[i].cond_o[j] ].obj[0] == '?') {
                    this->var[ this->var[i].cond_o[j] ].dependent_of[ this->var[ this->var[i].cond_o[j] ].n_dep_of++ ] = i;
                    propagate_dependent(this->var[i].cond_o[j], 1);
                    if (this->var[ this->var[i].cond_p[j] ].obj[0] == '?') {
                        this->var[ this->var[i].cond_p[j] ].dependent_of[ this->var[ this->var[i].cond_p[j] ].n_dep_of++ ] = i;
                        propagate_dependent(this->var[i].cond_p[j], 1);
                    }
                }
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
                char *predicate = this->var[i].cond_p[j] == -1 ? relation : ( this->var[i].cond_p[j] == -2 ? relation_value : this->var[this->var[i].cond_p[j]].obj);
                char *object = this->var[this->var[i].cond_o[j]].obj;
//printf("\nCondition from %i to %i, j = %i. Subject = %s, %i candidates\n", i, this->var[i].cond_o[j], j, subject, this->var[i].n_cand);
                if (subject[0] == '?') 
                {
                    if (this->var[i].n_cand == 0)
                        bool match = get_candidates(i, this->var[i].cond_o[j], 0, subject[0] == '?' ? star : subject, predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[i].notexists, -1);
                    int limit_cand = this->var[i].n_cand;
//printf("=== There are %i candidates\n", limit_cand);
                    for (int l = 0; l < limit_cand; l++) {
                        bool match;
                        if (predicate == relation) {
                            match = get_candidates(i, this->var[i].cond_o[j], l, this->var[i].cand[l], object[0] == '?' ? star : object, star, this->var[this->var[i].cond_o[j]].notexists, -1);
                        }
                        else if (predicate == relation_value) {
//printf("dependent of %i variables\n", this->var[i].n_dep_of);
                            for (int n = 0; n < this->var[i].n_dep_of; n++) {
                                int dep_of = this->var[i].dependent_of[n];
                                int limit = this->var[dep_of].n_sol[i];
                                for (int m = 0; m < limit; m++) {
                                    if (l != this->var[dep_of].solution[i][m]) continue;
//printf("m = %i: Parent candidate (bearer) is %s\n", m, this->var[dep_of].cand[ this->var[dep_of].solution_cand[i][m] ]);
                                    match = get_candidates(i, this->var[i].cond_o[j], l, this->var[dep_of].cand[ this->var[dep_of].solution_cand[i][m] ], this->var[i].cand[l], object[0] == '?' ? star : object, this->var[this->var[i].cond_o[j]].notexists, this->var[dep_of].solution_cand[i][m]);
                                }
                            }
                        }
                        else
                            match = get_candidates(i, this->var[i].cond_o[j], l, this->var[i].cand[l], predicate[0] == '?' ? star : predicate, object[0] == '?' ? star : object, this->var[this->var[i].cond_o[j]].notexists, -1);
                        // Inverse condition - matching objects shall not exist
                        if( this->var[ this->var[i].cond_o[j] ].notexists && !match )
                            this->var[i].cand[l] = NULL;
                    }
                }
                else if (subject[0] != '?') add_candidate(i, subject, NULL, NULL);
            }
        }
    // Remove references to non-existant variables
    for (int i = 0; i < n; i++) {
        for(int j=0; j<this->var[i].n; j++) {
            bool notexists = false;
            if (this->var[i].cond_p[j] >= 0) {
                if (this->var[this->var[i].cond_p[j]].notexists)
                    notexists = true;
            }
            if (notexists || this->var[this->var[i].cond_o[j]].notexists) {
                if (j < this->var[i].n-1) {
                    memmove(&this->var[i].cond_p[j], &this->var[i].cond_p[j+1], sizeof(int)*(this->var[i].n - j));
                    memmove(&this->var[i].cond_o[j], &this->var[i].cond_o[j+1], sizeof(int)*(this->var[i].n - j));
                }
                this->var[i].n --;
            }
        }
    }
    //Clean up incomplete solutions
    for (int x = 0; x < n; x++) {
        int i = order[x];
        if (this->var[i].obj[0] != '?' && this->var[i].n == 0)
            continue;
        for (int k = 0; k < 32; k++) {
            for (int j = 0; j < this->var[i].n_sol[k]; j++) {
                if (this->var[i].solution[k][j] < 0) {
                    if (j < this->var[i].n_sol[k] - 1) {
                        memmove(&this->var[i].solution[k][j], &this->var[i].solution[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                        memmove(&this->var[i].solution_cand[k][j], &this->var[i].solution_cand[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                        if(this->var[i].bearer[k])
                            memmove(&this->var[i].bearer[k][j], &this->var[i].bearer[k][j+1], (this->var[i].n_sol[k] - j) * sizeof(int));
                    }
                    this->var[i].n_sol[k]--;
                    j--;
                }
            }
        }
    }
/*
printf("Variables:\n");
for(int x=0; x<n; x++) {
    int i = order[x];
    if(this->var[i].obj[0] != '?' && this->var[i].n == 0)
        continue;
    printf("%i:\t%s, dep = %i, optional = %i, notexists = %i\n", i, this->var[i].obj, this->var[i].dep, this->var[i].optional, this->var[i].notexists);
    for(int j=0; j<this->var[i].n; j++)
        printf("\t\t%s => %s\n", this->var[i].cond_p[j] == -1 ? "relation" : ( this->var[i].cond_p[j] == -2 ? relation_value : this->var[this->var[i].cond_p[j]].obj), this->var[this->var[i].cond_o[j]].obj);
    if(this->var[i].n_dep_of) {
        printf("\tDependent of: ");
        for(int j=0; j<this->var[i].n_dep_of; j++)
            printf(" %i", this->var[i].dependent_of[j]);
        printf("\n");
    }
    if(!this->var[i].n_cand) continue;
    printf("\tCandidates:\n");
    for(int j=0; j<this->var[i].n_cand; j++) {
        if(!this->var[i].cand[j]) continue;
        printf("\t\t%s\n", this->var[i].cand[j]);
    }
    printf("\tSolutions:\n");
    for(int k=0; k<32; k++) {
        if(this->var[i].n_sol[k] == 0) continue;
        printf("\t\t%s: %i\n", this->var[k].obj, this->var[i].n_sol[k]);
        for(int j=0; j<this->var[i].n_sol[k]; j++) {
            printf("\t\t\t%s\t", this->var[i].cand[ this->var[i].solution_cand[k][j] ]);
            if(this->var[i].solution[k][j] >= 0 && this->var[k].n_cand > this->var[i].solution[k][j]) {
                if(this->var[k].cand[ this->var[i].solution[k][j] ])
                    printf("%s", this->var[k].cand[ this->var[i].solution[k][j] ]);
            }
            if(this->var[i].bearer[k]) {
                if(this->var[i].bearer[k][j] != -1)
                    printf(" (%s)", this->var[ this->var[i].dependent_of[0] ].cand[ this->var[i].bearer[k][j] ]);
            }
            printf("\n");
        }
    }
}
/*
printf("Filter groups: %i\n", this->_n_filter_groups);
for(int i=0; i<this->_n_filter_groups; i++) {
    printf("%i: %i, %i\n", i, this->_filter_groups[i].group, this->_filter_groups[i].logic);
}

printf("Filters: %i\n", this->_n_filters);
for(int i=0; i<this->_n_filters; i++) {
    printf("%i (%i): %s - %i - %s\n", i, this->_filters[i].group, this->_filters[i].variable, this->_filters[i].operation, this->_filters[i].value);
}
*/
        // Build combinations for each variable having candidates and dependent variables
        for (int x = 0; x < n; x++)
        {
            int i = order[x];
            if (this->var[i].obj[0] != '?' || this->var[i].n_cand == 0 || this->var[i].notexists)
                continue;
            // Check if there is at least one dependent variable
            if (max_dep > 0) {
                bool found = false;
                for (int y = 0; y < n; y++) {
                    for (int z = 0; z < this->var[y].n_dep_of; z++) {
                        if (this->var[y].dependent_of[z] == i)
                            found = true;
                    }
                }
                if(!found) continue;
            }
            bool relation = false;
            for (int y = 0; y < this->var[i].n; y++) {
                if (this->var[i].cond_p[y] == -2)
                    relation = true;
            }
            // Cycle through candidates and build combination starting with each of them
            // For predicate variables
            if(relation) {
                int dep = this->var[i].dependent_of[0];
//printf("Variable %i has %i candidates\n", dep, this->var[dep].n_cand);
                for (int y = 0; y < this->var[dep].n_cand; y++) {
                    int combination[32];
                    memset(&combination, -1, sizeof(int)*32);
                    combination[dep] = y; // Put source object (bearer) to the combination
//printf("Source candidate object %i\nThere are %i solutions from %i to %i\n", y, this->var[dep].n_sol[i], dep, i);
                    // Cycle through solutions from the parent variable to the current variable
                    for (int z = 0; z < this->var[dep].n_sol[i]; z++) {
                        if (this->var[dep].solution_cand[i][z] != y) continue;
                        // Find out next variable which represent object in triples where the current variable is a predicate
                        int next_var = -1;
                        for (int v = 0; v < this->var[i].n; v++) {
                            if (this->var[i].cond_p[v] == -2)
                                next_var = this->var[i].cond_o[v];
                        }
//printf("next var is %i\n", next_var);
                        if (next_var == -1) continue;
                        // Check if there is a solution with this source object and predicate
//printf("Check if among the solutions from %i to %i is the source variable's candidate %i as bearer\n", i, next_var, y);
                        bool found = false;
                        if (this->var[i].bearer) {
                            for (int n = 0; n < this->var[i].n_sol[next_var]; n++) {
                                if (this->var[i].bearer[next_var][n] == y) {
                                    found = true;
                                    break;
                                }
                            }
                        }
                        if (!found) break;
//printf("Go build combinations for variable %i and predicate %i, while bearer is %i\n", i, this->var[dep].solution[i][z], dep, y);
                        build_combination(i, this->var[dep].solution[i][z], combination, 0, y);
//                        combination[i] = this->var[dep].solution[i][z];
                    }
                }
            }
            // For ordinary variables
            else {
                for (int y = 0; y < this->var[i].n_cand; y++) {
                    if (this->var[i].cand[y] == NULL) continue;
                    int combination[32];
                    memset(&combination, -1, sizeof(int)*32);
                    combination[i] = y;
                    build_combination(i, y, combination, 0, 1);
                }
            }
        }
        // Join combinations of dependent variables from the end of dependency tree to the start
        for (int x = n-1; x >= 0; x--)
        {
            int i = order[x];
            for (int y = 0; y < n; y++) {
                bool is_dependent = false;
                for (int z = 0; z < this->var[y].n_dep_of; z++) {
                    if (this->var[y].dependent_of[z] == i) {
                        is_dependent = true;
                        break;
                    }
                }
                if (!is_dependent) continue;
                int limit = this->var[i].n_comb;
                // If there is no combinations in the parent variable, just copy them from the child one
                if (limit == 0) {
                    for (int k = 0; k < this->var[y].n_comb; k++) {
                        if (this->var[y].comb_value[k][0] == -2) continue;
                        push_combination(i, this->var[y].comb_value[k]);
                    }
                    continue;
                }
                for (int j = 0; j < limit; j++) {
                    if (this->var[i].comb_value[j][0] == -2) continue;
                    bool unset = false;
                    for (int k = 0; k < this->var[y].n_comb; k++) {
                        bool incompatible = false;
                        int newcomb[32];
                        memset(newcomb, -1, 32*sizeof(int));
                        for (int z = 0; z < n; z++) {
                            if (this->var[i].comb_value[j][z] != -1 && this->var[y].comb_value[k][z] != -1 && this->var[i].comb_value[j][z] != this->var[y].comb_value[k][z]) {
                                incompatible = true;
                                break;
                            }
                            if (this->var[i].comb_value[j][z] != -1)
                                newcomb[z] = this->var[i].comb_value[j][z];
                            else if (var[y].comb_value[k][z] != -1)
                                newcomb[z] = this->var[y].comb_value[k][z];
                        }
                        if (!incompatible) {
                            push_combination(i, newcomb);
                            unset = true;
                        }
                    }
                    if (unset)
                        this->var[i].comb_value[j][0] = -2;
                }
            }
        }
/*
printf("\nCombinations for variables:\n");
for (int x = 0; x < n; x++)
{
    int i = order[x];
    if(this->var[i].n_comb == 0) continue;
    printf("\nVariable: %s, %i combinations\n", this->var[i].obj, this->var[i].n_comb);
    for(int y = 0; y < this->var[i].n_comb; y++) {
        if(this->var[i].comb_value[y][0] == -2) continue;
        printf("\t");
        for(int z = 0; z < 32; z++) {
            if(this->var[i].comb_value[y][z] != -1)
                printf("%s = %s, ", this->var[z].obj, this->var[z].cand[this->var[i].comb_value[y][z]]);
        }
        printf("\n");
    }
}
*/
        // Push the combination of the independent variables into the global array of combinations, and transform candidates indexes into URIs or literal values
        for (int i = 0; i < n; i++) {
            if (this->var[i].dep > 0) continue;
            for (int j = 0; j < this->var[i].n_comb; j++) {
                if (this->var[i].comb_value[j][0] != -2)
                    global_push_combination(i, j);
            }
        }

        for (int i = 0; i < n; i++) {
            if (this->var[i].cand) free(this->var[i].cand);
            if (this->var[i].cand_s) free(this->var[i].cand_s);
            if (this->var[i].cand_p) free(this->var[i].cand_p);
            if (this->var[i].cand_d) free(this->var[i].cand_d);
            if (this->var[i].cand_l) free(this->var[i].cand_l);
            for (int j = 0; j < n; j++) {
                if (this->var[i].solution[j]) free(this->var[i].solution[j]);
                if (this->var[i].solution_cand[j]) free(this->var[i].solution_cand[j]);
                if (this->var[i].bearer[j]) free(this->var[i].bearer[j]);
            }
            if (this->var[i].comb_value) {
                for (int j = 0; j < this->var[i].n_comb; i++)
                    free(this->var[i].comb_value[j]);
                free(this->var[i].comb_value);
            }
        }

        // Cycle through the combinations generated for independent variables to make complete ones
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
/*
printf("\nCombinations:\n");
for (int x = 0; x < this->n_pcv; x++) {
    for (int l = 0; l < this->n_var; l++) {
        if (this->pre_comb_value[x][l])
            printf("%s%s = %s", (l!=0?", ":""), this->var[l].obj, this->pre_comb_value[x][l]);
    }
    printf("\n\n");
}
*/
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
                if((!lvalue || !rvalue ) && this->_filters[i].operation != COMPARE_NOTEXISTS) {
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
//printf("incomplete: %i, nfg: %i\n", incomplete, this->_n_filter_groups);
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
                if (this->var[i].notexists || this->var[i].optional)
                    continue;
                if (this->var[i].obj[0] != '?')
                    continue;
                if (this->pre_comb_value[z][i] == NULL) {
//printf("%s has no value", this->var[i].obj);
                    complete = false;
                    break;
                }
            }
            if (!complete) {
//printf(" - incomplete\n");
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
//        logger(LOG, "Goint to free_all", "", 0);
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
