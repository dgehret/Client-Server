#------------------------------------------------------------------------------
#  Makefile for CSE156 Lab 3
# 
#  make             makes main
#  make clean       removes object files
#  make check       runs valgrind to check for memory errors
#
#  server.cc		this is where myserver is implemented
#  client.cc		this is where myclient is implemented
#
#------------------------------------------------------------------------------

CC=g++
CFLAGS=-g 
CPPFLAGS=-Wall
LDLIBS=-lhpdf
OBJDIR=bin
vpath %.cc src
vpath %.h src

all: bin/myserver bin/myclient

bin/myserver:	bin/myserver.o 
	$(CC) -g -o bin/myserver bin/myserver.o

bin/myclient:	bin/myclient.o 
	$(CC) -g -o bin/myclient bin/myclient.o

bin/myserver.o: src/server.cc src/servers.h
	$(CC) -c -o $@ $<

bin/myclient.o: src/client.cc src/servers.h
	$(CC) -c -o $@ $<

#
#bin/$(objects):	%.cc
#	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
#


clean:
	rm -f bin/myserver.o bin/myclient.o bin/myserver bin/myclient

submit: clean
	@utils/submit.sh
#	@../utils/submit.sh

