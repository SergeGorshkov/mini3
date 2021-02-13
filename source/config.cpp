
void load_config(char *message) {
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKENS];
    int r, status;
    char t[1024], prev_t[1024], cause[32]; t[0] = 0; prev_t[0] = 0;
    broker[0] = 0; broker_host[0] = 0; broker_port[0] = 0; broker_user[0] = 0; broker_password[0] = 0; broker_queue[0] = 0;
    char *rbuffer = file_get_contents("config.json");
	jsmn_init(&parser);
    memset(&tokens, 0, MAX_JSON_TOKENS*sizeof(jsmntok_t));
    r = jsmn_parse(&parser, rbuffer, strlen(rbuffer), tokens, MAX_JSON_TOKENS);
    for(int i=0; i<r; i++) {
		strcpy(prev_t, t);
        strncpy(t, (char*)((long)rbuffer+tokens[i].start), tokens[i].end-tokens[i].start);
        t[tokens[i].end-tokens[i].start] = 0;
		if(strcmp(prev_t, "broker") == 0)
			strcpy(broker, t);
		if(strcmp(prev_t, "broker_host") == 0)
			strcpy(broker_host, t);
		if(strcmp(prev_t, "broker_port") == 0)
			strcpy(broker_port, t);
		if(strcmp(prev_t, "broker_user") == 0)
			strcpy(broker_user, t);
		if(strcmp(prev_t, "broker_password") == 0)
			strcpy(broker_password, t);
		if(strcmp(prev_t, "broker_queue") == 0)
			strcpy(broker_queue, t);
		if(strcmp(prev_t, "broker_output_queue") == 0)
			strcpy(broker_output_queue, t);
		if(strcmp(prev_t, "database") == 0)
			strcpy(database_path, t);
	}
    free(rbuffer);
    if(database_path[0] == 0) {
        printf("Incomplete configuration\n");
        exit(0);
    }
}
