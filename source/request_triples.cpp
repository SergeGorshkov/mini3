    char *request::make_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
    {
        char *message;
//printf("Make triple %s - %s - %s - %s - %s\n", *subject, *predicate, *object, *datatype, lang);
        if( strcmp(*subject, "*") == 0 || strcmp(*subject, "*") == 0 ) {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
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
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        this->_triples[this->_n_triples].s = (char *)malloc(strlen(*subject) + 1);
        if(!this->_triples[this->_n_triples].s) utils::out_of_memory();
        strcpy(this->_triples[this->_n_triples].s, *subject);
        this->_triples[this->_n_triples].p = (char *)malloc(strlen(*predicate) + 1);
        if(!this->_triples[this->_n_triples].p) utils::out_of_memory();
        strcpy(this->_triples[this->_n_triples].p, *predicate);
        this->_triples[this->_n_triples].o = (char *)malloc(strlen(*object) + 1);
        if(!this->_triples[this->_n_triples].o) utils::out_of_memory();
        strcpy(this->_triples[this->_n_triples].o, *object);
        int datatype_len = 0;
        if(datatype != NULL) {
            if(*datatype != NULL) {
                if(*datatype[0] != 0) {
                    this->_triples[this->_n_triples].d = (char *)malloc(strlen(*datatype) + 1);
                    if(!this->_triples[this->_n_triples].d) utils::out_of_memory();
                    strcpy(this->_triples[this->_n_triples].d, *datatype);
                    datatype_len = strlen(*datatype);
                }
            }
        }
        if(!datatype_len) this->_triples[this->_n_triples].d = NULL;
        strcpy(this->_triples[this->_n_triples].l, lang);
        char *data = (char*)malloc(strlen(*subject) + strlen(*predicate) + strlen(*object) + datatype_len + 128);
        if(!data) utils::out_of_memory();
        sprintf(data, "%s | %s | %s", *subject, *predicate, *object);
        memset(this->_triples[this->_n_triples].hash, 0, SHA_DIGEST_LENGTH);
        SHA1((unsigned char *)data, strlen(data), this->_triples[this->_n_triples].hash);
        free(data);
        this->_triples[this->_n_triples].mini_hash = triples_index::get_mini_hash(this->_triples[this->_n_triples].hash);
        this->_n_triples++;
        return NULL;
    };

    char *request::delete_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
    {
        char *message;
        if (resolve_prefixes_in_triple(subject, predicate, object, datatype, &message) == -1) {
            *status = -1;
            return message;
        }
        if (this->_n_triples >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(triple));
        unsigned long n = 0;
        unsigned long *res = triples_index::find_matching_triples(*subject, *predicate, *object, (datatype ? *datatype : NULL), lang, &n);
        if (n > 0)
        {
            if (this->_to_delete)
                this->_to_delete = (unsigned long *)realloc(this->_to_delete, sizeof(unsigned long) * (this->_n_delete + n));
            else
                this->_to_delete = (unsigned long *)malloc(sizeof(unsigned long) * (this->_n_delete + n));
            if(!this->_to_delete) utils::out_of_memory();
            for (unsigned long i = 0; i < n; i++)
                this->_to_delete[this->_n_delete + i] = res[i];
            this->_n_delete += n;
        }
        free(res);
        return NULL;
    };

    char *request::get_triple(char **subject, char **predicate, char **object, char **datatype, char *lang, int *status)
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
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many triples or patterns");
            *status = -1;
            return message;
        }
        if (this->n_var > 0)
        {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Variables are not allowed in patterns; use Chain parameter");
            *status = -1;
            return message;
        }
        memset(&this->_triples[this->_n_triples], 0, sizeof(local_triple));
        unsigned long n = 0;
        unsigned long *res = triples_index::find_matching_triples(*subject, *predicate, *object, (datatype ? *datatype : NULL), lang, &n);
        if (n > 0)
        {
            if (this->_to_send)
                this->_to_send = (unsigned long *)realloc(this->_to_send, sizeof(unsigned long) * (this->_n_send + n));
            else
                this->_to_send = (unsigned long *)malloc(sizeof(unsigned long) * (this->_n_send + n));
            if(!this->_to_send) utils::out_of_memory();
            for (unsigned long i = 0; i < n; i++)
                this->_to_send[this->_n_send + i] = res[i];
            this->_n_send += n;
        }
        free(res);
        *status = 0;
        return NULL;
    };

    char *request::commit_triples(int *pip)
    {
        if (pip[1] == -1)
            return NULL;
        char *message = (char *)malloc(1024 * 10);
        if(!message) utils::out_of_memory();
        message[0] = 0;
        sem_wait(wsem);
        //utils::logger(LOG, "(child) Commit triples", "", this->_n_triples);
        for (int i = 0; i < this->_n_triples; i++)
        {
//utils::logger(LOG, "Commit triple", "", i);
            // Send to the parent process for commit
            int command = COMMIT_TRIPLE;
            if (!transaction::write_to_pipe(pip[1], command, &this->_triples[i]))
            {
                strcpy(message, "Pipe write failed");
                sem_post(wsem);
                return message;
            }
            int ret = 0;
            if (!read(pip[2], &ret, sizeof(int)))
                utils::logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
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
    };

    char *request::commit_delete(int *pip)
    {
        //utils::logger(LOG, "(child) Commit delete", "", this->_n_delete);
        if (pip[1] == -1)
            return NULL;
        char *message = (char *)malloc(1024 * 10);
        if(!message) utils::out_of_memory();
        message[0] = 0;
        int command = DELETE_TRIPLE;
        sem_wait(wsem);
        for (int i = 0; i < this->_n_delete; i++)
        {
            if (!write(pip[1], &command, sizeof(int)))
            {
                char *message = (char *)malloc(128);
                if(!message) utils::out_of_memory();
                strcpy(message, "Error writing to pipe");
                sem_post(wsem);
                return message;
            }
            if (!write(pip[1], &this->_to_delete[i], sizeof(unsigned long)))
            {
                char *message = (char *)malloc(128);
                if(!message) utils::out_of_memory();
                strcpy(message, "Error writing to pipe");
                sem_post(wsem);
                return message;
            }
            int ret = 0;
            if (!read(pip[2], &ret, sizeof(int)))
                utils::logger(ERROR, "Error reading pipe when commiting transaction", "", 0);
            if (ret == RESULT_ERROR)
                strcat(message, "Transaction commit error. ");
        }
        sem_post(wsem);
        free(this->_to_delete);
        if (message[0])
            return message;
        free(message);
        return NULL;
    };

    char *request::return_triples(void) {
        //utils::logger(LOG, "(child) Return triples", "", this->_n_send);
        char *answer = (char *)malloc(this->_n_send * 1024 * 10);
        if(!answer) utils::out_of_memory();
        if(this->modifier & MOD_COUNT) {
            sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Count\":\"%i\"}", this->request_id, this->_n_send);
            return answer;
        }
        sprintf(answer, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Triples\":[", this->request_id);
        // Sort combinations. Coarse, temporary implementation, could be optimized
        for(int j = this->_n_orders-1; j >= 0; j--) {
            if(strcmp(this->_orders[j].direction, "desc") == 0) sort_order = 1;
            else sort_order = 0;
            char **unique = (char**)malloc(this->_n_send*sizeof(char*));
            if(!unique) utils::out_of_memory();
            unsigned long *orders = (unsigned long*)malloc(this->_n_send*sizeof(unsigned long));
            if(!orders) utils::out_of_memory();
            int sort_var_index = 0;
            char empty_str[2] = "\0";
            if(strcmp(this->_orders[j].variable, "predicate") == 0) sort_var_index = 1;
            else if(strcmp(this->_orders[j].variable, "object") == 0) sort_var_index = 2;
            for (int i = 0; i < this->_n_send; i++) {
                int pos = (sort_var_index == 1 ? triples[this->_to_send[i]].p_pos : (sort_var_index == 2 ? triples[this->_to_send[i]].o_pos : triples[this->_to_send[i]].s_pos ));
                unique[i] = utils::get_string(pos);
                if (!unique[i]) unique[i] = empty_str;
            }
            qsort(unique, this->_n_send, sizeof(char*), utils::cstring_cmp);
            // Cycle through sorted unique items
            for (int y = 0; y < this->_n_send; y++) {
                // Cycle through all triples to find the next unique item
                for (int z = 0; z < this->_n_send; z++) {
                    bool placed = false;
                    char *obj = utils::get_string((sort_var_index == 1 ? triples[this->_to_send[z]].p_pos : (sort_var_index == 2 ? triples[this->_to_send[z]].o_pos : triples[this->_to_send[z]].s_pos )));
                    if (!obj) continue;
                    if (strcmp(obj, unique[y]) == 0) {
                        bool found = false;
                        for (int v = 0; v < y; v++) {
                            if(orders[v] == this->_to_send[z]) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
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
        // Result output
        int sent = 0;
        for (int i = 0; i < this->_n_send; i++) {
            if (this->offset != -1 && i < this->offset) continue;
            if (this->limit != -1 && sent >= this->limit) break;
            sent++;
            char ans[1024 * 10];
            sprintf(ans, "[\"%s\", \"%s\", \"%s\"", utils::get_string(triples[this->_to_send[i]].s_pos), utils::get_string(triples[this->_to_send[i]].p_pos), utils::get_string(triples[this->_to_send[i]].o_pos));
            if (triples[this->_to_send[i]].d_len) {
                char *dt = utils::get_string(triples[this->_to_send[i]].d_pos);
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
    };
