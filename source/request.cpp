
class request
{

private:
    int _n_triples = 0, _n_prefixes = 0, _n_orders = 0, _n_filters = 0, _n_filter_groups = 0, modifier = 0, current_filter_group[N_MAX_FILTER_GROUPS], limit = -1, offset = -1;
    local_triple _triples[1024];
    prefix _prefixes[1024];
    order _orders[N_MAX_ORDERS];
    filter _filters[N_MAX_FILTERS];
    filter_group _filter_groups[N_MAX_FILTER_GROUPS];
    unsigned long *_to_delete = 0, _n_delete = 0;
    unsigned long *_to_send = 0, _n_send = 0;
    char type[256];
    int current_union = -1;

    // Variables set for chain request
    chain_variable var[N_MAX_VARIABLES];
    int n_var = 0, n_optional = 0, depth = 0;
    char *optional[N_MAX_VARIABLES];
    int optional_group[N_MAX_VARIABLES], current_optional = -1;

    // Set of candidate combinations
    char ***pre_comb_value, ***pre_comb_value_type, ***pre_comb_value_lang;
    int n_pcv = 0;

public:
    char request_id[1024];

    bool parse_request(char *buffer, char **message, int fd, int operation, int endpoint, int modifier, int *pip) {
        int r = 0, pos = 0, status = 0, in_block = 0, current_level = 0, nested_array_items[32];
        char *t, *subject = NULL, *predicate = NULL, *object = NULL, *datatype = NULL, lang[8];
        bool in_method = false, in_id = false, in_limit = false, in_offset = false;

        memset(nested_array_items, 0, 32*sizeof(int));
        memset(this->current_filter_group, -1, N_MAX_FILTER_GROUPS*sizeof(int));
        memset(this->optional_group, -1, N_MAX_VARIABLES*sizeof(int));
        utils::init_globals(O_RDONLY);
        utils::unicode_to_utf8(buffer);
        t = (char *)malloc(strlen(buffer) + 1);
        if(!t) utils::out_of_memory(fd);
        t[0] = 0;
        this->type[0] = 0;
        this->request_id[0] = 0;
        lang[0] = 0;

        // JSON
        if (buffer[0] != '{') {
            *message = (char *)malloc(128);
            if(!message) utils::out_of_memory(fd);
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
            if(!message) utils::out_of_memory(fd);
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
                if (tokens[i].type == 2 && (in_block == B_CHAIN || in_block == B_PATTERN || in_block == B_OPTIONAL || in_block == B_ORDER || in_block == B_FILTER)) {
                    if (nested_array_items[current_level] > 0) {
                        nested_array_items[++current_level] = tokens[i].size;
                        if (in_block == B_CHAIN && current_level == 2)
                            current_union++;
                        if (in_block == B_OPTIONAL && current_level == 1)
                            current_optional++;
                    }
                    else
                        nested_array_items[current_level] = tokens[i].size;
                }
                pos = 0;
            }

            // This item is a string or number
            if (tokens[i].type == 3 || tokens[i].type == 4)
            {
                if (in_block != B_PATTERN && in_block != B_CHAIN && in_block != B_PREFIX && in_block != B_ORDER && in_block != B_FILTER)
                    utils::strtolower(t);
                else
                    utils::descreen(t);
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
                if (strcmp(t, "limit") == 0)
                {
                    in_limit = true;
                    continue;
                }
                if (strcmp(t, "offset") == 0)
                {
                    in_offset = true;
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
                else if (in_limit)
                {
                    this->limit = atoi(t);
                    in_limit = false;
                }
                if (in_offset)
                {
                    this->offset = atoi(t);
                    in_offset = false;
                }
                else if (in_method)
                {
                    if (strcmp(t, "get") == 0)
                        operation = OP_GET;
                    else if (strcmp(t, "post") == 0)
                        operation = OP_GET;
                    else if (strcmp(t, "put") == 0)
                        operation = OP_PUT;
                    else if (strcmp(t, "patch") == 0)
                        operation = OP_PUT;
                    else if (strcmp(t, "delete") == 0)
                        operation = OP_DELETE;
                    if (!operation)
                    {
                        *message = (char *)malloc(128 + strlen(t));
                        if(!message) utils::out_of_memory(fd);
                        sprintf(*message, "Unknown method: %s", t);
                        goto send_parse_error;
                    }
                    strcpy(this->type, t);
                    in_method = false;
                }
                else if (in_block == B_OPTIONAL) {
                    this->optional[this->n_optional] = (char*)malloc(strlen(t) + 1);
                    strcpy(this->optional[this->n_optional], t);
                    if (current_level == 1)
                        this->optional_group[this->n_optional] = current_optional;
                    else
                        this->optional_group[this->n_optional] = -1;
                    this->n_optional++;
                }
                else if (in_block == B_PATTERN || in_block == B_CHAIN || in_block == B_FILTER || in_block == B_PREFIX || in_block == B_ORDER)
                {
                    if (tokens[i].end - tokens[i].start > 65535) {
                        *message = (char *)malloc(128 + tokens[i].end - tokens[i].start);
                        if(!message) utils::out_of_memory(fd);
                        sprintf(*message, "Token is too long: %s", t);
                        goto send_parse_error;
                    }
                    switch (pos) {
                    case 0:
                        if(subject) free(subject);
                        subject = (char*)malloc(strlen(t)+1);
                        if(!subject) utils::out_of_memory();
                        strcpy(subject, t);
                        break;
                    case 1:
                        if(predicate) free(predicate);
                        predicate = (char*)malloc(strlen(t)+1);
                        if(!predicate) utils::out_of_memory();
                        strcpy(predicate, t);
                        break;
                    case 2:
                        if(object) free(object);
                        object = (char*)malloc(strlen(t)+1);
                        if(!object) utils::out_of_memory();
                        strcpy(object, t);
                        break;
                    case 3:
                        if(datatype) free(datatype);
                        datatype = (char*)malloc(strlen(t)+1);
                        if(!datatype) utils::out_of_memory();
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
        if (endpoint == EP_PREFIX && this->_n_prefixes && operation == OP_PUT)
        {
            utils::logger(LOG, "PUT prefix request", this->request_id, 0);
            if (*message = this->commit_prefixes())
                goto send_parse_error;
        }
        else if (endpoint == EP_PREFIX && operation == OP_GET)
        {
            utils::logger(LOG, "GET prefix request", this->request_id, 0);
            *message = this->return_prefixes();
            return true;
        }
        else if (endpoint == EP_PREFIX && operation == OP_DELETE)
        {
            utils::logger(LOG, "DELETE prefix request", this->request_id, 0);
            if (*message = this->delete_prefixes())
                goto send_parse_error;
        }
        else if (endpoint == EP_TRIPLE && this->_n_triples && operation == OP_PUT)
        {
            utils::logger(LOG, "PUT triples request", this->request_id, 0);
            if (*message = this->commit_triples(pip))
                goto send_parse_error;
        }
        else if (endpoint == EP_TRIPLE && this->_n_send && operation == OP_GET)
        {
            utils::logger(LOG, "GET triples request", this->request_id, 0);
            *message = this->return_triples();
            return true;
        }
        else if (endpoint == EP_CHAIN && this->_n_triples && this->n_var && operation == OP_GET)
        {
            utils::logger(LOG, "GET triples request", this->request_id, 0);
            *message = this->return_triples_chain(this->request_id);
            return true;
        }
        else if (endpoint == EP_TRIPLE && this->_n_delete && operation == OP_DELETE)
        {
            utils::logger(LOG, "DELETE triples request", this->request_id, 0);
            if (*message = this->commit_delete(pip))
                goto send_parse_error;
        }
        return true;
    wrong_parameter:
        *message = (char *)malloc(128);
        if(!message) utils::out_of_memory(fd);
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
            if(!message) utils::out_of_memory(fd);
            strcpy(*message, "Parsing error");
        }
        char *inv = (char *)malloc(256 + strlen(*message));
        if(!inv) utils::out_of_memory(fd);
        sprintf(inv, "{\"Status\":\"Error\", \"RequestId\":\"%s\", \"Message\":\"%s\"}", this->request_id, *message);
        if (*message)
            free(*message);
        *message = inv;
        return false;
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

    private:

    char *save_parsed_item(char **subject, char **predicate, char **object, char **datatype, char *lang, int operation, int in_block, int current_level, int *pos, int *status) {
        char *message = NULL;
        bool done = false;
        if(subject == NULL) return NULL;
//printf("Save parsed item %s - %s\n", *subject, *predicate);
        if (*subject && *predicate) {
            if (in_block == B_PATTERN && *subject[0] && operation == OP_PUT) {
                message = this->make_triple(subject, predicate, object, datatype, lang, status, current_level);
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
                message = this->make_triple(subject, predicate, object, datatype, lang, status, current_level);
                done = true;
            }
            else if (in_block == B_ORDER && *subject[0] && operation == OP_GET) {
                message = this->save_order(*subject, *predicate, status);
                done = true;
            }
            else if (in_block == B_PREFIX && (*subject[0] || *predicate[0])) {
                message = this->make_prefix(*subject, *predicate, status);
                done = true;
            }
        }
        if (*subject) {
            if (in_block == B_FILTER && *subject[0] && operation == OP_GET) {
                message = this->save_filter(*subject, *predicate, *object, status, current_level);
                done = true;
            }
        }
        if(*subject) { free(*subject); *subject = NULL; }
        if(*predicate) { free(*predicate); *predicate = NULL; }
        if(*object) { free(*object); *object = NULL; }
        if(*datatype) { free(*datatype); *datatype = NULL; }
        return message;
    }

    char *make_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status, int current_level);
    char *delete_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status);
    char *get_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status);
    char *commit_delete(int *pip);
    char *commit_triples(int *pip);

    char *make_prefix(char *subject, char *object, int *status);
    char *resolve_prefix(char **s);
    int resolve_prefixes_in_triple(char **subject, char **predicate, char **object, char **datatype, char **message);
    char *commit_prefixes(void);
    char *return_prefixes(void);
    char *delete_prefixes(void);

    char *save_filter(char *subject, char *predicate, char *object, int *status, int current_level);
    char *save_order(char *subject, char *object, int *status);
    char *return_triples(void);
    int find_cond(char *obj);
    int add_cond(char *obj);
    void propagate_dependent(int i, int dep);
    bool check_candidate_match(char *s, char *predicate, int cond_o);
    int add_candidate(int j, char *s, char *datatype, char *lang);
    void add_solution(int i, int j, int cand, int sol, int bearer);
    bool get_candidates(int i, int j, int cand_subject, char *subject, char *predicate, char *object, bool negative, int bearer);
    void push_combination(int i, int *comb);
    void build_combination(int i, int id, int *comb, int ind_cond, int bearer);
    void global_push_combination(int i, int j);
    char *return_triples_chain(char *reqid);

};

   // Instantiate Request class, process request and free memory
    char *utils::process_request(char *buffer, int operation, int endpoint, int modifier, int *pip)
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
    //        utils::logger(LOG, "Goint to free_all", "", 0);
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
            if(!answer) utils::out_of_memory();
            sprintf(answer, "{\"RequestId\":\"%s\", \"Status\":\"Ok\"}", req.request_id);
            return answer;
        }
    }
