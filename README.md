# Reliable UDP Transport in C

A reliable, in-order transport protocol built on top of UDP. UDP can drop, delay, or reorder packets; this library adds sequence numbers, acknowledgments, timeouts, retransmission, RTT estimation, and a clean connection teardown so a file sent from one machine arrives intact on another.

Built with [Germain Hirwa](https://github.com/ghirwa1) for CS 43: Computer Networks at Swarthmore College.

**Stack:** C · POSIX sockets · `select()` · Make

---

## How it works

The sender and receiver talk through a small socket-style API (`my_socket`, `my_connect`, `my_send`, `my_recv`, `my_close`, `my_rtt`). Every packet carries a 12-byte header:

```c
struct lab5_hdr {
    uint32_t sequence_number;
    uint32_t ack_number;
    uint32_t flags;
};
```

Packets are at most 1,400 bytes, so each one carries up to 1,388 bytes of data.

**Stop-and-wait delivery.** The sender transmits one segment, then waits for an ACK with the matching sequence number before sending the next. The receiver ACKs what it got and only passes the next expected segment up to the application, so data arrives in order.

**Timeouts with `select()`.** The sender waits for an ACK with `select()` and a timeout instead of blocking forever. If the timer expires, it retransmits and **doubles the timeout** (exponential backoff), up to 5 retries.

**RTT estimation.** The timeout adapts to the network using the same smoothing TCP uses:

```
EstimatedRTT = 7/8 · EstimatedRTT + 1/8 · SampleRTT
DevRTT       = 3/4 · DevRTT       + 1/4 · |SampleRTT − EstimatedRTT|
Timeout      = EstimatedRTT + 4 · DevRTT
```

Samples are only taken from segments that were ACKed on the first try (Karn's algorithm), so a retransmission can't be mistaken for a slow round trip.

**Connection teardown.** When the sender is done, it sends an empty segment as a FIN and waits for it to be ACKed, retransmitting with backoff if needed. Once the FIN is acknowledged, both sides agree the connection is closed and the receiver stops.

## Build

```bash
make
```

This builds `lab5_sender` and `lab5_receiver` with `-Wall -Werror`.

## Run

Start the receiver, writing whatever it receives to a file:

```bash
./lab5_receiver 9000 > received.txt
```

From another machine, send a file (the sender binds the same port number locally, so run it on a different host):

```bash
./lab5_sender <receiver-ip> 9000 < 1000lines
```

Both sides print the transfer size, time, and throughput; the sender also prints its final RTT estimate.

## Test

Confirm the file arrived byte for byte:

```bash
md5sum 1000lines received.txt
```

`generate_testfiles.py` creates larger inputs (10,000, 100,000, and 1,000,000 lines) for longer transfers.

## Files

| File | Purpose |
|---|---|
| `lab5.c` | The protocol: sending, receiving, ACKs, timeouts, RTT estimation, teardown |
| `lab5.h` | Packet header and API |
| `lab5_sender.c` | Reads stdin and sends it over the protocol |
| `lab5_receiver.c` | Receives data and writes it to stdout |
| `generate_testfiles.py` | Generates test input files |

## Next steps

- Sliding window (Go-Back-N or Selective Repeat) to keep multiple packets in flight
- Duplicate detection on the receiver so retransmitted segments are re-ACKed correctly
- Checksums to catch corrupted packets
