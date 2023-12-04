GOAL = server client
REQ = $(addsuffix .o, $(GOAL))

all : $(GOAL)

client : client.o 
	cc -g3 -o  $@ $^ -lpthread 

server : server.o 
	cc -g3 -o  $@ $^ -lpthread

%.o : %.c
	cc -g3 -Wall -o $@ -c $<

clean :
	rm $(GOAL) $(REQ)