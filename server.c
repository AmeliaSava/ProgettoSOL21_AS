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
#include <HashLFU.h>
//#include <clientlist.h>

#define CONFIG_FL "./config.txt"
#define MAX_BUF 2048
#define TAB_SIZE 10

// Global Variables
long NUM_THREAD_WORKERS;
long MAX_MEMORY_MB; //ATTENTION da lockare?
long MAX_MEMORY_TOT;
long MAX_NUM_FILES;
char* SOCKET_NAME = NULL;
char* LOG_NAME = NULL;
int DEBUG = 1;

FILE* log_file = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
// HashTable for the file memory system
Table cacheMemory;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

//list to handle client requests
MSGlist* client_requests;
pthread_mutex_t cli_req = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_list = PTHREAD_COND_INITIALIZER;

pthread_t* thread_ids = NULL;
//int* pipe_m = NULL; ATTENTION

fd_set set; //active file descriptor set
pthread_mutex_t set_lock = PTHREAD_MUTEX_INITIALIZER;

int fd_max = 0; //max fd
pthread_mutex_t max_lock = PTHREAD_MUTEX_INITIALIZER;

//makes sure the socket name is unlinked
void unlinksock() {
    unlink(SOCKET_NAME);
}


// returns the max index of fd
int updateMax(fd_set set, int fdmax) {
    for(int i = (fdmax-1); i >= 0; i--)
    	if (FD_ISSET(i, &set)) return i;
    	assert(1 == 0);
    	return -1;
}

//returns a response according to the result of the operation
int report_ops(long connfd, op op_type) {

	if(DEBUG) fprintf(stderr, "Returning result: %d\n", op_type);

	if (writen(connfd, &op_type, sizeof(op)) <= 0)
	{ 
		perror("ERROR: writeok"); 
		return -1;
	}

	return 0;
}

//converts a file into a msg
void file_to_msg(FileNode* file, msg* msg)
{
	//ATTENTION
	memcpy(msg->filecontents, file->textFile, file->FileSize);
	//msg->filecontents[(file->FileSize) + 1] = '\0';
	strncpy(msg->filename, file->nameFile, strlen(file->nameFile));
	msg->filename[(strlen(file->nameFile)) + 1] = '\0';
	msg->size = file->FileSize;
	msg->namelenght = strlen(file->nameFile);

	return;
}

//Handling of API operations on server side

int write_file_svr(long connfd, msg file, int flag, pid_t pid){
	fprintf(stderr, "dentro write\n");
	
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, file.filename)) == NULL) return report_ops(connfd, SRV_FILE_NOT_FOUND);
	else
	{
		if(current->lock == 1 && current->lock_pid != pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process	
		if(current->FileSize > 0) return report_ops(connfd, SRV_FILE_ALREADY_PRESENT); //node exists but has content inside, can't over write
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED); //node exists but is closed
		if(MAX_MEMORY_TOT < file.size) return report_ops(connfd, SRV_MEM_FULL); //the file is bigger than the whole available memory

		if((MAX_NUM_FILES == 0) || (MAX_MEMORY_MB < file.size)) //not enough space for new node
		{ 
			long removed = 0; //ATTENTION
			Hash_LFUremove(&cacheMemory);
			if(MAX_MEMORY_MB < file.size) MAX_MEMORY_MB += file.size;
			if(MAX_NUM_FILES == 0) MAX_NUM_FILES++;
			if(DEBUG) fprintf(stdout, "Bytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
			return write_file_svr(connfd, file, flag, pid);
		} else {
					MAX_MEMORY_MB -= file.size;
					MAX_NUM_FILES--;
					if(DEBUG) fprintf(stdout, "Bytes added: %ld\nMemory left:%ld\nInserting file...\n", file.size, MAX_MEMORY_MB);
					node_update(current, current->frequency +  1, file.filecontents, file.size);
					Hash_Inc(&cacheMemory, current);
				} 				
	}
	return report_ops(connfd, SRV_OK);
}

int open_file_svr(long connfd, char* name, int flag, pid_t pid)
{
	fprintf(stderr, "dentro open\n");
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, name)) != NULL) // file already exists
	{	//file is either unlocked or locked by the same client
		if(current->lock == 0 || (current->lock == 1 && current->lock_pid == pid))
		{	// if the file is closed, open file
			if(current->status == 1)
			{ 
				fprintf(stderr, "found closed file\n");
				fprintf(stderr, "Stato: %d\n", current->status);
				Hash_Inc(&cacheMemory, current);
				current->status = 0; //opening file
				fprintf(stderr, "Stato Updated: %d\n", current->status);
				//O_LOCK flag set, lock the file.
				//if O_LOCK is tryied on already locked file nothing happens
				if(flag == 2 && current->lock != 1) node_lock(current, pid);
			} else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT); // file already exists and its open
		} else //file is locked by some other process
			return report_ops(connfd, SRV_FILE_LOCKED);
	} else if(!flag) return report_ops(connfd, SRV_NOK); //tried to create a file with no O_CREATE flag set
			else
			{	
				//ready for write
				Hash_Insert(&cacheMemory, 0, name, 0);

				if(flag == 3)
				{
					current = Hash_SearchNode(&cacheMemory, name);
					node_lock(current, pid);
					return report_ops(connfd, SRV_READY_FOR_WRITE); 
				}

				return report_ops(connfd, SRV_READY_FOR_WRITE);
			} 
	return report_ops(connfd, SRV_OK);
}

int close_file_svr(long connfd, msg info, char* name)
{
	fprintf(stderr, "dentro close\n");
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, name)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process
		if(current->status == 0)
		{ // is open
			fprintf(stderr, "chiudo il nodo\n");
			Hash_Inc(&cacheMemory, current);
			current->status = 1; // close file
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT); // file already closed
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND);
	return report_ops(connfd, SRV_OK);
}

int read_file_svr(long connfd, msg info, char** tmp, size_t* size) 
{
	fprintf(stderr, "%s\n", info.filename);

	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, info.filename)) != NULL) 
	{	// file found
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process
		fprintf(stderr, "%s\n", current->nameFile);

		if(current->status == 1) return 13; // file is closed, cannot read

		else 
		{	// file already exists and its open
			Hash_Inc(&cacheMemory, current);

			*size = current->FileSize;

			if ((*tmp = malloc((*size)*sizeof(char))) == NULL) return 8; //error
			memcpy(*tmp, current->textFile, *size);
			
			return 0; //ok
		} 
	}
	return 9; //file not found
}

int read_n_file_svr(long connfd, msg info) {

	fprintf(stderr, "dentro read n\n");
	FileNode* to_send = NULL;
	int tot = 0;

	Hash_Read(&cacheMemory, info.namelenght, &to_send, &tot);

	fprintf(stderr, "tot:%d\n", tot);

	msg send[tot];
	FileNode* current = to_send;
	int i = 0;
	
	while(current != NULL)
	{
		file_to_msg(current, &send[i]);

		current = current->next;
		i++;
	}

	if(writen(connfd, &tot, sizeof(int)) <= 0) 
	{
		perror("ERROR: write read file len");
		return -1;
	}

	for(int j  = 0; j < tot; j++) {

		int ret = 0;

		if((ret = writen(connfd, &(send[j]), sizeof(msg))) <= 0)
		{
			perror("ERROR: write read file");
			return -1;
		} else fprintf(stderr, "Write: %d\n", ret);
		
	}

	return 0;
}

int append_file_svr(long connfd, msg info) 
{
	fprintf(stderr, "dentro append\n");
	FileNode* current = NULL;
	if((current = Hash_SearchNode(&cacheMemory, info.filename)) != NULL) 
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED); // file is closed
		else {
			if(MAX_MEMORY_MB < info.size) 
			{
				fprintf(stderr, "append LFU\n");
				long removed = 0; //ATTENTION
				Hash_LFUremove(&cacheMemory);
				MAX_MEMORY_MB += info.size;
				if(DEBUG) fprintf(stdout, "Bytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
				return append_file_svr(connfd, info);
			} else 
			{
				fprintf(stderr, "append ok\n");
				MAX_MEMORY_MB -= info.size;
				if(DEBUG) fprintf(stdout, "Bytes added: %ld\nMemory left:%ld\nInserting file...\n", info.size, MAX_MEMORY_MB);
				node_append(current, current->frequency + 1, info.filecontents, (current->FileSize + info.size)); //file is open, enough memory, make append
				Hash_Inc(&cacheMemory, current);
			}	
		}
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND);
	return report_ops(connfd, SRV_OK);
}

int remove_file_svr(long connfd, msg info) {
	fprintf(stderr, "dentro remove\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info.filename)) != NULL) 
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED); // file is closed
		else
		{ //removing the node
			MAX_MEMORY_MB += current->FileSize;
			MAX_NUM_FILES++;
			if(DEBUG) fprintf(stdout, "Bytes freed: %ld\nMemory left:%ld\n", current->FileSize, MAX_MEMORY_MB);
			Hash_Remove(&cacheMemory, info.filename);
		} 
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND);
	return report_ops(connfd, SRV_OK);
}

int lock_file_srv(long connfd, msg info){
	fprintf(stderr, "dentro lock\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info.filename)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process	
		if(current->lock == 0) //file is not already locked
		{
			fprintf(stderr, "locking\n");
			Hash_Inc(&cacheMemory, current);
			current->lock = 1; // lock file
			current->lock_pid = info.pid;
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT); // file already locked by the same process
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND);
	return report_ops(connfd, SRV_OK);
	return 0;
}

int unlock_file_srv(long connfd, msg info){
	fprintf(stderr, "dentro unlock\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info.filename)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info.pid) return report_ops(connfd, SRV_FILE_LOCKED); //file is locked by some other process	
		if(current->lock == 1 && current->lock_pid == info.pid) //file is not already locked
		{
			fprintf(stderr, "unlocking\n");
			Hash_Inc(&cacheMemory, current);
			current->lock = 0; // unlock file
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT); // file already unlocked
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND);
	return report_ops(connfd, SRV_OK);
	return 0;
}

//WIP
int cmd(int connfd, msg info) {

	fprintf(stderr, "dentro cmd: %d\n", info.op_type);

	int flag;

	switch(info.op_type) 
	{
		case OPEN_FILE: 
		{
			//int count = 0;
			//while(1) count ++;
			fprintf(stderr, "openfile cmd\n");

			fprintf(stderr, "Flag: %d\n", info.flag);
    		fprintf(stderr, "Name: %s\n", info.filename);
			fprintf(stderr, "PID: %d\n", info.pid);

			return open_file_svr(connfd, info.filename, info.flag, info.pid);

			break;
		}
		case CLOSE_FILE: 
		{
			fprintf(stderr, "closefile cmd\n");
			fprintf(stderr, "%s\n", info.filename);

			return close_file_svr(connfd, info, info.filename);
			break;
		}

		case READ_FILE:	
		{
			fprintf(stderr, "dentro cmd read\n");
			fprintf(stderr, "%s\n", info.filename);
			fprintf(stderr, "prima ret\n");

			char* tmp_buf = NULL;
			size_t tmp_size;
			int ret = read_file_svr(connfd, info, &tmp_buf, &tmp_size);

			fprintf(stderr, "dopo read");
			fprintf(stderr, "%s\n", tmp_buf);
			fprintf(stderr, "%ld\n", info.size);
			fprintf(stderr, "Result Return: %d\n", ret);

			if(ret == 0) {
				fprintf(stderr, "dentro ok read");
			    if(report_ops(connfd, SRV_OK) == -1) return -1;

			    if(writen(connfd, &tmp_size, sizeof(long)) <= 0) 
				{
					perror("ERROR: write read file len");
					return -1;
				}

				if(writen(connfd, tmp_buf, tmp_size*sizeof(char)) <= 0)
				{
					perror("ERROR: write read file");
					return -1;
				}
			} else return report_ops(connfd, ret);
			break;
		}

		case READ_FILE_N: 
		{
			return read_n_file_svr(connfd, info);
			break;
		}

		case WRITE_FILE:
		{
			flag = 1; //in write the flag will be always set to create
			return write_file_svr(connfd, info, flag, info.pid);
			break;
		}
		case APPEND_FILE:
		{
			fprintf(stderr, "append cmd\n");
			return append_file_svr(connfd, info);
			break;
		}
		case REMOVE_FILE:
    	{
			fprintf(stderr, "%s\n", info.filename);
			return remove_file_svr(connfd, info);
			break;
		}

		case LOCK_FILE:
		{
			return lock_file_srv(connfd, info);
			break;
		}

		case UNLOCK_FILE:
		{
			return unlock_file_srv(connfd, info);
			break;
		}
		case END_COMMUNICATION:
		{
			return report_ops(connfd, SRV_OK);
			break;
		}
		default: 
		{
			fprintf(stderr, "Command not found SRV\n");
			return -1;
		}
	}

	return 0;
}

/*
	ConnessioniSelect
		select
			if (connessione)
				connetti
			else
				prendi l'fd che è pronto
				leggi
				metti la richiesta in coda
	
	Worker
		while (true)
			if (!coda vuota)
				esaudisci richiesta
*/

/*
int getMSG(int connfd)
{
	msg* file = safe_malloc(sizeof(msg));
	
	if (readn(connfd, file, sizeof(msg))<=0) return -1;

	fprintf(stderr, "Nome: %s\n", file->filename);

	return cmd(connfd, *file);
}
*/

void* getMSG(void* arg)
{

	fprintf(stderr, "dentro getmsg thread\n");

	for(;;) 
	{
		msg* operation;

		pthread_mutex_lock(&cli_req);
		pthread_cond_wait(&wait_list, &cli_req);
		operation = msg_list_pop_return(client_requests);

		fprintf(stderr, "prelevo coda\n");
		pthread_mutex_unlock(&cli_req);

		cmd(operation->fd_con, *operation);
		//fprintf(stderr, "fd: %ld\n", operation->fd_con);
		if(operation->op_type != 20)
		{
			LOCK(&set_lock);
			FD_SET(operation->fd_con, &set);
			UNLOCK(&set_lock);
		}
		
	}

	fflush(stdout);
	return NULL;
}

//ok, better clean up
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
			case 4:
				while((*equals) != '\0' && isspace(*equals)) ++equals;
				if((*equals) == '\0') 
				{
					fprintf(stderr, "wrong config.txt file format: fourth line must be log file name\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
				strncpy(LOG_NAME, equals, MAX_BUF);
		}

		count++;
		
	}

	fclose(config_input);
	free(buffer);
	
}

void log_create()
{
	
	char* log_path = safe_malloc(2 + strlen(LOG_NAME));
	sprintf(log_path, "./%s.txt", LOG_NAME);
	
	log_file = fopen(log_path, "w");

	if(log_file != NULL) 
	{
		fprintf(log_file, "LOG FILE\n");
		fflush(log_file);
	}
	
	return;
}

int main (int argc, char* argv[]) {

	atexit(unlinksock);  
	
	//allocating the global socket name
	SOCKET_NAME = safe_malloc(MAX_BUF * sizeof(char));
	LOG_NAME = safe_malloc(MAX_BUF * sizeof(char));

	// parsing the config file
	configParsing();

	log_create(LOG_NAME);

	fprintf(log_file, "Server created\nMax number of thread workers: %ld\nMax memory available: %ld\nCurrently available memory: %ld\nMax Number of files: %ld\nSocket Name: %s\nLog file name: %s\n", 
		NUM_THREAD_WORKERS, MAX_MEMORY_MB, MAX_MEMORY_TOT, MAX_NUM_FILES, SOCKET_NAME, LOG_NAME);
	fflush(log_file);

	//initializing cache memory
	Hash_Init(&cacheMemory, TAB_SIZE);

	fprintf(log_file, "Hash memory initialized successfully\n");

	//ATTENTION list init
	client_requests = safe_malloc(sizeof(MSGlist));
	msg_list_init(client_requests);

	//Accepting connections
	unlinksock();

	int fd_skt; //connection socket
	int fd_sel; //index to verify select results
	int fd_con; //I/O socket with client

	fd_set rdset; //set of fd wating for reading

	//creating threads

	CHECK_EQ_EXIT((thread_ids = (pthread_t*) calloc(NUM_THREAD_WORKERS, sizeof(pthread_t))), NULL, "ERROR: calloc threads");

	int res = 0;

	for(int i = 0; i < NUM_THREAD_WORKERS; i++) {
		if((res = pthread_create(&(thread_ids[i]), NULL, &getMSG, NULL) != 0)) { 
			perror("ERROR: threads init");
			exit(EXIT_FAILURE);
		}
	}
	
	//Initializing Socket

	SYSCALL_EXIT("socket", fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "ERROR: socket", "");
	fprintf(log_file, "Connecting to %s\n", SOCKET_NAME);

	//socket address
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

	/**
	 * Working single thread socket communication
	for(;;) 
	{      
		rdset = set; //saving the set in the temporary one

		//+1 because I need the number of active file descriptors, not the max index
		if (select(fd_max + 1, &rdset, NULL, NULL, NULL) == -1)
		{
		    perror("ERROR: select");
			return EXIT_FAILURE;
		} 
		else { 
			//select ok
			fprintf(stderr, "select ok\n");
			fprintf(stderr, "max bef for: %d\n", fd_max);

			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++)
			{
				//accepting new connections
				fprintf(stderr, "accepting connections\n");

			    if (FD_ISSET(fd_sel, &rdset))
				{ //is it ready?
			    	fprintf(stderr, "ready?\n");

					if (fd_sel == fd_skt)
					{ // sock connect ready
						fprintf(stderr, "sock ready\n");
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						
						// updating max
						if(fd_con > fd_max) fd_max = fd_con;  
						fprintf(stderr, "max after connection:%d\n", fd_max);
						//fprintf(stderr, "Client fd: %d\n", fd_con);
						continue;
					} 
					fd_con = fd_sel;

					//fprintf(stderr, "Client da ascoltare: %d\n", fd_con);
					if(getMSG(fd_con) < 0) 
					{
						close(fd_con); 
						FD_CLR(fd_con, &set);

						if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
					}
		   		}
			}
		}
    }
	*
	**/

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;
	
	for(;;) 
	{      
		rdset = set; //saving the set in the temporary one

		//+1 because I need the number of active file descriptors, not the max index
		if (select(fd_max + 1, &rdset, NULL, NULL, &timeout) == -1)
		{
		    perror("ERROR: select");
			return EXIT_FAILURE;
		} 
		else { 
			//select ok
			//fprintf(stderr, "select ok\n");
			//fprintf(stderr, "max bef for: %d\n", fd_max);

			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++)
			{
				//accepting new connections
				//fprintf(stderr, "accepting connections\n");

			    if (FD_ISSET(fd_sel, &rdset))
				{ //is it ready?
			    	//fprintf(stderr, "ready?\n");

					if (fd_sel == fd_skt)
					{ // sock connect ready
						fprintf(stderr, "sock ready\n");
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						
						// updating max
						if(fd_con > fd_max) fd_max = fd_con;  
						fprintf(stderr, "max after connection:%d\n", fd_max);
						fprintf(log_file, "Connected to client: %d\n", fd_con);
						fflush(log_file);
						continue;
					} 

					fd_con = fd_sel;

					fprintf(log_file, "Listening to client: %d\n", fd_con);
					fflush(log_file);

					msg* request = safe_malloc(sizeof(msg));
	
					if (readn(fd_con, request, sizeof(msg))<=0) 
					{	//dividere -! e 0?
						//FD_CLR(fd_con, &set);
						//if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
						perror("ERROR: reading msg");
						return EXIT_FAILURE;
					}

					//fprintf(stderr, "Nome: %s\n", request->filename);

					request->fd_con = fd_con;
				
					fprintf(stderr, "adding request to list\n");
					pthread_mutex_lock(&cli_req);
					msg_push_head(request, client_requests);
					pthread_cond_signal(&wait_list);
					pthread_mutex_unlock(&cli_req);

					//close(fd_con); 
					FD_CLR(fd_con, &set);

					//if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
					
		   		}
			}
		}
    }

	//DOSEN'T WORK
	/*
    if(DEBUG) fprintf(stderr, "Max fd at start: %d\n", fd_max);
	for(;;)
	{   

		LOCK(&set_lock);  
		rdset = set; //saving the set in the temporary one
		UNLOCK(&set_lock);

		int connected = client_connected.lenght;

		if(connected != 0) {
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 1;
			//+1 because I need the number of active file descriptors, not the max index
			if (select(fd_max + 1, &rdset, NULL, NULL, &timeout) == -1) 
			{
				perror("ERROR: select");
				return EXIT_FAILURE;
			} 
			
		}
		
		//select ok
		printf("QUI\n");
		fprintf(stderr, "select ok\n");

		long fd_con; // I/O socket with a client

		fprintf(stderr, "max bef for: %d\n", fd_max);

		for(fd_sel = 0; fd_sel <= fd_max; fd_sel++) 
		{
			//accepting new connections
			fprintf(stderr, "accepting connections\n");

			if (FD_ISSET(fd_sel, &rdset)) { //is it ready?
				fprintf(stderr, "ready?\n");

				if (fd_sel == fd_skt)
				{	// sock connect ready
					fprintf(stderr, "sock ready, accept\n");

					SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");

					LOCK(&set_lock);
					FD_SET(fd_con, &set);  // adding fd to starting set
					UNLOCK(&set_lock);
						
					// updating max
					LOCK(&max_lock);
					if(fd_con > fd_max) fd_max = fd_con;
					UNLOCK(&mac_lock);

					fprintf(stderr, "Max after accept:%d\n", fd_max);
				} else {
					//request
		   			fprintf(stderr, "handling request\n");

		   			long client;
		   			op rep;

					if (readn(pipe_m[0], &client, sizeof(long)) > 0) 
						{ //read something
							if (readn(pipe_m[0], &rep, sizeof(op))<=0) {
								perror("reading pipe 2"); exit(EXIT_FAILURE);
							}
							if (rep == END_COMMUNICATION) 
							{
								fprintf(stderr, "interrmpo com client: %ld\n", client);
								FD_CLR(client, &set);
								fprintf(stderr, "max prima agg:%d\n", fd_max);
								if (client == fd_max) fd_max = updateMax(set, fd_max);
									fprintf(stderr, "max dopo agg:%d\n", fd_max);
									close(client);
							}
							else
							{
								FD_SET(client, &set);
								if(client > fd_max) fd_max = client;
							}
						} 
						else 
						{ 
							perror("reading pipe");
							 exit(EXIT_FAILURE); 
						}	
			   		
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
	*/

    //ogni volta che si chiude la comunicazione va aggiornato il massimo
    //sezione di deallocazione di tutte le risorse
	close(fd_skt);
    free(SOCKET_NAME);
	return EXIT_SUCCESS;
}