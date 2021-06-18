/*
    This file is part of mini-3 triple store.

    Mini-3 triple store is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Mini-3 triple store is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Mini-3 triple store.  If not, see <https://www.gnu.org/licenses/>.
*/

class webserver {

	public:

	// Child web server function
	static void web(int fd, int hit, int *pip)
	{
		int j, file_fd, buflen;
		long i, ret, len;
		char *fstr, *answer;
		char *buffer, *empty = "request= ";
		int operation = 0, endpoint = 0, modifier = 0;

		buffer = (char*)malloc(BUFSIZE*2 + 1);
		if(!buffer) utils::out_of_memory(fd);
		buffer[0] = 0;
		int pos = 0, content_length = 0, content_start = 0;
		do
		{
			if (pos != 0 && content_length != 0 && (pos >= content_start + content_length))
				break;
			ret = read(fd, (void *)((unsigned long)buffer + pos), BUFSIZE);
			if (ret > 0)
				pos += ret;
			if (content_length == 0 || content_start == 0)
			{
				for (int k = 0; k < pos; k++)
				{
					if (buffer[k] == 'C')
					{
						if (content_length == 0 && strncmp((char *)((unsigned long)buffer + k), "Content-Length", 14) == 0)
						{
							k += 16;
							int start = k;
							while (buffer[k] >= '0' && buffer[k] <= '9')
								k++;
							if (k > start && k-start < 1023)
							{
								char sub[1024];
								memcpy(sub, (void *)((unsigned long)buffer + start), k - start);
								sub[k - start] = 0;
								content_length = atoi(sub);
								buffer = (char*)realloc(buffer, BUFSIZE + content_length);
								if(!buffer)
									utils::logger(FORBIDDEN, "Request is too large", "", fd);
							}
						}
					}
					if (content_start == 0 && buffer[k] == 0x0d && buffer[k + 1] == 0x0a && buffer[k + 2] == 0x0d && buffer[k + 3] == 0x0a)
						content_start = k + 4;
				}
			}
		} while (ret > 0);

		if (pos > 0 && pos < ( content_length > 0 ? BUFSIZE + content_length : BUFSIZE ))
			buffer[pos] = 0;
		else
			buffer[0] = 0;
		for (i = 0; i < content_start; i++)
			if (buffer[i] == '\r' || buffer[i] == '\n')
				buffer[i] = '*';
		if (strncmp(buffer, "GET ", 4) == 0 || strncmp(buffer, "get ", 4) == 0)
			operation = OP_GET;
		else if (strncmp(buffer, "POST ", 5) == 0 || strncmp(buffer, "post ", 5) == 0)
			operation = OP_GET;
		else if (strncmp(buffer, "PUT ", 4) == 0 || strncmp(buffer, "put ", 4) == 0)
			operation = OP_PUT;
		else if (strncmp(buffer, "PATCH ", 4) == 0 || strncmp(buffer, "patch ", 4) == 0)
			operation = OP_PUT;
		else if (strncmp(buffer, "DELETE ", 7) == 0 || strncmp(buffer, "delete ", 7) == 0)
			operation = OP_DELETE;
		if (!operation)
			utils::logger(FORBIDDEN, "Only GET, POST, PUT, DELETE operations are supported", buffer, fd);

		char *endpos = strstr(buffer, "HTTP/");
		char c = endpos[0];
		endpos[0] = 0;
		if ((unsigned long)strstr(buffer, "/prefix") > 0)
			endpoint = EP_PREFIX;
		else if ((unsigned long)strstr(buffer, "/triple") > 0)
			endpoint = EP_TRIPLE;
		else if ((unsigned long)strstr(buffer, "/chain") > 0)
			endpoint = EP_CHAIN;
		endpos[0] = c;
		if ((unsigned long)strstr(buffer, "/count") > 0)
			modifier |= MOD_COUNT;
		if (!endpoint)
			utils::logger(FORBIDDEN, "Endpoint is not specified", buffer, fd);

		char *rp = (char *)malloc(strlen(buffer) + 1);
		if(!rp) utils::out_of_memory(fd);
		char *req = strstr(buffer, "request=");
		if(!req && endpoint == EP_PREFIX && operation == OP_GET)
			req = empty;
		if(!req)
			utils::logger(FORBIDDEN, "No request= parameter is given", "", fd);
		strcpy(rp, (char *)((unsigned long)req + 8));
		if (rp == NULL || strlen(rp) == 0) {
			free(buffer);
			utils::logger(FORBIDDEN, "No request= parameter is given", "", fd);
		}
		#ifdef LOG_REQUESTS
		utils::logger(LOG, "Request", rp, hit);
		#endif
		char *response = utils::process_request(rp, operation, endpoint, modifier, pip);
		len = strlen(response);
		(void)sprintf(buffer, "HTTP/1.1 200 OK\nServer: mini-3/%s\nContent-Length: %ld\nConnection: close\nContent-Type: application/json\n\n", VERSION, len); // Header + a blank line
		#ifdef LOG_REQUESTS
		utils::logger(LOG, "Answer", response, hit);
		#endif
		(void)write(fd, buffer, strlen(buffer));
		(void)write(fd, response, len);
		free(response);
		free(buffer);
		close(fd);
		exit(1);
	}

	static int start_web_server(int argc, char **argv)
	{
		int i, port, pid, listenfd, socketfd, hit;
		socklen_t length;
		static struct sockaddr_in cli_addr;	 /* static = initialised to zeros */
		static struct sockaddr_in serv_addr; /* static = initialised to zeros */

		printf("Start web server\n");
		if (fork() != 0)
			return 0;
		(void)signal(SIGCHLD, SIG_IGN);
		(void)signal(SIGHUP, SIG_IGN);

		triples = NULL;
		full_index = NULL;
		s_index = NULL;
		p_index = NULL;
		o_index = NULL;
		stringtable = NULL;
		global_block_ul = NULL;
		utils::logger(LOG, "mini-3 web server starting", argv[1], getpid());
		/* setup the network socket */
		if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			utils::logger(ERROR, "system call", "socket", 0);
		port = atoi(argv[1]);
		if (port < 0 || port > 60000)
			utils::logger(ERROR, "Invalid port number (try 1->60000)", argv[1], 0);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port = htons(port);
		if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			utils::logger(ERROR, "system call", "bind", 0);
		if (listen(listenfd, 64) < 0)
			utils::logger(ERROR, "system call", "listen", 0);
		for (hit = 1;; hit++)
		{
			length = sizeof(cli_addr);
			if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
				utils::logger(ERROR, "system call", "accept", 0);
			if ((pid = fork()) < 0)
				utils::logger(ERROR, "system call", "fork", 0);
			else
			{
				if (pid == 0)
				{ /* child */
					(void)close(listenfd);
					web(socketfd, hit, global_web_pip); /* never returns */
				}
				else
				{ /* parent */
					(void)close(socketfd);
				}
			}
		}
	}

};