#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{	
	int sockfd, sf, n, ret, new_int;
	char nagle_off = 1;
	char sign;
	double new_double;
	uint32_t my_int;
	uint16_t my_short;
	uint8_t my_char;
	char buffer[BUFLEN], topic[TOPICLEN], command[COMMAND], id[COMMAND];
	struct sockaddr_in serv_addr;
	struct subject* subj = (struct subject*) malloc(sizeof(struct subject));
	struct message* mess = (struct message*) malloc(sizeof(struct message));
	struct fromudp* udp_mess;
	struct in_addr addr;

	fd_set read_fds; // Set of available sockets to read from
	fd_set tmp_fds; // Temporary set
	int fdmax; // Max value of sockets in read_fds

	// Verifying if program executes correctly
	if (argc < 4) {
		usage(argv[0]);
	}

	// Empty sets of sockets when server starts
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Convert id argument to string
	memcpy(id, argv[1], COMMAND);

	// Open TCP socket to communicate with the server
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "Couldn't open socket TCP listen socket");
	// Deactivating Nagle's algorithm
	n = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle_off, sizeof(int));
	DIE(n < 0, "Couldn't deactivate Nagle's algorithm");

	// Set struct sockaddr_in to connect to server
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "Couldn't convert ip adress to uint32_t");

	// Connect TCP client to server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "Couldn't connect to server");

	// Adding TCP socket to set and also stdin(to read from)
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd; // // Set fdmax

	// Send connect type message(sf == INT_MAX)
	memcpy(subj->id, id, COMMAND);
	memset(subj->topic, 0, TOPICLEN);
	subj->sf = INT_MAX;
	memset(buffer, 0, BUFLEN);
	memcpy(buffer, subj, sizeof(struct subject));

	n = send(sockfd, buffer, sizeof(buffer), 0);
	DIE(n < 0, "Couldn't send message to server");

	// Deactivate buffering on printing
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	forever {
		tmp_fds = read_fds; // Set temporary set to select a socket from

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL); // Select from set
		DIE(ret < 0, "Couldn't select any socket to set");

		memset(buffer, 0, BUFLEN); // Reset buffer of data recieved
		memset(topic, 0, TOPICLEN);
		memset(mess, 0, sizeof(struct message));

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// Read from stdin
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, SUBSCRIBE, 9) == 0) {
				// subscribe <TOPIC> <SF> message - sf = 0/1
				sscanf(buffer, "%s %s %d", command, topic, &sf);
				// Ignore invalid message
				if ((sf != 0 && sf != 1) || topic[0] == 0 || strcmp(topic, ENTER) == 0) {
					continue;
				}
				subj->sf = sf;
			} else if (strncmp(buffer, UNSUBSCRIBE, 11) == 0) {
				// unsubscribe <TOPIC> message - sf = -1
				sscanf(buffer, "%s %s", command, topic);
				// Ignore invalid message
				if (topic[0] == 0 || strcmp(topic, ENTER) == 0) {
					continue;
				}
				subj->sf = -1;
			} else if (strcmp(buffer, EXIT_ENTER) == 0) {
				// exit message - sf = INT_MIN
				memset(subj, 0, sizeof(struct subject));
				memcpy(subj->id, argv[1], COMMAND);
				subj->sf = INT_MIN;
				memcpy(buffer, subj, sizeof(struct subject));

				// Send id to server to disconnect
				n = send(sockfd, buffer, sizeof(buffer), 0);
				DIE(n < 0, "Couldn't send message to server");
				break;
			} else {
				// Ignore other invalid messages
				continue;
			}

			// Set topic and id for valid subscribe/unsubscribe message to send
			memcpy(subj->topic, topic, TOPICLEN);
			memcpy(subj->id, argv[1], COMMAND);
			memset(buffer, 0, BUFLEN);
			memcpy(buffer, subj, sizeof(struct subject));

			// Send subscribe/unsubscribe message(struct subject) to server
			n = send(sockfd, buffer, sizeof(buffer), 0);
		 	DIE(n < 0, "Couldn't send message to server");

			// Print to stdout depending of sf value
			if (subj->sf == -1) {
				printf("Unsubscribed from topic.\n");
			} else {
				printf("Subscribed to topic.\n");
			}
		} else if (FD_ISSET(sockfd, &tmp_fds)) {
			// Recieved data from server
			n = recv(sockfd, buffer, sizeof(buffer), 0);
			DIE(n < 0, "Couldn't recieve message from server");

			// Exit directly if message if "exit"
			if (strcmp(buffer, EXIT_ENTER) == 0) {
				break;
			}

			// Parse struct fromudp
			udp_mess = (struct fromudp*) buffer;
			// Parse struct message to print
			mess = &udp_mess->mess;

			addr.s_addr = udp_mess->ip;
			printf("%s:%hu - %s - ", inet_ntoa(addr), ntohs(udp_mess->port), mess->topic);

			if (mess->data_type == 0) { // INT
				memcpy(&sign, mess->data, sizeof(char)); // Parse sign
				memcpy(&my_int, mess->data + sizeof(char), sizeof(int)); // Parse int
				my_int = ntohl(my_int); // Convert from network byte order
				new_int = (int) my_int; // Convert to int

				if ((new_int > 0 && sign == 1) || (new_int < 0 && sign == 0)) {
					new_int = -new_int; // Convert int if sign doesn't match
				}
				printf("%s - %d\n", UDP_INT, new_int);
			} else if (mess->data_type == 1) { // SHORT_REAL
				memcpy(&my_short, mess->data, sizeof(uint16_t)); // Parse short
				my_short = ntohs(my_short); // Convert from network byte order
				new_double = (double) my_short / SR; // DIvide by 100, convert to double

				printf("%s - %.2f\n", UDP_SHORT_REAL, new_double);
			} else if (mess->data_type == 2) { // FLOAT
				memcpy(&sign, mess->data, sizeof(char)); // Parse sign
				memcpy(&my_int, mess->data + sizeof(char), sizeof(uint32_t)); // Parse int
				my_int = ntohl(my_int); // Convert from network byte order
				memcpy(&my_char, mess->data + sizeof(char)
						+ sizeof(uint32_t), sizeof(uint8_t)); // Parse precission
				new_double = (double) my_int; // Convert result to double

				while (my_char) {
					new_double /= 10; // Divide by <precission> times
					my_char--;
				}

				if ((new_double > 0 && sign == 1) || (new_double < 0 && sign == 0)) {
					new_double = -new_double; // Convert double if sign doesn't match
				}

				printf("%s - %.8g\n", UDP_FLOAT, new_double);
			} else if (mess->data_type == 3) { // STRING
				printf("%s - %s\n", UDP_STRING, mess->data);
			}
		}
	}

	free(subj);
	close(sockfd);
	return 0;
}
