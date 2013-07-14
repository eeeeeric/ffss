CXX = gcc
LIBS = -lpng -lffms2 -lboost_program_options -lstdc++ -lpthread
OUT = bin/ffss
SRC = src/main.cpp

build :
	gcc -o $(OUT) $(SRC) $(LIBS)

debug :
	$(CXX) -g -Wall -o $(OUT) $(SRC) $(LIBS)

clean :
	rm $(OUT)
