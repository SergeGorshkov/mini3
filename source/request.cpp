
class request {

public:
    int _n_triples = 0, _n_prefixes = 0;
    local_triple _triples[1024];
    prefix _prefixes[1024];
    unsigned long *_to_delete = 0, _n_delete = 0;
    unsigned long *_to_send = 0, _n_send = 0;
    char type[256];
    char request_id[1024];

    bool parse_request(char *buffer, char **message, int fd, int operation, int endpoint, int *pip) {
        int r = 0, pos = 0, status = 0;
        char *t, subject[MAX_VALUE_LEN], predicate[MAX_VALUE_LEN], object[MAX_VALUE_LEN], datatype[MAX_VALUE_LEN], lang[8];
        bool in_method = false, in_id = false, in_pattern = false, in_prefix = false;

        init_globals(O_RDONLY);
        unicode_to_utf8(buffer);
//logger(LOG, "Request: ", buffer, 0);
        t = (char*)malloc(strlen(buffer)+1);
        t[0] = 0; this->type[0] = 0; this->request_id[0] = 0; subject[0] = 0; predicate[0] = 0; object[0] = 0; datatype[0] = 0; lang[0] = 0;

        // JSON
        if(buffer[0] != '{') {
            *message = (char*)malloc(128);
            strcpy(*message, "Unknown request format (expecting: JSON)");
            this->free_all();
            goto send_parse_error;
        }
        jsmn_parser parser;
        jsmntok_t tokens[MAX_JSON_TOKENS];
        jsmn_init(&parser);
        r = jsmn_parse(&parser, buffer, strlen(buffer), tokens, MAX_JSON_TOKENS);
//printf("%i tokens\n",r);
        for(int i=0; i<r; i++) {
            strncpy(t, (char*)((long)buffer + tokens[i].start), tokens[i].end - tokens[i].start);
            t[tokens[i].end - tokens[i].start] = 0;
//printf("token %s: type: %i, start: %i, end: %i, size: %i, in_prefix: %i\n",t,tokens[i].type,tokens[i].start,tokens[i].end,tokens[i].size,in_prefix);
            if(in_pattern && tokens[i].type == 2 && subject[0] && operation == OP_PUT) {
                pos = 0;
                if(*message = this->make_triple(subject, predicate, object, datatype, lang))
                    goto send_parse_error;
                subject[0] = predicate[0] = object[0] = datatype[0] = lang[0] = 0;
            }
            else if(in_pattern && tokens[i].type == 2 && subject[0] && operation == OP_DELETE) {
                pos = 0;
                *message = this->delete_triple(subject, predicate, object, datatype, lang, &status);
                if(status == -1)
                    goto send_parse_error;
                subject[0] = predicate[0] = object[0] = datatype[0] = lang[0] = 0;
            }
            else if(in_pattern && tokens[i].type == 2 && subject[0] && operation == OP_GET) {
                pos = 0;
                *message = this->get_triple(subject, predicate, object, datatype, lang, &status);
                if(status == -1)
                    goto send_parse_error;
                subject[0] = predicate[0] = object[0] = datatype[0] = lang[0] = 0;
            }
            else if(in_prefix && (subject[0] || object[0]) && (tokens[i].type == 1 || tokens[i].type == 2)) {
                pos = 0;
                if(*message = this->make_prefix(subject, object))
                    goto send_parse_error;
                subject[0] = object[0] = 0;
            }
            
            if(tokens[i].type == 3 && t[0] != '{') {
                if(!in_pattern && !in_prefix) strtolower(t);
                else descreen(t);
                if(strcmp(t, "method") == 0) {
                    in_method = true; continue;
                }
                if(strcmp(t, "requestid") == 0) {
                    in_id = true; continue;
                }
                if(strcmp(t, "pattern") == 0 || strcmp(t, "triple") == 0) {
                    if(!endpoint) endpoint = EP_TRIPLE;
                    if(endpoint != EP_TRIPLE) {
                        *message = (char*)malloc(128);
                        sprintf(*message, "%s parameter cannot be specified in the request to this endpoint", t);
                        this->free_all();
                        goto send_parse_error;
                    }
                    in_pattern = true; continue;
                }
                if(strcmp(t, "prefix") == 0) {
                    if(!endpoint) endpoint = EP_PREFIX;
                    if(endpoint != EP_PREFIX) {
                        *message = (char*)malloc(128);
                        sprintf(*message, "%s parameter cannot be specified in the request to this endpoint", t);
                        this->free_all();
                        goto send_parse_error;
                    }
                    in_prefix = true; continue;
                }
                if(in_id) {
                    strcpy(this->request_id, t);
                    in_id = false;
                }
                else if(in_method) {
                    if(strcmp(t, "get") == 0)
                        operation = OP_GET;
                    else if(strcmp(t, "put") == 0)
                        operation = OP_PUT;
                    else if(strcmp(t, "post") == 0)
                        operation = OP_POST;
                    else if(strcmp(t, "delete") == 0)
                        operation = OP_DELETE;
                    if(!operation) {
                        *message = (char*)malloc(128 + strlen(t));
                        sprintf(*message, "Unknown method: %s", t);
                        goto send_parse_error;
                    }
                    strcpy(this->type, t);
                    in_method = false;
                }
                else if(in_pattern) {
                    if(tokens[i].end - tokens[i].start > MAX_VALUE_LEN - 1) {
                        *message = (char*)malloc(128 + tokens[i].end - tokens[i].start);
                        sprintf(*message, "Token is too long: %s", t);
                        goto send_parse_error;
                    }
                    switch(pos) {
                        case 0: strcpy(subject, t); break;
                        case 1: strcpy(predicate, t); break;
                        case 2: strcpy(object, t); break;
                        case 3: strcpy(datatype, t); break;
                        case 4: if(strlen(t) < 7) strcpy(lang, t); break;
                    }
                    pos++;
                    continue;
                }
                else if(in_prefix) {
                    if(tokens[i].end - tokens[i].start > MAX_VALUE_LEN - 1) {
                        *message = (char*)malloc(128 + tokens[i].end - tokens[i].start);
                        sprintf(*message, "Token is too long: %s", t);
                        goto send_parse_error;
                    }
                    switch(pos) {
                        case 0: strcpy(subject, t); break;
                        case 1: strcpy(object, t); break;
                    }
                    pos++;
                    continue;
                }
            }
        }

        // Записываем последний блок из массива
        if(in_prefix && subject[0] && (operation == OP_PUT || operation == OP_POST)) {
            if(*message = this->make_prefix(subject, object))
                goto send_parse_error;
        }
        else if(in_pattern && subject[0] && operation == OP_PUT) {
            if(*message = this->make_triple(subject, predicate, object, datatype, lang))
                goto send_parse_error;
        }
        else if(in_pattern && subject[0] && operation == OP_DELETE) {
            *message = this->delete_triple(subject, predicate, object, datatype, lang, &status);
            if(status == -1)
                goto send_parse_error;
        }
        else if(in_pattern && subject[0] && operation == OP_GET) {
            *message = this->get_triple(subject, predicate, object, datatype, lang, &status);
            if(status == -1)
                goto send_parse_error;
        }
        free(buffer); buffer = NULL;
        free(t); t = NULL;

        // Закончили обработку JSON, начинаем выполнять действия
        if(this->_n_prefixes && (operation == OP_PUT || operation == OP_POST)) {
            logger(LOG, "PUT prefix request", this->request_id, 0);
            if(*message = this->commit_prefixes())
                goto send_parse_error;
        }
        else if(this->_n_triples && operation == OP_PUT) {
            logger(LOG, "PUT triples request", this->request_id, 0);
            if(*message = this->commit_triples(pip))
                goto send_parse_error;
        }
        else if(this->_n_send && operation == OP_GET) {
            logger(LOG, "GET triples request", this->request_id, 0);
            *message = this->return_triples();
            return true;
        }
        else if(this->_n_delete && operation == OP_DELETE) {
            logger(LOG, "DELETE triples request", this->request_id, 0);
            if(*message = this->commit_delete(pip))
                goto send_parse_error;
        }
        return true;
    send_parse_error:
        if(buffer) free(buffer); if(t) free(t);
        if(!*message) {
            *message = (char*)malloc(128);
            strcpy(*message, "Parsing error");
        }
        char *inv = (char*)malloc(256 + strlen(*message));
        sprintf(inv, "{\"Status\":\"Error\",\"RequestId\":\"%s\",\"Message\":\"%s\"}", this->request_id, *message);
        if(*message)
            free(*message);
	    *message = inv;
        return false;
    }

    char *make_triple(char *subject, char *predicate, char *object, char *datatype, char *lang) {
        char *message;
        if(resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1)
            return message;
        if(this->_n_triples >= 1024) {
            message = (char*)malloc(128);
            sprintf(message, "Too many triples or patterns");
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        strcpy(this->_triples[this->_n_triples].s, subject);
        strcpy(this->_triples[this->_n_triples].p, predicate);
        this->_triples[this->_n_triples].o = (char*)malloc(strlen(object) + 1);
        strcpy(this->_triples[this->_n_triples].o, object);
        strcpy(this->_triples[this->_n_triples].d, datatype);
        strcpy(this->_triples[this->_n_triples].l, lang);
        char data[MAX_VALUE_LEN * 5 + 128];
        sprintf(data, "%s | %s | %s | %s | %s", subject, predicate, object, datatype, lang);
        SHA1((unsigned char*)data, strlen(data), this->_triples[this->_n_triples].hash);
        this->_triples[this->_n_triples].mini_hash = get_mini_hash(this->_triples[this->_n_triples].hash);
        this->_n_triples++;
        return NULL;
    }

    char *delete_triple(char *subject, char *predicate, char *object, char *datatype, char *lang, int *status) {
        char *message;
        if(resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1)
            return message;
        if(this->_n_triples >= 1024) {
            message = (char*)malloc(128);
            sprintf(message, "Too many triples or patterns");
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        unsigned long n = 0;
        unsigned long *res = find_matching_triples(subject, predicate, object, datatype, lang, &n);
        if(n > 0) {
            if(this->_to_delete) this->_to_delete = (unsigned long*)realloc(this->_to_delete, sizeof(unsigned long)*(this->_n_delete + n));
            else this->_to_delete = (unsigned long*)malloc(sizeof(unsigned long)*(this->_n_delete + n));
            for(unsigned long i=0; i<n; i++)
                this->_to_delete[this->_n_delete + i] = res[i];
            this->_n_delete += n;
        }
        free(res);
        return NULL;
    }

    char *get_triple(char *subject, char *predicate, char *object, char *datatype, char *lang, int *status) {
        char *message;
        if(resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1) {
            *status = -1;
            return message;
        }
        if(this->_n_triples >= 1024) {
            message = (char*)malloc(128);
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        unsigned long n = 0;
        unsigned long *res = find_matching_triples(subject, predicate, object, datatype, lang, &n);
        if(n > 0) {
            if(this->_to_send) this->_to_send = (unsigned long*)realloc(this->_to_send, sizeof(unsigned long)*(this->_n_send + n));
            else this->_to_send = (unsigned long*)malloc(sizeof(unsigned long)*(this->_n_send + n));
            for(unsigned long i=0; i<n; i++)
                this->_to_send[this->_n_send + i] = res[i];
            this->_n_send += n;
        }
        free(res);
        *status = 0;
        return NULL;
    }

    char *make_prefix(char *subject, char *object) {
        char *message;
        if(this->_n_prefixes >= 1024) {
            message = (char*)malloc(128);
            sprintf(message, "Too many prefixes");
            return message;
        }
        strcpy(this->_prefixes[this->_n_prefixes].shortcut, subject);
        strcpy(this->_prefixes[this->_n_prefixes].value, object);
        this->_prefixes[this->_n_prefixes].len = strlen(this->_prefixes[this->_n_prefixes].shortcut);
        this->_n_prefixes++;
        return NULL;
    }

    char *resolve_prefix(char *s) {
        char st[MAX_VALUE_LEN];
        if(strncmp(s, "http", 4) == 0 || s[0]=='*') return NULL;
        for(int i=0; i<*n_prefixes; i++) {
            if(strncmp(s, prefixes[i].shortcut, prefixes[i].len) == 0 && s[prefixes[i].len] == ':') {
                if(strlen(prefixes[i].value) + strlen(s) + 1 > MAX_VALUE_LEN) {
                    char *message = (char*)malloc(128+strlen(s));
                    sprintf(message, "Cannot resolve prefix, URI is too long", s);
                    return message;
                }
                strcpy(st, prefixes[i].value);
                strcpy(st + strlen(st), (char*)((long)s + prefixes[i].len + 1));
                strcpy(s, st);
                return NULL;
            }
        }
        for(int i=0; i<*n_prefixes; i++) {
            if(prefixes[i].shortcut[0] != 0) continue;
            strcpy(st, prefixes[i].value);
            strcpy(st + strlen(st), s);
            strcpy(s, st);
        }
        return NULL;
    }

    int resolve_prefixes_in_triple(char *subject, char *predicate, char *object, char * datatype, char **message) {
        if(subject[0]) {
            if(*message = this->resolve_prefix(subject))
                return -1;
        }
        if(predicate[0]) {
            if(*message = this->resolve_prefix(predicate))
                return -1;
        }
        if(datatype[0] == 0) {
            if(*message = this->resolve_prefix(object))
                return -1;
        }
        else {
            if(*message = this->resolve_prefix(datatype))
                return -1;
        }
        return 0;
    }

    char *commit_prefixes() {
//logger(LOG, "Commit prefixes", "", this->_n_prefixes);
        bool added = false;
        sem_wait(psem);
        for(int i=0; i < this->_n_prefixes; i++) {
            bool found = false;
            for(int j=0; j < (*n_prefixes); j++) {
                if(strcmp(prefixes[j].shortcut, this->_prefixes[i].shortcut) == 0) {
                    found = true;
                    if(strcmp(prefixes[j].value, this->_prefixes[i].value) != 0) {
                        strcpy(prefixes[j].value, this->_prefixes[i].value);
                        added = true;
                    }
                }
            }
            if(!found) {
                memcpy(&prefixes[(*n_prefixes)++], &this->_prefixes[i], sizeof(prefix));
                added = true;
            }
        }
        sem_post(psem);
        save_globals();
//logger(LOG, "Prefixes committed", "", *n_prefixes);
        return NULL;
    }

    char *return_triples(void) {
//logger(LOG, "(child) Return triples", "", this->_n_send);
        char *answer = (char*)malloc(this->_n_send*1024*10);
        sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Triples\":[", this->request_id);
        for(int i=0; i<this->_n_send; i++) {
            char ans[1024*10];
            sprintf(ans, "[\"%s\", \"%s\", \"%s\"", get_string(triples[this->_to_send[i]].s_pos), get_string(triples[this->_to_send[i]].p_pos), get_string(triples[this->_to_send[i]].o_pos));
            if( triples[this->_to_send[i]].d_len || triples[this->_to_send[i]].l[0]) {
                char *dt = get_string(triples[this->_to_send[i]].d_pos);
                if(dt) sprintf((char*)((unsigned long)ans + strlen(ans)), ", \"%s\"", dt);
                else strcat(ans, ", \"\"");
                if( triples[this->_to_send[i]].l[0] )
                    sprintf((char*)((unsigned long)ans + strlen(ans)), ", \"%s\"", triples[this->_to_send[i]].l);
            }
            strcat(ans, "]");
            if( i > 0 ) strcat(answer, ", ");
            strcat(answer, ans);
        }
        strcat(answer, "]}");
        return answer;
    }

    char *commit_triples(int *pip) {
        if(pip[1] == -1) return NULL;
        char *message = (char*)malloc(1024*10); message[0] = 0;
        sem_wait(wsem);
//logger(LOG, "(child) Commit triples", "", this->_n_triples);
        for(int i=0; i<this->_n_triples; i++) {
            // Иначе отправляем в родительский процесс для записи
            int command = COMMIT_TRIPLE;
            if(!write_to_pipe(pip[1], command, &this->_triples[i])) {
                strcpy(message, "Pipe write failed");
                sem_post(wsem);
                return message;
            }
            int ret = 0;
    		if(!read(pip[2], &ret, sizeof(int)))
                logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
            if(ret == RESULT_ERROR)
                strcat(message, "Transaction commit error. ");
    	    if(ret & RESULT_CHUNKS_REBUILT)
	        	chunks_rebuilt = true;
    	    if(ret & RESULT_TRIPLES_REALLOCATED)
	        	triples_reallocated = true;
        }
        sem_post(wsem);
        if(message[0])
            return message;
        free(message);
        return NULL;
    }

    char *commit_delete(int *pip) {
//logger(LOG, "(child) Commit delete", "", this->_n_delete);
        if(pip[1] == -1) return NULL;
        char *message = (char*)malloc(1024*10); message[0] = 0;
        int command = DELETE_TRIPLE;
        sem_wait(wsem);
        for(int i=0; i<this->_n_delete; i++) {
            if(!write(pip[1], &command, sizeof(int))) { char *message = (char*)malloc(128); strcpy(message, "Error writing to pipe"); sem_post(wsem); return message; }
            if(!write(pip[1], &this->_to_delete[i], sizeof(unsigned long))) { char *message = (char*)malloc(128); strcpy(message, "Error writing to pipe"); sem_post(wsem); return message; }
            int ret = 0;
    		if(!read(pip[2], &ret, sizeof(int)))
                logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
            if(ret == RESULT_ERROR)
                strcat(message, "Transaction commit error. ");
        }
        sem_post(wsem);
        free(this->_to_delete);
        if(message[0])
            return message;
        free(message);
        return NULL;
    }

    void free_all(void) {
        for(int i=0; i<this->_n_triples; i++) {
            if(this->_triples[i].o)
                free(this->_triples[i].o);
        }
        if(prefixes) munmap(prefixes, N_MAX_PREFIX*sizeof(prefix));
        if(triples && !triples_reallocated) munmap(triples, (*allocated)*sizeof(triple));
        if(chunks_size) munmap(chunks_size, MAX_CHUNKS*sizeof(unsigned long));
        if(s_chunks_size) munmap(s_chunks_size, MAX_CHUNKS*sizeof(unsigned long));
        if(p_chunks_size) munmap(p_chunks_size, MAX_CHUNKS*sizeof(unsigned long));
        if(o_chunks_size) munmap(o_chunks_size, MAX_CHUNKS*sizeof(unsigned long));
    	if(!chunks_rebuilt) {
    	    if(full_index) munmap(full_index, (*n_chunks)*CHUNK_SIZE*sizeof(mini_index));
    	    if(s_index) munmap(s_index, (*n_chunks)*CHUNK_SIZE*sizeof(mini_index));
    	    if(p_index) munmap(p_index, (*n_chunks)*CHUNK_SIZE*sizeof(mini_index));
    	    if(o_index) munmap(o_index, (*n_chunks)*CHUNK_SIZE*sizeof(mini_index));
    	}
        if(stringtable) munmap(stringtable, *string_allocated);
        if(global_block_ul) munmap(global_block_ul, sizeof(unsigned long)*N_GLOBAL_VARS);
        triples = NULL; full_index = NULL; s_index = NULL; p_index = NULL; o_index = NULL; stringtable = NULL; global_block_ul = NULL;
    }

};

    char *process_request(char *buffer, int operation, int endpoint, int *pip) {
		request req;
		char *answer = NULL;
		#ifdef PROFILE
		struct timeval start, end;
		gettimeofday(&start, NULL);
		#endif
		char *pmessage = NULL;
		if(!req.parse_request(buffer, &pmessage, 0, operation, endpoint, pip)) {
		    req.free_all();
    		return pmessage;
		}
		else {
            #ifdef PROFILE
            gettimeofday(&end, NULL);
            printf("Total request processing %f\n", (float)((float)end.tv_sec-(float)start.tv_sec+(float)(end.tv_usec-start.tv_usec)/1000000));
            #endif
			if(pmessage) {
        		req.free_all();
                return pmessage;
            }
    		req.free_all();
            answer = (char*)malloc(1200);
            sprintf(answer, "{\"RequestId\":\"%s\",\"Status\":\"Ok\"}", req.request_id);
			return answer;
		}
    }
