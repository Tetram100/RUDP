#!/usr/bin/env python

"""
vs_recv: a simple RUDP receiver to receive files from remote hosts.
It takes only one argument - local port to be used.

"""
import sys
import os
import socket
import getopt
import time
import struct
import re

import event
import vsftp
import rudp

# define some global variables
debug = False
rxfiles = {}

def usage():
    print "Usage: vs_recv.py [-d] port"
    sys.exit(1)

def main():
    global debug
    
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
        if len(args) == 1:
            port = int(args[0])
        else:
            usage()

        rsock = rudp.rudp_socket(port)
    # Catch errors in argument parsing
    except (getopt.GetoptError, socket.error, SyntaxError) as err:
        print str(err)
        usage()
    # Register receiver and event handler callback functions
    rudp.rudp_recvfrom_handler(rsock, recvfromhandler)
    rudp.rudp_event_handler(rsock, eventhandler)
    
    if debug:
        print "RUDP receiver waiting on port %d" % (port)
    # Hand over control to event manager
    event.eventloop()

def eventhandler(rsocket, event, remote):
    global rxfiles
    
    if event == rudp.RUDP_EVENT_TIMEOUT:
        print "vs_recv: timeout in communication with %s:%d" % (ip, p)
        if remote and rxfiles.has_key(remote):
            (filename, fd) = rxfiles[remote]
            fd.close()
            del rxfiles[remote]
    elif event == rudp.RUDP_EVENT_CLOSED:
        if remote and rxfiles.has_key(remote):
            print "vs_recv: communication closed too soon with %s:%d" % (ip, p)
            (filename, fd) = rxfiles[remote]
            fd.close()
            del rxfiles[remote]
        # else ignore
    else:
        print "vs_recv: event handler received unknown event %d" % (event)
    return 0

def recvfromhandler(rsocket, remote, buf):
    global rxfiles

    vsm = vsftp.vsftp()
    vsm.unpack(buf)
    if vsm.vs_type == vsftp.VS_TYPE_BEGIN:
		# Verify that file name is valid. Only alpha-numerical, period, dash and
        # underscore are allowed
        filename = vsm.vs_filename
        if re.search("[^a-zA-Z0-9_\-\.]", filename):
            raise ValueError("Invalid character(s) in VSFTP file name (%s)" % (vsm.vs_filename))
        if debug:
            (ip, p, f, c) = remote
            print "vs_recv: BEGIN \"%s\" (%d bytes) from %s:%d" % (filename, len(buf), ip, p)
        fd = open(filename, "wb")
        rxfiles[remote] = (filename, fd)

    elif vsm.vs_type == vsftp.VS_TYPE_DATA:
        if debug:
            (ip, p, f, c) = remote
            print "vs_recv: DATA  (%d bytes) from %s:%d" % (len(buf), ip, p)
        (filename, fd) = rxfiles[remote]
        fd.write(vsm.vs_data)

    elif vsm.vs_type == vsftp.VS_TYPE_END:
        if debug:
            (ip, p, f, c) = remote
            print "vs_recv: END  (%d bytes) from %s:%d" % (len(buf), ip, p)
        (filename, fd) = rxfiles[remote]
        fd.close()
        del rxfiles[remote]
    return 0

if __name__ == "__main__": main()
