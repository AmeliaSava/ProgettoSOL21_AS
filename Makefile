CC		=	gcc
CFLAGS		+=	-std=c99 -Wall -Werror -g -pedantic
INCLUDES	=	-I. -I ./include
LDFLAGS		=	-L.
OPTFLAGS 	=
LIBS		=	-lpthread

TARGETS 	=	server		\
			client_prova	\
			treeLFU		\
			testingtree


.PHONY: all clean cleanall
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all		: $(TARGETS)

server: server.o treeLFU.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@
server.o: server.c treeLFU.h

treeLFU.o: treeLFU.c treeLFU.h

testingtree: testingtree.o treeLFU.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@
testingtree.o: testingtree.c treeLFU.h

treeLFU.o: treeLFU.c treeLFU.h

clean		: 
	rm -f $(TARGETS)

cleanall	: clean
	\rm -f *.o *~ *.a ./storage_sock 