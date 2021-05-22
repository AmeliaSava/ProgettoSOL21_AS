#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <util.h>
#include <conn.h>


int main(int argc, char *argv[]) {
  if (argc == 1) {
		fprintf(stderr, "usa: %s stringa [stringa]\n", argv[0]);
		exit(EXIT_FAILURE);
  }
  struct sockaddr_un serv_addr;
  int sockfd;
	
  SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
	 
	memset(&serv_addr, '0', sizeof(serv_addr));

  serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path,SOCKNAME, strlen(SOCKNAME)+1);
	int notused;
	
	SYSCALL_EXIT("connect", notused, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");
    
  char *buffer=NULL;
  for(int i=1; i<argc;++i) {

	int n=strlen(argv[i])+1;
	/*
	 *  NOTA: provare ad utilizzare writev (man 2 writev) per fare un'unica SC
	 */
	SYSCALL_EXIT("writen", notused, writen(sockfd, &n, sizeof(int)), "write", "");
	SYSCALL_EXIT("writen", notused, writen(sockfd, argv[i], n*sizeof(char)), "write", "");

	buffer = realloc(buffer, n*sizeof(char));
	if (!buffer) {
   perror("realloc");
   fprintf(stderr, "Memoria esaurita....\n");
   break;
	}
	/*
	 *  NOTA: provare ad utilizzare readv (man 2 readv) per fare un'unica SC
	 */
////////da problemi la read n
	SYSCALL_EXIT("readn", notused, readn(sockfd, &n, sizeof(int)), "read","");
	SYSCALL_EXIT("readn", notused, readn(sockfd, buffer, n*sizeof(char)), "read","");
	buffer[n] = '\0';
	printf("result: %s\n", buffer);
    }
  close(sockfd);
  if (buffer) free(buffer);
return 0;
}
