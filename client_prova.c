#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <ops.h>
#include <coms.h>
#include <conn.h>


int main(int argc, char *argv[]) {
	//openConnection(SOCKNAME);
	struct sockaddr_un serv_addr;
	int sockfd;

	SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
	 
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME)+1);
	int result;
	SYSCALL_EXIT("connect", result, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");
	//end openConnection
	char *buffer = NULL;
	buffer = malloc(1024*sizeof(char));
	char* nome;
	char* testo;

	

		nome = malloc(1024*sizeof(char));
		strcpy(nome, "nomefile");
		int n = strlen(nome)+1;
		testo = malloc(1024*sizeof(char));
		strcpy(testo, "01010101010");
		op ops0 = 0;

		SYSCALL_EXIT("writen", result, writen(sockfd, &n, sizeof(int)), "write", "");
		SYSCALL_EXIT("writen", result, writen(sockfd, nome, n*sizeof(char)), "write", "");
		SYSCALL_EXIT("writen", result, writen(sockfd, testo, n*sizeof(char)), "write", "");
		SYSCALL_EXIT("writen", result, writen(sockfd, &ops0, sizeof(op)), "write", "");

		SYSCALL_EXIT("readn", result, readn(sockfd, buffer, n*sizeof(char)), "read","");
		buffer[n] = '\0';
		printf("result: %s\n", buffer);


	if (buffer) free(buffer);
	close(sockfd);
	//closeConnection(SOCKNAME);
return 0;
}
