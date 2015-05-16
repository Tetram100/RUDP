#!/usr/bin/env python

"""
vs_send: A simple RUDP sender that can be used to transfer files.
Arguments: destination address * (dot quadded or host.domain),  
remote port number, and a list of files
"""
import sys
import os
import socket
import getopt
import time
import struct

import event
import vsftp
import rudp
import getaddr

# define some global variables
debug = False
peers = []

def usage():
    print "Usage: vs_send.py [-d] host1:port1 [host2:port2] ... file1 [file2] ..."
    sys.exit(1)

def main():
    global debug
    global peers
    
    if not len(sys.argv[1:]):
        usage()
    # read the commandline options
    try:
        opts, args = getopt.getopt(sys.argv[1:],"d")
        for o,a in opts:
            if o in ("-d"):
                debug = True
            else:
                assert False,"Unhandled Option"
        for a in args:
            sa = getaddr.getaddr(a)
            if not sa: break
            peers.append(sa)
    # Catch errors in argument parsing
    except (getopt.GetoptError, socket.error, SyntaxError) as err:
        print str(err)
        usage()
    
    # Need at least one peer
    if len(peers) == 0: usage()

    # Remainder of args is file names
    for a in args[len(peers):]:
        send_file(a)
    # Hand over control to event manager
    event.eventloop()

def eventhandler(rsocket, event, remote):
    if event == rudp.RUDP_EVENT_TIMEOUT:
        (ip, p, f, c) = remote
        print "vs_send: timeout in communication with %s:%d" % (ip, p)
        sys.exit(1)
    elif event == rudp.RUDP_EVENT_CLOSED:
        if debug:
            print "vs_send: socket closed"
    else:
        print "Unknown event %d" % (event)
    return 0

# VS sender does not receive data, really
def recvfrom_handler(rsocket, remote, buf):
    (ip, p, f, c) = remote
    print "vs_send: received data from %s:%d. This was unexpected." % (ip, p)
    return 0

def send_file(filename):
    """Initiate sending of a file with RUDP.

    Create a RUDP socket for sending. Send the file name to the VS receiver.
    Register a handler for input event, which will take care of sending
    file data"""

    f = open(filename, "rb") 

    rsock = rudp.rudp_socket(0)
    rudp.rudp_event_handler(rsock, eventhandler)
    rudp.rudp_recvfrom_handler(rsock, recvfrom_handler)    

    # Strip of any leading path name and create start packet
    vsm = vsftp.vsftp()
    vsm.vs_type = vsftp.VS_TYPE_BEGIN
    vsm.vs_filename = os.path.basename(filename)
    buf = vsm.pack()
    for p in peers:
        (address, port, flow, scope) = p
        if debug:
            print "vs_send: send BEGIN \"%s\" (%d bytes) to %s:%s" % (filename, len(buf), address, port)
        rudp.rudp_sendto(rsock, buf, p)
    # Launch event driven filesender to send file content
    event.event_fd(f, filesender, rsock, "filesender")
    
def filesender(file, rsock):
    """
    Callback function for handling sending of the file.
    
    Will be called when data is available on the file (which is always
    true, until the file is closed...). 
    Send file data. Detect end of file and tell VS peers that transfer is
    complete"""

    str = file.read(vsftp.VS_MAXDATA)
    if len(str) == 0: # end of file
        vsm = vsftp.vsftp()
        vsm.vs_type = vsftp.VS_TYPE_END
        buf = vsm.pack()
        for p in peers:
            if debug:
                (address, port, flow, scope) = p
                print "vs_send: send END (%d bytes) to %s:%s" % (len(buf), address, port)
            rudp.rudp_sendto(rsock, buf, p)
        event.event_fd_delete(filesender, rsock)
        file.close()
        rudp.rudp_close(rsock)
    else:
        vsm = vsftp.vsftp()
        vsm.vs_type = vsftp.VS_TYPE_DATA
        vsm.vs_data = str
        buf = vsm.pack()
        for p in peers:
            if debug:
                (address, port, flow, scope) = p
                print "vs_send: send DATA (%d bytes) to %s:%s" % (len(buf), address, port)
            rudp.rudp_sendto(rsock, buf, p)
    return 0
    
if __name__ == "__main__": main()
