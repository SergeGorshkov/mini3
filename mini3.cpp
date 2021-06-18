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

#pragma GCC diagnostic ignored "-Wwrite-strings"
#define JSMN_PARENT_LINKS 1
#define MULTITHREAD
#define LISTEN_QUEUES
//#define PRINT_DIAGNOSTICS
//#define LOG_REQUESTS
#define WEBSERVER
#define VERBOSE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <time.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <math.h>
#include "rabbitmq/librabbitmq/amqp.h"
#include "rabbitmq/librabbitmq/amqp_tcp_socket.h"
#include <librdkafka/rdkafka.h>
#include "jsmn/jsmn.h"
#include "source/struct.h"
#include "source/common.cpp"
#include "source/queues.cpp"
#include "source/triples.cpp"
#include "source/transaction.cpp"
#include "source/request.cpp"
#include "source/request_triples.cpp"
#include "source/request_prefixes.cpp"
#include "source/request_chain.cpp"
#include "source/webserver.cpp"

int main(int argc, char **argv)
{
	char message[1024], config[1024];
	message[0] = 0;
	utils::load_config(message);

	sem_unlink("mini3_transactionCommitSem");
	sem = sem_open("mini3_transactionCommitSem", O_CREAT | O_EXCL, 0644, 1);
	if (sem == SEM_FAILED)
		utils::logger(ERROR, "Cannot open transaction semaphore", "", 0);
	sem_unlink("mini3_pipeWriteSem");
	wsem = sem_open("mini3_pipeWriteSem", O_CREAT | O_EXCL, 0644, 1);
	if (wsem == SEM_FAILED)
		utils::logger(ERROR, "Cannot open pipe write semaphore", "", 0);
	sem_unlink("mini3_pipeReadSem");
	rsem = sem_open("mini3_pipeReadSem", O_CREAT | O_EXCL, 0644, 1);
	if (rsem == SEM_FAILED)
		utils::logger(ERROR, "Cannot open pipe read semaphore", "", 0);
	sem_unlink("mini3_logSem");
	lsem = sem_open("mini3_logSem", O_CREAT | O_EXCL, 0644, 1);
	if (lsem == SEM_FAILED)
		utils::logger(ERROR, "Cannot open log semaphore", "", 0);
	sem_unlink("mini3_prefixSem");
	psem = sem_open("mini3_prefixSem", O_CREAT | O_EXCL, 0644, 1);
	if (psem == SEM_FAILED)
		utils::logger(ERROR, "Cannot open prefixes update semaphore", "", 0);

	memset(global_web_pip, 0, sizeof(int) * 4);
	if (pipe2(global_web_pip, 0) == -1)
		utils::logger(ERROR, "Cannot initialize global write pipe", "", 0);
	if (message[0])
	{
		printf("%s\n", message);
		exit(0);
	}
	memcpy((void *)((unsigned long)global_web_pip + sizeof(int) * 2), global_web_pip, sizeof(int) * 2);
	if (pipe2(global_web_pip, 0) == -1)
		utils::logger(ERROR, "Cannot initialize global read pipe", "", 0);
	if (message[0])
	{
		printf("%s\n", message);
		exit(0);
	}
	fcntl(global_web_pip[0], F_SETPIPE_SZ, PIPESIZE);
	fcntl(global_web_pip[1], F_SETPIPE_SZ, PIPESIZE);
#ifdef WEBSERVER
	if (argc < 2 || argc > 3 || !strcmp(argv[1], "-?"))
	{
		(void)printf("Usage: mini-3 Port-Number [-n]\n\n"
					 "\tExample: mini-3 8080\n"
					 "\t-n means that the server shall not become a daemon. It will not return to the console until stopped.\n\n");
		exit(0);
	}
	if(argc == 3) {
		if(strcmp(argv[2], "-n") == 0)
			nodaemon = true;
	}
	webserver::start_web_server(argc, argv);
#endif
#ifdef LISTEN_QUEUES
	queues q;
	q.listen_queues();
#endif
	utils::init_globals(O_RDWR);
	printf("Started!\n\n");

#if defined(WEBSERVER) || defined(LISTEN_QUEUES)
	if(argc == 3) {
		if(strcmp(argv[2], "-n") == 0)
			nodaemon = true;
	}
	if(!nodaemon) {
		// Since this point only the child process works which listens pipe and performs transactions
		if (fork() != 0)
			exit(0);
		(void)signal(SIGCHLD, SIG_IGN);
		(void)signal(SIGHUP, SIG_IGN);
	}
	utils::init_globals(O_RDWR);
	while (true)
	{
		int command = 0;
		local_triple t;
		unsigned long ind;
		char *error;
		//utils::logger(LOG, "Waiting for command", "", command);
		if (read(global_web_pip[0], &command, sizeof(int)) > 0)
		{
			//utils::logger(LOG, "Command", "", command);
			if (command <= 0)
				continue;
			switch (command)
			{
			case COMMIT_TRIPLE:
				transaction::read_from_pipe(global_web_pip[0], &t);
				//utils::logger(LOG, "COMMIT_TRIPLE command", t.s, 0);
				error = transaction::global_commit_triple(&t);
				free(t.s); free(t.p); free(t.o);
				if(t.d) free(t.d);
				if (error)
				{
					command = RESULT_ERROR;
					write(global_web_pip[3], &command, sizeof(int));
					utils::logger(ERROR, "Commit transaction error", error, 0);
				}
				command = RESULT_OK;
				if (chunks_rebuilt)
					command |= RESULT_CHUNKS_REBUILT;
				if (triples_reallocated)
					command |= RESULT_TRIPLES_REALLOCATED;
				write(global_web_pip[3], &command, sizeof(int));
				break;
			case DELETE_TRIPLE:
				//utils::logger(LOG, "DELETE_TRIPLE command", "", 0);
				sem_wait(rsem);
				read(global_web_pip[0], &ind, sizeof(unsigned long));
				sem_post(rsem);
				error = transaction::global_delete_triple(ind);
				if (error)
				{
					command = RESULT_ERROR;
					write(global_web_pip[3], &command, sizeof(int));
					utils::logger(ERROR, "Delete transaction error", error, 0);
				}
				command = RESULT_OK;
				write(global_web_pip[3], &command, sizeof(int));
				break;
			}
/*
//unlink("triples.log");
FILE *fp = fopen("triples.log", "a");
fprintf(fp, "\nDumping %lu triples\n", *n_triples);
for(int i=0; i<*n_triples; i++) {
	char *s = utils::get_string(triples[i].s_pos);
	char *p = utils::get_string(triples[i].p_pos);
	char *o = utils::get_string(triples[i].o_pos);
	fprintf(fp, "%lx\t%s\t%s\t%s\t%i", triples[i].mini_hash, s, p, o, triples[i].status);
	if(triples[i].d_pos)
		fprintf(fp, "\t%s\t%s", utils::get_string(triples[i].d_pos), triples[i].l);
	fprintf(fp, "\n");
}
fprintf(fp, "\nDumping %lu full index\n", *n_triples);
for(unsigned long c=0; c<*n_chunks; c++) {
	fprintf(fp, "Chunk %lu, %lu items\n", c, chunks_size[c]);
	for(int i=0; i<chunks_size[c]; i++)
		fprintf(fp, "%016lx\t%lu\n", full_index[c*(*single_chunk_size) + i].mini_hash, full_index[c*(*single_chunk_size) + i].index);
}
fprintf(fp, "\nDumping %lu s-index\n", *n_triples);
for(unsigned long c=0; c<*n_chunks; c++) {
	fprintf(fp, "Chunk (s) %lu, %lu items\n", c, s_chunks_size[c]);
	for(int i=0; i<s_chunks_size[c]; i++)
		fprintf(fp, "%016lx\t%lu\t%020lu\n", s_index[c*(*single_chunk_size) + i].mini_hash, s_index[c*(*single_chunk_size) + i].index, s_index[c*(*single_chunk_size) + i].mini_hash);
}
fprintf(fp, "\nDumping %lu p-index\n", *n_triples);
for(unsigned long c=0; c<*n_chunks; c++) {
	fprintf(fp, "Chunk (p) %lu, %lu items\n", c, p_chunks_size[c]);
	for(int i=0; i<p_chunks_size[c]; i++)
		fprintf(fp, "%016lx\t%lu\t%lu\n", p_index[c*(*single_chunk_size) + i].mini_hash, p_index[c*(*single_chunk_size) + i].index, c*(*single_chunk_size) + i);
}
fprintf(fp, "\nDumping %lu o-index\n", *n_triples);
for(unsigned long c=0; c<*n_chunks; c++) {
	fprintf(fp, "Chunk (o) %lu, %lu items\n", c, o_chunks_size[c]);
	for(int i=0; i<o_chunks_size[c]; i++)
		fprintf(fp, "%lx\t%lu\n", o_index[c*(*single_chunk_size) + i].mini_hash, o_index[c*(*single_chunk_size) + i].index);
}
fclose(fp);
*/
		}
	}
#endif
}
