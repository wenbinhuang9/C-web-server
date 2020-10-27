

LIBS = -lpthread #-lsocket
server: newserver.c
	gcc -g -W -Wall $(LIBS) -o $@ $<
