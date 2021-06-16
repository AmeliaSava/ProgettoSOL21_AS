#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>

#include <ops.h>
#include <coms.h>
#include <conn.h>
#include <message.h>

int print = 0;

void print_h() {
	printf("usage: ./client [option]\n");
	printf("options: \n");
	printf("-h -> prints helper\n");
	printf("-f filename -> used to specify the name of the socketfile\n");
	printf("-w dirname[, n = 0]-> sends request for writing the files in the directory -dirname- in the file storage.\n");
	printf("						if n is specified, sends n files. If n = 0 or not specified there's not limit to the number of sent files\n");
	printf("-W file1[, file2] -> list of file pathnames to write in the server");
	printf("-r file1[, file2]-> list of file pathnames to read from the file storage, speparated by commas.\n");
	printf("-R [n = 0] -> allows to read n files from the file storage, if n = 0 or not specified all the files are read\n");
	printf("-d dirname -> memory folder where the file read with -r and -R are written. Must be used in addiction to those.\n");
	printf("				if not used after, the read files will not be memorized\n");
	printf("-t -> time in milliseconds that passes between sending a request from the other to the server\n");
	printf("		if not specified...\n");
	printf("-c file1[, file2]-> list of file to remove from the storage, if present\n");
	printf("-p -> prints\n");
}

int main(int argc, char *argv[]) {
	openConnection(SOCKNAME);

	//const char* nome = "./storage/test.txt";
	//const char* nome1 = "./storage/es8.tgz";
	//const char* nome2 = "./storage/manuale-unix.pdf";
	const char* nome3 = "./storage/it is a mystery.mp3";

	int opt;

	while((opt = getopt(argc, argv, "hf::w::W:r:R::d:t:c:p")) != -1) { 
		switch(opt) { 
            case 'h': {
            	print_h();
				break;
            }
            case 'f': {
                writeFile(nome3, NULL);
            	printf("filename: %s\n", argv[optind]);
            	break;
            }
            case 'w': {
                openFile(nome3, 1);

                break;
            }
            case 'W': { 
            	optind--;
            	for(; optind<argc && *argv[optind] != '-'; optind++)
                	printf("filename: %s\n", argv[optind]); 
                break;
            }
            case 'r': { 
            	optind--;
            	for(; optind<argc && *argv[optind] != '-'; optind++)
                	printf("filename: %s\n", argv[optind]); 
                break;
            }
			case 'R': {
                void* buf = NULL;
				size_t sz;
				int r = readFile(nome3, &buf, &sz);
				if(!r) printf("Buf: %p\nSize: %zu\n", buf, sz); // qualcosa non va nel puntatore
                break;
            }
			case 'd': {
                printf("option: %s\n", optarg); 
                break;
            }
			case 't': {
                printf("option: %s\n", optarg); 
                break;
            }
			case 'c': {
                printf("option: %s\n", optarg); 
                break;
            }
			case 'p': {
                printf("prints activated");
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

/*
	

	if(atoi(argv[1])==0) {
		openFile(nome, 1);
		
		//openFile(nome1, 1);
		//openFile(nome2, 1);
		
	}
	if(atoi(argv[1])==1) {
		writeFile(nome, NULL);
	}
	if(atoi(argv[1])==2) {
		closeFile(nome);
	}
	if(atoi(argv[1])==3) {
		removeFile(nome);
	}
	if(atoi(argv[1])==4) {
		
	}
	if(atoi(argv[1])==5) {
		char* append = " append";
		size_t size = sizeof(append);
		void* buf = append;
		appendToFile(nome, buf, size, NULL);
		//parte una open a caso, perchè???
	}
*/
	closeConnection(SOCKNAME);

	return 0;
}
