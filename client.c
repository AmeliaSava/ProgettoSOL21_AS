#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <time.h>
#include <ops.h>
#include <coms.h>
#include <conn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define SOCK_NAME "./storage_sock"

int print = 1;
char* SOCKNAME = NULL;
char* SAVE_DIR = NULL;
long sleeptime = 0;

//flag
int isW = 0;
int isR = 0;

void print_h() {
	printf("usage: ./client [option]\n");
	printf("options: \n");
	printf("-h -> prints helper\n");
	printf("-f filename -> used to specify the name of the socketfile\n");
	printf("-w dirname[,n = 0]-> sends request for writing the files in the directory -dirname- in the file storage.\n");
	printf("						if n is specified, sends n files. If n = 0 or not specified there's not limit to the number of sent files\n");
	printf("-W file1[ file2] -> list of file pathnames to write in the server, speparated by commas.\n");
	printf("-D dirname -> folder where the files expelled because of capacity misses will be saved in case of ‘-w’ e ‘-W’.\n");
	printf("So it must be used with those options. If it is not specified all the files will be discarded");
	printf("-r file1[, file2]-> list of file pathnames to read from the file storage, speparated by commas.\n");
	printf("-R [n = 0] -> allows to read n files from the file storage, if n = 0 or not specified all the files are read\n");
	printf("-d dirname -> memory folder where the file read with -r and -R are written. Must be used in addiction to those.\n");
	printf("				If it is not specified, the read files will not be memorized\n");
	printf("-t -> time in milliseconds that passes between sending a request from the previous to the server\n");
	printf("		if not specified, t is assumed set to 0\n");
	printf("-c file1[, file2]-> list of files to remove from the storage, if present.\n");
	printf("-l file1[,file2] -> lista di nomi di file su cui acquisire la mutua esclusione;\n");
	printf("-u file1[,file2] -> lista di nomi di file su cui rilasciare la mutua esclusione;\n");
	printf("-p -> enables prints throughout the code\n");
}

int add_current_folder(char** pathname, char* name)
{
	char* folder = NULL;
	if((folder = malloc((strlen(name) + 2)*sizeof(char))) == NULL) return -1;
	if((*pathname = malloc(2*sizeof(char))) == NULL) return -1;
	strncpy(*pathname, "./", 3);
	strncpy(folder, name, strlen(name));
	strncat(*pathname, folder, strlen(folder));
	free(folder);
	return 0;
}

int isdot (const char dir[])
{
	int l = strlen(dir);
	if((l > 0 && dir[l - 1] == '.')) return 1;
	return 0;
}

// function to obtain absolute pathname of a file
char* cwd() 
{
	char* buf = safe_malloc(MAX_SIZE*sizeof(char));

	if(getcwd(buf, MAX_SIZE) == NULL) 
	{
		perror("cwd during getcwd");
		free(buf);
		return NULL; 
	}
	
	return buf;
}

// function that reads n or all files in a given directory
// returns 0 if it was not able to enter the directory
// -1 error
// 1 success
int write_from_dir_find (const char* dir, long* n) 
{

	//enetering directory
	if(chdir(dir) == -1) 
	{ 
		print_error("error entering directory %s\n", dir); 
		return 0; 
	}

	DIR *d;

	//opening directory
	if((d = opendir(".")) == NULL) 
	{ 
		print_error("error entering directory %s\n", dir);
		return -1;
	} 
	else {

		struct dirent *file;

		// reading all files
		// setto errno prima di chiamare readdir, per discriminare i due casi in cui ritorna NULL
		// 1. se c'è un errore, e in quel caso setta errno
		// 2. se è arrivato alla fine della dir e non c'è più niente da leggere
		while((errno = 0, file = readdir(d)) != NULL) {

			struct stat statb;

			//getting stats
			if(stat(file->d_name, &statb) == -1) 
			{ 
				print_error("Error during stat of %s\n", file->d_name);
				return -1;
			}

			//if a directory is found...
			if(S_ISDIR(statb.st_mode)) {
				//...ed ho escluso il caso che sia la stessa directory in cui sono adesso o la directory padre...
				if(!isdot(file->d_name)) {
					//...chiamo la funzione ricorsivamente
					if(write_from_dir_find(file->d_name, n) != 0) {
						// quando ho finito risalgo nella directory precedente
						// WARNING: dato che stat segue i link simbolici ritornando gli attributi
						// del file puntato (e non del link) se un link simbolico punta ad una
						// directory entrero' in quella directory pero' poi salire di livello
						// con .. non va bene perche' non ritornero' nella parent directory.
						// per diminuire la complessità dell'esercizio non si controlla questa cosa    
						if (chdir("..") == -1) 
						{
							print_error("Impossibile risalire alla directory padre.\n");
							return -1;
						}
					}
				}
			// the file is not a directory
			} else {
					if(*n == 0) return 1;
					char* buf = cwd();
					buf = strncat(buf, "/", 2);
					buf = strncat(buf, file->d_name, strlen(file->d_name));
					if (buf == NULL) return -1;

					//printf("n: %ld", *n);
					//printf("%s\n", buf);

					
					if((openFile(buf, 1)) == 1) 
					{ 
						printf("%s\n", buf);
						if((writeFile(buf, NULL)) == 0) 
						{
							*n = *n - 1;
						} 
						//else return 1;
						
					}
					//else return 1;

					//*n = *n - 1;

					free(buf);
	
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

	while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d::t:l:u:c:p")) != -1) { 
		switch(opt) { 
            case 'h': {
            	print_h();
				break;
            }
            case 'f': 
			{
				if(add_current_folder(&SOCKNAME, optarg) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				if(print) printf("Socket file pathname: %s\n", SOCKNAME);
				if((clock_gettime(CLOCK_REALTIME, &abstime)) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				abstime.tv_sec += 2;
            	if((openConnection(SOCKNAME, 1000, abstime)) == -1){errno = ECONNREFUSED; perror("openConnection"); exit(EXIT_FAILURE);}
            	else fprintf(stderr, "Connected\n");
            	break;
            }
            case 'w': 
			{
				isW = 1;
            	int i = 0;
            	long n = -1;
            	char* dirname = NULL;
				char* token;
            	token = strtok(optarg,",");
            	while(token != NULL) {
            		if(i==0) {
            			if(add_current_folder(&dirname, token) == -1) 
						{
							errno = -1; 
							perror("ERROR: -w"); 
							exit(EXIT_FAILURE);
						}
						i++;
            		}
            		if(i==1) isNumber(token, &n);
            		token = strtok(NULL, ",");
            	}
				printf("n: %ld\n", n);
				write_from_dir_find(dirname, &n);
                break;
            }
            case 'W': 
			{ 
				isW = 1;
            	optind--;
            	for(; optind < argc && *argv[optind] != '-'; optind++)
				{
					int ret;
            		if((ret = openFile(argv[optind], 1)) == 1) 
					{
						if((ret = writeFile(argv[optind], NULL)) != 0)
							exit(EXIT_FAILURE); //in realtà bisognerebbe usare ret per l'errore
					}
					else continue;
                }
                break;
            }
			case 'D':
			{
				if(!isW)
				{
					printf("-D option must be used with -w or -W options preceeding it.");
					break;
				}
				isW = 0;
				break;
			}
            case 'r': {

				optind--;

				for(; optind<argc && *argv[optind] != '-'; optind++)
				{
					void* buf = NULL;
                	size_t sz;
                	int r = readFile(argv[optind], &buf, &sz);
                	if(!r) printf("Buf: %p\nSize: %zu\n", buf, sz); // qualcosa non va nel puntatore
				}

                break;
            }
			case 'R': {
				char *tmp_optarg = optarg;
				long r = 0;
				// if `optarg` isn't set and argv[optind] isn't another option,
				// then it's the arg and overtly modify optind to compensate.
				if( !optarg && NULL != argv[optind] && '-' != argv[optind][0] ) {
					tmp_optarg = argv[optind++];
				}
				// if the argument is a number it will be assigned to r
				// else nothing happens and r stays 0, as if no argument was given
				if (tmp_optarg) isNumber(tmp_optarg, &r);

				//is SAVE_DIR was not specified before NULL wil be passed
				readNfiles(r, SAVE_DIR);

                break;
            }
			case 'd': {

				if(!isR)
				{
					printf("-D option must be used with -w or -W options preceeding it.");
					break;
				}
				isR = 0;
				//optarg
                break;
            }
			case 't': {
                
				if((isNumber(optarg, &sleeptime)) == 1)
				{
					printf("option %s is not a number\n", optarg);
					return EXIT_FAILURE;
				}
				
                break;
            }
			case 'l':
			{
				optind--;

				for(; optind<argc && *argv[optind] != '-'; optind++)
				{
					lockFile(argv[optind]);
					printf("filename: %s\n", argv[optind]);
				}				
				break;
			}
			case 'u':
			{
				optind--;

				for(; optind<argc && *argv[optind] != '-'; optind++)
				{
					unlockFile(argv[optind]);
					printf("filename: %s\n", argv[optind]);
				}

				break;
			}
			case 'c': 
			{
				optind--;

				for(; optind<argc && *argv[optind] != '-'; optind++)
				{
					removeFile(argv[optind]);
					printf("filename: %s\n", argv[optind]);
				}
	 
                break;
            }
			case 'p': 
			{
                printf("prints activated\n");
                print = 1;
                break;
            }
            case ':': 
			{
                printf("option needs a value\n"); 
                break;
			}
			case '?': 
			{
				printf("unknown option: %c\n", optopt);
				break; 
			}
        }

		usleep(sleeptime*1000);
    }

	//after completing all request the connection is closed
	closeConnection(SOCKNAME);

	return 0;
}
