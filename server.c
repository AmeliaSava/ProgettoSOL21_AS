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
#include <signal.h>

#include <ops.h>
#include <conn.h>
#include <coms.h>
#include <HashLFU.h>

#define CONFIG_FL "./config.txt"
#define TAB_SIZE 10

//configuration variables
long NUM_THREAD_WORKERS;
long MAX_MEMORY_MB; //ATTENTION da lockare?
long MAX_MEMORY_TOT;
long MAX_NUM_FILES;
long MAX_NUM_FILES_TOT;
char* SOCKET_NAME = NULL;
char* LOG_NAME = NULL;

//stats variables
long MAX_FILES_MEMORIZED = 0;
pthread_mutex_t max_file_lock = PTHREAD_MUTEX_INITIALIZER;
long MAX_MEMORY_EVER = 0;
pthread_mutex_t max_mem_lock = PTHREAD_MUTEX_INITIALIZER;
long EXPELLED_COUNT = 0;
pthread_mutex_t exp_lock = PTHREAD_MUTEX_INITIALIZER;

//log file
FILE* log_file = NULL;
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

//table for the file memory system
Table cacheMemory;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

//list to handle client requests
MSGlist* client_requests;
pthread_mutex_t cli_req = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t wait_list = PTHREAD_COND_INITIALIZER;

//list of expelled file
MSGlist* expelled_files;
pthread_mutex_t expfiles_lock = PTHREAD_MUTEX_INITIALIZER;

//thread array
pthread_t* thread_ids = NULL;

//active file descriptor set
fd_set set; 
pthread_mutex_t set_lock = PTHREAD_MUTEX_INITIALIZER;

//max fd
int fd_max = 0;
pthread_mutex_t max_lock = PTHREAD_MUTEX_INITIALIZER;

//signal handling
volatile sig_atomic_t fast_stop = 0;
volatile sig_atomic_t slow_stop = 0;
volatile sig_atomic_t signal_stop = 0;

pthread_t signal_handler;

sigset_t signal_mask;

/** 
 * SERVER FUNCTIONS 
 */

//makes sure the socket name is unlinked
void unlinksock() 
{
    unlink(SOCKET_NAME);
}

// returns the max index of fd
// ATTENTION non la stai usando
int updateMax(fd_set set, int fdmax)
{
    for(int i = (fdmax-1); i >= 0; i--)
    	if (FD_ISSET(i, &set)) return i;
    	assert(1 == 0);
    	return -1;
}

//converts a file into a msg
void file_to_msg(FileNode* file, msg* msg)
{
	fprintf(stderr,"file to msg\n");
	//ATTENTION
	memcpy(msg->filecontents, file->textFile, file->FileSize);
	fprintf(stderr,"contents\n");
	//msg->filecontents[(file->FileSize) + 1] = '\0';
	strncpy(msg->filename, file->nameFile, strlen(file->nameFile));
	fprintf(stderr,"name\n");
	msg->filename[(strlen(file->nameFile)) + 1] = '\0';
	fprintf(stderr,"termination\n");
	msg->size = file->FileSize;
	msg->namelenght = strlen(file->nameFile);

	return;
}

//returns a response according to the result of the operation
//if it was a successfull write we also send expelled files
int report_ops(long connfd, op op_type, int ok_write) 
{

	//ATTENTION scrivere anche il nome?
	fprintf(log_file, "Returning result: %d\n", op_type);
	fflush(log_file);

	if (writen(connfd, &op_type, sizeof(op)) <= 0)
	{ 
		perror("ERROR: write report ops"); 
		return -1;
	}
	
	if(ok_write)
	{
		LOCK(&expfiles_lock);
		
		fprintf(stderr,"Sending:%zu", expelled_files->size);
		if (writen(connfd, &(expelled_files->size), sizeof(int)) <= 0)
		{ 
			perror("ERROR: write sending expelled files"); 
			return -1;
		}
		
		while (expelled_files->size > 0)
		{	fprintf(stderr, "list head before return:%s\n", expelled_files->head->filecontents);
			msg* cur_msg = safe_malloc(sizeof(msg));
			msg_list_pop_return(expelled_files, &cur_msg);
			fprintf(stderr, "return:%s\n", cur_msg->filecontents);
			if (writen(connfd, cur_msg, sizeof(msg)) <= 0)
			{
				perror("ERROR: write sending expelled files"); 
				return -1;
			}
			free(cur_msg);

		}
		UNLOCK(&expfiles_lock);
	}

	return 0;
}

//Handling of API operations on server side

int write_file_svr(long connfd, msg* file, pid_t pid)
{

	fprintf(stderr, "Writing file on server memory\n");
	
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, file->filename)) == NULL) return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	else
	{
		fprintf(stderr,"Found file: %s\n", file->filename);
		if(current->lock == 1 && current->lock_pid != pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process	
		if(current->FileSize > 0) return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); //node exists but has content inside, can't over write
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED, 0); //node exists but is closed
		if(MAX_MEMORY_TOT < file->size) return report_ops(connfd, SRV_MEM_FULL, 0); //the file is bigger than the whole available memory

		if((MAX_NUM_FILES == 0) || (MAX_MEMORY_MB < file->size)) //not enough space for new node
		{ 
			long removed = 0;

			//saving the expelled file

			LOCK(&cache_lock);
			FileNode* expelled = Hash_LFUremove(&cacheMemory);
			UNLOCK(&cache_lock);

			msg* expelled_msg = safe_malloc(sizeof(msg));
			
			file_to_msg(expelled, expelled_msg);
			
			//updating stats
			if(MAX_MEMORY_MB < file->size) MAX_MEMORY_MB += file->size;
			if(MAX_NUM_FILES == 0) MAX_NUM_FILES++;
			LOCK(&exp_lock);
			EXPELLED_COUNT++;
			UNLOCK(&exp_lock);

			//inserting expelled node in a list to send it back to the client
			LOCK(&expfiles_lock);
			msg_push_head(expelled_msg, expelled_files);
			UNLOCK(&expfiles_lock);
			
			removed = expelled_msg->size;

			LOCK(&log_lock);
			fprintf(log_file, "Capacity Miss\nBytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
			fflush(log_file);
			UNLOCK(&log_lock);

			return write_file_svr(connfd, file, pid);
		} else {
					MAX_MEMORY_MB -= file->size;
					MAX_NUM_FILES--;

					if((MAX_NUM_FILES_TOT - MAX_NUM_FILES) > MAX_FILES_MEMORIZED) 
					{
						LOCK(&max_file_lock);
						MAX_FILES_MEMORIZED = MAX_NUM_FILES_TOT - MAX_NUM_FILES;
						UNLOCK(&max_file_lock);
					}

					if((MAX_MEMORY_TOT - MAX_MEMORY_MB) > MAX_MEMORY_EVER) 
					{
						LOCK(&max_mem_lock);
						MAX_MEMORY_EVER = MAX_MEMORY_TOT - MAX_MEMORY_MB;
						UNLOCK(&max_mem_lock);
					}
					 
					fprintf(stdout, "Bytes added: %ld\nMemory left:%ld\nInserting file...\n", file->size, MAX_MEMORY_MB);
					//ATTENTION frequency set to one to erease max freq from open
					node_update(current, 1, file->filecontents, file->size);
					Hash_Inc(&cacheMemory, current);
				} 				
	}
	
	return report_ops(connfd, SRV_OK, 1);
}

int open_file_svr(long connfd, char* name, int flag, pid_t pid)
{
	fprintf(stderr, "Opening file %s\n", name);

	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, name)) != NULL) // file already exists
	{	//tried to create an already present file
		if(!flag) return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		//file is either unlocked or locked by the same client
		if(current->lock == 0 || (current->lock == 1 && current->lock_pid == pid))
		{	// if the file is closed, open file
			if(current->status == 1)
			{ 
				fprintf(stderr, "Found closed file\n");
				fprintf(stderr, "Status: %d\n", current->status);
				LOCK(&cache_lock);
				Hash_Inc(&cacheMemory, current);
				UNLOCK(&cache_lock);

				current->status = 0; //opening file

				fprintf(stderr, "Status Updated: %d\n", current->status);
				
				//O_LOCK flag set, lock the file.
				//if O_LOCK is tryied on already locked file nothing happens
				if(flag == 2 && current->lock != 1) node_lock(current, pid);
			} else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); // file already exists and its open
		} else //file is locked by some other process
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
	} else if(!flag) return report_ops(connfd, SRV_NOK, 0); //tried to create a file with no O_CREATE flag set
			else
			{	
				//ready for write
				//max frequency so it doesn't get caught if the next write expelles a file
				Hash_Insert(&cacheMemory, MAX_INT, name, 0);

				if(flag == 3)
				{
					current = Hash_SearchNode(&cacheMemory, name);
					node_lock(current, pid);
					return report_ops(connfd, SRV_READY_FOR_WRITE, 0); 
				}

				return report_ops(connfd, SRV_READY_FOR_WRITE, 0);
			} 
	return report_ops(connfd, SRV_OK, 0);
}

int close_file_svr(long connfd, msg* info, char* name)
{
	fprintf(stderr, "dentro close\n");
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, name)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process
		if(current->status == 0)
		{ // is open
			fprintf(stderr, "chiudo il nodo\n");
			Hash_Inc(&cacheMemory, current);
			current->status = 1; // close file
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); // file already closed
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	return report_ops(connfd, SRV_OK, 0);
}

int read_file_svr(long connfd, msg* info, char** tmp, size_t* size) 
{
	fprintf(stderr, "Reading file: %s\n", info->filename);

	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{	// file found
		fprintf(stderr, "Found file: %s\n", current->nameFile);

		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process

		if(current->status == 1) return 13; // file is closed, cannot read

		else 
		{	// file already exists and its open
	
			*size = current->FileSize;
			*tmp = safe_malloc((*size)*sizeof(char));
			fprintf(stderr, "File Size: %zu\nAssigned Size: %zu\n", current->FileSize, *size);
			memcpy(*tmp, current->textFile, *size);

			Hash_Inc(&cacheMemory, current);

			return 0; //ok
		} 
	}
	return 9; //file not found
}

int read_n_file_svr(long connfd, msg* info) {

	fprintf(stderr, "dentro read n\n");
	FileNode* to_send = NULL;
	int tot = 0;

	Hash_Read(&cacheMemory, info->namelenght, &to_send, &tot);

	fprintf(stderr, "tot:%d\n", tot);

	msg* send[tot];
	FileNode* current = to_send;
	
	for(int i = 0; i < tot; i++)
	{
		file_to_msg(current, send[i]);
		printf("file %d: %s\n",i,send[i]->filename);
		current = current->next;
	}

	if(writen(connfd, &tot, sizeof(int)) <= 0) 
	{
		perror("ERROR: write read file len");
		return -1;
	}

	for(int j  = 0; j < tot; j++) {

		int ret = 0;

		if((ret = writen(connfd, send[j], sizeof(msg))) <= 0)
		{
			perror("ERROR: write read file");
			return -1;
		} else fprintf(stderr, "Write: %d\n", ret);
		
	}

	return 0;
}

int append_file_svr(long connfd, msg* info) 
{
	fprintf(stderr, "dentro append\n");
	FileNode* current = NULL;
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED, 0); // file is closed
		else {
			//not enough space for append
			if(MAX_MEMORY_MB < info->size) 
			{
				fprintf(stderr, "Removing a file to do an append operation\n");

				long removed = 0; //ATTENTION

				LOCK(&cache_lock);
				FileNode* expelled = Hash_LFUremove(&cacheMemory);
				UNLOCK(&cache_lock);

				msg* expelled_msg = safe_malloc(sizeof(msg));
			
				file_to_msg(expelled, expelled_msg);

				MAX_MEMORY_MB += info->size;

				LOCK(&exp_lock);
				EXPELLED_COUNT ++;
				UNLOCK(&exp_lock);

				//inserting expelled node in a list to send it back to the client
				LOCK(&expfiles_lock);
				msg_push_head(expelled_msg, expelled_files);
				UNLOCK(&expfiles_lock);

				removed = expelled_msg->size;

				LOCK(&log_lock);
				fprintf(log_file, "Capacity Miss\nBytes removed: %ld\nMemory left:%ld\nRetrying write...\n", removed, MAX_MEMORY_MB);
				fflush(log_file);
				UNLOCK(&log_lock);

				//retrying the operation with more memory
				return append_file_svr(connfd, info);
			} else 
			{
				fprintf(stderr, "append ok\n");
				MAX_MEMORY_MB -= info->size;

				//updating the max memory for stats
				if((MAX_MEMORY_TOT - MAX_MEMORY_MB) > MAX_MEMORY_EVER) 
				{
					LOCK(&max_mem_lock);
					MAX_MEMORY_EVER = MAX_MEMORY_TOT - MAX_MEMORY_MB;
					UNLOCK(&max_mem_lock);
				}

				fprintf(stdout, "Bytes added: %ld\nMemory left:%ld\nInserting file...\n", info->size, MAX_MEMORY_MB);
				node_append(current, current->frequency + 1, info->filecontents, (current->FileSize + info->size)); //file is open, enough memory, make append
				Hash_Inc(&cacheMemory, current);
			}	
		}
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	return report_ops(connfd, SRV_OK, 1);
}

int remove_file_svr(long connfd, msg* info) 
{
	fprintf(stderr, "dentro remove\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process
		if(current->status == 1) return report_ops(connfd, SRV_FILE_CLOSED, 0); // file is closed
		else
		{ //removing the node
			MAX_MEMORY_MB += current->FileSize;
			MAX_NUM_FILES++;
			fprintf(stdout, "Bytes freed: %ld\nMemory left:%ld\n", current->FileSize, MAX_MEMORY_MB);
			Hash_Remove(&cacheMemory, info->filename);
		} 
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	return report_ops(connfd, SRV_OK, 0);
}

int lock_file_srv(long connfd, msg* info)
{
	fprintf(stderr, "dentro lock\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process	
		if(current->lock == 0) //file is not already locked
		{
			fprintf(stderr, "locking\n");
			Hash_Inc(&cacheMemory, current);
			current->lock = 1; // lock file
			current->lock_pid = info->pid;
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); // file already locked by the same process
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	return report_ops(connfd, SRV_OK, 0);
	return 0;
}

int unlock_file_srv(long connfd, msg* info)
{
	fprintf(stderr, "dentro unlock\n");
	FileNode* current = NULL;
	
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL)
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid) return report_ops(connfd, SRV_FILE_LOCKED, 0); //file is locked by some other process	
		if(current->lock == 1 && current->lock_pid == info->pid) //file is not already locked
		{
			fprintf(stderr, "unlocking\n");
			Hash_Inc(&cacheMemory, current);
			current->lock = 0; // unlock file
		}	
		else return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); // file already unlocked
	} else return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	return report_ops(connfd, SRV_OK, 0);
	return 0;
}

//WIP
int cmd(int connfd, msg* info) 
{

	//fprintf(stderr, "dentro cmd: %d\n", info.op_type);

	switch(info->op_type) 
	{
		case OPEN_FILE: 
		{
			//int count = 0;
			//while(1) count ++;
			
			fprintf(stderr, "openfile cmd\n");

			fprintf(stderr, "Flag: %d\n", info->flag);
    		fprintf(stderr, "Name: %s\n", info->filename);
			fprintf(stderr, "PID: %d\n", info->pid);

			return open_file_svr(connfd, info->filename, info->flag, info->pid);

			break;
		}
		case CLOSE_FILE: 
		{
			fprintf(stderr, "closefile cmd\n");
			fprintf(stderr, "%s\n", info->filename);

			return close_file_svr(connfd, info, info->filename);
			break;
		}

		case READ_FILE:	
		{
			fprintf(stderr, "dentro cmd read\n");
			fprintf(stderr, "Requested file: %s\n", info->filename);
			fprintf(stderr, "prima chiamata read\n");

			char* tmp_buf = NULL;
			size_t tmp_size;
			int ret = read_file_svr(connfd, info, &tmp_buf, &tmp_size);

			fprintf(stderr, "Read file:\n");
			fprintf(stderr, "Contenets: %s\n", tmp_buf);
			fprintf(stderr, "Size: %ld\n", tmp_size);
			fprintf(stderr, "Result Return: %d\n", ret);

			if(ret == 0) {
				fprintf(stderr, "dentro ok read\n");

			    if(report_ops(connfd, SRV_OK, 0) == -1) return -1;

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
			} else return report_ops(connfd, ret, 0);
			break;
		}

		case READ_FILE_N: 
		{
			return read_n_file_svr(connfd, info);
			break;
		}

		case WRITE_FILE:
		{
			fprintf(stderr, "write cmd\n");
			return write_file_svr(connfd, info, info->pid);
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
			fprintf(stderr, "%s\n", info->filename);
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
			return report_ops(connfd, SRV_OK, 0);
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


void* getMSG(void* arg)
{
	fprintf(stderr, "dentro getmsg thread\n");

	while(!fast_stop)
	{
		if(slow_stop && client_requests->head == NULL) 
		{
			fprintf(stderr, "fine slow stop\n");
			break;
		}
			

		msg* operation = safe_malloc(sizeof(msg));

		pthread_mutex_lock(&cli_req);
		pthread_cond_wait(&wait_list, &cli_req);

		if(!fast_stop && client_requests->head != NULL)
		{
			msg_list_pop_return(client_requests, &operation);

			fprintf(stderr, "prelevo coda\n");
			pthread_mutex_unlock(&cli_req);

			cmd(operation->fd_con, operation);
			fprintf(stderr, "fd: %ld\n", operation->fd_con);
			if(operation->op_type != 20)
			{
				LOCK(&set_lock);
				FD_SET(operation->fd_con, &set);
				UNLOCK(&set_lock);
			}
			else
			{
				//operation is close connection
				fprintf(stderr, "Closing connection with client %ld", operation->fd_con);
				close(operation->fd_con);
				if (operation->fd_con == fd_max) fd_max = updateMax(set, fd_max);
			}
		}
		else
			pthread_mutex_unlock(&cli_req);
	}

	fflush(stdout);
	pthread_exit(NULL);
}

void* signalhandler(void* arg)
{
	fprintf(stderr, "Signal handler activated\n");
	
	int signal = -1;

	while(!signal_stop)
	{
		
		sigwait(&signal_mask, &signal);
		fprintf(stderr, "Recived a signal\n");

		//I've recived a signal
		signal_stop = 1;

		if (signal == SIGINT || signal == SIGQUIT)
        {
			fprintf(stderr, "SIGINT or SIGQUIT\n");
			//blocco i worker e l'accettazione di nuove connesioni
			fast_stop = 1;
			pthread_cond_broadcast(&wait_list);

			//chiudo tutte le connesioni e svuoto la lista richieste
			msg* current = client_requests->head;
			msg* next;

			while(current != NULL)
			{
				close(current->fd_con);
				next = current->next;
				free(current);
				current = next;
			}

			for (int i=0; i<NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}

			fprintf(stderr, "Spaccati di botte i thread\n");
		}

		if(signal == SIGHUP) 
		{
			fprintf(stderr, "SIGHUP\n");

			slow_stop = 1;
			
			pthread_cond_broadcast(&wait_list);

			//waiting for thread workers to finish
			for(int i = 0; i < NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}
			fprintf(stderr, "cortesemente interrotti i thread\n");
		}
	}
	fprintf(stderr, "Exit sighan\n");
	pthread_exit(NULL);
}

//ok, better clean up
void configParsing() 
{
	FILE *config_input = NULL;
	
	char* buffer = safe_malloc(MAX_BUF);

	if ((config_input = fopen(CONFIG_FL, "r")) == NULL) 
	{
		perror("ERROR: opening config file");
		fclose(config_input);
		exit(EXIT_FAILURE);
	} 

	int count = 0;

	while(fgets(buffer, MAX_BUF, config_input) != NULL) 
	{

		char* comment;

		if ((comment = strchr(buffer, '#')) != NULL) 
		{ 
			continue;
		}

		char* semicolon;
		CHECK_EQ_EXIT((semicolon = strchr(buffer, ';')), NULL, "wrong config.txt file format: need ';' before newline\n");
				
		*semicolon = '\0';

		char* equals;
		CHECK_EQ_EXIT((equals = strchr(buffer, '=')), NULL, "wrong config.txt file format, usage: <name> = <value>;\n");

		++equals;

		switch (count)
		{

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
				} else {MAX_MEMORY_TOT = MAX_MEMORY_MB;}
			case 2:
				if(isNumber(equals, &MAX_NUM_FILES) != 0) {
					fprintf(stderr, "wrong config.txt file format: third line must be maximum of managable files\n");
					free(buffer);
					exit(EXIT_FAILURE);
				} else {MAX_NUM_FILES_TOT = MAX_NUM_FILES;}
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
	
	char* log_path = safe_malloc(7 + strlen(LOG_NAME));
	sprintf(log_path, "./%s.txt", LOG_NAME);
	
	log_file = fopen(log_path, "w");

	if(log_file != NULL) 
	{
		fprintf(log_file, "LOG FILE\n");
		fflush(log_file);
	}
	
	free(log_path);

	return;
}

void print_stat()
{
	printf("Server Stats:\n");
	printf("Max number of memorized files: %ld\n", MAX_FILES_MEMORIZED);
	printf("Max memory used: %ld\n", MAX_MEMORY_EVER);
	printf("%ld files were expelled\n", EXPELLED_COUNT);
	printf("\n");
	printf("Cache Memory Contenents:\n");
	Hash_Print(&cacheMemory);
	if(MAX_NUM_FILES == MAX_NUM_FILES_TOT)
		printf("Memory is empty\n");
}

int main (int argc, char* argv[]) 
{

	fprintf(stderr, "Main pid: %ld", (long)getpid());

	atexit(unlinksock);

	//signal masking
	int err = 0;

    sigset_t oldmask;
    SYSCALL_EXIT("sigemptyset", err, sigemptyset(&signal_mask), "ERROR: sigemptyset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGHUP), "ERROR: sigaddset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGINT), "ERROR: sigaddset", "");
    SYSCALL_EXIT("sigaddset", err, sigaddset(&signal_mask, SIGQUIT), "ERROR: sigaddset", "");

    //apply mask
    SYSCALL_EXIT("pthread_sigmask", err, pthread_sigmask(SIG_SETMASK, &signal_mask, &oldmask), "ERROR: pthread_sigmask", "");

    //activating signal handling thread
    SYSCALL_EXIT("pthread_create", err, pthread_create(&signal_handler, NULL, &signalhandler, NULL), "ERROR: pthread_create", "");  
	
	//allocating the global socket name
	SOCKET_NAME = safe_malloc(MAX_BUF);
	LOG_NAME = safe_malloc(MAX_BUF);

	// parsing the config file
	configParsing();

	log_create(LOG_NAME);

	fprintf(log_file, "\nServer created\nMax number of thread workers: %ld\nMax memory available: %ld\nCurrently available memory: %ld\nMax Number of files: %ld\nSocket Name: %s\nLog file name: %s\n\n", 
		NUM_THREAD_WORKERS, MAX_MEMORY_MB, MAX_MEMORY_TOT, MAX_NUM_FILES, SOCKET_NAME, LOG_NAME);
	fflush(log_file);

	//initializing cache memory
	Hash_Init(&cacheMemory, TAB_SIZE);

	fprintf(log_file, "Hash memory initialized successfully\n");

	//list init
	client_requests = safe_malloc(sizeof(MSGlist));
	msg_list_init(client_requests);

	expelled_files = safe_malloc(sizeof(MSGlist));
	msg_list_init(expelled_files);

	//Accepting connections
	unlinksock();

	int fd_skt; //connection socket
	int fd_sel; //index to verify select results
	int fd_con; //I/O socket with client

	fd_set rdset; //set of fd wating for reading

	//creating threads

	CHECK_EQ_EXIT((thread_ids = (pthread_t*) calloc(NUM_THREAD_WORKERS, sizeof(pthread_t))), NULL, "ERROR: calloc threads");

	int res = 0;

	for(int i = 0; i < NUM_THREAD_WORKERS; i++) 
	{
		if((res = pthread_create(&(thread_ids[i]), NULL, &getMSG, NULL) != 0)) 
		{ 
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

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1;
	
	while(!slow_stop && !fast_stop)  
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

	//waiting for signal handler thread to terminate
	pthread_join(signal_handler, NULL);

	/*Printing server stats:
		Max number of memorized files
		Max memory size reached
		Numer of files expelled for memory
		List of all files
	*/
	print_stat();

    //cleanup section

	close(fd_skt);

    free(SOCKET_NAME);
	free(LOG_NAME);

	msg_list_destroy(client_requests);
	msg_list_destroy(expelled_files);

	//Hash_Destroy(&cacheMemory);

	pthread_exit(NULL);
	//return EXIT_SUCCESS;
}