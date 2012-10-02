CC = g++
CCOPTS = -O3 -Wall -Werror -g -Iinclude -I/usr/local/include

LDOPTS = -L/usr/local/lib/
LIBS = -lboost_filesystem -lboost_system

all: driver

driver.o: driver.cpp include/*
	$(CC) $(CCOPTS) $(LDOPTS) -c driver.cpp -o driver.o

driver: driver.o
	$(CC) $(CCOPTS) $(LDOPTS) -o driver driver.o $(LIBS)

test: test.cpp include/*
	$(CC) $(CCOPTS) $(LDOPTS) -o test -Ideps/Catch/include test.cpp $(LIBS)
	# And now, run
	./test

clean:
	rm -rf driver *.o *.dSYM
