    char *request::make_prefix(char *subject, char *object, int *status)
    {
        char *message;
        if (this->_n_prefixes >= 1024)
        {
            message = (char *)malloc(128);
            if(!message) utils::out_of_memory();
            sprintf(message, "Too many prefixes");
            *status = -1;
            return message;
        }
        strcpy(this->_prefixes[this->_n_prefixes].shortcut, subject);
        strcpy(this->_prefixes[this->_n_prefixes].value, object);
        this->_prefixes[this->_n_prefixes].len = strlen(this->_prefixes[this->_n_prefixes].shortcut);
        this->_n_prefixes++;
        return NULL;
    };

    char *request::resolve_prefix(char **s)
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
            if (!found && this->n_var < N_MAX_VARIABLES)
                strcpy(this->var[this->n_var++].obj, *s);
            return NULL;
        }
        for (int i = 0; i < *n_prefixes; i++)
        {
            if (strncmp(*s, prefixes[i].shortcut, prefixes[i].len) == 0 && ((char)*(char*)((unsigned long)*s + prefixes[i].len) == ':'))
            {
                char *st = (char*)malloc(strlen(*s) + strlen(prefixes[i].value) + 1);
                if(!st) utils::out_of_memory();
                strcpy(st, prefixes[i].value);
                strcat(st, (char *)((unsigned long)(*s) + prefixes[i].len + 1));
                *s = (char*)realloc(*s, strlen(st) + 1);
                if(!*s) utils::out_of_memory();
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
            if(!st) utils::out_of_memory();
            strcpy(st, prefixes[i].value);
            strcat(st, *s);
            *s = (char*)realloc(*s, strlen(st) + 1024);
            if(!*s) utils::out_of_memory();
            strcpy(*s, st);
            free(st);
            break;
        }
        return NULL;
    };

    int request::resolve_prefixes_in_triple(char **subject, char **predicate, char **object, char **datatype, char **message)
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
    };

    char *request::commit_prefixes(void)
    {
        //utils::logger(LOG, "Commit prefixes", "", this->_n_prefixes);
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
        transaction::save_globals();
        //utils::logger(LOG, "Prefixes committed", "", *n_prefixes);
        return NULL;
    };

    char *request::return_prefixes(void)
    {
        char *result = (char*)malloc((*n_prefixes)*1600);
        if(!result) utils::out_of_memory();
        sprintf(result, "{\"Status\":\"Ok\", \"RequestId\":\"%s\", \"Prefixes\": [", this->request_id);
        //utils::logger(LOG, "Get prefixes", "", this->_n_prefixes);
        for (int j = 0; j < (*n_prefixes); j++)
        {
            if(j>0) strcat(result, ", ");
            sprintf((char*)((unsigned long)result+strlen(result)), "{\"%s\": \"%s\"}", prefixes[j].shortcut, prefixes[j].value);
        }
        strcat(result,"]}");
        return result;
    };

    char *request::delete_prefixes(void)
    {
        //utils::logger(LOG, "Delete prefixes", "", this->_n_prefixes);
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
        transaction::save_globals();
        //utils::logger(LOG, "Prefixes deleted", "", *n_prefixes);
        return NULL;
    };
