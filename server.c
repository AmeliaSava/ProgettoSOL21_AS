#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h> //ind AF_UNIX
#include <sys/select.h>
#include <assert.h>
#include <pthread.h>

#include <ops.h>
#include <conn.h>
#include <coms.h>
#include <message.h>
#include <HashLFU.h>
#include <clientlist.h>

#define CONFIG_FL "./config.txt"
#define MAX_BUF 2048
#define TAB_SIZE 1028

// Global Variables
long NUM_THREAD_WORKERS;
long MAX_MEMORY_MB;
long MAX_MEMORY_TOT;
long MAX_NUM_FILES;
char* SOCKET_NAME = NULL;
int DEBUG = 1;


// HashTable for the file memory system
Table cacheMemory;

node* client_requests = NULL;
pthread_mutex_t cli_req = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_list = PTHREAD_COND_INITIALIZER;

pthread_t* thread_ids = NULL;
int* pipe_m = NULL;

//makes sure the socket name is unlinked
void unlinksock() {
    unlink(SOCKET_NAME);
}

//WRITE NOT WORKING
int reportOps(long connfd, op op_type) {
	fprintf(stderr, "dentro return\n");

	char* buf = NULL;
	char message[1024];

	sprintf(message, "Operation completed successfully");

	switch(op_type) {
		case SRV_OK: {
					fprintf(stderr, "sending ok\n");

			int l = 33;

			if (writen(connfd, &l, sizeof(int))<=0) //fai macro
			{
				perror("ERROR: writeok");
				return -1;
			}

			if (writen(connfd, "Operation completed successfully", l*sizeof(char))<=0)
			{ 
				perror("ERROR: writeok"); 
				return -1;
			}
			
			break;
		}
		case SRV_NOK: {
			int l = 32;
			if (writen(connfd, &l, sizeof(int))<=0) { perror("ERROR: writeok"); return -1;}
			if (writen(connfd, "Error while executing operation", l*sizeof(char))<=0) { perror("ERROR: writeok"); return -1;}
			return -1;
		}
		case SRV_FILE_NOT_FOUND: {
			int l = 15;
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, "File not found", l*sizeof(char))<=0) { free(buf); return -1;}
			return -1;
		}
		case SRV_FILE_ALREADY_PRESENT: {
			int l = 21;
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, "File already present", l*sizeof(char))<=0) { free(buf); return -1;}

			return -1;
		}
		case SRV_MEM_FULL: {
			int l = 24;
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, "File too big for memory", l*sizeof(char))<=0) { free(buf); return -1;}
			return -1;
		}
		case SRV_READY_FOR_WRITE: {
			fprintf(stderr, "sending ready\n");
			int l = 19;
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, "Ready for write...", l*sizeof(char))<=0) { free(buf); return -1;}
			return -1;
		}
		case SRV_FILE_CLOSED: {
			fprintf(stderr, "sending closed\n");
			int l = 15;
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, "File is closed", l*sizeof(char))<=0) { free(buf); return -1;}
			return -1;
		}
		default: {
			fprintf(stderr, "command not found\n");
			return -1;
		}
	}
	return 0;
}

int write_file_svr(long connfd, int len, char* name, char* file, int flag){
	fprintf(stderr, "dentro write\n");
	if(strcmp(name, "median") == 0) return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // cannot modify root
	NodeFile* current = NULL;
	fprintf(stderr, "%s\n", name);
	if((current = searchNode(&cacheMemory, name)) == NULL) return reportOps(connfd, SRV_FILE_NOT_FOUND);
	else {
		if(current->FileSize > 0) return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); //node exists but has content inside, can't over write
		if(current->status == 1) return reportOps(connfd, SRV_FILE_CLOSED); //node exists but is closed
		if(MAX_MEMORY_TOT < len) return reportOps(connfd, SRV_MEM_FULL); //the file is bigger than the whole available memory
		if((MAX_NUM_FILES == 0) || (MAX_MEMORY_MB < len)) { //not enough space for new node
			long removed;
			LFU_Remove(&cacheMemory, &removed);
			if(MAX_MEMORY_MB < len) MAX_MEMORY_MB += len;
			if(MAX_NUM_FILES == 0) MAX_NUM_FILES++;
			if(DEBUG) fprintf(stdout, "Bytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
			return write_file_svr(connfd, len, name, file, flag);
		} else {
			if(flag == 2) return reportOps(connfd, SRV_NOK); //insert O_LOCK handling -> use pid and lock field in node struct
				else {
					MAX_MEMORY_MB -= len;
					MAX_NUM_FILES--;
					if(DEBUG) fprintf(stdout, "Bytes added: %d\nMemory left:%ld\nInserting file...\n", len, MAX_MEMORY_MB);
					UpdateNode(current, current->frequency +  1, file, len);
				}
		} 
	}
	return reportOps(connfd, SRV_OK);
}

int open_file_svr(long connfd, char* name, int flag) {
	fprintf(stderr, "dentro open\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) { // if the file is closed, open file
			fprintf(stderr, "found closed file\n");
			fprintf(stderr, "Stato: %d\n", current->status);
			increaseF(current);
			current->status = 0; //opening file
			fprintf(stderr, "Stato Updated: %d\n", current->status);
		} else return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // file already exists and its open
	} else if(!flag) return reportOps(connfd, SRV_NOK); //tried to create a file with no O_CREATE flag set
			else { //ready for write
					PushNode(&cacheMemory, 0, name, NULL, 0, 0);
					return reportOps(connfd, SRV_READY_FOR_WRITE); 
			} 
	return reportOps(connfd, SRV_OK);
}

int close_file_svr(long connfd, char* name) {
	fprintf(stderr, "dentro close\n");
	NodeFile* current = NULL;
	if(strcmp(name, "median") == 0) return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // cannot modify root
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 0) { // is open
			fprintf(stderr, "chiudo il nodo\n");
			increaseF (current);
			current->status = 1; // close file
		}	
		else return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // file already closed
	} else return reportOps(connfd, SRV_FILE_NOT_FOUND);
	return reportOps(connfd, SRV_OK);
}

int read_file_svr(long connfd, msg info, char** tmp, long* size) {
	fprintf(stderr, "%s\n", info.filename);
	NodeFile* current = NULL;
	if(strcmp(info.filename, "median") == 0) return 10; // cannot modify root
	if((current = searchNode(&cacheMemory, info.filename)) != NULL) { // file found
		fprintf(stderr, "%s\n", current->nameFile);
		if(current->status == 1) return 13; // file is closed, cannot read
		else { // file already exists and its open
			increaseF (current);
			*size = current->FileSize;
			if ((*tmp = malloc(*size*sizeof(char))) == NULL) return 8;
			fprintf(stderr, "Testo nel nodo: %s\n", current->textFile);
			strncpy(*tmp, current->textFile, *size);
			fprintf(stderr, "Copyed node: %s\n", *tmp);
			return 0;
		} 
	}
	return 9; //file not found
}

int read_n_file_svr(long connfd, msg info, int n) {
	return 0;
}

int append_file_svr(long connfd, int len, char* name, char* file) {
	fprintf(stderr, "dentro append\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) return reportOps(connfd, SRV_FILE_CLOSED); // file is closed
		else {
			if(MAX_MEMORY_MB < len) {
				fprintf(stderr, "append LFU\n");
				long removed;
				LFU_Remove(&cacheMemory, &removed);
				MAX_MEMORY_MB += len;
				if(DEBUG) fprintf(stdout, "Bytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
				return append_file_svr(connfd, len, name, file);
			} else {
				fprintf(stderr, "append ok\n");
				MAX_MEMORY_MB -= len;
				if(DEBUG) fprintf(stdout, "Bytes added: %d\nMemory left:%ld\nInserting file...\n", len, MAX_MEMORY_MB);
				AppendNode(current, current->frequency + 1, file, len); //file is open, enough memory, make append
			}	
		}
	} else return reportOps(connfd, SRV_FILE_NOT_FOUND);
	return reportOps(connfd, SRV_OK);
}

int remove_file_svr(long connfd, char* name) {
	fprintf(stderr, "dentro remove\n");
	NodeFile* current = NULL;
	if(strcmp(name, "median") == 0) return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // cannot modify root
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) return reportOps(connfd, SRV_FILE_CLOSED); // file is closed
		else { //removing the node
			MAX_MEMORY_MB += current->FileSize;
			MAX_NUM_FILES++;
			if(DEBUG) fprintf(stdout, "Bytes freed: %ld\nMemory left:%ld\n", current->FileSize, MAX_MEMORY_MB);
			RemoveFile(current, &cacheMemory, name);
		} 
	} else return reportOps(connfd, SRV_FILE_NOT_FOUND);
	return reportOps(connfd, SRV_OK);
}

//WIP
int cmd(long connfd, long pipe_fd, msg info) {
	fprintf(stderr, "dentro cmd\n");
	int flag;
	switch(info.op_type) {
		case OPEN_FILE: { // read: flag, lenght of file, filename
			fprintf(stderr, "openfile cmd");
			if (readn(connfd, &flag, sizeof(int)) <= 0) return -1;
			fprintf(stderr, "Flag: %d\n", flag);
			if (readn(connfd, &info.lenN, sizeof(int)) <= 0) return -1;
    		if ((info.filename = malloc(info.lenN*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
    		if (readn(connfd, info.filename, info.lenN*sizeof(char)) <= 0) { perror("ERROR:read in open"); free(info.filename); return -1;}
    		fprintf(stderr, "%s\n", info.filename);
			//free(info.filename);
			//free(info.filecontents);
			return open_file_svr(connfd, info.filename, flag);
			break;
		}
		case CLOSE_FILE: {
			if (readn(connfd, &info.lenN, sizeof(int)) <= 0) return -1;
    		if ((info.filename = malloc(info.lenN*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
    		if (readn(connfd, info.filename, info.lenN*sizeof(char)) <= 0) { perror("ERROR:read in open"); free(info.filename); return -1;} 

    		fprintf(stderr, "%s\n", info.filename);
			return close_file_svr(connfd, info.filename);
			break;
		}
		case READ_FILE:	{
			fprintf(stderr, "dentro cmd read\n");
			if (readn(connfd, &info.lenN, sizeof(int))<=0) return -1;
    		if ((info.filename = calloc((info.lenN), sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
			fprintf(stderr, "dopo calloc\n");
			if (readn(connfd, info.filename, info.lenN*sizeof(char))<=0){ free(info.filename); return -1;}
			fprintf(stderr, "prima ret\n");
			char* temp = NULL;
			int ret = read_file_svr(connfd, info, &temp, &info.size);
			fprintf(stderr, "dopo read");
			fprintf(stderr, "%s\n", temp);
			fprintf(stderr, "%ld\n", info.size);
			fprintf(stderr, "Result Return: %d\n", ret);
			if(ret == 0) {
				fprintf(stderr, "dentro ok read");
			    if(reportOps(connfd, SRV_OK) == -1) return -1;
			    if(writen(connfd, &info.size, sizeof(long)) <= 0) { perror("ERROR: write read file len"); return -1; }
				if(writen(connfd, temp, info.size*sizeof(char)) <= 0) { perror("ERROR: write read file"); return -1; }
			} else return reportOps(connfd, ret);
			break;
		}
		case READ_FILE_N: {
			/*int n;
			if (readn(connfd, &n, sizeof(int))<=0) return -1;
			if(n==0) return read_cache(connfd);
			int ret = read_n_file_svr(connfd, info, n);*/
			break;
		}
		case WRITE_FILE:
			flag = 1; //in write the flag will be always set to create
			if (readn(connfd, &info.lenN, sizeof(int)) <= 0) return -1;
    		if ((info.filename = malloc(info.lenN*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
    		if (readn(connfd, info.filename, info.lenN*sizeof(char)) <= 0) { perror("ERROR:read in open"); free(info.filename); return -1;} 
			if (readn(connfd, &info.size, sizeof(long))<=0) { perror("ERROR:read in open"); free(info.filename); return -1;} 
    		if ((info.filecontents = malloc(info.size*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); free(info.filecontents); return -1;}
    		if (readn(connfd, info.filecontents, info.size*sizeof(char))<=0) { perror("ERROR:read in open"); free(info.filename); free(info.filecontents); return -1;}
			return write_file_svr(connfd, info.size, info.filename, info.filecontents, flag);
			break;
		case APPEND_FILE:
		fprintf(stderr, "append cmd\n");
			if (readn(connfd, &info.lenN, sizeof(int)) <= 0) return -1;
    		if ((info.filename = malloc(info.lenN*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
    		if (readn(connfd, info.filename, info.lenN*sizeof(char)) <= 0) { perror("ERROR:read in open"); free(info.filename); return -1;} 
			if (readn(connfd, &info.size, sizeof(long))<=0) { perror("ERROR:read in open"); free(info.filename); return -1;} 
    		if ((info.filecontents = malloc(info.size*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); free(info.filecontents); return -1;}
    		if (readn(connfd, info.filecontents, info.size*sizeof(char))<=0) { perror("ERROR:read in open"); free(info.filename); free(info.filecontents); return -1;}
			return append_file_svr(connfd, info.size, info.filename, info.filecontents);
			break;
		case REMOVE_FILE:
			if (readn(connfd, &info.lenN, sizeof(int)) <= 0) return -1;
    		if ((info.filename = malloc(info.lenN*sizeof(char))) == NULL) { perror("ERROR:calloc"); free(info.filename); return -1;}
    		if (readn(connfd, info.filename, info.lenN*sizeof(char)) <= 0) { perror("ERROR:read in open"); free(info.filename); return -1;} 
    		fprintf(stderr, "%s\n", info.filename);
			return remove_file_svr(connfd, info.filename);
			break;
		default: {
			fprintf(stderr, "Command not found\n");
			return -1;
		}
	}

	return 0;
}

void* getMSG(void* arg){
	long args = *((long*)arg);
	long client_f;
	op com_op;
	fprintf(stderr, "dentro getmsg thread\n");
	for(;;) {
		msg operation;
		pthread_mutex_lock(&cli_req);
		pthread_cond_wait(&wait_list, &cli_req);
		client_f = pop_tail(client_requests);

		fprintf(stderr, "prelevo coda\n");
		pthread_mutex_unlock(&cli_req);

		if (readn(args, &operation.op_type, sizeof(op)) <= 0){
			fprintf(stderr, "END_COMMUNICATION\n");
			com_op = END_COMMUNICATION;
			write(args, &client_f, sizeof(long));
			write(args, &com_op, sizeof(int));
		} else {
			
			cmd(client_f, args, operation);
		}
	}
	fflush(stdout);
	return NULL;
}

// returns the max index of fd
int updateMax(fd_set set, int fdmax) {
    for(int i = (fdmax-1); i >= 0; i--)
    	if (FD_ISSET(i, &set)) return i;
    	assert(1 == 0);
    	return -1;
}

//PULISCI MEGLIO QUANDO QUALCOSA CHIUDE CHIUDE TUUTO
void configParsing() {
	FILE *config_input = NULL;
	
	char* buffer = NULL;

	if ((config_input = fopen(CONFIG_FL, "r")) == NULL) {
		perror("ERROR: opening config file");
		fclose(config_input);
		exit(EXIT_FAILURE);
	} 

	CHECK_EQ_EXIT((buffer = malloc(MAX_BUF * sizeof(char))), NULL, "ERROR: malloc buffer");

	int count = 0;

	while(fgets(buffer, MAX_BUF, config_input) != NULL) {

		char* comment;

		if ((comment = strchr(buffer, '#')) != NULL) { 
			continue;
		}

		char* semicolon;
		CHECK_EQ_EXIT((semicolon = strchr(buffer, ';')), NULL, "wrong config.txt file format: need ';' before newline\n");
				
		*semicolon = '\0';

		char* equals;
		CHECK_EQ_EXIT((equals = strchr(buffer, '=')), NULL, "wrong config.txt file format, usage: <name> = <value>;\n");

		++equals;

		switch (count) {

			case 0:
				if(isNumber(equals,  &NUM_THREAD_WORKERS) != 0) {
					fprintf(stderr, "wrong config.txt file format: first line must be number of thread workers\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}			
			case 1:
				if(isNumber(equals, &MAX_MEMORY_MB) != 0) {
					fprintf(stderr, "wrong config.txt file format: second line must be maximum memory allocable\n");
					free(buffer);
					exit(EXIT_FAILURE);
				} else {MAX_MEMORY_MB = MAX_MEMORY_MB*(1048576); MAX_MEMORY_TOT = MAX_MEMORY_MB;}
			case 2:
				if(isNumber(equals, &MAX_NUM_FILES) != 0) {
					fprintf(stderr, "wrong config.txt file format: third line must be maximum of managable files\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
			case 3:
				while((*equals) != '\0' && isspace(*equals)) ++equals;
				if((*equals) == '\0') {
					fprintf(stderr, "wrong config.txt file format: third line must be socket file name\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
				strncpy(SOCKET_NAME, equals, MAX_BUF);
		}

		count++;
		
	}

	fclose(config_input);
	free(buffer);
	
}

int main (int argc, char* argv[]) {

	atexit(unlinksock);  
	
	//allocating the global socket name
	CHECK_EQ_EXIT((SOCKET_NAME = malloc(MAX_BUF * sizeof(char))), NULL, "ERROR: malloc socket name");

	// parsing the config file
	configParsing();

	printf("%ld\n%ld\n%ld\n%ld\n%s\n", NUM_THREAD_WORKERS, MAX_MEMORY_MB, MAX_MEMORY_TOT, MAX_NUM_FILES, SOCKET_NAME);
	
	//initializing cache memory
	Hash_Init(&cacheMemory, TAB_SIZE);

	//Accepting connections
	unlinksock();

	int fd_skt; //connection socket
	int fd_max = 0; //max fd
	int fd_sel; //index to verify select results

	fd_set set; //active file descriptor set
	fd_set rdset; //set of fd wating for reading

	//creating threads and pipe
	CHECK_EQ_EXIT((thread_ids = (pthread_t*) calloc(NUM_THREAD_WORKERS, sizeof(pthread_t))), NULL, "ERROR: calloc threads");
	CHECK_EQ_EXIT((pipe_m = (int*) calloc(2,sizeof(int))), NULL, "ERROR: calloc pipe");
	if(pipe(pipe_m) == -1) { errno = -1; perror("ERROR: pipe"); free(pipe_m); free(thread_ids); free(SOCKET_NAME);}

	int res = 0;
	for(int i = 0; i < NUM_THREAD_WORKERS; i++) {
		if((res = pthread_create(&(thread_ids[i]), NULL, getMSG, (void*)(&pipe_m[1])) != 0)) { 
			free(pipe_m); free(thread_ids); free(SOCKET_NAME);
			perror("ERROR: threads init");
			exit(EXIT_FAILURE);
		}
	}

	//creating socket
	SYSCALL_EXIT("socket", fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "ERROR: socket", "");
	fprintf(stderr, "Connected to %s\n", SOCKET_NAME);
	
	//server struct
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME)+1);



 	//preparing socket
	int result;
    SYSCALL_EXIT("bind", result, bind(fd_skt, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "ERROR: bind", "");
    SYSCALL_EXIT("listen", result, listen(fd_skt, MAXBACKLOG), "ERROR: listen", "");
    
	//keeping max fd index updated
	if(fd_skt > fd_max) fd_max = fd_skt;

    //setting the sets
    FD_ZERO(&set);
    FD_ZERO(&rdset);
    FD_SET(fd_skt, &set);

	//DOSEN'T WORK
	/*
    //adding the pipe to the master set
	if(pipe_m[0] > fd_max) fd_max = pipe_m[0];
	FD_SET(pipe_m[0], &set);

    //fprintf(stderr, "%d\n", fd_max);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;

    for(;;) {      
		rdset = set; //saving the set in the temporary one
		//+1 because I need the number of active file descriptors, not the max index
		if (select(fd_max + 1, &rdset, NULL, NULL, &timeout) == -1) {
		    perror("ERROR: select");
			return EXIT_FAILURE;
		} 
		else { 
			printf("QUI\n");
			//select ok
			fprintf(stderr, "select ok\n");
			long fd_con; // I/O socket with a client
			fprintf(stderr, "max bef for: %d\n", fd_max);

			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++) {
				//accepting new connections
				fprintf(stderr, "accepting connections\n");

			    if (FD_ISSET(fd_sel, &rdset)) { //is it ready?
			    	fprintf(stderr, "ready?\n");

					if (fd_sel == fd_skt) { // sock connect ready
						fprintf(stderr, "sock ready\n");
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						
						// updating max
						if(fd_con > fd_max) fd_max = fd_con;  
						fprintf(stderr, "max:%d\n", fd_max);
					} else {
		   				fprintf(stderr, "else \n");
		   				if (fd_sel == pipe_m[0]){
		   					fprintf(stderr, "pipe\n");
		   					long client;
		   					op rep;
							if (readn(pipe_m[0], &client, sizeof(long)) > 0) { //read something
								if (readn(pipe_m[0], &rep, sizeof(op))<=0) { perror("reading pipe 2"); exit(EXIT_FAILURE);}
								if (rep == END_COMMUNICATION) {
									fprintf(stderr, "interrmpo com client: %ld\n", client);
									FD_CLR(client, &set);
									fprintf(stderr, "max prima agg:%d\n", fd_max);
									if (client == fd_max) fd_max = updateMax(set, fd_max);
									fprintf(stderr, "max dopo agg:%d\n", fd_max);
									close(client);
								} else {
									FD_SET(client, &set);
									if(client > fd_max) fd_max = client;
								}
							} else { perror("reading pipe"); exit(EXIT_FAILURE); }	
			   			} else {
							fprintf(stderr, "adding request to list\n");
							pthread_mutex_lock(&cli_req);
							push_head(&client_requests, fd_sel);
							pthread_cond_signal(&wait_list);
							pthread_mutex_unlock(&cli_req);
							FD_CLR(fd_sel, &set);
						}
		   			}
		   		}
			}
		}
    }
	*/

    //ogni volta che si chiude la comunicazione va aggiornato il massimo
    //sezione di deallocazione di tutte le risorse
	close(fd_skt);
    free(SOCKET_NAME);
	return EXIT_SUCCESS;
}