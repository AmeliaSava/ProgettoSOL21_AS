#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include <ops.h>
#include <coms.h>
#include <conn.h>
#include <message.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define SOCK_NAME "./storage_sock"

int print = 1;
char* SOCKNAME = NULL;

void print_h() {
	printf("usage: ./client [option]\n");
	printf("options: \n");
	printf("-h -> prints helper\n");
	printf("-f filename -> used to specify the name of the socketfile\n");
	printf("-w dirname[, n = 0]-> sends request for writing the files in the directory -dirname- in the file storage.\n");
	printf("						if n is specified, sends n files. If n = 0 or not specified there's not limit to the number of sent files\n");
	printf("-W file1[, file2] -> list of file pathnames to write in the server, speparated by commas.\n");
	printf("-r file1[, file2]-> list of file pathnames to read from the file storage, speparated by commas.\n");
	printf("-R [n = 0] -> allows to read n files from the file storage, if n = 0 or not specified all the files are read\n");
	printf("-d dirname -> memory folder where the file read with -r and -R are written. Must be used in addiction to those.\n");
	printf("				If it is not specified, the read files will not be memorized\n");
	printf("-t -> time in milliseconds that passes between sending a request from the previous to the server\n");
	printf("		if not specified, t is assumed set to 0\n");
	printf("-c file1[, file2]-> list of files to remove from the storage, if present.\n");
	printf("-p -> enables prints throughout the code\n");
}

int add_current_folder(char** pathname, char* name) {
	char* folder = NULL;
	if((folder = malloc((strlen(name) + 2)*sizeof(char))) == NULL) return -1;
	if((*pathname = malloc(2*sizeof(char))) == NULL) return -1;
	strncpy(*pathname, "./", 3);
	strncpy(folder, name, strlen(name));
	strncat(*pathname, folder, strlen(folder));
	free(folder);
	return 0;
}

int isdot (const char dir[]){
	int l = strlen(dir);
	if((l > 0 && dir[l - 1] == '.')) return 1;
	return 0;
}

// function to obtain absolute pathname of a file
char* cwd() {

	char* buf = malloc(MAX_SIZE*sizeof(char));
	if(!buf) { perror("cwd malloc"); return NULL;}

	if(getcwd(buf, MAX_SIZE) == NULL) {	perror("cwd during getcwd"); free(buf);	return NULL; }
	
	return buf;
}

// function that finds a file or reads all the files in a given directory if the second parameter is NULL
int write_from_dir_find (const char* dir, const char *infile, long n) {

	if(chdir(dir) == -1) { print_error("error entering directory %s\n", dir); return 0; }

	DIR *d;
	// apro la directory
	if((d = opendir(".")) == NULL) { perror("opening cwd in findfile");	return -1;
	} else { // non c'è stato errore
		struct dirent *file;

		// leggo tutti i file
		// setto errno prima di chiamare readdir, per discriminare i due casi in cui ritorna NULL
		// 1. se c'è un errore, e in quel caso setta errno
		// 2. se è arrivato alla fine della dir e non c'è più niente da leggere
		while((errno = 0, file = readdir(d)) != NULL) {
			struct stat statb;
			// prendo le statistiche
			if(stat(file->d_name, &statb) == -1) {
				perror("stat");
				print_error("Error during stat of %s\n", file->d_name);
				return -1;
			}
			//Se ho trovato una directory...
			if(S_ISDIR(statb.st_mode)) {
				//...ed ho escluso il caso che sia la stessa directory in cui sono adesso o la directory padre...
				if(!isdot(file->d_name)) {
					//...chiamo la funzione ricorsivamente
					if(write_from_dir_find(file->d_name, infile, n) != 0) {
						// quando ho finito risalgo nella directory precedente
						// WARNING: dato che stat segue i link simbolici ritornando gli attributi
						// del file puntato (e non del link) se un link simbolico punta ad una
						// directory entrero' in quella directory pero' poi salire di livello
						// con .. non va bene perche' non ritornero' nella parent directory.
						// per diminuire la complessità dell'esercizio non si controlla questa cosa    
						if (chdir("..") == -1) {
							print_error("Impossibile risalire alla directory padre.\n");
							return -1;
						}
					}
				}
			// caso in cui il file non è una directory
			} else {
				if(infile) {
					if (strncmp((file->d_name), infile, strlen(infile)) == 0) {
						// per trovare il path assoluto del file utilizzo la funzione cwd
						char* buf = cwd();
						if (buf == NULL) return -1;
						// stampo il path, il nome del file e la sua data di modifica convertita da ctime
						int ret;
						if((ret = openFile(buf, 1)) == 1) writeFile(buf, NULL);
							else return ret;
						free(buf);
					}
				} else {
					char* buf = cwd();
					if (buf == NULL) return -1;
					// stampo il path, il nome del file e la sua data di modifica convertita da ctime
					int ret;
					if((ret = openFile(buf, 1)) == 1) { if(writeFile(buf, NULL) == 0) n--;}
						else return ret;
					free(buf);
				}
			}
		}
		// c'è stato un errore e stampo
		if (errno != 0) perror("readdir");
		closedir(d);
	}

	return 1;
}

int main(int argc, char *argv[]) {

	struct timespec abstime;
	int opt;

	while((opt = getopt(argc, argv, "hf:w:W:r:R::d::t:c:p")) != -1) { 
		switch(opt) { 
            case 'h': {
            	print_h();
				break;
            }
            case 'f': {
				if(add_current_folder(&SOCKNAME, optarg) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				if(print) printf("Socket file pathname: %s\n", SOCKNAME);
				if((clock_gettime(CLOCK_REALTIME, &abstime)) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				abstime.tv_sec += 2;
            	if((openConnection(SOCKNAME, 1000, abstime)) == -1){errno = ECONNREFUSED; perror("openConnection"); exit(EXIT_FAILURE);}
            	else fprintf(stderr, "Connected\n");
            	break;
            }
            case 'w': {
            	int i = 0;
            	long n = -1;
            	char* dirname = NULL;
				char* token;
            	token = strtok(optarg,",");
            	while(token != NULL) {
            		if(i==0) {
            			if(add_current_folder(&dirname, token) == -1) {errno = -1; perror("ERROR: -w"); exit(EXIT_FAILURE);}
            			i++;
            		}
            		if(i==1) isNumber(token, &n);
            		token = strtok(NULL, ",");
            	}
				write_from_dir_find(dirname, NULL, n);
                break;
            }
            case 'W': { 
            	optind--;
            	for(; optind<argc && *argv[optind] != '-'; optind++) {
            		//write_from_dir_find(".", argv[optind], 0);
            		openFile(argv[optind], 1);
                	printf("filename: %s\n", argv[optind]); 
                }
                break;
            }
            case 'r': {
            	char* token;
            	token = strtok(optarg,",");
            	while(token != NULL) {
            		openFile(token,1);
            		printf("filename: %s\n", token);
            		token = strtok(NULL, ",");
            	}
            	                	 
                //for(int i = 0; i < 1; i++){
                	//void* buf = NULL;
                	//size_t sz;
                	//int r = readFile(nome3, &buf, &sz);
                	//if(!r) printf("Buf: %p\nSize: %zu\n", buf, sz); // qualcosa non va nel puntatore
                //}
                break;
            }
			case 'R': {
				//readNfiles(0, NULL);
				FileSend("./storage/test.txt");
                break;
            }
			case 'd': {
                printf("option: %s\n", optarg);
                char* file = "./storage/test.txt";
                openFile(file, 1);
                break;
            }
			case 't': {
                printf("option: %s\n", optarg); 
                break;
            }
			case 'c': {
            	optind--;
				/*
            	for(; optind<argc && *argv[optind] != '-'; optind++)
            		removeFile(argv[optind]);
                	printf("filename: %s\n", argv[optind]); 
					*/
                break;
            }
			case 'p': {
                printf("prints activated\n");
                print = 1;
                break;
            }
            case ':': {
                printf("option needs a value\n"); 
                break;
			}
			case '?': {
				printf("unknown option: %c\n", optopt);
				break; 
			}
        } 
    }

	closeConnection(SOCKNAME);

	return 0;
}
