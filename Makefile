CC = g++
CCOPTS = -O3 -Wall -Werror -g -Iinclude

all: driver

driver: driver.cpp include/*
	$(CC) $(CCOPTS) -o driver driver.cpp

clean:
	rm -rf driver *.o *.dSYM
