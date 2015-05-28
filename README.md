RUDP
====

This is a simple implementation of the RUDP protocol for the IK2213 course. RUDP is a protocol just in top of UDP that makes it reliable. It supports sliding window flow control, ARQ-based packet retransmissions and reordering of packets that arrive out of order. The program is composed of an event handler calling the RUDP fonctions.

How to compile
-----------------------
Just run "make" in order to compile the program. You can change the compilation by editing the makefile


How to configure
-----------------------
Parameters are availalbe in the file rudp.h. You can change the version of RUDP used, the maximal size of a packet, the maximal number of retransmit, the timeout for the retransmission and the size of the sliding window which is the maximal number of unacknowledged packets that can be sent to the network.


How to use
-----------------------
You can use the simple applications vs_send and vs_recv that send and receive a file using RUDP.

On the receiver side: ./vs_recv -d port

On the sender side: ./vs_send -d address:port file

You can write other applications with RUDP using the fonctions define in the rudp_api.h file.