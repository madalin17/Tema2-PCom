#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <limits.h>
#include <unordered_map>
#include <queue>

#include "helpers.h"
using namespace std;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{	
	int sock_tcp, sock_udp, newsockfd, to_client_sock, n, i, ret, sf, opt_val = 1;
	short port;
	char nagle_off = 1;
	char buffer[BUFLEN];
	string id;
	struct sockaddr_in serv_addr, cli_addr, my_sockaddr, from_station;
	socklen_t clilen = sizeof(cli_addr), socklen = sizeof(from_station);
	struct subject *subj;
	struct message *mess;
	struct fromudp *udp_mess;

	// Map of pairs (topic, other_map); interior map is a map of pairs (id, sf)
	unordered_map<string, unordered_map<string, int>> topic_map;
	// Map of pairs (id, socket TCP client)
	unordered_map<string, int> sock_map;
	// Map of pairs (id, 0/1); 1 = connect; 0 = disconnect
	unordered_map<string, int> connected_map;
	// Map of pairs (id, queue of messages to send to client id when connected)
	unordered_map<string, queue<struct fromudp*>> queue_map;
	// Map of pairs (socket TCP client, struct with ip and port of TCP client)
	unordered_map<int, struct sockaddr_in> ip_port_map;

	fd_set read_fds; // Set of available sockets to read from
	fd_set tmp_fds;	// Temporary set
	int fdmax; // Max value of sockets in read_fds

	// Verifying if program executes correctly
	if (argc < 2) {
		usage(argv[0]);
	}

	// Empty sets of sockets when server starts
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Convert port argument to short
	sscanf(argv[1], "%hd", &port);
	DIE(port < 0, "Couldn't convert port to short");

	// UDP start socket creation - Open UDP socket
	sock_udp = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sock_udp < 0, "Can't find socket to open\n");
	
	// Set struct sockaddr_in to listen on this port
	my_sockaddr.sin_family = AF_INET; // set family field
	my_sockaddr.sin_port = htons(port); // set port field
	my_sockaddr.sin_addr.s_addr = INADDR_ANY; // set address field
	
	// Binding socket proprieties
	ret = bind(sock_udp, (struct sockaddr*) &my_sockaddr, sizeof(my_sockaddr));
	DIE(ret < 0, "Couldn't bind to socket");

	// TCP start socket creation
	sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sock_tcp < 0, "Couldn't open socket TCP listen socket");
	// Trying to reuse an occupied port for our server
	n = setsockopt(sock_tcp, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int));
	DIE(n < 0, "setsockopt(SO_REUSEADDR) - Reusing port failed");
	// Deactivating Nagle's algorithm
	n = setsockopt(sock_tcp, IPPROTO_TCP, TCP_NODELAY, &nagle_off, sizeof(int));
	DIE(n < 0, "Couldn't deactivate Nagle's algorithm");

	// Set struct sockaddr_in to listen on this port
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sock_tcp, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "Couldn't bind to socket");

	// Let current tcp socket listen for a maximum of MAX_CLIENTS connections
	ret = listen(sock_tcp, MAX_CLIENTS);
	DIE(ret < 0, "Couldn't listen on socket");

	// Adding UDP and TCP sockets to set and also stdin(to read from)
	FD_SET(sock_tcp, &read_fds);
	FD_SET(sock_udp, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	fdmax = sock_tcp > sock_udp ? sock_tcp : sock_udp; // Set fdmax

	// Deactivate buffering on printing
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	forever {
		tmp_fds = read_fds; // Set temporary set to select a socket from
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL); // Select from set
		DIE(ret < 0, "Couldn't select any socket to set");

		memset(buffer, 0, BUFLEN); // Reset buffer of data recieved

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == STDIN_FILENO) {
					// Reading from stdin
					fgets(buffer, BUFLEN - 1, stdin);

					// If "exit" read from stdin
					if (strcmp(buffer, EXIT_ENTER) == 0) {
						for (auto pair : connected_map) {
							int connected = pair.second;
							// Send "exit" message to any connected TCP client
							if (connected == 1) {
								to_client_sock = sock_map[pair.first];
								n = send(to_client_sock, buffer, sizeof(buffer), 0);
		 						DIE(n < 0, "Couldn't send message to TCP client");

								// Close socket; no longer accept data if server about to close
								close(to_client_sock);
								FD_CLR(to_client_sock, &read_fds);
							}
						}
						// Exit loop; close server
						goto close_server;
					}
				} else if (i == sock_udp) {
					// Reading from a UDP client
					int r = recvfrom(sock_udp, buffer, BUFLEN, 0,
							(struct sockaddr*) &from_station, &socklen);
					DIE(r < 0, "Couldn't recieve message from UDP client");

					mess = (struct message*) buffer;

					string t(mess->topic);
					if (topic_map.find(t) == topic_map.end()) {
						// If no TCP client is subscribed to this topic, ignore topic
						continue;
					}

					udp_mess = (struct fromudp*) calloc(1, sizeof(struct fromudp));
					udp_mess->ip = from_station.sin_addr.s_addr;
					udp_mess->port = from_station.sin_port;
					memcpy(&udp_mess->mess.topic, mess->topic, TOPICLEN);
					udp_mess->mess.data_type = mess->data_type;
					memcpy(&udp_mess->mess.data, mess->data, DATALEN);
					for (auto pair : topic_map[t]) {
						// For every TCP client subscribed to this topic
						id = pair.first;
						sf = pair.second;;
						if (connected_map[id] == 0 && sf == 1) {
							// If TCP client not connected, but sf is 1
							// save the messages in queue to send when he will reconnect
							// aux_mess = (struct message*) malloc(sizeof(struct message));
							// memcpy(aux_mess, mess, sizeof(struct message));
							// queue_map[id].push(aux_mess);
							queue_map[id].push(udp_mess);
						} else if (connected_map[id] == 1) {
							// If TCP client connected, forward the message from the UDP client
							to_client_sock = sock_map[id];

							memcpy(buffer, udp_mess, sizeof(struct fromudp));
							n = send(to_client_sock, buffer, sizeof(buffer), 0);
	 						DIE(n < 0, "Couldn't send message to TCP client");

							//free(udp_mess);
						}
					}
				} else if (i == sock_tcp) {              
					// Connection request to accept on TCP listen socket
					newsockfd = accept(sock_tcp, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "Couldn't accept new connection");
					// Trying to reuse an occupied port for our server
					n = setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int));
					DIE(n < 0, "setsockopt(SO_REUSEADDR) - Reusing port failed");
					// Deactivating Nagle's algorithm
					setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &nagle_off, sizeof(int));
					DIE(n < 0, "Couldn't deactivate Nagle's algorithm");

					FD_SET(newsockfd, &read_fds); // Add new TCP client socket to set
					fdmax = newsockfd > fdmax ? newsockfd : fdmax; // Update fdmax

					// Insert pair of (TCP client socket, struct with ip and port of socket)
					ip_port_map.insert(make_pair(newsockfd, cli_addr));
				} else {
					// Recieved data from a TCP client
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "Couldn't recieve message from TCP client");

					// TCP clients only send struct subject
					subj = (struct subject *) buffer;

					string new_id(subj->id), t(subj->topic);
					queue<struct fromudp*> q;
					if (subj->sf == INT_MAX) {
						// Connect type message(if sf == INT_MAX)
						// TCP client recently connected and wants to send a message
						// with only the id to server for the server to print
						if (sock_map.find(new_id) == sock_map.end()) {
							// If it is the first time a client connects with this id
							sock_map.insert(make_pair(new_id, i));
							connected_map.insert(make_pair(new_id, 1));
							queue_map.insert(make_pair(id, q));

							// Get ip and port of TCP client from ip_port_map
							cli_addr = ip_port_map[sock_map[new_id]];
							printf("New client %s connected from %s:%d.\n",
										subj->id, inet_ntoa(cli_addr.sin_addr),
										ntohs(cli_addr.sin_port));
						} else if (connected_map.find(new_id) != connected_map.end()
										&& connected_map[new_id] == 0) {
							// If a client reconnects with this id
							sock_map[new_id] = i; // Update socket of id
							connected_map[new_id] = 1; // Update connection status

							// Get ip and port of TCP client from ip_port_map
							cli_addr = ip_port_map[i];
							printf("New client %s connected from %s:%hu.\n",
										subj->id, inet_ntoa(cli_addr.sin_addr),
										ntohs(cli_addr.sin_port));

							while (!queue_map[new_id].empty()) {
								// Send all messages in queue(for topics with sf == 1) if any
								udp_mess = queue_map[new_id].front();
									
								memset(buffer, 0, BUFLEN);
								memcpy(buffer, udp_mess, sizeof(struct fromudp));
								n = send(i, buffer, sizeof(buffer), 0);
		 						DIE(n < 0, "Couldn't send message to TCP client");

								//free(udp_mess);
								queue_map[new_id].pop();
							}
						} else {
							// Another client wants to connect with the same id
							printf("Client %s already connected.\n", subj->id);

							// Send message to client to close the socket
							memset(buffer, 0, sizeof(buffer));
							strcpy(buffer, EXIT_ENTER);

							n = send(i, buffer, sizeof(buffer), 0);
		 					DIE(n < 0, "Couldn't send message to TCP client");

							// Close TCP invalid client and take out of set
							close(i);
							FD_CLR(i, &read_fds);
						}
					} else if (connected_map.find(new_id) != connected_map.end()
									&& subj->sf == INT_MIN) {
						// Disconnect type message(if sf == INT_MIN)
						connected_map[new_id] = 0;

						printf("Client %s disconnected.\n", subj->id);
						// Close TCP invalid client and take out of set
						close(i);
						FD_CLR(i, &read_fds);
					} else if (subj->sf == -1) {
						// Unsubscribe type message(if sf == -1)
						// Take (new_id, sf) pair out of topic t's map
						topic_map[t].erase(new_id);
					} else {
						// Subscribe type message(if sf == 0/1)

						// If topic not in topic_map, add
						unordered_map<string, int> interior_map;
						if (topic_map.find(t) == topic_map.end()) {
							topic_map.insert(make_pair(t, interior_map));
						}

						// Add (id, pair) from struct subject to topic t's map
						topic_map[t].insert(make_pair(new_id, subj->sf));
					}
				}
			}
		}
	}

close_server:
	close(sock_udp);
	close(sock_tcp);
	return 0;
}
