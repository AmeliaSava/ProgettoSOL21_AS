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

long MAX_MEMORY_MB;
pthread_mutex_t mem_lock = PTHREAD_MUTEX_INITIALIZER;
long MAX_MEMORY_TOT;
pthread_mutex_t memt_lock = PTHREAD_MUTEX_INITIALIZER;
long MAX_NUM_FILES;
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;
long MAX_NUM_FILES_TOT;
pthread_mutex_t filet_lock = PTHREAD_MUTEX_INITIALIZER;

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

/**
 * \brief: returns the max index of fd
 */
int updateMax(fd_set set, int fdmax)
{
    for(int i = (fdmax-1); i >= 0; i--)
    	if (FD_ISSET(i, &set)) return i;
    	assert(1 == 0);
    	return -1;
}

/**
 * \brief: converts a file into a msg
 */
void file_to_msg(FileNode* file, msg* msg)
{
	//copying the file contenent
	memcpy(msg->filecontents, file->textFile, file->FileSize);

	//copying the name	
	strncpy(msg->filename, file->nameFile, strlen(file->nameFile));
	msg->filename[(strlen(file->nameFile)) + 1] = '\0';

	//size and namelenght
	msg->size = file->FileSize;
	msg->namelenght = strlen(file->nameFile);

	return;
}

/**
 * \brief: returns a response according to the result of the operation
 * 			if it was a successfull write also sends expelled files
 * \param connfd: the fd where the comunication is happening
 * \param op_type: the response
 * \param ok_write: used to check if the preceding operation was a successful write, so it is possible
 * 						to send expelled files, if there are any
 */
int report_ops(long connfd, op op_type, int ok_write) 
{

	LOCK(&log_lock);
	fprintf(log_file, "\nCLIENT:%ld Returning result: %d\n", connfd, op_type);
	fflush(log_file);
	UNLOCK(&log_lock);

	//send the result to the client
	if (writen(connfd, &op_type, sizeof(op)) <= 0)
	{ 
		perror("ERROR: write report ops"); 
		return -1;
	}
	
	//was there a successful write operation before?
	if(ok_write)
	{
		LOCK(&expfiles_lock);

		//send how many expelled files are there to recieve
		if (writen(connfd, &(expelled_files->size), sizeof(int)) <= 0)
		{ 
			perror("ERROR: write sending expelled files"); 
			return -1;
		}
		
		//as long as there is a file to send
		while (expelled_files->size > 0)
		{	
			//get the message
			msg* cur_msg = safe_malloc(sizeof(msg));
			msg_list_pop_return(expelled_files, &cur_msg);

			//send it
			if (writen(connfd, cur_msg, sizeof(msg)) <= 0)
			{
				perror("ERROR: write sending expelled files"); 
				return -1;
			}
			//free it
			free(cur_msg);

		}
		UNLOCK(&expfiles_lock);
	}

	return 0;
}

/**
 * Handling of API operations on server side
 */

/**
 * \brief: writes a file in the memory, udating an already existing node
 * \param connfd: the fd where the comunication is happening
 * \param file: the file that needs to be written
 * \param pid: the calling process pid
 */
int write_file_svr(long connfd, msg* file, pid_t pid)
{

	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, file->filename)) == NULL)
	{
		//there's no node created by the open file operation
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}
	else 
	{
		//found the empty file created by openFile
		if(current->lock == 1 && current->lock_pid != pid)
		{
			//file is locked by some other process
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		}

		if(current->FileSize > 0)
		{
			//node exists but has content inside, can't over write
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		}

		if(current->status == 1)
		{
			//node exists but is closed
			return report_ops(connfd, SRV_FILE_CLOSED, 0);
		}

		if(MAX_MEMORY_TOT < file->size)
		{
			//the file is bigger than the whole available memory	
			return report_ops(connfd, SRV_MEM_FULL, 0);
		} 
		
		if((MAX_NUM_FILES == 0) || (MAX_MEMORY_MB < file->size)) 
		{ 
			//not enough space for new node
			long removed = 0;

			//saving the expelled file
			FileNode* expelled = Hash_LFUremove(&cacheMemory);
			
			msg* expelled_msg = safe_malloc(sizeof(msg));

			//converting the file to msg so it can be sent to the client
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
			fprintf(log_file, "Capacity Miss\nFile expelled:%s\nRemoved: %ld\nMemory left:%ld\nRetrying write...\n", expelled_msg->filename, removed, MAX_MEMORY_MB);
			fflush(log_file);
			UNLOCK(&log_lock);

			//calling the function recursivly now that memory and file count have been updated
			return write_file_svr(connfd, file, pid);
		} 
		else 
		{
			//enough space for file
			MAX_MEMORY_MB -= file->size;
			MAX_NUM_FILES--;

			if((MAX_NUM_FILES_TOT - MAX_NUM_FILES) > MAX_FILES_MEMORIZED) 
			{
				MAX_FILES_MEMORIZED = MAX_NUM_FILES_TOT - MAX_NUM_FILES;
			}

			if((MAX_MEMORY_TOT - MAX_MEMORY_MB) > MAX_MEMORY_EVER) 
			{	
				MAX_MEMORY_EVER = MAX_MEMORY_TOT - MAX_MEMORY_MB;
			}

			LOCK(&log_lock);	 
			fprintf(log_file, "WriteFile %s\nBytes:%ld\nMemory left:%ld\nInserting file...\n", file->filename, file->size, MAX_MEMORY_MB);
			fflush(log_file);
			UNLOCK(&log_lock);

			//frequency set to one to erease max freq from open
			node_update(current, 1, file->filecontents, file->size);
			Hash_Inc(&cacheMemory, current);
		}				
	}
	
	return report_ops(connfd, SRV_OK, 1);
}

/**
 * \brief: opens an existing file, or creates an empty node to be writte, can also lock files
 * \param connfd: the fd where the comunication is happening
 * \param name: the name of the file to open
 * \param flag: the flag of the open operation
 * \param pid: the calling process pid
 */
int open_file_svr(long connfd, char* name, int flag, pid_t pid)
{
	FileNode* current = NULL;

	if((current = Hash_SearchNode(&cacheMemory, name)) != NULL) // file already exists
	{	
		//tried to create an already present file
		if(flag == 1) 
		{
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		}

		//file is either unlocked or locked by the same client
		if(current->lock == 0 || (current->lock == 1 && current->lock_pid == pid))
		{	
			// if the file is closed, open file
			if(current->status == 1)
			{ 
				//increase frequency
				Hash_Inc(&cacheMemory, current);
				
				current->status = 0; //opening file

				LOCK(&log_lock);
				fprintf(log_file, "OpenFile %s\nFlag: %d\n", name, flag);
				fflush(log_file);
				UNLOCK(&log_lock);
				
				//O_LOCK flag set, lock the file.
				//if O_LOCK is tryied on already locked file nothing happens
				if(flag == 2 && current->lock != 1)
				{
					LOCK(&log_lock);
					fprintf(log_file, "OpenLock: %s\n", name);
					fflush(log_file);
					UNLOCK(&log_lock);
					node_lock(current, pid);
				} 
	
			} 
			else 
			{
				return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
			} // file already exists and its open
		} 
		else
		{
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		} //file is locked by some other process		
	}
	else
	{
		if(!flag)
		{
			//tried to create a file with no O_CREATE flag set
			return report_ops(connfd, SRV_NOK, 0); 
		} 
		else
		{	
			//ready for write
			//max frequency so it doesn't get caught if the next write expelles a file
			LOCK(&log_lock);
			fprintf(log_file, "Create file %s\n", name);
			fflush(log_file);
			UNLOCK(&log_lock);
			Hash_Insert(&cacheMemory, MAX_INT, name, 0);

			//O_CREATE && O_LOCK
			if(flag == 3)
			{
				LOCK(&log_lock);
				fprintf(log_file, "OpenLock %s\n", name);
				fflush(log_file);
				UNLOCK(&log_lock);
				
				current = Hash_SearchNode(&cacheMemory, name);
				node_lock(current, pid);
	
				return report_ops(connfd, SRV_READY_FOR_WRITE, 0); 
			}
			return report_ops(connfd, SRV_READY_FOR_WRITE, 0);
		}
	}
	return report_ops(connfd, SRV_OK, 0);
}

/**
 * \brief: closes a file
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int close_file_svr(long connfd, msg* info)
{
	FileNode* current = NULL;

	LOCK(&cache_lock);
	// if the file exists
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL)
	{ 
		if(current->lock == 1 && current->lock_pid != info->pid)
		{	//file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0); 
		}
			
		if(current->status == 0)
		{ 
			// the file is open
			LOCK(&log_lock);
			fprintf(log_file, "Close file %s\n", info->filename);
			fflush(log_file);
			UNLOCK(&log_lock);
			
			Hash_Inc(&cacheMemory, current);

			current->status = 1; // close file
		}	
		else 
		{	// file already closed
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0); 
		}
	} 
	else
	{ 
		//file not found
		UNLOCK(&cache_lock);
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}
	UNLOCK(&cache_lock);
	return report_ops(connfd, SRV_OK, 0);
}

/**
 * \brief: reads a file from the memory
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 * \param tmp: a pointer to a string where the file contents will be saved to return to the client
 * \param size: a pointer to a size_t where the file size will be saved to return to the client
 */
int read_file_svr(long connfd, msg* info, char** tmp, size_t* size) 
{
	
	FileNode* current = NULL;

	LOCK(&cache_lock);
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{	
		// file found
		if(current->lock == 1 && current->lock_pid != info->pid)
		{
			// file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		} 

		if(current->status == 1)
		{
			// file is closed, cannot read
			UNLOCK(&cache_lock);
			return 13; 
		} 

		else 
		{	
			// file exists and its open
			*size = current->FileSize;
			*tmp = safe_malloc((*size));
			
			LOCK(&log_lock);
			fprintf(log_file, "ReadFile %s\nSize:%zu\n", current->nameFile, current->FileSize);
			fflush(log_file);
			UNLOCK(&log_lock);

			memcpy(*tmp, current->textFile, *size);

			Hash_Inc(&cacheMemory, current);

			UNLOCK(&cache_lock);
			return 0; //ok
		} 
	}
	UNLOCK(&cache_lock);
	return 9; //file not found
}

/**
 * \brief: reads n files from the memory, or all the files
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int read_n_file_svr(long connfd, msg* info) {

	FileNode* to_send = NULL;
	int tot = 0;

	//hash function that reads n files and adds them to a list
	Hash_Read(&cacheMemory, info->namelenght, &to_send, &tot);

	//fprintf(stderr, "tot:%d\n", tot);

	FileNode* current = to_send;
	
	//sending how many files have been actually read
	if(writen(connfd, &tot, sizeof(int)) <= 0) 
	{
		perror("ERROR: write read file len");
		return -1;
	}

	//sending the files
	for(int i = 0; i < tot; i++)
	{
		//converting the file to a msg
		msg* send = safe_malloc(sizeof(msg));
		file_to_msg(current, send);

		LOCK(&log_lock);
		fprintf(log_file, "ReadFile: %s\nSize:%ld\n", send->filename, send->size);
		fflush(log_file);
		UNLOCK(&log_lock);
		
		//sending the msg
		if((writen(connfd, send, sizeof(msg))) <= 0)
		{
			perror("ERROR: write read file");
			exit(EXIT_FAILURE);
		}
		//deleting msg, sending the next
		free(send);
		current = current->next;
	}

	//cleaning the list of files to send
	FileNode* delete = to_send;
	FileNode* next;

	while (delete != NULL)
	{
		next = delete->next;
		free(delete->nameFile);
		free(delete->textFile);
		free(delete);
		delete = next;
	}
	
	return 0;
}

/**
 * \brief: appends something to a file
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int append_file_svr(long connfd, msg* info) 
{
	FileNode* current = NULL;

	LOCK(&cache_lock);
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{ // file already exists
		if(current->lock == 1 && current->lock_pid != info->pid)
		{
			//file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		}
		if(current->status == 1)
		{
			// file is closed
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_CLOSED, 0); 
		}
		else 
		{
			LOCK(&mem_lock);
			//not enough space for append
			if(MAX_MEMORY_MB < info->size) 
			{

				long removed = 0;

				FileNode* expelled = Hash_LFUremove(&cacheMemory);

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
				fprintf(log_file, "Capacity Miss\nRemoved: %ld\nMemory left:%ld\nRetrying append...\n", removed, MAX_MEMORY_MB);
				fflush(log_file);
				UNLOCK(&log_lock);

				//retrying the operation with more memory
				return append_file_svr(connfd, info);
			}
			else 
			{	
				MAX_MEMORY_MB -= info->size;

				LOCK(&memt_lock);
				LOCK(&max_mem_lock);
				//updating the max memory for stats
				if((MAX_MEMORY_TOT - MAX_MEMORY_MB) > MAX_MEMORY_EVER) 
				{	
					MAX_MEMORY_EVER = MAX_MEMORY_TOT - MAX_MEMORY_MB;
				}
				UNLOCK(&memt_lock);
				UNLOCK(&max_mem_lock);

				LOCK(&log_lock);
				fprintf(log_file, "Append\nBytes: %ld\nMemory left:%ld\nInserting file...\n", info->size, MAX_MEMORY_MB);
				fflush(log_file);
				UNLOCK(&log_lock);

				//file is open, enough memory, make append
				node_append(current, current->frequency + 1, info->filecontents, (current->FileSize + info->size)); 
				Hash_Inc(&cacheMemory, current);
			}
			UNLOCK(&mem_lock);
		}
	} 
	else 
	{
		UNLOCK(&cache_lock);
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}
	UNLOCK(&cache_lock);
	return report_ops(connfd, SRV_OK, 1);
}

/**
 * \brief: removes a file, only if it's locked
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int remove_file_svr(long connfd, msg* info) 
{
	FileNode* current = NULL;

	LOCK(&cache_lock);
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL) 
	{ 
		// file already exists
		if(current->lock == 1 && current->lock_pid != info->pid)
		{
			//file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		}
		if(current->status == 1)
		{ 
			// file is closed
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_CLOSED, 0); 
		}
		if(current->lock == 1 && current->lock_pid == info->pid)
		{
			//file is locked by the same process
			//removing the node
			LOCK(&mem_lock);
			MAX_MEMORY_MB += current->FileSize;
			UNLOCK(&mem_lock);
			LOCK(&file_lock);
			MAX_NUM_FILES++;
			UNLOCK(&file_lock);

			LOCK(&log_lock);
			fprintf(log_file, "Remove File\nFreed: %ld\nMemory left:%ld\n", current->FileSize, MAX_MEMORY_MB);
			fflush(log_file);
			UNLOCK(&log_lock);

			Hash_Remove(&cacheMemory, info->filename);
		}
		else
		{
			//can only remove a locked file
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_NOK, 0);
		}
	}
	else 
	{
		UNLOCK(&cache_lock);
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}
	UNLOCK(&cache_lock);
	return report_ops(connfd, SRV_OK, 0);
}

/**
 * \brief: locks a file
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int lock_file_srv(long connfd, msg* info)
{
	FileNode* current = NULL;
	
	LOCK(&cache_lock);
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL)
	{ 
		// file already exists
		if(current->lock == 1 && current->lock_pid != info->pid)
		{
			//file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		}	
		if(current->lock == 0) //file is not already locked
		{
			LOCK(&log_lock);
			fprintf(log_file, "LockFile: %s\n", info->filename);
			fflush(log_file);
			UNLOCK(&log_lock);

			Hash_Inc(&cacheMemory, current);
			current->lock = 1; // lock file
			current->lock_pid = info->pid;
		}	
		else
		{
			// file already locked by the same process
			UNLOCK(&cache_lock); 
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		}
	} 
	else
	{ 
		UNLOCK(&cache_lock);
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}

	UNLOCK(&cache_lock);
	return report_ops(connfd, SRV_OK, 0);
}

/**
 * \brief: unlocks a file
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
int unlock_file_srv(long connfd, msg* info)
{
	FileNode* current = NULL;
	LOCK(&cache_lock);
	if((current = Hash_SearchNode(&cacheMemory, info->filename)) != NULL)
	{ 
		// file already exists
		if(current->lock == 1 && current->lock_pid != info->pid)
		{		
			//file is locked by some other process
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_LOCKED, 0);
		} 	
		if(current->lock == 1 && current->lock_pid == info->pid) 
		{
			//file is locked by the same process
			LOCK(&log_lock);
			fprintf(log_file, "UnlockFile: %s\n", info->filename);
			fflush(log_file);
			UNLOCK(&log_lock);

			Hash_Inc(&cacheMemory, current);
			current->lock = 0; // unlock file
		}	
		else
		{ 
			// file already unlocked
			UNLOCK(&cache_lock);
			return report_ops(connfd, SRV_FILE_ALREADY_PRESENT, 0);
		}
	} 
	else 
	{
		UNLOCK(&cache_lock);
		return report_ops(connfd, SRV_FILE_NOT_FOUND, 0);
	}

	UNLOCK(&cache_lock);
	return report_ops(connfd, SRV_OK, 0);
}

/**
 * \brief: switched between possible required operations
 * \param connfd: the fd where the comunication is happening
 * \param info: the msg containing all information needed to perform the operation
 */
	int cmd(int connfd, msg* info) 
{

	//fprintf(stderr, "dentro cmd: %d\n", info.op_type);

	switch(info->op_type) 
	{
		case OPEN_FILE: 
		{
			//fprintf(stderr, "Flag: %d\n", info->flag);
    		//fprintf(stderr, "Name: %s\n", info->filename);
    		LOCK(&cache_lock);
			int ret = open_file_svr(connfd, info->filename, info->flag, info->pid);
			UNLOCK(&cache_lock);
			return ret;
			break;
		}
		case CLOSE_FILE: 
		{
			//fprintf(stderr, "Name: %s\n", info->filename);
			return close_file_svr(connfd, info);
			break;
		}
		case READ_FILE:	
		{
			//data that needs to be sent to the client
			char* tmp_buf;
			size_t tmp_size = 0;
			int ret = read_file_svr(connfd, info, &tmp_buf, &tmp_size);

			//fprintf(stderr, "Read file\n");
			//fprintf(stderr, "Contenets: %s\n", tmp_buf);
			//fprintf(stderr, "Size: %ld\n", tmp_size);
			//fprintf(stderr, "Result Return: %d\n", ret);

			//if the operation completed with success
			if(ret == 0) 
			{
				//send ok to the client
			    if(report_ops(connfd, SRV_OK, 0) == -1) return -1;
				
				//send the size
			    if(writen(connfd, &tmp_size, sizeof(long)) <= 0) 
				{
					perror("ERROR: write read file len");
					return -1;
				}

				//send buffer with data
				if(writen(connfd, tmp_buf, tmp_size) <= 0)
				{
					perror("ERROR: write read file");
					return -1;
				}

				free(tmp_buf);
			}
			else
			{
				//something went wrong the buffer is not needed
				free(tmp_buf);
				//report the result
				return report_ops(connfd, ret, 0);
			}
				
			break;
		}
		case READ_FILE_N: 
		{
			LOCK(&cache_lock);
			int ret = read_n_file_svr(connfd, info);
			UNLOCK(&cache_lock);
			return ret;
			break;
		}
		case WRITE_FILE:
		{
			LOCK(&cache_lock);
			LOCK(&memt_lock);
			LOCK(&mem_lock);
			LOCK(&file_lock);
			int ret =  write_file_svr(connfd, info, info->pid);
			UNLOCK(&cache_lock);
			UNLOCK(&memt_lock);
			UNLOCK(&file_lock);
			UNLOCK(&mem_lock);
			return ret;
			break;
		}
		case APPEND_FILE:
		{
			return append_file_svr(connfd, info);
			break;
		}
		case REMOVE_FILE:
    	{
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

/**
 * \brief: the thread worker that takes an element from the requests list
 */
void* getMSG(void* arg)
{
	LOCK(&log_lock);
	fprintf(log_file, "\nStartedThread:%ld\n", pthread_self());
	fflush(log_file);
	UNLOCK(&log_lock);

	//not signal to interrupt immediatly sent
	while(!fast_stop)
	{
		//a slow stop signal was recieved and all the remaining requests were served, break
		if(slow_stop && client_requests->head == NULL) 
		{
			LOCK(&log_lock);
			fprintf(log_file, "SIGHUP: Finished serving all requests\n");
			fflush(log_file);
			UNLOCK(&log_lock);
			break;
		}	

		msg* operation = safe_malloc(sizeof(msg));

		//waits for a request to enter the list
		LOCK(&cli_req);
		WAIT(&wait_list, &cli_req);

		//was the wroker interrupted? are there any requests to process?
		if(!fast_stop && client_requests->head != NULL)
		{
			//get the request
			msg_list_pop_return(client_requests, &operation);

			UNLOCK(&cli_req);

			LOCK(&log_lock);
			fprintf(log_file, "\nTHREAD:%ld\nServing request from client: %ld\n", pthread_self(), operation->fd_con);
			fflush(log_file);
			UNLOCK(&log_lock);

			//execute it
			cmd(operation->fd_con, operation);

			//if the client didn't request to close the connection, reinsert the client in the fdset
			if(operation->op_type != 20)
			{
				LOCK(&set_lock);
				FD_SET(operation->fd_con, &set);
				UNLOCK(&set_lock);
			}
			else
			{
				//operation is close connection
				LOCK(&log_lock);
				fprintf(log_file, "Closing connection\nClient: %ld", operation->fd_con);
				fflush(log_file);
				UNLOCK(&log_lock);
				//close connection to client
				close(operation->fd_con);
				//get the new max fd
				if (operation->fd_con == fd_max) fd_max = updateMax(set, fd_max);
			}
		}
		else
		{
			UNLOCK(&cli_req);
		}		
		free(operation);
	}
	
	fflush(stdout);
	pthread_exit(NULL);
}

/**
 * \brief: signal handler thread
 */
void* signalhandler(void* arg)
{
	//fprintf(stderr, "\nSignal handler activated\n");
	
	int signal = -1;

	//the signal handler will stop if a signal was already recived
	while(!signal_stop)
	{
		//wait for signal
		sigwait(&signal_mask, &signal);

		LOCK(&log_lock);
		fprintf(log_file, "\nRecived a signal\n");
		fflush(log_file);
		UNLOCK(&log_lock);

		//I've recived a signal
		signal_stop = 1;

		//fast stop
		if (signal == SIGINT || signal == SIGQUIT)
        {
			LOCK(&log_lock);
			fprintf(log_file, "SIGINT or SIGQUIT\n");
			fflush(log_file);
			UNLOCK(&log_lock);

			//blocking the workers and no new connections will be accepted
			fast_stop = 1;
			//unblocks list
			pthread_cond_broadcast(&wait_list);

			//closing all connections and emptying request list
			msg* current = client_requests->head;
			msg* next;

			while(current != NULL)
			{
				close(current->fd_con);
				next = current->next;
				free(current);
				current = next;
			}

			//close all threads
			for (int i=0; i<NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}
		}

		//slow stop
		if(signal == SIGHUP) 
		{
			LOCK(&log_lock);
			fprintf(log_file, "SIGHUP\n");
			fflush(log_file);
			UNLOCK(&log_lock);

			slow_stop = 1;
			
			pthread_cond_broadcast(&wait_list);

			//waiting for thread workers to finish
			for(int i = 0; i < NUM_THREAD_WORKERS; i++)
			{
				pthread_join(thread_ids[i], NULL);
			}
		}
	}
	//fprintf(stderr, "Exit sighan\n");
	pthread_exit(NULL);
}

/**
 * \brief: parses the config file
 */
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

/**
 * \brief: creates the log file
 */
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

/**
 * \brief: prints the stats of the server session
 */
void print_stat()
{
	printf("\n");
	printf("/-------------------------/\n");
	printf("      Server Stats:\n");
	printf("/-------------------------/\n");
	printf("\n");
	printf("Max number of memorized files: %ld\n", MAX_FILES_MEMORIZED);

	fprintf(log_file, "\nMAXFILES:%ld\n", MAX_FILES_MEMORIZED);
	fflush(log_file);

	printf("Max memory used: %ld\n", MAX_MEMORY_EVER);

	fprintf(log_file, "\nMAXMEM:%ld\n", MAX_MEMORY_EVER);
	fflush(log_file);

	printf("%ld files were expelled\n", EXPELLED_COUNT);

	fprintf(log_file, "\nEXP:%ld\n", EXPELLED_COUNT);
	fflush(log_file);

	printf("\n");
	printf("/-------------------------/\n");
	printf("  Cache Memory Contenents:\n");
	printf("/-------------------------/\n");
	printf("\n");
	if(cacheMemory.curSize == 0)
		printf("   Memory is empty\n");
	Hash_Print(&cacheMemory);
}

int main (int argc, char* argv[]) 
{
	//fprintf(stderr, "Main PID: %ld\n", (long)getpid());

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

	// allocating the global socket and log names
	SOCKET_NAME = safe_malloc(MAX_BUF);
	LOG_NAME = safe_malloc(MAX_BUF);

	// parsing the config file
	configParsing();

	// creating the log file
	log_create(LOG_NAME);

	fprintf(log_file, "\nSERVER CREATED\nMax number of thread workers: %ld\nMax memory available: %ld\nCurrently available memory: %ld\nMax Number of files: %ld\nSocket Name: %s\nLog file name: %s\n\n", 
		NUM_THREAD_WORKERS, MAX_MEMORY_MB, MAX_MEMORY_TOT, MAX_NUM_FILES, SOCKET_NAME, LOG_NAME);
	fflush(log_file);

	//initializing cache memory
	Hash_Init(&cacheMemory, TAB_SIZE);

	fprintf(log_file, "Hash memory initialized successfully\n");
	fflush(log_file);

	//list init
	client_requests = safe_malloc(sizeof(MSGlist));
	msg_list_init(client_requests);

	expelled_files = safe_malloc(sizeof(MSGlist));
	msg_list_init(expelled_files);

	//Accepting connections
	unlink(SOCKET_NAME);

	int fd_skt; //connection socket
	int fd_sel; //index to verify select results
	int fd_con; //I/O socket with client

	fd_set rdset; //set of fd wating for reading

	// creating  and activating threads

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
	fprintf(log_file, "\nConnecting to %s\n", SOCKET_NAME);

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

	int print_count = 0;
	
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

			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++)
			{
				//accepting new connections
				//fprintf(stderr, "accepting connections\n");
			    if (FD_ISSET(fd_sel, &rdset))
				{ 
					//is it ready?
			    	//fprintf(stderr, "ready?\n");

					if (fd_sel == fd_skt)
					{ 
						// sock connect ready
						//fprintf(stderr, "sock ready\n");
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						
						// updating max
						if(fd_con > fd_max) fd_max = fd_con;  
						if(print_count == 0)
						{
							fprintf(log_file, "\nConnected to client: %d\n", fd_con);
							fflush(log_file);
							print_count++;
						}
						continue;
					} 

					fd_con = fd_sel;

					//fprintf(log_file, "Listening to client: %d\n", fd_con);
					//fflush(log_file);

					msg* request = safe_malloc(sizeof(msg));
	
					if (readn(fd_con, request, sizeof(msg))<=0) 
					{
						perror("ERROR: reading msg");
						return EXIT_FAILURE;
					}

					//fprintf(log_file, "\nFile: %s\n", request->filename);
					//fflush(log_file);

					//take out the client file descriptor to avoid recieving unwanted messages
					//if needed it will be readded later on
					FD_CLR(fd_con, &set);

					request->fd_con = fd_con;
				
					//fprintf(stderr, "adding request to list\n");
					//adding request to the list and signal that is not empty anymore
					LOCK(&cli_req);
					msg_push_head(request, client_requests);
					pthread_cond_signal(&wait_list);
					UNLOCK(&cli_req);
 
										
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
	unlink(SOCKET_NAME);

	fclose(log_file);

    free(SOCKET_NAME);
	free(LOG_NAME);

	msg_list_destroy(client_requests);
	msg_list_destroy(expelled_files);

	free(thread_ids);

	Hash_Destroy(&cacheMemory);

	pthread_exit(NULL);
}