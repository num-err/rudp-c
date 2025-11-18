/* Swarthmore College, CS 43, Lab 5
 * Copyright (c) 2022 Swarthmore College Computer Science Department,
 * Swarthmore PA
 * Professor Kevin Webb
 * Professor Ranysha Ware
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include "lab5.h"

/* Global connection state.
 * You'll probably need more than just a sequence number... */
int sequence_number;

/* The following three helper functions make dealing with time values a bit
 * easier.  You probably don't need to modify them, but you can if you want. */
int timeval_to_msec(struct timeval *t) {
    return t->tv_sec*1000+t->tv_usec/1000;
}

void msec_to_timeval(int millis, struct timeval *out_timeval) {
    out_timeval->tv_sec = millis/1000;
    out_timeval->tv_usec = (millis%1000)*1000;
}

int current_msec() {
    struct timeval t;
    gettimeofday(&t,0);
    return timeval_to_msec(&t);
}
/* End time functions. */

int my_socket(int domain, int type, int protocol) {
    /* Initialize state. */
    sequence_number = 0;

    return socket(domain, type, protocol);
}

int my_connect(int socket,struct sockaddr_in *toaddr,int addrsize) {
    return connect(socket,(struct sockaddr*)toaddr,addrsize);
}

int my_rtt(int socket) {
    return 1000;
}

void my_send(int sock, void *buf, size_t len)
{
    /* Buffer for the packet. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));

    struct lab5_hdr *hdr = (struct lab5_hdr *) packet;

    /* Set the header fields as needed. */
    hdr->sequence_number = htonl(sequence_number);

    /* Copy the payload into our packet.  hdr + 1 is the address of the
     * first byte after the header. */
    memcpy(hdr+1,buf,len);

    send(sock, packet, sizeof(struct lab5_hdr) + len, 0);
    sequence_number++;
}

int my_recv(int sock, void *buf, size_t length) {
    /* Buffer for the packet. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));

    struct lab5_hdr *hdr = (struct lab5_hdr *) packet;

    struct sockaddr_in fromaddr;
    socklen_t addrlen = sizeof(fromaddr);
    int recv_count = recvfrom(sock, packet, MAX_PACKET, 0,
                              (struct sockaddr *) &fromaddr, &addrlen);

    /* Associate this UDP socket with a remote address.  This is simply
     * for convenience so that we can use send() rather than sendto(), since
     * sendto() requires an additional argument that we don't want to deal with
     * every time we need to send something. */
    if(connect(sock, (struct sockaddr *) &fromaddr, addrlen)) {
        perror("connect (my_recv)");
    }

    fprintf(stderr, "Got packet %d\n", ntohl(hdr->sequence_number));

    /* Copy the payload into the user-supplied buffer. */
    memcpy(buf, packet + sizeof(struct lab5_hdr),
           recv_count - sizeof(struct lab5_hdr));

    return recv_count - sizeof(struct lab5_hdr);
}

int my_close(int sock) {
    /* In this function, you probably want to do different things depending on
     * whether your are the first to call close (i.e., active close at the
     * sender) vs.  the passive receiver. */

    /* Send an empty packet to signal end of file. */
    my_send(sock, NULL, 0);

    return close(sock);
}
