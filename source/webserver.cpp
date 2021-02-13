
/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit, int *pip) {
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr, *answer;
	static char buffer[BUFSIZE+1];
	int operation = 0, endpoint = 0;

	memset(buffer, 0, BUFSIZE+1);
	int pos = 0, content_length = 0, content_start = 0;
	do {
	    if( pos != 0 && content_length != 0 && (pos >= content_start + content_length) ) break;
	    ret = read(fd,(void*)((long)buffer+pos),BUFSIZE - pos); 	/* read Web request in one go */
	    if(ret>0) pos += ret;
	    if(!content_length || !content_start) {
			for(int k=0; k<pos; k++) {
				if(buffer[k] == 'C') {
					if(strncmp((char*)((long)buffer+k), "Content-Length", 14) == 0) {
						k += 16;
						int start = k;
						while(buffer[k] >= '0' && buffer[k] <= '9') k++;
						if(k > start) {
							char sub[1024];
							memcpy(sub, (void*)((long)buffer+start), k-start);
							sub[k-start] = 0;
							content_length = atoi(sub);
						}
					}
				}
				if(buffer[k] == 0x0d && buffer[k+1] == 0x0a && buffer[k+2] == 0x0d && buffer[k+3] == 0x0a)
				content_start = k+4;
			}
	    }
	} while (ret>0);
	if(pos > 0 && pos < BUFSIZE)	/* return code is valid chars */
		buffer[pos]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
//	logger(LOG,"request (webserver)",buffer,hit);
	if( strncmp(buffer,"GET ", 4) == 0 || strncmp(buffer, "get ", 4) == 0)
		operation = OP_GET;
	else if( strncmp(buffer, "POST ", 5) == 0 || strncmp(buffer, "post ", 5) == 0)
		operation = OP_POST;
	else if( strncmp(buffer, "PUT ", 4) == 0 || strncmp(buffer, "put ", 4) == 0 )
		operation = OP_PUT;
	else if( strncmp(buffer, "DELETE ", 7) == 0 || strncmp(buffer, "delete ", 7) == 0 )
		operation = OP_DELETE;
	if(!operation)
		logger(FORBIDDEN,"Only GET, POST, PUT, DELETE operations are supported", buffer, fd);

	if(strstr(buffer, "/prefix") > 0)
		endpoint = EP_PREFIX;
	else if(strstr(buffer, "/triple") > 0)
		endpoint = EP_TRIPLE;
	if(!endpoint)
		logger(FORBIDDEN,"Endpoint is not specified", buffer, fd);

	char *rp = (char*)malloc(strlen(buffer)+1);
	strcpy(rp, (char*)((long)strstr(buffer, "request=") + 8));
	if( rp == NULL || strlen(rp) == 0 )
		logger(FORBIDDEN,"No request= parameter is given", buffer, fd);
//	logger(LOG,"Param",rp,fd);
	char *response = process_request(rp, operation, endpoint, pip);
	len = strlen( response );
	(void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: mini3/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: application/json\n\n", VERSION, len); // Header + a blank line
//	logger(LOG,"Header",buffer,hit);
	logger(LOG,"Answer",response,hit);
	(void)write(fd,buffer,strlen(buffer));
	(void)write(fd,response,len);
	free(response);
	close(fd);
	exit(1);
}

int start_web_server(int argc, char **argv) {
	int i, port, pid, listenfd, socketfd, hit;
	socklen_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	printf("Start web server\n");
	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0)
		return 0; /* parent returns OK to shell */
	(void)signal(SIGCHLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	//(void)setpgrp();		/* break away from process group */

    triples = NULL; full_index = NULL; s_index = NULL; p_index = NULL; o_index = NULL; stringtable = NULL; global_block_ul = NULL;
	logger(LOG,"mini3 web server starting", argv[1], getpid());
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		logger(ERROR, "system call","socket",0);
	port = atoi(argv[1]);
	if(port < 0 || port >60000)
		logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
		logger(ERROR,"system call","bind",0);
	if(listen(listenfd, 64) < 0)
		logger(ERROR,"system call","listen",0);
	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			logger(ERROR,"system call","accept",0);
		int command;
		if((pid = fork()) < 0)
			logger(ERROR,"system call","fork",0);
		else {
			if(pid == 0) { 	/* child */
				(void)close(listenfd);
				web(socketfd, hit, global_web_pip); /* never returns */
			} else { 	/* parent */
				(void)close(socketfd);
			}
		}
	}
}
