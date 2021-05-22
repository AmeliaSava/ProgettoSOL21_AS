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
//altri include

#define CONFIG_FL "./config.txt"
#define MAX_BUF 1024
//#define MAXBACKLOG 100
//define

//funzioni
//provvisorio test server
typedef struct msg { 
		char *nome; 
		int len;
		char *str;
} msg_t;

long NUM_THREAD_WORKERS;
long MAX_MEMORY_MB;
char* SOCKET_NAME = NULL;

//FUNZIONI TEST
void cleanup() {
    unlink(SOCKET_NAME);
}

// PROVA
void toup(char *str) {
    char *p = str;
    while(*p != '\0') { 
        *p = (islower(*p)?toupper(*p):*p); 
	++p;
    }        
}

//PROVA
int cmd(long connfd) {
    msg_t str;
    if (readn(connfd, &str.len, sizeof(int))<=0) return -1;
    str.str = calloc((str.len), sizeof(char));
    if (readn(connfd, str.str, str.len*sizeof(char))<=0) return -1;
		fprintf(stderr,"lunghezza:%d\n testo:%s\n",str.len,str.str);
    toup(str.str);

    if (writen(connfd, &str.len, sizeof(int))<=0) { free(str.str); return -1;}
    if (writen(connfd, str.str, str.len*sizeof(char))<=0) { free(str.str); return -1;}
    free(str.str);
    return 0;
}
//PROVA
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


	
	CHECK_EQ_EXIT((SOCKET_NAME = malloc(MAX_BUF * sizeof(char))), NULL, "ERROR: malloc");
	// parsing the config file
	configParsing(&NUM_THREAD_WORKERS, &MAX_MEMORY_MB, &SOCKET_NAME);

	//CONNESSIONE
	
    cleanup();    
    atexit(cleanup);  
    
    int fd_skt; //connection socket
    int fd_max = 0; //max fd
    int fd_sel; //index to verify select results
    int fd_con; // I/O socket with a client

    char* buffer;
    CHECK_EQ_EXIT((buffer = malloc(MAX_BUF * sizeof(char))), NULL, "ERROR: malloc buffer");

    fd_set set; //active file descriptor set
    fd_set rdset; //set of fd wating for reading
    int nread; //number of read characters

    SYSCALL_EXIT("socket", fd_skt, socket(AF_UNIX, SOCK_STREAM, 0), "ERROR: socket", "");
    //server struct
    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, SOCKET_NAME, strlen(SOCKET_NAME)+1);
 
	int result;
	//preparing socket
    SYSCALL_EXIT("bind", result, bind(fd_skt, (struct sockaddr*)&serv_addr,sizeof(serv_addr)), "ERROR: bind", "");
    SYSCALL_EXIT("listen", result, listen(fd_skt, MAXBACKLOG), "ERROR: listen", "");
    //keeping max fd index updated
    if(fd_skt > fd_max) fd_max = fd_skt;
    //setting the sets
    FD_ZERO(&set);
    FD_ZERO(&rdset);
    FD_SET(fd_skt, &set);

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
					  SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, 0), "ERROR: accept", "");
					  FD_SET(fd_con, &set);  // adding fd to starting set
					  if(fd_con > fd_max) fd_max = fd_con;  // updating max
					  continue; //???
					} //else sock I/O ready

					//PARTE DA SISTEMARE
					fd_con = fd_sel;  // e' una nuova richiesta da un client già connesso
					fprintf(stderr,"%d\n", fd_con);
					// eseguo il comando e se c'e' un errore lo tolgo dal master set
					if (cmd(fd_con) < 0) { 
						//devo generare i thread in quelche modo
						//pool di thread?


						  close(fd_con); 
						  FD_CLR(fd_con, &set); 
						  // controllo se deve aggiornare il massimo
							//senza questo gli fd risultano sempre in richiesta di attenzione non so perchè
						  if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
					}
		   		}
			}
		}

		// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta

    }
    
    close(fd_skt);

	return EXIT_SUCCESS;
}
