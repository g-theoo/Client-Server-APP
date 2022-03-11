CC = g++ -std=c++11
CFLAGS = -Wall -Wextra

all: server subscriber

server: server.o
	$(CC) $(CFLAGS) server.o -o server

server.o: server.cpp
	$(CC) $(CFLAGS) server.cpp -c

subscriber: subscriber.o
	$(CC) $(CFLAGS) subscriber.o -o subscriber

subscriber.o: subscriber.cpp
	$(CC) $(CFLAGS) subscriber.cpp -c

clean:
	rm -f server subscriber *.o