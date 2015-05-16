"""
RUDP protocol.
"""


# RUDP events
RUDP_EVENT_TIMEOUT = 0
RUDP_EVENT_CLOSED = 1

# Protocol constants

RUDP_MAXPKTSIZE = 1000  # Number of data bytes that can sent in a packet, RUDP header not included 
RUDP_MAXRETRANS = 5     # Max. number of retransmissions 
RUDP_TIMEOUT =	2000    # Timeout for the first retransmission in milliseconds 
RUDP_WINDOW =	3       # Max. number of unacknowledged packets that can be sent to the network

# Header constants
RUDP_VERSION = 1		# Protocol version 
# Packet types 
RUDP_DATA =	1
RUDP_ACK  =	2
RUDP_SYN  =	4
RUDP_FIN  =	5

# Functions for sequence number comparison

def SEQ_LT(a, b):
        return (a < b and b - a < 2**(32 - 1)) or (a > b and a - b > 2**(32 - 1))
def	SEQ_LEQ(a,b):
    	return a == b or SEQ_LT(a, b)
def SEQ_GT(a, b):
    return (a < b and b - a > 2**(32 - 1)) or (a > b and a - b < 2**(32 - 1))
def SEQ_GEQ(a, b):
    	return a == b or SEQ_GT(a, b)    

def rudp_socket(port):
    """
    Create a RUDP socket. May use a random port by setting port to zero.
    """

def rudp_close(rsock):
    """
    Close a RUDP socket, previosuly opened by rudp_socket()
    """

def rudp_sendto(rsock, data, to):
    """
    Send a datagram.

    Data is passed as a string. The "to" parameter is the destination, given
    as an AF_INET6 socket address, as returned by getaddrinfo(). That is,
    a 4-tuple consisting of IPv6-address, port number, flow, and scope id.
    """
    
def rudp_recvfrom_handler(rsock, recvfromhandler):
    """
    Register a handler function (callback function) for packet reception.
    The handler function will be called whenever RUDP data is available
    on the RUDP socket.
    """
    
def rudp_event_handler(rsock, recvfromhandler):
    """
    Register a handler function (callback function) for RUDP events.
    The handler function will be called whenever a RUDP event happens
    on the RUDP socket.
    """
    
