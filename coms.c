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

//EXTRA FUNCTIONS
char* readFileBytes(const char *name, long* filelen) 
{
	fprintf(stderr, "dentro readbyte\n");
	FILE *file = NULL;

	if ((file = fopen(name, "rb")) == NULL) 
	{
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

//ATTENTION CORRUPT FILE
int WriteFilefromByte(const char* name, char* text, long size, const char* dirname) 
{
	FILE *fp1;
	char fullpath[MAX_SIZE];

	char* truename = strrchr(name, '/');
	truename++;
	
	sprintf(fullpath,"%s/%s", dirname, truename);

	if((fp1 = fopen(fullpath, "wb")) == NULL) return -1;

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

	msg* close_connection = safe_malloc(sizeof(msg));
	close_connection->op_type = 20;

	if(writen(sockfd, close_connection, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		return -1;
	}

	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

	if(response == SRV_OK) return 0;

	close(sockfd);
	return 0;
}

/** TESTING

int FileSend(const char* pathname) 
{
	fprintf(stderr, "Sending file\n");

	msg* fileC = safe_malloc(sizeof(msg));
	errno = 0;

	//copying pathname
	int nameL = strlen(pathname)+1;

	fprintf(stderr, "%d\n", nameL);

	strncpy(fileC->filename, pathname, nameL);
	fileC->filename[nameL] = '\0';

	fprintf(stderr, "%s\n", fileC->filename);

	fileC->namelenght = nameL;
	fileC->op_type = 0;
	fileC->size = 0;

	fprintf(stderr, "prima write\n");
	if(writen(sockfd, fileC, sizeof(msg)) <= 0) return -1;
	
	//recivieng outcome of operation
	int buflen;
	fprintf(stderr, "read\n");
	if(readn(sockfd, &buflen, sizeof(int)) <= 0) return -1;
	char* buf = NULL;
	if((buf = malloc((buflen)*sizeof(char))) == NULL) return -1; 
	if (readn(sockfd, buf, (buflen)*sizeof(char)) <= 0)	return -1;
	buf[buflen] = '\0';
	
	fprintf(stderr, "buf2: %s\n", buf);
	
	if(buf) free(buf);
	return 0;
	
}
*/

/*
 * Richiede di apertura o di creazione di un file. 
 * I flag possono essere 0 = NONE, 1 = O_CREATE, 2 = O_LOCK, 3 = O_CREATE && O_LOCK
 */
int openFile(const char* pathname, int flags)
{
	fprintf(stderr, "dentro openFile API\n");

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
	fprintf(stderr, "%d\n", open_file->pid);
	fprintf(stderr, "%d\n", getppid());

	open_file->flag = flags;
	
	
	if(writen(sockfd, open_file, sizeof(msg)) <= 0)
	{
		errno = -1;
		perror("ERROR: write openFile");
		return -1;
	}
	
	//recivieng outcome of operation

	//int count = 0;
	//while(1) count ++;

	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

		print_op(response);

	if(response == SRV_READY_FOR_WRITE)	return 1;

	if(response == SRV_OK) return 0;

	return -1;
}


/*
 * Legge tutto il contenuto del file server (se esiste) ritornando un puntatore ad un'area allocatata sullo heap nel parametro 'buf', 
 * mentre 'size' conterrà la dimensione del buffer dati (ossia la dimensione in bytes del file letto). In caso di errore, 'buf' e 
 * 'size' non sono validi. Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene viene settato opportunamente.
 */

int readFile(const char* pathname, void** buf, size_t* size)
{
	fprintf(stderr, "dentro readFile API\n");

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
		perror("ERROR: read2");
		return -1;
	}

	print_op(response);

	//if there were no errors on the server's side
	if(response == SRV_OK) 
	{
		if(readn(sockfd, size, sizeof(size_t)) <= 0) 
		{ 
			errno = -1; 
			perror("ERROR: read1");
			return -1;
		}  //reciveing file len

		fprintf(stderr, "Recived size: %zu\n", *size);
		tmp_buf = safe_malloc((*size)*sizeof(char));

		if(readn(sockfd, tmp_buf, (*size)*sizeof(char)) <= 0) 
		{ 
			errno = -1;
			perror("ERROR: read2");
			return -1;
		}  //reciveing byte file

		fprintf(stderr, "Recived text: %s\n", tmp_buf);
		fprintf(stderr, "Stored at %p\n", (void*)tmp_buf);

		*buf = (void*)tmp_buf;

	} else return -1;

	return 0;
}

/*
 * Richiede al server la lettura di ‘N’ files qualsiasi da memorizzare nella directory ‘dirname’ lato client.
 * Se il server ha meno di ‘N’ file disponibili, li invia tutti. Se N <= 0 la richiesta al server è quella di
 * leggere tutti i file memorizzati al suo interno. Ritorna un valore maggiore o uguale a 0 in caso di
 * successo (cioè ritorna il n. di file effettivamente letti), -1 in caso di fallimento, errno viene settato opportunamente.
 */


int readNfiles(int N, const char* dirname) 
{
	msg* readN_file = safe_malloc(sizeof(msg));
	readN_file->op_type = 3;
	readN_file->namelenght = N; //using namelenght field to store N
	errno = 0;

	fprintf(stderr, "dentro readNFile API\n");

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
		perror("ERROR: read2");
		return -1;
	}

	fprintf(stderr, "Files Read: %d\n", result);

	if(result > 0) {
		for(int i = 0; i < result; i++)
		{		
			msg file;
			int ret = 0;
			if((ret = readn(sockfd, &file, sizeof(msg))) <= 0) 
			{
				errno = -1;
				perror("ERROR: read1");
				return -1;
			} else fprintf(stderr, "Read: %d\n", ret);

			fprintf(stderr, "File Name: %s\n", file.filename); 

			//questa parte funziona
			char* p;
			p = strrchr(file.filename, '/'); //ATTENTION306
			++p;
			printf("name: %s\n", p);
			if((WriteFilefromByte(p, file.filecontents, file.size, dirname)) == -1) 
			{
				errno = -1;
				perror("ERROR: writefb");
				return -1;
			}
			//fine parte che funziona
		}	
	} else return -1;
	
	return 0;
}

/*
 * Scrive tutto il file puntato da pathname nel file server. Ritorna successo solo se la precedente operazione,
 * terminata con successo, è stata openFile. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito dal 
 * server perchè espulso dalla cache per far posto al file ‘pathname’ dovrà essere  scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int writeFile(const char* pathname, const char* dirname) 
{
	fprintf(stderr, "dentro writeFile API\n");

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
	char* buf = NULL;
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

	fprintf(stderr, "sending\n");
	//sending
	if(writen(sockfd, write_file, sizeof(msg)) <= 0) 
	{
		errno = -1;
		perror("ERROR: write");
		return -1;
	}

	//are there expelled files to recieve?

	int exp_recieve;

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

		if(dirname)
		{
			char* p;
			p = strrchr(cur_msg->filename, '/'); //ATTENTION306
			++p;
			printf("name: %s\n", p);
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
	
	//recieving outcome of operation
	fprintf(stderr, "response\n");
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read recieving response");
		return -1;
	}

	print_op(response);
	fprintf(stderr, "recieved\n");
	if(response == SRV_OK) return 0;

	return -1;
}

/*
 * Richiesta di scrivere in append al file ‘pathname‘ i ‘size‘ bytes contenuti nel buffer ‘buf’. L’operazione di append
 * nel file è garantita essere atomica dal file server. Se ‘dirname’ è diverso da NULL, il file eventualmente spedito
 * dal server perchè espulso dalla cache per far posto ai nuovi dati di ‘pathname’ dovrà essere scritto in ‘dirname’;
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname) 
{
	fprintf(stderr, "dentro appendToFile API\n");
	fprintf(stderr, "%s\n", (char*)buf);

	msg* append_file = safe_malloc(sizeof(msg));
	append_file->op_type = 5;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;
	
	strncpy(append_file->filename, pathname, name_lenght);
	append_file->filename[name_lenght] = '\0';

	//copying append in msg contents and size
	append_file->size = size;
	memcpy(append_file->filecontents, buf, size);;

	//passing the process' pid
	append_file->pid = getpid();

	if(writen(sockfd, append_file, sizeof(msg)) <= 0) 
	{
		errno = -1;
		perror("ERROR: write1");
		return -1;
	} 

	fprintf(stderr, "recivieng result\n");
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

	if(response == SRV_OK) return 0;

	return -1;
}

/*
*In caso di successo setta il flag O_LOCK al file. Se il file era stato aperto/creato con il flag O_LOCK e la
*richiesta proviene dallo stesso processo, oppure se il file non ha il flag O_LOCK settato, l’operazione termina
*immediatamente con successo, altrimenti l’operazione non viene completata fino a quando il flag O_LOCK non
*viene resettato dal detentore della lock. L’ordine di acquisizione della lock sul file non è specificato. Ritorna 0 in
*caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
*/
int lockFile(const char* pathname)
{
	fprintf(stderr, "dentro lockFile API\n");

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
		return -1;
	}

	//recivieng outcome of operation
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

	print_op(response);

	if(response == SRV_OK) return 0;

	return -1;
}

/*
* Resetta il flag O_LOCK sul file ‘pathname’. L’operazione ha successo solo se l’owner della lock è il processo che
* ha richiesto l’operazione, altrimenti l’operazione termina con errore. Ritorna 0 in caso di successo, -1 in caso di
* fallimento, errno viene settato opportunamente.
*/

int unlockFile(const char* pathname) 
{
	fprintf(stderr, "dentro unlockFile API\n");

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
		return -1;
	}

	//recivieng outcome of operation
	
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

	print_op(response);

	if(response == SRV_OK) return 0;

	return -1;
}


/*
 * Richiesta di chiusura del file puntato da ‘pathname’. Eventuali operazioni sul file dopo la closeFile falliscono.
 * Ritorna 0 in caso di successo, -1 in caso di fallimento, errno viene settato opportunamente.
 */
int closeFile(const char* pathname) 
{
	fprintf(stderr, "dentro closeFile API\n");

	msg* close_file = safe_malloc(sizeof(msg));
	close_file->op_type = 1;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;

	strncpy(close_file->filename, pathname, name_lenght);

	close_file->filename[name_lenght] = '\0';
	close_file->namelenght = name_lenght;
	close_file->size = 0;
	
	if(writen(sockfd, close_file, sizeof(msg)) <= 0)
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
		perror("ERROR: read2");
		return -1;
	}

	print_op(response);

	if(response == SRV_OK) return 0;

	return -1;
}

/*
 * Rimuove il file cancellandolo dal file storage server.
 */
int removeFile(const char* pathname) {

	fprintf(stderr, "dentro removeFile API\n");

	msg* remove_file = safe_malloc(sizeof(msg));
	remove_file->op_type = 6;

	errno = 0;

	//copying pathname
	int name_lenght = strlen(pathname)+1;
	strncpy(remove_file->filename, pathname, name_lenght);
	remove_file->filename[name_lenght] = '\0';
	
	if(writen(sockfd, remove_file, sizeof(msg)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: write1");
		return -1;
	}  //sending operation type
	
	//recivieng outcome of operation
	op response;

	if (readn(sockfd, &response, sizeof(op)) <= 0) 
	{
		errno = -1; 
		perror("ERROR: read2");
		return -1;
	}

	print_op(response);

	if(response == SRV_OK) return 0;

	return -1;
}


