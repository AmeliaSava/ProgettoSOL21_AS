#define _POSIX_C_SOURCE  200112L
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

#include <ops.h> //provvisorio
#include <conn.h>
#include <coms.h>
#include <treeLFU.h>
//altri include

#define CONFIG_FL "./config.txt"
#define MAX_BUF 2048

//funzioni
//provvisorio test server
typedef struct msg {
		int lenN;
		long size;
		op op_type; 
		char* nome;
		char* testo;
} msg;

// Global Variables
long NUM_THREAD_WORKERS;
long MAX_MEMORY_MB;
char* SOCKET_NAME = NULL;

//global root of tree
NodeFile cacheMemory = {MAX_INT, "median", "fixed_tree_root", 0, 0, NULL, NULL};

//makes sure the socket name is unlinked
void unlinksock() {
    unlink(SOCKET_NAME);
}

//WRITE NOT WORKING
int reportOps(long connfd, op op_type) {
		fprintf(stderr, "dentro return\n");

	char* buf = NULL;

	switch(op_type) {
		case SRV_OK: {
			int l = strlen("Operation completed successfully") + 1;
			buf = malloc(l*sizeof(char));
			strncpy(buf, "Operation completed successfully", l);
			if (writen(connfd, &l, sizeof(int))<=0) { perror("ERROR: writeok"); free(buf); return -1;}
			if (writen(connfd, buf, l*sizeof(char))<=0) { perror("ERROR: writeok"); free(buf); return -1;}
			break;
		}
		case SRV_NOK: {
			int l = strlen("Error while executing operation") + 1;
			buf = malloc(l*sizeof(char));
			strncpy(buf, "Error while executing operation", l);
			if (writen(connfd, &l, sizeof(int))<=0) { free(buf); return -1;}
			if (writen(connfd, buf, MAX_BUF*sizeof(char))<=0) { free(buf); return -1;}
			free(buf);
			return -1;
		}
		case SRV_FILE_NOT_FOUND: {
			strncpy(buf, "File not found\0", MAX_BUF);
			if (writen(connfd, buf, MAX_BUF*sizeof(char))<=0) { free(buf); return -1;}
			free(buf);
			return -1;
		}
		case SRV_FILE_ALREADY_PRESENT: {
			strncpy(buf, "File already present\0", MAX_BUF);
			if (writen(connfd, buf, MAX_BUF*sizeof(char))<=0) { free(buf); return -1;}
			free(buf);
			return -1;
		}
		case SRV_MEM_FULL: {
			strncpy(buf, "Memory Full\0", MAX_BUF);
			if (writen(connfd, buf, MAX_BUF*sizeof(char))<=0) { free(buf); return -1;}
			free(buf);
			return -1;
		}
		default: {
			fprintf(stderr, "command not found\n");
			free(buf);
			return -1;
		}
	}
	free(buf);
	return 0;
}

int open_file_svr(long connfd, int len, char* name, char* file){
	fprintf(stderr, "dentro open\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) UpdateStat(current); // open file
		else return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // file already exists and its open
	}
	fprintf(stderr, "primadipush\n");
	PushNode(&cacheMemory, 0, name, file, 0, len); //creating empty file
	fprintf(stderr, "dopodipush\n");
	return reportOps(connfd, SRV_OK);
}

int close_file_svr(long connfd, int len, char* name, char* file){
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 0) {
			increaseF (current);
			UpdateStat(current); // close file
		}	
		else return reportOps(connfd, SRV_FILE_ALREADY_PRESENT); // file already closed
	} 
	return reportOps(connfd, SRV_OK);
}

int read_file_svr(long connfd, int len, char* name){
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file found
		if(current->status == 1) {perror("ERROR: file closed"); return -1; } // file is closed, cannot read
		else { // file already exists and its open
			 increaseF (current);
			 if(writen(connfd, &current->FileSize, sizeof(long)) <= 0) { perror("ERROR: write");
				errno = -1;
				return -1;
			} //sending name len
			if(writen(connfd, current->textFile, 2048*sizeof(char)) <= 0) { perror("ERROR: write2");
				errno = -1;
				return -1;
			}  //sending name
			return reportOps(connfd, SRV_OK);
		} 
	} 
	perror("ERROR: file not found"); return -1; //file not found
}

int read_n_file_svr(long connfd, int len, char* name, char* file) {
	return 0;
}

int write_file_svr(long connfd, int len, char* name, char* file){
	fprintf(stderr, "dentro write\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) return reportOps(connfd, SRV_NOK); // file is closed
		else  UpdateNode(current, current->frequency + 1, name, file, len); //writing on the node
	} 
	return reportOps(connfd, SRV_OK);
}

int append_file_svr(long connfd, int len, char* name, char* file) {
	fprintf(stderr, "dentro append\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) return reportOps(connfd, SRV_NOK); // file is closed
		else  UpdateNode(current, current->frequency + 1, name, file, len); //must make append
	} 
	return reportOps(connfd, SRV_OK);
}

int remove_file_svr(long connfd, int len, char* name, char* file) {
	fprintf(stderr, "dentro remove\n");
	NodeFile* current = NULL;
	if((current = searchNode(&cacheMemory, name)) != NULL) { // file already exists
		if(current->status == 1) return reportOps(connfd, SRV_NOK); // file is closed
		else  RemoveFile(current, name); //writing on the node
	} 
	return reportOps(connfd, SRV_OK);
}

//WIP
int cmd(long connfd, op op_type, msg str) {
	fprintf(stderr, "dentro cmd\n");
	switch(op_type){
		case OPEN_FILE:{
			if (readn(connfd, &str.lenN, sizeof(int))<=0) return -1;
    		str.nome = calloc((str.lenN), sizeof(char));
    		if (readn(connfd, str.nome, str.lenN*sizeof(char))<=0){ free(str.nome); return -1;} 
    		if (readn(connfd, &str.size, sizeof(long))<=0) return -1;
    		str.testo = calloc((str.size), sizeof(char));
    		if (readn(connfd, str.testo, str.size*sizeof(char))<=0) { free(str.nome);  free(str.testo); return -1;}
    
			fprintf(stderr,"nome:%s\ntesto:%s\n", str.nome, str.testo); //perchè non stampa il nome??
			return open_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		}
		case CLOSE_FILE:
			close_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		case READ_FILE:
			if (readn(connfd, &str.lenN, sizeof(int))<=0) return -1;
    		str.nome = calloc((str.lenN), sizeof(char));
    		if (readn(connfd, str.nome, str.lenN*sizeof(char))<=0){ free(str.nome); return -1;} 
			return read_file_svr(connfd, str.size, str.nome);
			break;
		case READ_FILE_N:
			read_n_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		case WRITE_FILE:
			write_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		case APPEND_FILE:
			append_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		case REMOVE_FILE:
			remove_file_svr(connfd, str.size, str.nome, str.testo);
			break;
		default: {
			fprintf(stderr, "command not found\n");
			return -1;
		}
	}
	return 0;
}

int getMSG(long connfd){
	fprintf(stderr, "dentro getmsg\n");
	msg str;

	if (readn(connfd, &str.op_type, sizeof(op))<=0) return -1;
    
 	return (cmd(connfd, str.op_type, str));
}

// returns the max index of fd
int updateMax(fd_set set, int fdmax) {
    for(int i = (fdmax-1); i >= 0; i--)
    	if (FD_ISSET(i, &set)) return i;
    	assert(1 == 0);
    	return -1;
}

//PULISCI MEGLIO QUANDO QUALCOSA CHIUDE CHIUDE TUUTO
void configParsing(long* Nthreads, long* MaxMem, char** sockN) {
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
				if(isNumber(equals,  Nthreads) != 0) {
					fprintf(stderr, "wrong config.txt file format: first line must be number of thread workers\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}			
			case 1:
				if(isNumber(equals, MaxMem) != 0) {
					fprintf(stderr, "wrong config.txt file format: second line must be maximum memory allocable\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
			case 2:
				while((*equals) != '\0' && isspace(*equals)) ++equals;
				if((*equals) == '\0') {
					fprintf(stderr, "wrong config.txt file format: third line must be socket file name\n");
					free(buffer);
					exit(EXIT_FAILURE);
				}
				strncpy(*sockN, equals, MAX_BUF);
		}

		count++;
		
	}

	fclose(config_input);
	free(buffer);
	
}

int main (int argc, char* argv[]) {

	atexit(unlinksock);  
	//atexit(deallocall); funzione che con array/lista delle risorse correntemente in uso chiude in maniera appropiata
	
	//allocating the global socket name
	CHECK_EQ_EXIT((SOCKET_NAME = malloc(MAX_BUF * sizeof(char))), NULL, "ERROR: malloc socket name");
	// parsing the config file
	configParsing(&NUM_THREAD_WORKERS, &MAX_MEMORY_MB, &SOCKET_NAME);

	//Accepting connections
	
    unlinksock();    

	int fd_skt; //connection socket
	int fd_max; //max fd
	int fd_sel; //index to verify select results
	long fd_con; // I/O socket with a client

	fd_set set; //active file descriptor set
	fd_set rdset; //set of fd wating for reading

	SYSCALL_EXIT("socket", fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "ERROR: socket", "");

	//server struct
	struct sockaddr_un serv_addr;
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME)+1);
	
 	//preparing socket
	int result;
    SYSCALL_EXIT("bind", result, bind(fd_skt, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "ERROR: bind", "");
    SYSCALL_EXIT("listen", result, listen(fd_skt, MAXBACKLOG), "ERROR: listen", "");
    
    //GESTIONE THREAD DA IMPLEMENTARE

    //setting the sets
    FD_ZERO(&set);
    FD_ZERO(&rdset);
    FD_SET(fd_skt, &set);

    //keeping max fd index updated
    //if(fd_skt > fd_max) fd_max = fd_skt;
    fd_max = fd_skt;
    //fprintf(stderr, "%d\n", fd_max);

    for(;;) {      
		rdset = set; //saving the set in the temporary one
		//+1 because I need the number of active file descriptors, not the max index
		if (select(fd_max + 1, &rdset, NULL, NULL, NULL) == -1) {
		    perror("ERROR: select");
			return EXIT_FAILURE;
		} else { //select ok
			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++) {
				//accepting new connections
			    if (FD_ISSET(fd_sel, &rdset)) { //is it ready?
					if (fd_sel == fd_skt) { // sock connect ready
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						if(fd_con > fd_max) fd_max = fd_con;  // updating max
						fprintf(stderr, "max:%d\n", fd_max);
						continue; //???
					} 
 
					fd_con = fd_sel;  // new request from already connected client
					fprintf(stderr,"con:%ld\n", fd_con);
					// read msg and executing if there's and error remove from
					// ???
					if (getMSG(fd_con) < 0) {
						close(fd_con); 
						FD_CLR(fd_con, &set); // remove from set
						// if we are deleteting the max make new max
						if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
						fprintf(stderr,"ult:%ld\n", fd_con);
					}
		   		}
			}
		}
    }
    //ogni volta che si chiude la comunicazione va aggiornato il massimo
    //sezione di deallocazione di tutte le risorse
	close(fd_skt);
    free(SOCKET_NAME);


	return EXIT_SUCCESS;
}
