#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <ops.h>
#include <coms.h>
#include <conn.h>



int main(int argc, char *argv[]) {

	openConnection(SOCKNAME);

	const char* nome = "./imm.png";
	if(atoi(argv[1])==0) openFile(nome, 0);
	else {
		void* buf = malloc(1024*sizeof(char));
		size_t sz = 1024;
		readFile(nome, &buf, &sz);
		printf("%p\n", buf);
	}

	closeConnection(SOCKNAME);
	return 0;
}
