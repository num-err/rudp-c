/* Swarthmore College, CS 43, Lab 5
 * Copyright (c) 2022 Swarthmore College Computer Science Department,
 * Swarthmore PA
 * Professor Kevin Webb
 * Professor Ranysha Ware
 */

#include <sys/time.h>

struct lab5_hdr {
    uint32_t sequence_number;
    uint32_t ack_number;
    uint32_t flags;
};

#define MAX_PACKET 1400
#define MAX_SEGMENT (MAX_PACKET-sizeof(struct lab5_hdr))
#define FLAG_FIN 0x01 // THis is the finish flag to close the connection

int my_socket(int domain, int type, int protocol);
int my_connect(int socket,struct sockaddr_in *toaddr,int addrsize);
void my_send(int sock, void *buf, size_t len);
int my_recv(int sock, void *buf, size_t length);
int my_close(int sock);
int my_rtt(int sock);

int timeval_to_msec(struct timeval *t);
void msec_to_timeval(int millis, struct timeval *out_timeval);
int current_msec();