FLAGS=$(shell pkg-config --libs --cflags opencv)

all:
	g++ -o zed zed_ocv.cpp -march=native -O3 --std=c++11 -lpthread ${FLAGS}

debug:
	g++ -o zed zed_ocv.cpp -g --std=c++11 -lpthread ${FLAGS}
