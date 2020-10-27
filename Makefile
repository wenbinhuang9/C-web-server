

LIBS = -lpthread #-lsocket
server: server.c
	gcc -g -W -Wall $(LIBS) -o $@ $<
