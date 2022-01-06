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

typedef struct ARGS 
{
	struct ARGS* next;
	int opt;
	char* args;
} cmd_args;

int print = 1;
char* SOCKNAME = NULL;
long sleeptime = 0;

cmd_args* opt_args = NULL;

char* expelled_dir;
char* save_dir;

void print_h()
{
	printf("usage: ./client [option]\n");
	printf("options: \n");
	printf("-h -> prints helper\n");
	printf("-f filename -> used to specify the name of the socketfile\n");
	printf("-w dirname[,n = 0]-> sends request for writing the files in the directory -dirname- in the file storage.\n");
	printf("						if n is specified, sends n files. If n = 0 or not specified there's not limit to the number of sent files\n");
	printf("-W file1[,file2] -> list of file pathnames to write in the server, speparated by commas.\n");
	printf("-D dirname -> folder where the files expelled because of capacity misses will be saved in case of ‘-w’ e ‘-W’.\n");
	printf("So it must be used with those options. If it is not specified all the files will be discarded");
	printf("-r file1[,file2]-> list of file pathnames to read from the file storage, speparated by commas.\n");
	printf("-R [n = 0] -> allows to read n files from the file storage, if n = 0 or not specified all the files are read\n");
	printf("-d dirname -> memory folder where the file read with -r and -R are written. Must be used in addiction to those.\n");
	printf("				If it is not specified, the read files will not be memorized\n");
	printf("-t -> time in milliseconds that passes between sending a request from the previous to the server\n");
	printf("		if not specified, t is assumed set to 0\n");
	printf("-c file1[, file2]-> list of files to remove from the storage, if present.\n");
	printf("-l file1[,file2] -> list of files to lock\n");
	printf("-u file1[,file2] -> list of files to unlock\n");
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
					fprintf(stderr,"buf before open:%s\n", buf);

					
					if((openFile(buf, 1)) == 1) 
					{ 
						fprintf(stderr,"after open:%s\n", buf);
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

void arg_ins(int opt, char* args)
{
	fprintf(stderr,"inserito1\n");
	cmd_args* current = opt_args;

	while (current->next != NULL)
	{
		current = current->next;
	}
	
	cmd_args* new_arg;
	new_arg = safe_malloc(sizeof(cmd_args));

	new_arg->opt = opt;
	new_arg->args = safe_malloc(strlen(args));
	strncpy((new_arg->args), args, strlen(args));

	current->next = new_arg;
	new_arg->next = NULL;

	return;
}

/*
Returns 0 on success, -1 otherwise
*/

int commandline_serve()
{
	cmd_args* ptr = opt_args;
	cmd_args* next;

	while(ptr != NULL) 
	{
		//fprintf(stderr,"giro %d\n", ptr->opt);
		
		switch(ptr->opt)
		{
			case 0:
			{
				//we don't talk about this part of the code
				//this shouldn't happen but it does
				//so we just ignore it
				//and pretend it doesn't exists
				next = ptr->next;
				free(ptr);
				ptr = next;
				break;
			}
			case 1: //-w
			{
				int i = 0;
				long n = -1;
				char* dirname;
				char* token;
				token = strtok(ptr->args,",");
				
				while(token != NULL) 
				{
					//fprintf(stderr,"tok:%s\n", token);

					if(i==0) 
					{
						dirname = safe_malloc(strlen(token));
						strncpy(dirname, token, strlen(token));
						//fprintf(stderr,"dn:%s\n", dirname);
						i++;
					}

					if(i==1) isNumber(token, &n);

					token = strtok(NULL, ",");
				}

				//printf("tn: %ld\n", n);
				write_from_dir_find(dirname, &n);

				next = ptr->next;
				free(ptr);
				ptr = next;
				
				break;
			}
			case 2: //-W
			{
				char* token;
				char* buf;
				token = strtok(ptr->args,",");

            	while(token != NULL)
				{	
					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						buf = cwd();
						buf = strncat(buf, "/", 2);
						buf = strncat(buf, token, strlen(token));
					}
					
					int ret;
            		if((ret = openFile(buf, 1)) == 1) //ATTENTION errori?
					{
						//if expelled_dir was earlier defined it is used, otherwise it remains NULL
						if((ret = writeFile(buf, expelled_dir)) != 0)
							return -1;
					}
					else 
						token = strtok(NULL, ",");
					token = strtok(NULL, ",");
                }

				next = ptr->next;
				free(ptr);
				ptr = next;

				break;
			}
			case 3: //-D
			{
				//the element was already check and saved
				//just taking it out of the list

				next = ptr->next;
				free(ptr);
				ptr = next;

				break;
			}
			case 4: //-r
			{
				char* token;
				token = strtok(ptr->args,",");

				while(token != NULL)
				{
					void* buf = NULL;
					char* dirbuf;
					
                	size_t sz;

					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						dirbuf = cwd();
						dirbuf = strncat(dirbuf, "/", 2);
						dirbuf = strncat(dirbuf, token, strlen(token));
					}

                	int r = readFile(dirbuf, &buf, &sz);

					char* file = buf;

                	if(!r) printf("Buf: %p\nSize: %zu\nFile:%s\n", buf, sz, file);
					else return -1;

					if(save_dir != NULL)
					{
						if((WriteFilefromByte(token, file, sz, save_dir)) == -1) 
							return -1;
					}

					token = strtok(NULL, ",");
				}

				next = ptr->next;
				free(ptr);
				ptr = next;

				break;

			}
			case 5: //-R
			{
				long n;
				n = strtol(ptr->args, NULL, 10);

				//is save_dir was not specified before NULL wil be passed
				readNfiles(n, save_dir);

				break;
			}
			case 6: //-d
			{
				//the element was already check and saved
				//just taking it out of the list

				next = ptr->next;
				free(ptr);
				ptr = next;

				break;
			}
			case 7: //-l
			{
				char* token;
				token = strtok(ptr->args,",");
				char* dirbuf;

				while(token != NULL)
				{
					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						dirbuf = cwd();
						dirbuf = strncat(dirbuf, "/", 2);
						dirbuf = strncat(dirbuf, token, strlen(token));
					}

					lockFile(dirbuf);

					printf("Locked file: %s\n", dirbuf);

					token = strtok(NULL, ",");
				}

				next = ptr->next;
				free(ptr);
				ptr = next;	

				break;
			}
			case 8: //-u
			{
				char* token;
				token = strtok(ptr->args,",");
				char* dirbuf;

				while(token != NULL)
				{
					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						dirbuf = cwd();
						dirbuf = strncat(dirbuf, "/", 2);
						dirbuf = strncat(dirbuf, token, strlen(token));
					}

					unlockFile(dirbuf);

					printf("Unlocked file: %s\n", dirbuf);

					token = strtok(NULL, ",");
				}

				next = ptr->next;
				free(ptr);
				ptr = next;	

				break;
			}
			case 9: //-c
			{
				char* token;
				token = strtok(ptr->args,",");
				char* dirbuf;

				while(token != NULL)
				{
					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						dirbuf = cwd();
						dirbuf = strncat(dirbuf, "/", 2);
						dirbuf = strncat(dirbuf, token, strlen(token));
					}

					removeFile(dirbuf);
					printf("Deleted file: %s\n", dirbuf);

					token = strtok(NULL, ",");
				}

				next = ptr->next;
				free(ptr);
				ptr = next;

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
	}
	return 0;
}

int commandline_check()
{
	int isW = 0;
	int isR = 0;
	int isd = 0;
	int isD = 0;

	cmd_args* ptr = opt_args;
	char* dir1;
	char* dir2;

	while(ptr != NULL) 
	{
		if(ptr->opt == 1 || ptr->opt == 2)
			isW = 1;

		if(ptr->opt == 4 || ptr->opt == 5)
			isR = 1;

		if(ptr->opt == 3)
		{
			isD = 1;
			dir1 = safe_malloc(strlen(ptr->args));
			strncpy(dir1, ptr->args, strlen(ptr->args));
		}

		if(ptr->opt == 6)
		{
			isd = 1;
			dir2 = safe_malloc(strlen(ptr->args));
			strncpy(dir2, ptr->args, strlen(ptr->args));
		}

		ptr = ptr->next;
	}

	if(!isW && isD)
	{
		fprintf(stderr,"-D option must be used with -w or -W\n");
		//resetting string if it was set without the other options
		return -1;
	}

	if(!isR && isd)
	{
		fprintf(stderr,"-d option must be used with -r or -R\n");
		//resetting string if it was set without the other options
		return -1;
	}

	if(isD)
	{
		expelled_dir = safe_malloc(strlen(dir1));
		strncpy(expelled_dir, dir1, strlen(dir1));

		fprintf(stderr,"Expelled files will be saved into:\n%s\n", expelled_dir);
	}
			
	if(isd)
	{
		save_dir = safe_malloc(strlen(dir2));
		strncpy(save_dir, dir2, strlen(dir2));

		fprintf(stderr,"Read files will be saved into:\n%s\n", save_dir);
	}

	return 0;
}

int main(int argc, char *argv[]) 
{

	struct timespec abstime;
	int opt;

	opt_args = safe_malloc(sizeof(opt_args));


	while((opt = getopt(argc, argv, "hf:w:W:D:r:R::d:t:l:u:c:p")) != -1) { 
		switch(opt) { 
            case 'h': 
			{
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
            	arg_ins(1, optarg);
				
                break;
            }
            case 'W': 
			{ 
				arg_ins(2, optarg);
            	
                break;
            }
			case 'D':
			{
				arg_ins(3, optarg);
				break;
			}
            case 'r': 
			{
				arg_ins(4, optarg);
                break;
            }
			case 'R': 
			{
				
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

				char* cur_arg = safe_malloc(sizeof(long));
				sprintf(cur_arg, "%ld", r);

				fprintf(stderr, "%s\n", cur_arg);
				arg_ins(5, cur_arg);

                break;
            }
			case 'd': 
			{
				arg_ins(6, optarg);
                break;
            }
			case 't': 
			{
				if((isNumber(optarg, &sleeptime)) == 1)
				{
					printf("option %s is not a number\n", optarg);
					return EXIT_FAILURE;
				}
				
                break;
            }
			case 'l':
			{
				arg_ins(7, optarg);				
				break;
			}
			case 'u':
			{
				arg_ins(8, optarg);
				break;
			}
			case 'c': 
			{
				arg_ins(9, optarg);
                break;
            }
			case 'p': 
			{
                printf("Prints activated\n");
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

	if(commandline_check() != 0)
	{
		errno = -1;
		perror("ERROR: Wrong command line usage");
		exit(EXIT_FAILURE);
	}
	
	if(commandline_serve() != 0)
	{
		errno = -1;
		perror("ERROR: Unable to serve command line requests");
		exit(EXIT_FAILURE);
	}

	//after completing all request the connection is closed
	closeConnection(SOCKNAME);
	
	return 0;
}
