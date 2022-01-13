#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <time.h>

#include <ops.h>
#include <coms.h>
#include <conn.h>

int sockfd;
int print_coms = 0;

//EXTRA FUNCTIONS
char* readFileBytes(const char *name, long* filelen) 
{
	//fprintf(stderr, "dentro readbyte\n");
	FILE *file = NULL;

	if ((file = fopen(name, "rb")) == NULL) 
	{
		perror("ERROR: opening file");
		fclose(file);
		exit(EXIT_FAILURE);
	}

	//go to end of file
	if(fseek(file, 0, SEEK_END) == -1) 
	{
		perror("ERROR: fseek"); 
		fclose(file);
		exit(EXIT_FAILURE);
	} 

	long len = ftell(file); //current byte offset
	*filelen = len; //assigning the lenght to external value

	char* ret; //return string
	
	ret = safe_malloc(len);

	//starting at the beginning of the file
	if(fseek(file, 0, SEEK_SET) != 0) 
	{
		perror("ERROR: fseek");
		fclose(file); 
		free(ret);
		exit(EXIT_FAILURE);
	}

	int err;

	//fread must return the same number as the size passed
	if((err = fread(ret, 1, len, file)) != len)
	{
		perror("ERROR: fread");
		fclose(file); 
		free(ret);
		exit(EXIT_FAILURE);
	}

	fclose(file);
	return ret;
}

//ATTENTION CORRUPT FILE
int WriteFilefromByte(const char* name, char* text, long size, const char* dirname) 
{
	FILE *fp1;
	char* fullpath = safe_malloc(strlen(dirname) + strlen(name) + 1);
	
	sprintf(fullpath,"%s/%s", dirname, name);
	fullpath[strlen(dirname) + strlen(name) + 1] = '\0';
	//fprintf(stderr, "%s", fullpath);
	errno = 0;
	if((fp1 = fopen(fullpath, "wb")) == NULL) {
		fprintf(stderr, "errno:%d\n", errno);
		return -1;
	}

	if((fwrite(text, sizeof(char), size, fp1)) != size) return -1;

	fclose(fp1);

	return 0;
}

void set_prints()
{
	if(!print_coms) print_coms = 1;
	else print_coms = 0;
}

/**
 * API FUNCTIONS
 **/

int openConnection(const char* sockname, int msec, const struct timespec abstime) {

	errno = 0;
	int result = 0;
	
	//checking if parameters are correct
	if(sockname == NULL || msec < 0) { errno = EINVAL; return -1;}
	
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);
	
	if((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) { errno = -1; return -1;}

	struct timespec current_time;
	if((clock_gettime(CLOCK_REALTIME, &current_time)) == -1) return -1;

	while ((result = connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) != 0 && abstime.tv_sec > current_time.tv_sec) {
		if((result = usleep(msec*1000)) != 0) return result;
		if((clock_gettime(CLOCK_REALTIME, &current_time)) == -1) return -1;
	}

	if(result != -1) return 0;

	return result;
}

int closeConnection(const char* sockname) 
{

	errno = 0;

	msg* close_connection = safe_malloc(sizeof(msg));
	close_connection->op_type = 20;

	if(writen(sockfd, close_connection, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write closeConnection");
		return -1;
	}

	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: reading response closeConnection");
		return -1;
	}

	close(sockfd);

	if(response == SRV_OK) return 0;

	return -1;
}

int openFile(const char* pathname, int flags)
{
	//fprintf(stderr, "dentro openFile API\n");

	msg* open_file = safe_malloc(sizeof(msg));
	open_file->op_type = 0;
	
	errno = 0;
	
	//copying pathname
	int name_lenght = strlen(pathname)+1;

	strncpy(open_file->filename, pathname, name_lenght);

	open_file->filename[name_lenght] = '\0';
	open_file->namelenght = name_lenght;
	open_file->size = 0;

	//passing the process' pid
	open_file->pid = getpid();
	//fprintf(stderr, "%d\n", open_file->pid);
	//fprintf(stderr, "%d\n", getppid());

	open_file->flag = flags;
		
	if(writen(sockfd, open_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		return -1;
	}
	
	//recivieng outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read response openFile");
		return -1;
	}

	if(print_coms)
	{
		fprintf(stdout,"Open File:\n");
		print_op(response);
	}

	if(response == SRV_READY_FOR_WRITE)	return 1;

	if(response == SRV_OK) return 0;

	return -1;
}

int readFile(const char* pathname, void** buf, size_t* size)
{
	//fprintf(stderr, "dentro readFile API\n");

	msg* read_file = safe_malloc(sizeof(msg));
	read_file->op_type = 2;

	errno = 0;

	char* tmp_buf;

	//copying pathname in buffer
	int name_lenght = strlen(pathname)+1;

	strncpy(read_file->filename, pathname, name_lenght);

	read_file->filename[name_lenght] = '\0';
	read_file->namelenght = name_lenght;
	read_file->size = 0;

	//passing the process' pid
	read_file->pid = getpid();

	if(writen(sockfd, read_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		return -1;
	}

	//recivieng outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read response ReadFile");
		return -1;
	}

	if(print_coms)
	{
		fprintf(stdout,"Read File:\n");
		print_op(response);
	}

	//if there were no errors on the server's side
	if(response == SRV_OK) 
	{
		if(readn(sockfd, size, sizeof(size_t)) <= 0) 
		{ 
			errno = -1; 
			perror("ERROR: read1");
			return -1;
		}  //reciveing file len

		//fprintf(stderr, "Recived size: %zu\n", *size);
		tmp_buf = safe_malloc((*size));

		if(readn(sockfd, tmp_buf, (*size)) <= 0) 
		{ 
			errno = -1;
			perror("ERROR: read response readFile");
			return -1;
		}  //reciveing byte file

		//fprintf(stderr, "Recived text: %s\n", tmp_buf);
		//fprintf(stderr, "Stored at %p\n", (void*)tmp_buf);

		*buf = (void*)tmp_buf;

	} else return -1;

	return 0;
}

int readNfiles(int N, const char* dirname) 
{
	msg* readN_file = safe_malloc(sizeof(msg));
	readN_file->op_type = 3;
	readN_file->namelenght = N; //using namelenght field to store N
	errno = 0;

	readN_file->pid = getpid();
	//fprintf(stderr, "dentro readNFile API\n");

	if(writen(sockfd, readN_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write1");
		return -1;
	}
	
	int result;

	if (readn(sockfd, &result, sizeof(int)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read response readNFiles");
		return -1;
	}

	//fprintf(stderr, "Files Read: %d\n", result);

	if(result > 0) {
		for(int i = 0; i < result; i++)
		{		
			msg* file = safe_malloc(sizeof(msg));
			int ret = 0;

			if((ret = readn(sockfd, file, sizeof(msg))) <= 0) 
			{
				errno = -1;
				perror("ERROR: read1");
				exit(EXIT_FAILURE);
			}

			if(print_coms)
			{
				fprintf(stdout, "Read N Files:"); 
				fprintf(stdout, "Read file: %s\n", file->filename); 
				fprintf(stdout, "%ld bytes read\n\n", file->size);
			}

			if(dirname)
			{
				//questa parte funziona
				char* p;
				p = strrchr(file->filename, '/'); //ATTENTION306
				++p;
				//printf("name: %s\n", p);
				if((WriteFilefromByte(p, file->filecontents, file->size, dirname)) == -1) 
				{
					errno = -1;
					perror("ERROR: writefb");
					return -1;
				}
				//fine parte che funziona
			}
		}	
	} else return -1;
	
	return result;
}

int writeFile(const char* pathname, const char* dirname) 
{
	// fprintf(stderr, "dentro writeFile API\n");

	msg* write_file = safe_malloc(sizeof(msg));
	write_file->op_type = 4;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;
	
	strncpy(write_file->filename, pathname, name_lenght);

	write_file->filename[name_lenght] = '\0';
	write_file->namelenght = name_lenght;

	//reading file
	long file_lenght;
	char* buf;
	if((buf = readFileBytes(pathname, &file_lenght)) == NULL) 
	{ 
		errno = -1;
		perror("ERROR: readFileBytes");
		return -1;
	}

	memcpy(write_file->filecontents, buf, file_lenght);
	write_file->size = file_lenght;

	//passing the pid of the calling process
	write_file->pid = getpid();
	//fprintf(stderr, "%d\n", write_file->pid);
	//fprintf(stderr, "%d\n", getppid());

	//fprintf(stderr, "sending\n");
	//sending
	if(writen(sockfd, write_file, sizeof(msg)) <= 0) 
	{
		errno = -1;
		perror("ERROR: write");
		return -1;
	}

	//recieving outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read recieving response");
		return -1;
	}

	if(print_coms)
	{
		print_op(response);
	}
	
	if(response != SRV_OK) return -1;
	if(response == SRV_OK) 
	{
		if(print_coms) fprintf(stdout, "%ld bytes written\n\n", file_lenght);
	}
	//are there expelled files to recieve?
	int exp_recieve = 0;

	if (readn(sockfd, &exp_recieve, sizeof(int)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read recieving expelled files");
		return -1;
	}

	
	while (exp_recieve > 0)
	{
		msg* cur_msg = safe_malloc(sizeof(msg));

		if (readn(sockfd, cur_msg, sizeof(msg)) <= 0) 
		{
			errno = -1; 
			perror("ERROR: read recieving expelled");
			return -1;
		}

		
		if(dirname[0] != 0)
		{
			//getting only the file name
			char* p;
			p = strrchr(cur_msg->filename, '/'); //ATTENTION306
			++p;
			//wrting the file
			if((WriteFilefromByte(p, cur_msg->filecontents, cur_msg->size, dirname)) == -1) 
			{
				errno = -1;
				perror("ERROR: writefb");
				return -1;
			}
		}

		free(cur_msg);

		exp_recieve--;
	}

	return 0;
}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) 
{
	//fprintf(stderr, "dentro appendToFile API\n");
	//fprintf(stderr, "%s\n", (char*)buf);

	msg* append_file = safe_malloc(sizeof(msg));
	append_file->op_type = 5;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;
	
	strncpy(append_file->filename, pathname, name_lenght);
	append_file->filename[name_lenght] = '\0';

	//copying append in msg contents and size
	append_file->size = size;
	memcpy(append_file->filecontents, buf, size);

	//passing the process' pid
	append_file->pid = getpid();

	if(writen(sockfd, append_file, sizeof(msg)) <= 0) 
	{
		errno = -1;
		perror("ERROR: append API");
		exit(EXIT_FAILURE);
	} 
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read appendToFile");
		exit(EXIT_FAILURE);
	}

	if(print_coms)
	{
		fprintf(stdout,"Append File:\n");
		print_op(response);
	}


	if(response != SRV_OK) return -1;

	//are there expelled files to recieve?
	int exp_recieve = 0;

	if (readn(sockfd, &exp_recieve, sizeof(int)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read recieving expelled files");
		return -1;
	}

	//fprintf(stderr, "exp:%d\n", exp_recieve);

	while (exp_recieve > 0)
	{
		msg* cur_msg = safe_malloc(sizeof(msg));

		if (readn(sockfd, cur_msg, sizeof(msg)) <= 0) 
		{
			errno = -1; 
			perror("ERROR: read recieving expelled");
			return -1;
		}


		if(dirname)
		{
			//fprintf(stderr, "filename:%s\n", cur_msg->filename);
			char* p;
			p = strrchr(cur_msg->filename, '/'); //ATTENTION306
			++p;
		
			if((WriteFilefromByte(p, cur_msg->filecontents, cur_msg->size, dirname)) == -1) 
			{
				errno = -1;
				perror("ERROR: writefb");
				return -1;
			}
		}

		free(cur_msg);

		exp_recieve--;
	}

	return 0;
}

int lockFile(const char* pathname)
{
	//fprintf(stderr, "dentro lockFile API\n");

	msg* lock_file = safe_malloc(sizeof(msg));
	lock_file->op_type = 14;
	
	errno = 0;
	
	//copying pathname
	int name_lenght = strlen(pathname)+1;

	strncpy(lock_file->filename, pathname, name_lenght);

	lock_file->filename[name_lenght] = '\0';
	lock_file->namelenght = name_lenght;
	lock_file->size = 0;

	//passing the process' pid
	lock_file->pid = getpid();
	
	if(writen(sockfd, lock_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		exit(EXIT_FAILURE);
	}

	//recivieng outcome of operation
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read response lockFile");
		exit(EXIT_FAILURE);
	}

	if(print_coms)
	{
		fprintf(stdout,"Lock File:\n");
		print_op(response);
	}

	if(response == SRV_OK) return 0;

	return -1;
}

int unlockFile(const char* pathname) 
{
	//fprintf(stderr, "dentro unlockFile API\n");

	msg* unlock_file = safe_malloc(sizeof(msg));
	unlock_file->op_type = 15;
	
	errno = 0;
	
	//copying pathname
	int name_lenght = strlen(pathname)+1;

	strncpy(unlock_file->filename, pathname, name_lenght);

	unlock_file->filename[name_lenght] = '\0';
	unlock_file->namelenght = name_lenght;
	unlock_file->size = 0;
	
	unlock_file->pid = getpid();

	if(writen(sockfd, unlock_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		exit(EXIT_FAILURE);
	}

	//recivieng outcome of operation
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read unlockFile");
		exit(EXIT_FAILURE);
	}

	if(print_coms)
	{
		fprintf(stdout,"Unlock File:\n");
		print_op(response);
	}

	if(response == SRV_OK) return 0;

	return -1;
}

int closeFile(const char* pathname) 
{
	//fprintf(stderr, "dentro closeFile API\n");

	msg* close_file = safe_malloc(sizeof(msg));
	close_file->op_type = 1;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;

	strncpy(close_file->filename, pathname, name_lenght);

	close_file->filename[name_lenght] = '\0';
	close_file->namelenght = name_lenght;
	close_file->size = 0;

	close_file->pid = getpid();
	
	if(writen(sockfd, close_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		exit(EXIT_FAILURE);
	}

	//recivieng outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read closeFile");
		exit(EXIT_FAILURE);
	}

	if(print_coms)
	{
		fprintf(stdout,"Close File:\n");
		print_op(response);
	}

	if(response == SRV_OK) return 0;

	return -1;
}

int removeFile(const char* pathname) {

	//fprintf(stderr, "dentro removeFile API\n");

	msg* remove_file = safe_malloc(sizeof(msg));
	remove_file->op_type = 6;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;
	strncpy(remove_file->filename, pathname, name_lenght);
	remove_file->filename[name_lenght] = '\0';

	remove_file->pid = getpid();
	
	if(writen(sockfd, remove_file, sizeof(msg)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: write1");
		exit(EXIT_FAILURE);
	}  //sending operation type
	
	//recivieng outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read response removeFile");
		exit(EXIT_FAILURE);
	}

	if(print_coms)
	{
		fprintf(stdout,"Remove File:\n");
		print_op(response);
	}

	if(response == SRV_OK) return 0;

	return -1;
}


