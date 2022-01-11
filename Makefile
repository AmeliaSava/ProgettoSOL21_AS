CC		=	gcc
CFLAGS		+=	-std=gnu99 -Wall -Werror -g -pedantic
INCLUDES	=	-I. -I ./include
LDFLAGS		=	-L.
OPTFLAGS 	=
T_LIBS		=	-pthread

TARGETS 	=	server		\
			client		


.PHONY: all clean cleanall server client test1

all		: $(TARGETS)


server: server.c libs/libhash.so libs/libcoms.so
	$(CC) $(CFLAGS) $(INCLUDES) $(T_LIBS) server.c -o server -Wl,-rpath,./libs -L ./libs -lhash -lcoms

client: client.c libs/libhash.so libs/libcoms.so
	$(CC) $(CFLAGS) $(INCLUDES) client.c -o client -Wl,-rpath,./libs -L ./libs -lhash -lcoms


libs/libhash.so: HashLFU.o
	$(CC) -shared -o libs/libhash.so $^
HashLFU.o:
	$(CC) $(CFLAGS) $(INCLUDES) HashLFU.c -c -fPIC -o $@

libs/libcoms.so: coms.o
	$(CC) -shared -o libs/libcoms.so $^
coms.o:
	$(CC) $(CFLAGS) $(INCLUDES) coms.c -c -fPIC -o $@

clean		: 
	rm -f $(TARGETS)

cleanall	: clean
	\rm -f *.o *~ *.a *.sk ./storage_sock ./log_file.txt ./valgrind-out.txt ./libs/*.* ./savedfiles/*.* ./expelled/*.*

test1: all
	./scripts/test1.sh

test2: all
	./scripts/test2.sh