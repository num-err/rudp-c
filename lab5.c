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
int expected_seq = 0;

// The rtt variables showing RTT state
int estimated_rtt = 200;
int dev_rtt = 50;

/* The following three helper functions make dealing with time values a bit
 * easier.  You probably don't need to modify them, but you can if you want. */
int timeval_to_msec(struct timeval *t)
{
    return t->tv_sec * 1000 + t->tv_usec / 1000;
}

void msec_to_timeval(int millis, struct timeval *out_timeval)
{
    out_timeval->tv_sec = millis / 1000;
    out_timeval->tv_usec = (millis % 1000) * 1000;
}

int current_msec()
{
    struct timeval t;
    gettimeofday(&t, 0);
    return timeval_to_msec(&t);
}

/* End time functions. */
int my_socket(int domain, int type, int protocol)
{
    sequence_number = 0;
    expected_seq = 0;

    estimated_rtt = 500;
    dev_rtt = 0;

    return socket(domain, type, protocol);
}

int my_connect(int socket, struct sockaddr_in *toaddr, int addrsize)
{
    return connect(socket, (struct sockaddr *)toaddr, addrsize);
}

int my_rtt(int socket)
{
    return estimated_rtt;
}

void my_send(int sock, void *buf, size_t len)
{
    /* Buffer for the packet. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));

    struct lab5_hdr *hdr = (struct lab5_hdr *)packet;

    /* Set the header fields as needed. */
    hdr->sequence_number = htonl(sequence_number);
    hdr->ack_number = htonl(0);
    hdr->flags = htonl(0);

    /* Copy the payload into our packet.  hdr + 1 is the address of the
     * first byte after the header. */
    if (len > 0 && buf != NULL)
    {
        memcpy(hdr + 1, buf, len);
    }

    int send_time = current_msec();

    send(sock, packet, sizeof(struct lab5_hdr) + len, 0);
    fprintf(stderr, "Sent packet seq=%d\n", sequence_number);

    /* Ack buffer */
    char ack_packet[MAX_PACKET];
    memset(ack_packet, 0, sizeof(ack_packet));
    struct lab5_hdr *ack_hdr = (struct lab5_hdr *)ack_packet;

    int retries = 0;
    int max_retries = 5;
    int timeout_ms = estimated_rtt + 4 * dev_rtt + 15;

    while (retries < max_retries)
    {

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        struct timeval timeout;
        msec_to_timeval(timeout_ms, &timeout);

        int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);

        if (ready > 0)
        {

            struct sockaddr_in fromaddr;
            socklen_t addrlen = sizeof(fromaddr);

            int recv_count = recvfrom(sock, ack_packet, MAX_PACKET, 0,
                                      (struct sockaddr *)&fromaddr, &addrlen);

            if (recv_count >= sizeof(struct lab5_hdr))
            {
                int ackno = ntohl(ack_hdr->ack_number);

                if (ackno == sequence_number)
                {
                    int recv_time = current_msec();
                    int sample = recv_time - send_time;

                    if (retries == 0)
                    {
                        estimated_rtt = (7 * estimated_rtt + sample) / 8;
                        int diff = sample - estimated_rtt;
                        if (diff < 0)
                            diff = -diff;
                        dev_rtt = (3 * dev_rtt + diff) / 4;

                        fprintf(stderr,
                                "RTT update: sample=%d est=%d dev=%d\n",
                                sample, estimated_rtt, dev_rtt);
                    }

                    sequence_number++;
                    return;
                }

                fprintf(stderr, "Ignoring wrong ACK=%d\n", ackno);
            }
        }
        else if (ready == 0)
        {
            fprintf(stderr,
                    "Timeout on seq=%d, retransmitting (retry %d/%d)\n",
                    sequence_number, retries + 1, max_retries);

            send(sock, packet, sizeof(struct lab5_hdr) + len, 0);

            timeout_ms *= 2;
            retries++;
        }
        else
        {
            perror("select");
            return;
        }
    }

    fprintf(stderr,
            "ERROR: Failed to receive ACK after %d retries\n",
            max_retries);
}

int my_recv(int sock, void *buf, size_t length)
{
    /* Buffer for the packet. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));
    struct lab5_hdr *hdr = (struct lab5_hdr *)packet;

    struct sockaddr_in fromaddr;
    socklen_t addrlen = sizeof(fromaddr);

    int recv_count = recvfrom(sock, packet, MAX_PACKET, 0,
                              (struct sockaddr *)&fromaddr, &addrlen);

    if (recv_count < sizeof(struct lab5_hdr))
    {
        fprintf(stderr, "Received too small packet\n");
        return -1;
    }
    /* Associate this UDP socket with a remote address.  This is simply
     * for convenience so that we can use send() rather than sendto(), since
     * sendto() requires an additional argument that we don't want to deal with
     * every time we need to send something. */
    if (connect(sock, (struct sockaddr *)&fromaddr, addrlen))
    {
        perror("connect (my_recv)");
    }

    int seq_net = ntohl(hdr->sequence_number);
    fprintf(stderr, "Got packet %d\n", seq_net);

    char ack_packet[MAX_PACKET];
    memset(ack_packet, 0, sizeof(ack_packet));
    struct lab5_hdr *ack_hdr = (struct lab5_hdr *)ack_packet;

    ack_hdr->ack_number = htonl(expected_seq);
    ack_hdr->flags = htonl(0);

    send(sock, ack_packet, sizeof(struct lab5_hdr), 0);
    fprintf(stderr, "Sent ACK=%d\n", expected_seq);

    if (seq_net != expected_seq)
    {
        fprintf(stderr, "Out-of-order packet. Expected=%d got=%d\n",
                expected_seq, seq_net);
        return -1;
    }

    expected_seq++;

    /* Copy the payload into the user-supplied buffer. */
    int payload_size = recv_count - sizeof(struct lab5_hdr);
    if (payload_size > 0 && buf != NULL)
    {
        memcpy(buf, packet + sizeof(struct lab5_hdr), payload_size);
    }

    return payload_size;
}

int my_close(int sock)
{
    /* In this function, you probably want to do different things depending on
     * whether your are the first to call close (i.e., active close at the
     * sender) vs.  the passive receiver. */
    char packet[MAX_PACKET];
    memset(packet, 0, sizeof(packet));

    struct lab5_hdr *hdr = (struct lab5_hdr *)packet;
    hdr->sequence_number = htonl(sequence_number);
    hdr->ack_number = htonl(0);
    hdr->flags = htonl(0);

    int retries = 0;
    int max_retries = 5;
    int timeout_ms = estimated_rtt + 4 * dev_rtt;
    if (timeout_ms < 100)
        timeout_ms = 100;

    int send_time = current_msec();
    send(sock, packet, sizeof(struct lab5_hdr), 0);

    while (retries < max_retries)
    {

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        struct timeval tv;
        msec_to_timeval(timeout_ms, &tv);

        int s = select(sock + 1, &readfds, NULL, NULL, &tv);

        if (s > 0)
        {
            /* Check for ACK of our FIN. */
            char ack_buf[MAX_PACKET];
            memset(ack_buf, 0, sizeof(ack_buf));
            struct lab5_hdr *ah = (struct lab5_hdr *)ack_buf;
            struct sockaddr_in fromaddr;
            socklen_t alen = sizeof(fromaddr);

            int r = recvfrom(sock, ack_buf, MAX_PACKET, 0,
                             (struct sockaddr *)&fromaddr, &alen);

            if (r >= sizeof(struct lab5_hdr))
            {

                int ackno = ntohl(ah->ack_number);

                if (ackno == sequence_number)
                {
                    /* FIN ACKed */
                    sequence_number++;

                    /* Update RTT using FIN ACK */
                    int recv_time = current_msec();
                    int sample_rtt = recv_time - send_time;

                    estimated_rtt = (7 * estimated_rtt + sample_rtt) / 8;
                    int diff = sample_rtt - estimated_rtt;
                    if (diff < 0)
                        diff = -diff;
                    dev_rtt = (3 * dev_rtt + diff) / 4;

                    return close(sock);
                }
            }
        }
        else if (s == 0)
        {
            /* timeout */
            retries++;
            timeout_ms *= 2;

            send(sock, packet, sizeof(struct lab5_hdr), 0);
            continue;
        }
        else
        {
            perror("select (my_close)");
            break;
        }
    }

    fprintf(stderr, "Warning: FIN not acknowledged after %d retries\n",
            max_retries);

    /* Close anyway. */
    return close(sock);
}