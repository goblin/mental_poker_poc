TMCG=$(shell libTMCG-config --libs --cflags) -Wl,-rpath $(shell libTMCG-config --prefix)/lib

all: client 

client: client.cc Makefile
	g++ -o client $(TMCG) -pthread -lboost_system client.cc

