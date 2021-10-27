CC		=	gcc
CFLAGS		+=	-std=gnu99 -Wall -Werror -g -pedantic
INCLUDES	=	-I. -I ./include
LDFLAGS		=	-L.
OPTFLAGS 	=
LIBS		=	-lpthread

TARGETS 	=	server		\
			client		


.PHONY: all clean cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)



server: server.o HashLFU.o -lpthread
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@

server.o: server.c HashLFU.h 

HashLFU.o: HashLFU.c HashLFU.h 

testinghash: testinghash.o HashLFU.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@

testinghash.o: testinghash.c HashLFU.h

HashLFU.o: HashLFU.c HashLFU.h

client: client.o coms.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@
client.o: client.c coms.h

coms.o: coms.c coms.h

clean		: 
	rm -f $(TARGETS)

cleanall	: clean
	\rm -f *.o *~ *.a ./storage_sock ./savedfiles/*.*