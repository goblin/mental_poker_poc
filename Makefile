TMCG=$(shell libTMCG-config --libs --cflags) -Wl,-rpath $(shell libTMCG-config --prefix)/lib

all: master slave

master: master.cc Makefile
	g++ -o master $(TMCG) master.cc

slave: slave.cc Makefile
	g++ -o slave $(TMCG) slave.cc
