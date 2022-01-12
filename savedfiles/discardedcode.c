COMS>C

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


SERVER>C
	/*
int getMSG(int connfd)
{
	msg* file = safe_malloc(sizeof(msg));
	
	if (readn(connfd, file, sizeof(msg))<=0) return -1;

	fprintf(stderr, "Nome: %s\n", file->filename);

	return cmd(connfd, *file);
}
*/

	/**
	 * Working single thread socket communication
	for(;;) 
	{      
		rdset = set; //saving the set in the temporary one

		//+1 because I need the number of active file descriptors, not the max index
		if (select(fd_max + 1, &rdset, NULL, NULL, NULL) == -1)
		{
		    perror("ERROR: select");
			return EXIT_FAILURE;
		} 
		else { 
			//select ok
			fprintf(stderr, "select ok\n");
			fprintf(stderr, "max bef for: %d\n", fd_max);

			for(fd_sel = 0; fd_sel <= fd_max; fd_sel++)
			{
				//accepting new connections
				fprintf(stderr, "accepting connections\n");

			    if (FD_ISSET(fd_sel, &rdset))
				{ //is it ready?
			    	fprintf(stderr, "ready?\n");

					if (fd_sel == fd_skt)
					{ // sock connect ready
						fprintf(stderr, "sock ready\n");
						SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");
						FD_SET(fd_con, &set);  // adding fd to starting set
						
						// updating max
						if(fd_con > fd_max) fd_max = fd_con;  
						fprintf(stderr, "max after connection:%d\n", fd_max);
						//fprintf(stderr, "Client fd: %d\n", fd_con);
						continue;
					} 
					fd_con = fd_sel;

					//fprintf(stderr, "Client da ascoltare: %d\n", fd_con);
					if(getMSG(fd_con) < 0) 
					{
						close(fd_con); 
						FD_CLR(fd_con, &set);

						if (fd_con == fd_max) fd_max = updateMax(set, fd_max);
					}
		   		}
			}
		}
    }
	*
	**/


//DOSEN'T WORK
	/*
    fprintf(stderr, "Max fd at start: %d\n", fd_max);
	for(;;)
	{   

		LOCK(&set_lock);  
		rdset = set; //saving the set in the temporary one
		UNLOCK(&set_lock);

		int connected = client_connected.lenght;

		if(connected != 0) {
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 1;
			//+1 because I need the number of active file descriptors, not the max index
			if (select(fd_max + 1, &rdset, NULL, NULL, &timeout) == -1) 
			{
				perror("ERROR: select");
				return EXIT_FAILURE;
			} 
			
		}
		
		//select ok
		printf("QUI\n");
		fprintf(stderr, "select ok\n");

		long fd_con; // I/O socket with a client

		fprintf(stderr, "max bef for: %d\n", fd_max);

		for(fd_sel = 0; fd_sel <= fd_max; fd_sel++) 
		{
			//accepting new connections
			fprintf(stderr, "accepting connections\n");

			if (FD_ISSET(fd_sel, &rdset)) { //is it ready?
				fprintf(stderr, "ready?\n");

				if (fd_sel == fd_skt)
				{	// sock connect ready
					fprintf(stderr, "sock ready, accept\n");

					SYSCALL_EXIT("accept", fd_con, accept(fd_skt, (struct sockaddr*)NULL, NULL), "ERROR: accept", "");

					LOCK(&set_lock);
					FD_SET(fd_con, &set);  // adding fd to starting set
					UNLOCK(&set_lock);
						
					// updating max
					LOCK(&max_lock);
					if(fd_con > fd_max) fd_max = fd_con;
					UNLOCK(&mac_lock);

					fprintf(stderr, "Max after accept:%d\n", fd_max);
				} else {
					//request
		   			fprintf(stderr, "handling request\n");

		   			long client;
		   			op rep;

					if (readn(pipe_m[0], &client, sizeof(long)) > 0) 
						{ //read something
							if (readn(pipe_m[0], &rep, sizeof(op))<=0) {
								perror("reading pipe 2"); exit(EXIT_FAILURE);
							}
							if (rep == END_COMMUNICATION) 
							{
								fprintf(stderr, "interrmpo com client: %ld\n", client);
								FD_CLR(client, &set);
								fprintf(stderr, "max prima agg:%d\n", fd_max);
								if (client == fd_max) fd_max = updateMax(set, fd_max);
									fprintf(stderr, "max dopo agg:%d\n", fd_max);
									close(client);
							}
							else
							{
								FD_SET(client, &set);
								if(client > fd_max) fd_max = client;
							}
						} 
						else 
						{ 
							perror("reading pipe");
							 exit(EXIT_FAILURE); 
						}	
			   		
						fprintf(stderr, "adding request to list\n");
						pthread_mutex_lock(&cli_req);
						push_head(&client_requests, fd_sel);
						pthread_cond_signal(&wait_list);
						pthread_mutex_unlock(&cli_req);
						FD_CLR(fd_sel, &set);
					
		   		}
		   	}
		}
		
    }
	*/

    //ogni volta che si chiude la comunicazione va aggiornato il massimo