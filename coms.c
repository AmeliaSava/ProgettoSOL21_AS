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

#include <ops.h>
#include <coms.h>
#include <conn.h>

struct sockaddr_un serv_addr;
int sockfd;

//EXTRA FUNCTIONS
char* readFileBytes(const char *name, long* filelen) {
	fprintf(stderr, "dentro readbyte\n");
	FILE *file = NULL;

	if ((file = fopen(name, "rb")) == NULL) {
		perror("ERROR: opening file");
		fclose(file);
		exit(EXIT_FAILURE);
	} 
	//go to end of file
	if(fseek(file, 0, SEEK_END) == -1) { perror("ERROR: fseek"); fclose(file);
		exit(EXIT_FAILURE);
	} 

	long len = ftell(file); //current byte offset
	*filelen = len; //assigning the lenght to external value

	char* ret = NULL; //return string
	if((ret = malloc(len * sizeof(char))) == NULL) { perror("ERROR: malloc"); fclose(file);	free(ret);
		exit(EXIT_FAILURE);
	}
	if(fseek(file, 0, SEEK_SET) == -1) { perror("ERROR: fseek"); fclose(file); free(ret);
		exit(EXIT_FAILURE);
	} 
	fread(ret, 1, len, file); //ERROR CHECK
	/*La funzione fread() ritorna il numero di elementi letti.
	In caso di errore o di raggiungimento della fine file viene restituito un numero minore di nmemb.
	E' necessario chiamare le funzioni feof() e/o ferror() in caso venga restituito un numero minore di nmemb*/
	fclose(file);
	return ret;
}
//API
/*
 * Apre una connesione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la richiesta di connessione,
 * la connessione da parte del client ripetur dopo 'msec' millisecondi e fino allo scadere del tempo assoluto 'abstime' specificato
 * come terzo argomento. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int openConnection(const char* sockname/*, int msec, const struct timespace abstime*/) {
	
	SYSCALL_EXIT("socket", sockfd, socket(AF_UNIX, SOCK_STREAM, 0), "socket", "");
	 
	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sun_family = AF_UNIX;    
	strncpy(serv_addr.sun_path, sockname, strlen(sockname)+1);
	int result;
	SYSCALL_EXIT("connect", result, connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)), "connect", "");

	return result;
}

/*
 * chiude la connessione AF_UNIX associata al socket file sockname. Ritorna 0 in caso di successo, -1 in caso di fallimento, 
 * errno viene settato opportunamente.
 */
int closeConnection(const char* sockname) {
	close(sockfd);
	return 0;
}

/*
 * Richiede di apertura o di creazione di un file. 
 * Si assume che il flag sia sempre O_CREATE 
 */
int openFile(const char* pathname, int flags) {
	op open_file = 0;
	fprintf(stderr, "dentro openFile\n");
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf); perror("ERROR: malloc"); errno = ENOMEM;
		return -1;
	} 
	strncpy(namebuf, pathname, nameL);
	long fileL;
	char* filebuf = NULL;
	if((filebuf = readFileBytes(pathname, &fileL)) == NULL) { free(namebuf); free(filebuf);	perror("ERROR: readFileBytes");
		errno = -1;
		return -1;
	}

	if(writen(sockfd, &open_file, sizeof(open_file)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write1");
		errno = -1;
		return -1;
	}  //sending operation type

	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write2");
		errno = -1;
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write3");
		errno = -1;
		return -1;
	}  //sending name
	if(writen(sockfd, &fileL, sizeof(long)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write4");
		errno = -1;
		return -1;
	}  //sending name len
	if(writen(sockfd, filebuf, fileL*sizeof(char)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write5");
		errno = -1;
		return -1;
	}  //sending byte file

	//recivieng outcome of operation
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: read1");
		errno = -1;
		return -1;
	}
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(filebuf); free(buf); perror("ERROR: malloc");
		errno = ENOMEM;
		return -1;
	} 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(filebuf); free(buf); perror("ERROR: read2");
		errno = -1;
		return -1;
	} 
	buf[buflen] = '\0';
	printf("result: %s\n", buf);
	if(namebuf) free(namebuf);
	if(filebuf) free(filebuf);
	if(buf) free(buf);
	return 0;
}

/*
 * Legge tutto il contenuto del file server (se esiste) ritornando un puntatore ad un'area allocatata sullo heap nel parametro 'buf', 
 * mentre 'size' conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In caso di errore, 'buf' e 
 * 'size' non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene viene settato opportunamente.
 */
int readFile(const char* pathname, void** buf, size_t* size) {
	op read_file = 2;
	fprintf(stderr, "dentro readFile\n");

	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf); perror("ERROR: malloc"); errno = ENOMEM;
		return -1;
	} 
	strncpy(namebuf, pathname, nameL);

	if(writen(sockfd, &read_file, sizeof(read_file)) <= 0) {free(namebuf); perror("ERROR: write1");
		errno = -1;
		return -1;
	}  //sending operation type
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); perror("ERROR: write2");
		errno = -1;
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); perror("ERROR: write3");
		errno = -1;
		return -1;
	}  //sending name

	long fileL;

	if(readn(sockfd, &fileL, sizeof(long)) <= 0) {free(namebuf); perror("ERROR: read1");
		errno = -1;
		return -1;
	}  //reciveing file len
	*size = fileL;
	char* filebuf = NULL;
	if((filebuf = malloc(fileL*sizeof(char))) == NULL) { free(namebuf); free(filebuf);	perror("ERROR: malloc");
		errno = -1;
		return -1;
	}
	if(readn(sockfd, filebuf, 2048*sizeof(char)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: read2");
		errno = -1;
		return -1;
	}  //reciveing byte file
	//putting the file in the buf
	strncpy(*buf, filebuf, fileL);
	//size = fileL;
	//recivieng outcome of operation
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); free(filebuf);	perror("ERROR: write");
		errno = -1;
		return -1;
	}
	char* rbuf = NULL;
	if((rbuf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(filebuf); free(buf); perror("ERROR: malloc");
		errno = ENOMEM;
		return -1;
	} 
	if (readn(sockfd, rbuf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(filebuf); free(buf); perror("ERROR: read");
		errno = -1;
		return -1;
	} 
	rbuf[buflen] = '\0';
	printf("result: %s\n", rbuf);
	//da problemi, perchè????
	/*if(namebuf) free(namebuf);
	if(filebuf) free(filebuf);
	if(buf) free(buf);*/
	return 0;
}

/*
 * Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client.
 * Se il server ha meno di ‘N’ file disponibili, li invia tutti. Se N <= 0 la richiesta al server è quella di
 * leggere tutti i file memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di
 * successo (cioè ritorna il n. di file effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.
 */
int readNfiles(int N, const char* dirname);

/*
 * Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
 * terminata con successo, è stata openFile. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal 
 * server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere  scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int writeFile(const char* pathname, const char* dirname);

/*
 * Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
 * nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname);

/*
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int closeFile(const char* pathname);

/*
 * Rimuove il file cancellandolo dal file storage server.
 */
int removeFile(const char* pathname);


