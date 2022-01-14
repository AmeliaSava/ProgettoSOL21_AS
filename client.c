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
/**
 * \brief: struct that contains command line argument information
 * \param next: the next arguments to process
 * \param opt: the opt type
 * \param args: the argument string
 */
typedef struct ARGS 
{
	struct ARGS* next;
	int opt;
	char* args;
} cmd_args;

//global settings
int print = 0;
char* SOCKNAME = NULL;
long sleeptime = 0;

//where the command line options are saved
cmd_args* opt_args = NULL;

//directories
char* expelled_dir;
char* save_dir;

/**
 * \brief: prints help information, used for -h option
 */
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

/**
 * \brief: adds "./" to a file name
 */ 
int add_current_folder(char** pathname, char* name)
{
	char* folder = safe_malloc(strlen(name) + 1);
	*pathname = safe_malloc(4 + strlen(name));

	strncpy(*pathname, "./", 3);
	strncpy(folder, name, strlen(name) + 1);
	strncat(*pathname, folder, strlen(folder));
	free(folder);
	return 0;
}

/**
 * \brief: checks if the directory is the current one or the parent one
 */ 
int isdot (const char dir[])
{
	int l = strlen(dir);
	if((l > 0 && dir[l - 1] == '.')) return 1;
	return 0;
}

/**
 * \brief: function to obtain absolute pathname of a file
 */ 
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

/**
 * \brief: function that reads n or all files in a given directory and all sub directories
 * \param dir: the directory to explore
 * \param n: pointer to the number of files to read
 * \return: 0 if it was not able to enter the directory
 * \return: -1 error
 * \return: 1 success
 */
int write_from_dir_find (const char* dir, long* n) 
{
	//enetering directory
	if(chdir(dir) == -1) 
	{ 
		//error while entering directory, finished reading all directories 
		return 0; 
	}

	DIR *d;

	//opening directory
	if((d = opendir(".")) == NULL) 
	{ 
		//error while entering directory, finished reading all directories
		return -1;
	} 
	else {

		struct dirent *file;

		// reading all files
		// setting errno before readdir, to distinguish between two case in which it returns NULL
		// 1. an error occurred, set errno
		// 2. end of directory, no more files to read
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
				//...and it's not the same directory or parent directory...
				if(!isdot(file->d_name)) {
					//...call recursively
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
					if(print) fprintf(stderr,"Writing file:%s\n", buf);

					
					if((openFile(buf, 1)) == 1) 
					{ 
						//fprintf(stderr,"after open:%s\n", buf);
						if((writeFile(buf, expelled_dir)) == 0) 
						{
							*n = *n - 1;
						} 
						//else return 1;
						
					}
					//else return 1;

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
	//fprintf(stderr,"inserito1\n");
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

				if(print)
				{
					if(n > 0) fprintf(stdout, "Writing %ld file(s) ", n);
					else fprintf(stdout,"Writing all file(s) ");
					fprintf(stdout,"from folder %s\n", dirname);
				}

				//0 counts as a request of reading all the files
				if(n == 0) n = -1;

				if(write_from_dir_find(dirname, &n) < 0)
					return -1;

				//go to the next request
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
					else
					{
						buf = safe_malloc(strlen(token));
						strncpy(buf, token, strlen(token));
					}
					
					if(print) fprintf(stderr,"Writing file:%s\n", buf);

					int ret;
            		if((ret = openFile(buf, 1)) == 1) //ATTENTION errori?
					{
						//if expelled_dir was earlier defined it is used, otherwise it remains NULL
						if((ret = writeFile(buf, expelled_dir)) != 0)
						{
							fprintf(stderr, "%s", strerror(errno));
							return -1;
						}
							
					}
					else 
						token = strtok(NULL, ","); //if failed to open file go to the next
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
					char* dirbuf = safe_malloc(MAX_SIZE);
					
                	size_t sz;

					//if the path was given with shortened current directory
					if(token[0] == '.' && token[1] == '/')
					{
						token += 2;
						dirbuf = cwd();
						dirbuf = strncat(dirbuf, "/", 2);
						dirbuf = strncat(dirbuf, token, strlen(token));
					}
					else
					{
						dirbuf = safe_malloc(strlen(token));
						strncpy(dirbuf, token, strlen(token));
					}

					if(print) fprintf(stdout, "Reading file: %s\n", dirbuf);
                	int r = readFile(dirbuf, &buf, &sz);

					char* file = buf;

                	if(!r)
					{
						if(print) printf("Bytes read: %zu\n\n", sz);
					}
					else token = strtok(NULL, ",");

					if(save_dir != NULL)
					{
						char* p;
						p = strrchr(token, '/'); //ATTENTION306
						++p;
						//printf("name: %s\n", p);
						if((WriteFilefromByte(p, file, sz, save_dir)) == -1) 
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
				long n = strtol(ptr->args, NULL, 10);
				//fprintf(stdout, "n:%ld\n", n);
				long ret;
				//is save_dir was not specified before NULL wil be passed
				if((ret = readNfiles(n, save_dir)) > 0)
				{
					if(print) fprintf(stdout, "Read %ld files\n\n", ret);
				} else if(print) fprintf(stdout, "Error while reading memory");

				next = ptr->next;
				free(ptr);
				ptr = next;

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
					else
					{
						dirbuf = safe_malloc(strlen(token));
						strncpy(dirbuf, token, strlen(token));
					}

					//fprintf(stdout, "dirbuf: %s\n\n", dirbuf);

					if(lockFile(dirbuf) == 0)
					{
						if(print) fprintf(stdout, "Locked file: %s\n\n", dirbuf);
					}

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
					else
					{
						dirbuf = safe_malloc(strlen(token));
						strncpy(dirbuf, token, strlen(token));
					}

					if(unlockFile(dirbuf) == 0)
					{
						if(print) fprintf(stdout, "Unlocked file: %s\n\n", dirbuf);
					}

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
					else
					{
						dirbuf = safe_malloc(strlen(token));
						strncpy(dirbuf, token, strlen(token));
					}

					//fprintf(stdout, "dirbuf: %s\n\n", dirbuf);

					if(removeFile(dirbuf) == 0)
					{
						if(print) fprintf(stdout, "Deleted file: %s\n\n", dirbuf);
					}

					token = strtok(NULL, ",");
				}

				next = ptr->next;
				free(ptr);
				ptr = next;

				break;
			}
			case 10: //-a
			{
				char* token;
				char* buf;
				char* append_buf;

				int c = 0;

				token = strtok(ptr->args,",");

            	while(token != NULL)
				{	
					if(!c)
					{
						//if the path was given with shortened current directory
						if(token[0] == '.' && token[1] == '/')
						{
							token += 2;
							buf = cwd();
							buf = strncat(buf, "/", 2);
							buf = strncat(buf, token, strlen(token));
						}
						else
						{
							buf = safe_malloc(strlen(token));
							strncpy(buf, token, strlen(token));
						}						
						token = strtok(NULL, ",");
						c++;
					}
					else
					{
						append_buf = safe_malloc(strlen(token));
						strncpy(append_buf, token, strlen(token));

						if(appendToFile(buf, &append_buf, strlen(token), expelled_dir) != 0)
						{
							perror("ERROR: append to file");
							exit(EXIT_FAILURE);
						}

						token = strtok(NULL, ",");
					}
					
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
		usleep(sleeptime*1000);
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
		if(ptr->opt == 1 || ptr->opt == 2 || ptr->opt == 10)
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
		fprintf(stderr,"-D option must be used with -w or -W\n\n");
		//resetting string if it was set without the other options
		return -1;
	}

	if(!isR && isd)
	{
		fprintf(stderr,"-d option must be used with -r or -R\n\n");
		//resetting string if it was set without the other options
		return -1;
	}

	if(isD)
	{
		char* dirbuf1;

		if(dir1[0] == '.' && dir1[1] == '/')
		{
			dir1 += 2;
			dirbuf1 = cwd();
			dirbuf1 = strncat(dirbuf1, "/", 2);
			dirbuf1 = strncat(dirbuf1, dir1, strlen(dir1));
		}
		
		expelled_dir = safe_malloc(strlen(dirbuf1));
		strncpy(expelled_dir, dirbuf1, strlen(dirbuf1));

		if(print) fprintf(stderr,"Expelled files will be saved into:\n%s\n\n", expelled_dir);
	}
			
	if(isd)
	{
		char* dirbuf2;

		if(dir2[0] == '.' && dir2[1] == '/')
		{
			dir2 += 2;
			dirbuf2 = cwd();
			dirbuf2 = strncat(dirbuf2, "/", 2);
			dirbuf2 = strncat(dirbuf2, dir2, strlen(dir2));
		}
		
		save_dir = safe_malloc(strlen(dirbuf2));
		strncpy(save_dir, dirbuf2, strlen(dirbuf2));

		if(print) fprintf(stderr,"Read files will be saved into:\n%s\n\n", save_dir);
	}

	return 0;
}

int main(int argc, char *argv[]) 
{

	struct timespec abstime;
	int opt;

	opt_args = safe_malloc(sizeof(opt_args));


	while((opt = getopt(argc, argv, "hf:w:W:a:D:r:R::d:t:l:u:c:q:p")) != -1) { 
		switch(opt) { 
            case 'h': 
			{
            	print_h();
				exit(EXIT_SUCCESS);
            }
            case 'f': 
			{
				if(add_current_folder(&SOCKNAME, optarg) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				if((clock_gettime(CLOCK_REALTIME, &abstime)) == -1) {errno = -1; perror("ERROR: -f"); exit(EXIT_FAILURE);}
				abstime.tv_sec += 2;
				if(print) fprintf(stdout, "Opening connection to: %s\n", SOCKNAME);
            	if((openConnection(SOCKNAME, 1000, abstime)) == -1){errno = ECONNREFUSED; perror("openConnection"); exit(EXIT_FAILURE);}
            	else if(print) fprintf(stdout, "Connected successfully to socket\n\n");
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
			case 'a': //addtional option to test append, usage: -a file,append
			{
				arg_ins(10, optarg);
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
				if( !optarg && NULL != argv[optind] && '-' != argv[optind][0] ) 
				{
					tmp_optarg = argv[optind++];
				}
				// if the argument is a number it will be assigned to r
				// else nothing happens and r stays 0, as if no argument was given
				if (tmp_optarg) isNumber(tmp_optarg, &r);
				
				char* cur_arg = safe_malloc(sizeof(long) + 1);
				sprintf(cur_arg, "%ld", r);

				//fprintf(stdout, "arg string:%s\n", cur_arg);
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
				if(print) fprintf(stdout, "Timeout between requests set to %ld\n\n", sleeptime);
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
                printf("Prints activated\n\n");
                print = 1;
				set_prints();
                break;
            }
			case 'q': 
			{
				//used for extra tests
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
	
	//if there was a severe error and not memory expected one
	if(commandline_serve() == -1 && errno != 0)
	{
		errno = -1;
		perror("ERROR: Unable to serve command line requests");
		exit(EXIT_FAILURE);
	}

	if(print) printf("All requests served, closing connection\n\n");

	//after completing all request the connection is closed
	if(closeConnection(SOCKNAME) != 0)
	{
		perror("ERROR: Unable to close connection correctly with server");
		exit(EXIT_FAILURE);
	}

	if(print) printf("Connection closed\n");
	
	return 0;
}
