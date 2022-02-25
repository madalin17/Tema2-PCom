#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

#define forever while (1)
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1600	// Max length of data transmitted on socket
#define DATALEN 	1500	// Max useful data recieved from UDP clients
#define MAX_CLIENTS	100		// Maximum number of TCP clients waiting to connect
#define SR 			100		// TO DIVIDE
#define TOPICLEN 	50		// Max length of topic
#define COMMAND 	11		// Max length of id

// Constant strings
#define SUBSCRIBE "subscribe"
#define UNSUBSCRIBE "unsubscribe"
#define EXIT_ENTER "exit\n"
#define ENTER "\n"
#define UDP_INT "INT"
#define UDP_SHORT_REAL "SHORT_REAL"
#define UDP_FLOAT "FLOAT"
#define UDP_STRING "STRING"

// Message recieved from UDP client
struct message {
	char topic[TOPICLEN];
	uint8_t data_type;
	char data[DATALEN];
};

struct fromudp {
	uint32_t ip;
	uint16_t port;
	struct message mess;
};

// Message sent from TCP client
struct subject {
	int sf;
	char id[COMMAND];
	char topic[TOPICLEN];
};
/**
 * My implemented protocol sends the messages:
 * 		- connect message = send id only(id, topic null, sf = INT_MAX)
 * 		- disconnect message = send id only(id, topic null, sf = INT_MIN)
 * 		- subscribe message = send id, topic, sf = 0/1
 * 		- unsubscribe message = send id, topic, sf = -1
 */

#endif
