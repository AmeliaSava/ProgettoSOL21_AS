#define _POSIX_C_SOURCE 199309L
#define _BSD_SOURCE
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
#include <message.h>

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

int WriteFilefromByte(char* pathname, char* text, long size) {
	FILE *fp1;
	if((fp1 = fopen(pathname, "w")) == NULL) return -1;

	if((fwrite(text, sizeof(char), size, fp1)) != size) return -1;

	fclose(fp1);
	return 0;
}
//API
/*
 * Apre una connesione AF_UNIX al socket file sockname. Se il server non accetta immediatamente la richiesta di connessione,
 * la connessione da parte del client ripetur dopo 'msec' millisecondi e fino allo scadere del tempo assoluto 'abstime' specificato
 * come terzo argomento. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int openConnection(const char* sockname, int msec, const struct timespec abstime) {

	errno = 0;
	int result = 0;
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
 * No O_LOCK implementation, flag can be 0 = NONE, 1 = O_CREATE, 2 = O_LOCK
 */
int openFile(const char* pathname, int flags) {
	fprintf(stderr, "dentro openFile API\n");

	op open_file = 0;
	errno = 0;
	//copying pathname
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if ((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf);  errno = ENOMEM; perror("ERROR: malloc openFile"); return -1;}
	strncpy(namebuf, pathname, nameL);
	namebuf[nameL] = '\0';
	
	// send: operation type, name lenght, name
	fprintf(stderr, "prima write\n");
	if(writen(sockfd, &open_file, sizeof(open_file)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write1");
		return -1;
	}  //sending operation type
		fprintf(stderr, "seconda write\n");
	if(writen(sockfd, &flags, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write openFile");
		return -1;
	}  //sending flag
	fprintf(stderr, "3 write\n");
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write2");
		return -1;
	} //sending name len
	fprintf(stderr, "4 write\n");
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write3");
		return -1;
	}  //sending name
	
	//recivieng outcome of operation
	int buflen;
	fprintf(stderr, "read\n");
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: read1");
		return -1;
	}
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(buf); errno = ENOMEM; perror("ERROR: malloc");
		return -1;
	} 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(buf); errno = -1; perror("ERROR: read2");
		return -1;
	}
	buf[buflen] = '\0';
	if(strncmp(buf, "Ready for write...", buflen) == 0) {
		fprintf(stderr, "buf: %s\n", buf);
		return 1;
	}
	fprintf(stderr, "buf2: %s\n", buf);
	if(namebuf) free(namebuf);
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
	errno = 0;
	fprintf(stderr, "dentro readFile API\n");
	//copying pathname in buffer
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if((namebuf = malloc(nameL * sizeof(char))) == NULL) { errno = ENOMEM; perror("ERROR: malloc"); free(namebuf);
		return -1;
	} 
	strncpy(namebuf, pathname, nameL);

	//sending operation type
	if(writen(sockfd, &read_file, sizeof(read_file)) <= 0) {errno = -1; perror("ERROR: write1"); free(namebuf); return -1;}
	//sending name len
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {errno = -1; perror("ERROR: write2"); free(namebuf); return -1;} 
	//sending name
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {errno = -1; perror("ERROR: write3"); free(namebuf); return -1;}

	//recivieng outcome of operation
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) { errno = -1; free(namebuf); perror("ERROR: readb"); return -1;}
	char* rbuf = NULL;
	if((rbuf = malloc((buflen)*sizeof(char))) == NULL) { errno = ENOMEM; free(namebuf); free(buf); perror("ERROR: malloc"); return -1;} 
	if (readn(sockfd, rbuf, (buflen)*sizeof(char)) <= 0) { free(namebuf); free(buf); errno = -1; perror("ERROR: read"); return -1;} 
	rbuf[buflen] = '\0';

	printf("result: %s\n", rbuf);

	if(strncmp(rbuf, "Operation completed successfully", buflen) == 0) {
		//recivieng data
		long fileL;

		if(readn(sockfd, &fileL, sizeof(long)) <= 0) { errno = -1; perror("ERROR: read1"); free(rbuf); free(namebuf);
			return -1;
		}  //reciveing file len
		*size = fileL;
		char* filebuf = NULL;
		if((filebuf = malloc(fileL*sizeof(char))) == NULL) { errno = ENOMEM; free(namebuf); free(rbuf); perror("ERROR: malloc");
			return -1;
		}
		if(readn(sockfd, filebuf, fileL*sizeof(char)) <= 0) { errno = -1; free(namebuf); free(filebuf);	perror("ERROR: read2");
			return -1;
		}  //reciveing byte file

		//putting the file in the buf
		*buf = filebuf;
		*size = fileL;
		if(filebuf) free(filebuf);
	} else return -1;

	if(namebuf) free(namebuf);
	if(rbuf) free(rbuf);
	return 0;
}

/*
 * Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client.
 * Se il server ha meno di ‘N’ file disponibili, li invia tutti. Se N <= 0 la richiesta al server è quella di
 * leggere tutti i file memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di
 * successo (cioè ritorna il n. di file effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.
 */
int readNfiles(int N, const char* dirname) {
	/*op readN_file = 3;
	errno = 0;
	fprintf(stderr, "dentro readNFile API\n");

	//sending operation type
	if(writen(sockfd, &readN_file, sizeof(readN_file)) <= 0) {errno = -1; perror("ERROR: write1"); free(namebuf); return -1;}
	//sending name len
	if(writen(sockfd, &N, sizeof(int)) <= 0) {errno = -1; perror("ERROR: write2"); free(namebuf); return -1;} 
	
	//recivieng outcome of operation
	int result;
	if(readn(sockfd, &result, sizeof(int)) <= 0) { errno = -1; free(namebuf); perror("ERROR: readb"); return -1;}
	
	printf("result: %d\n", result);

	if(result > 0) {
		for(int i = 0; i < result; i++){
			//recivieng data
			long fileL;
			if(readn(sockfd, &fileL, sizeof(long)) <= 0) { errno = -1; perror("ERROR: read1"); free(rbuf); free(namebuf);
				return -1;
			}  //reciveing file len
			if(readn(sockfd, filebuf, fileL*sizeof(char)) <= 0) { errno = -1; free(namebuf); free(filebuf);	perror("ERROR: read2");
				return -1;
			}  //reciveing byte file
			if(readn(sockfd, &nameL, sizeof(int)) <= 0) {errno = -1; perror("ERROR: read3"); free(namebuf); return -1;} 
			//sending name
			if(readn(sockfd, namebuf, nameL*sizeof(char)) <= 0) {errno = -1; perror("ERROR: read4"); free(namebuf); return -1;}

		//questa parte funziona
		char* p;
		p = strrchr(namebuf, '/');
		++p;
		printf("name: %s\n", p);
		if((WriteFilefromByte(p, filebuf, fileL)) == -1) { errno = -1; free(namebuf); free(filebuf); perror("ERROR: writefb");
			return -1;
		}
		//fine parte che funziona

			printf("File Name: %s\nFile Text:%s\n", namebuf, filebuf);
			if(filebuf) free(filebuf);
			if(namebuf) free(namebuf);		
		}
		
	
	} else return -1; */
	return 0;
}

/*
 * Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
 * terminata con successo, è stata openFile. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal 
 * server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere  scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int writeFile(const char* pathname, const char* dirname) {
	fprintf(stderr, "dentro writeFile API\n");
	op write_file = 4;
	errno = 0;
	//copying pathname
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if ((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf);  errno = ENOMEM; perror("ERROR: malloc openFile"); return -1;}
	strncpy(namebuf, pathname, nameL);
	namebuf[nameL] = '\0';
	//reading file
	long fileL;
	char* filebuf = NULL;
	if((filebuf = readFileBytes(pathname, &fileL)) == NULL) { free(namebuf); free(filebuf);	errno = -1; perror("ERROR: readFileBytes");
		return -1;
	}
	//sending
	fprintf(stderr, "sending\n");
	if(writen(sockfd, &write_file, sizeof(write_file)) <= 0) {free(namebuf); free(filebuf); errno = -1; perror("ERROR: write1");
		return -1;
	}  //sending operation type
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); free(filebuf);	errno = -1; perror("ERROR: write2");
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); free(filebuf);	errno = -1; perror("ERROR: write3");
		return -1;
	}  //sending name
	if(writen(sockfd, &fileL, sizeof(long)) <= 0) {free(namebuf); free(filebuf); errno = -1; perror("ERROR: write4");
		return -1;
	}  //sending file len
	if(writen(sockfd, filebuf, fileL*sizeof(char)) <= 0) {free(namebuf); free(filebuf);	errno = -1; perror("ERROR: write5");
		return -1;
	}  //sending byte file

	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); free(filebuf);	errno = -1; perror("ERROR: read1");
		return -1;
	}
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(filebuf); free(buf); errno = ENOMEM; perror("ERROR: malloc");
		return -1;
	} 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(filebuf); free(buf); errno = -1; perror("ERROR: read2");
		return -1;
	}
	buf[buflen] = '\0';
	fprintf(stderr, "%s\n", buf);
	if(filebuf) free(filebuf);
	if(buf) free(buf);
	return 0;
}

/*
 * Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
 * nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) {
	fprintf(stderr, "dentro appendToFile API\n");
	fprintf(stderr, "%s\n", (char*)buf);
	op append_file = 5;
	errno = 0;
	//copying pathname
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if ((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf);  errno = ENOMEM; perror("ERROR: malloc openFile"); return -1;}
	strncpy(namebuf, pathname, nameL);
	namebuf[nameL] = '\0';

	if(writen(sockfd, &append_file, sizeof(append_file)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write1");
		return -1;
	}  //sending operation type
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf);errno = -1; perror("ERROR: write2");
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write3");
		return -1;
	}  //sending name
	if(writen(sockfd, &size, sizeof(size_t)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write4");
		return -1;
	}  //sending file len
	if(writen(sockfd, buf, size*sizeof(void*)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write5");
		return -1;
	}  //sending byte file
	fprintf(stderr, "recivieng result\n");
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: read1");
		return -1;
	}
	char* rbuf = NULL;
	if((rbuf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(rbuf); errno = ENOMEM; perror("ERROR: malloc");
		return -1;
	} 
	if (readn(sockfd, rbuf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(rbuf); errno = -1; perror("ERROR: read2");
		return -1;
	}
	rbuf[buflen] = '\0';
	fprintf(stderr, "%s\n", rbuf);
	if(rbuf) free(rbuf);
	fprintf(stderr, "fine append\n");
	return 0;
}

/*
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int closeFile(const char* pathname) {
	fprintf(stderr, "dentro closeFile API\n");

	op close_file = 1;
	errno = 0;
	//copying pathname
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if ((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf);  errno = ENOMEM; perror("ERROR: malloc openFile"); return -1;}
	strncpy(namebuf, pathname, nameL);
	namebuf[nameL] = '\0';
	
	// send: operation type, name lenght, name
	if(writen(sockfd, &close_file, sizeof(close_file)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write1");
		return -1;
	}  //sending operation type
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write2");
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write3");
		return -1;
	}  //sending name
	
	//recivieng outcome of operation
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: read1");
		return -1;
	}
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(buf); errno = ENOMEM; perror("ERROR: malloc");
		return -1;
	} 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(buf); errno = -1; perror("ERROR: read2");
		return -1;
	}
	buf[buflen] = '\0';

	fprintf(stderr, "%s\n", buf);
	if(namebuf) free(namebuf);
	if(buf) free(buf);
	return 0;
}

/*
 * Rimuove il file cancellandolo dal file storage server.
 */
int removeFile(const char* pathname) {

	fprintf(stderr, "dentro removeFile API\n");

	op remove_file = 6;
	errno = 0;
	//copying pathname
	int nameL = strlen(pathname)+1;
	char* namebuf = NULL;
	if ((namebuf = malloc(nameL * sizeof(char))) == NULL) { free(namebuf);  errno = ENOMEM; perror("ERROR: malloc openFile"); return -1;}
	strncpy(namebuf, pathname, nameL);
	namebuf[nameL] = '\0';
	
	// send: operation type, name lenght, name
	if(writen(sockfd, &remove_file, sizeof(remove_file)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write1");
		return -1;
	}  //sending operation type
	if(writen(sockfd, &nameL, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write2");
		return -1;
	} //sending name len
	if(writen(sockfd, namebuf, nameL*sizeof(char)) <= 0) {free(namebuf); errno = -1; perror("ERROR: write3");
		return -1;
	}  //sending name
	
	//recivieng outcome of operation
	int buflen;
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) {free(namebuf); errno = -1; perror("ERROR: read1");
		return -1;
	}
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) {free(namebuf); free(buf); errno = ENOMEM; perror("ERROR: malloc");
		return -1;
	} 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0) {free(namebuf); free(buf); errno = -1; perror("ERROR: read2");
		return -1;
	}
	buf[buflen] = '\0';
	if(strncmp(buf, "Ready for write...", buflen) == 0) {
		fprintf(stderr, "%s\n", buf);
		return 1;
	}
	fprintf(stderr, "%s\n", buf);
	if(namebuf) free(namebuf);
	if(buf) free(buf);
	return 0;
}

