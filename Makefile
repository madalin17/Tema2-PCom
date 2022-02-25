CC = g++
CFLAGS = -Wall -Wextra -g

# ID-ul clientului deschis
ID_CLIENT = madalin

# Portul pe care asculta serverul
PORT = 12288

# Adresa IP a serverului
IP_SERVER = 127.0.0.1

all: server subscriber

server: server.cpp
	$(CC) $(CFLAGS) server.cpp -o server

subscriber: subscriber.cpp
	$(CC) $(CFLAGS) subscriber.cpp -o subscriber

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul
run_subscriber:
	./subscriber ${ID_CLIENT} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
