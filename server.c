#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <utils.h>
//altri include

#define CONFIG_FL "./config.txt"
#define MAX_BUF 1024
//define

//funzioni

int main (int argc, char* argv[]) {

	long NUM_THREAD_WORKERS;
	long MAX_MEMORY_MB;
	char* SOCKET_NAME = NULL;
	
	if((SOCKET_NAME = malloc(MAX_BUF * sizeof(char))) == NULL) {
		perror("malloc socket name");
		free(SOCKET_NAME);
		exit(EXIT_FAILURE);
	}

	FILE *config_input = NULL;
	
	char* buffer = NULL;
	
	if ((config_input = fopen(CONFIG_FL, "r")) == NULL) {
		perror("opening config file");
		fclose(config_input);
		exit(EXIT_FAILURE);
	}

	if((buffer = malloc(MAX_BUF * sizeof(char))) == NULL) {
		perror("malloc buffer");
		free(buffer);
		exit(EXIT_FAILURE);
	}

	int count = 0;

	while(fgets(buffer, MAX_BUF, config_input) != NULL) {

		char* comment;

		if ((comment = strchr(buffer, '#')) != NULL) { 
			continue;
		}

		char* semic;
		
		if ((semic = strchr(buffer, ';')) == NULL) { 
			fprintf(stderr, "wrong newline config.txt file format\n");
			exit(EXIT_FAILURE);
		}
		
		*semic = '\0';

		char* eq;

		if((eq = strchr(buffer, '=')) == NULL) {
			fprintf(stderr, "wrong equal config.txt file format\n");
			exit(EXIT_FAILURE);	
		}
		
		*eq++;
		
		switch (count) {
			case 0:
				if(isNumber(eq, &NUM_THREAD_WORKERS) != 0) {
					fprintf(stderr, "wrong format number of thread workers\n");
					exit(EXIT_FAILURE);
				}
			case 1:
				if(isNumber(eq, &MAX_MEMORY_MB) != 0) {
					fprintf(stderr, "wrong format max memory allocated\n");
					exit(EXIT_FAILURE);
				}
			case 2:
				strncpy(SOCKET_NAME, eq, MAX_BUF);
		}

		count++;
		
	}

	printf("%ld\n", NUM_THREAD_WORKERS);
	printf("%ld\n", MAX_MEMORY_MB);
	printf("%s\n", SOCKET_NAME);

	fclose(config_input);
	free(buffer);

	return EXIT_SUCCESS;
}
