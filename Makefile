TARGETS=lab5_sender lab5_receiver
CFLAGS=-O0 -g -Wall -Wvla -Werror -Wno-error=unused-variable

all: $(TARGETS)

lab5_sender: lab5_sender.c lab5.c
	gcc $(CFLAGS) -o lab5_sender lab5_sender.c lab5.c

lab5_receiver: lab5_receiver.c lab5.c
	gcc $(CFLAGS) -o lab5_receiver lab5_receiver.c lab5.c

clean:
	rm -f $(TARGETS)
