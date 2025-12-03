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
int expected_ack;
int expected_seq; // for the receiver

// The rtt variables
int estimated_rtt;
int dev_rtt;

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
    expected_ack = 0;
    expected_seq = 0;

    estimated_rtt = 500;
    dev_rtt = 0;

    return socket(domain, type, protocol);
}

int my_connect(int socket,struct sockaddr_in *toaddr,int addrsize) {
    return connect(socket,(struct sockaddr*)toaddr,addrsize);
}

int my_rtt(int socket) {
    return estimated_rtt;
}

void my_send(int sock, void *buf, size_t len)
{
    /* Buffer for the packet. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));

    struct lab5_hdr *hdr = (struct lab5_hdr *) packet;

    /* Set the header fields as needed. */
    hdr->sequence_number = htonl(sequence_number);
    hdr->ack_number = htonl(0);
    hdr->flags = htonl(0);

    /* Copy the payload into our packet.  hdr + 1 is the address of the
     * first byte after the header. */
    memcpy(hdr+1,buf,len);

    int send_time = current_msec();

    int bytes_sent = send(sock, packet, sizeof(struct lab5_hdr) + len, 0);

    fprintf(stderr, "Sent packet seq=%d, len=%zu, bytes_sent=%d\n", sequence_number, len, bytes_sent);

    char ack_packet[MAX_PACKET];
    struct lab5_hdr  * ack_hdr = (struct lab5_hdr * ) ack_packet;

    // Time out calculations
    int timeout_ms = estimated_rtt + 4 * dev_rtt + 15;
    struct timeval timeout;
    msec_to_timeval(timeout_ms, &timeout);

    // setup select for the timout
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, & read_fds);


    int retries = 0;
    int max_retries = 5;

    while (retries < max_retries) {

        FD_ZERO(&read_fds);        
        FD_SET(sock, &read_fds);

        msec_to_timeval(timeout_ms, &timeout);

        int select_result = select(sock + 1, &read_fds, NULL, NULL, &timeout);

        if (select_result > 0) {

        struct sockaddr_in fromaddr;
        socklen_t addrlen = sizeof(fromaddr);
        int recv_count = recvfrom(sock, ack_packet, MAX_PACKET, 0,
            (struct sockaddr *) &fromaddr, &addrlen);

        if (recv_count >= sizeof(struct lab5_hdr)) {   

         int ackno = ntohl(ack_hdr->ack_number);   
         if (ackno != sequence_number) {
                fprintf(stderr, "Wrong ACK=%d (expected %d), ignoring\n",
                 ackno, sequence_number);
                    continue;
                }

     int recv_time = current_msec();
        int sample_rtt = recv_time - send_time;

        if (retries == 0){
             estimated_rtt = (7 * estimated_rtt + sample_rtt) / 8;
            int diff = sample_rtt - estimated_rtt;
            if(diff < 0) diff = -diff;
            dev_rtt = (3 * dev_rtt + diff) / 4;
            fprintf(stderr, "RTT update: sample=%d, est=%d, dev=%d\n",
                sample_rtt, estimated_rtt, dev_rtt);
                }

            sequence_number++;
            expected_ack++;
            return;
            }
        }
        else if (select_result == 0){       

            fprintf(stderr,
                    "Timeout on seq=%d, retransmitting (retry %d/%d)\n",
                    sequence_number, retries + 1, max_retries);

            send(sock, packet, sizeof(struct lab5_hdr) + len, 0);

            timeout_ms *= 2;
            retries++;
        }
        else {
            perror("select");
            return;
        }
    }

    fprintf(stderr, "ERROR: Failed to receive ACK after %d retries\n",
            max_retries);
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



    if (recv_count < sizeof(struct lab5_hdr)){
        fprintf(stderr, "Received too small packet\n");
        return -1;
    }
    /* Associate this UDP socket with a remote address.  This is simply
     * for convenience so that we can use send() rather than sendto(), since
     * sendto() requires an additional argument that we don't want to deal with
     * every time we need to send something. */
    if(connect(sock, (struct sockaddr *) &fromaddr, addrlen)) {
        perror("connect (my_recv)");
    }

    int recv_seq = ntohl(hdr->sequence_number);
    int recv_flags = ntohl(hdr->flags );

    fprintf(stderr, "Received packet seq=%d(expecting %d), flags= 0x%x\n", 
    recv_seq, expected_seq, recv_flags);

    if(recv_flags & FLAG_FIN){
        fprintf(stderr, "Received FIN packet\n");
        char ack_packet[MAX_PACKET];
        struct lab5_hdr *ack_hdr = (struct lab5_hdr *) ack_packet;
        ack_hdr->sequence_number = htonl(0);
        ack_hdr->ack_number = htonl(recv_seq);
        ack_hdr->flags = htonl(0);

        send(sock, ack_packet, sizeof(struct lab5_hdr),0);
        fprintf(stderr, "sent ACK for FIN\n");

        return 0;
    }

    if(recv_seq == expected_seq){
        char ack_packet[MAX_PACKET];
        struct lab5_hdr *ack_hdr = (struct lab5_hdr *) ack_packet;
        ack_hdr->sequence_number = htonl(0);
        ack_hdr->ack_number = htonl(expected_seq);
        ack_hdr->flags = htonl(0);

        send(sock, ack_packet, sizeof(struct lab5_hdr), 0);
        fprintf(stderr, "Sent ACK=%d\n", expected_seq);

        expected_seq++;

        int payload_size = recv_count - sizeof(struct lab5_hdr);
        memcpy(buf , packet + sizeof(struct lab5_hdr), payload_size);
        return payload_size;


    }
    else{
        fprintf(stderr, "Out of order packet: got seq=%d, expected=%d\n",
        recv_seq, expected_seq);

        if(recv_seq < expected_seq){
            char ack_packet[MAX_PACKET];
            struct lab5_hdr *ack_hdr = (struct lab5_hdr * ) ack_packet;
            ack_hdr->sequence_number = htonl(0);
            ack_hdr->ack_number = htonl(recv_seq);
            ack_hdr->flags = htonl(0);

            send(sock, ack_packet, sizeof(struct lab5_hdr),0);
            fprintf(stderr, "Re-sent ACK=%d for duplicate\n", recv_seq);


        }
        return my_recv(sock,buf,length);

    }

    

}


   

int my_close(int sock) {
    /* In this function, you probably want to do different things depending on
     * whether your are the first to call close (i.e., active close at the
     * sender) vs.  the passive receiver. */
    fprintf(stderr, "closing connection...\n");

    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));
    struct lab5_hdr *hdr = (struct lab5_hdr *) packet;

    hdr->sequence_number = htonl(0);
    hdr->ack_number = htonl(recv_seq);
    hdr->flags = htonl(0);

    send(sock, packet, sizeof(struct lab5_hdr),0);
    fprintf(stderr, "sent FIN packet seq=%d\n", sequence_number);

    char ack_packet[MAX_PACKET];
    struct timeval timeout;
    msec_to_timeval(1000, &timeout);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    int select_result = select(sock+1, &read_fds, NULL, NULL, &timeout);
    if(select_result > 0){
        recvfrom(sock, ack_packet, MAX_PACKET, 0, NULL, NULL);
        fprintf(stderr, "Received ACK for FIN\n");

    }
    else{
        fprintf(stderr, "Timeout waiting for FIN ACK(continuing anyway)\n");
    }


    

    /* Send an empty packet to signal end of file. */
    my_send(sock, NULL, 0);

    return close(sock);
}
