"""
Rudp event handling - registering file descriptors and timeouts
                      and eventloop using the select() system call.
Author: Peter Sjodin

Ported from the C version and follows its structure very closely.
"""

class event_data(object):
    def __init__(self, fun, arg, str):
        self.e_fun = fun   # Callback function
        self.e_arg = arg   # function argument
        self.e_str = str   # string for identification/debugging

class event_data_timeout(event_data):
    def __init__(self, fun, arg, str, time):
        super(event_data_timeout, self).__init__(fun, arg, str)
        self.e_time = time # Timout
        
class event_data_fd(event_data):
    def __init__(self, fun, arg, str, fd):
        super(event_data_fd, self).__init__(fun, arg, str)
        self.e_fd = fd     # File descriptor

import time
ee = [] 		# list of file events
ee_timers = []  # list of timer events

def event_timeout(timer, callback, callback_arg, idstr):
    """Create event and sort into internal event list.

    Given a timeout time from current in microseconds, register function to call."""

    e = event_data_timeout(callback, callback_arg, idstr, time.time() + timer/1000000)
    ee_timers.append(e)
    ee_timers.sort(key=lambda e: e.e_time)

def event_fd(fd, callback, callback_arg, idstr):
    """Register a callback function when something occurs on a file descriptor.

    When an input event occurs on file desriptor <fd>, 
    the function <fun> shall be called  with argument <arg>.
    <str> is a debug string for logging."""
    
    for i in range(len(ee)):
        if ee_[i].e_fd == fd:
            raise ValueError("Event handler already registered for this file descriptor")
    e = event_data_fd(callback, callback_arg, idstr, fd)
    ee.append(e)

def event_delete(elist, callback, callback_arg):
    for i in range(len(elist)):
        if (elist[i].e_fun == callback) and (elist[i].e_arg == callback_arg):
            elist.pop(i)
            break

def event_timeout_delete(callback, callback_arg):
    """Deregister a timeout event."""
    event_delete(ee_timers, callback, callback_arg)

def event_fd_delete(callback, callback_arg):
    """Deregister a timeout event."""
    event_delete(ee, callback, callback_arg)
                
def eventloop():
    """Main event loop. Dispatch file descriptor events and timeouts by invoking callbacks.

    Return when no more registered events exist, or when a callback returns a negative value."""
    import select
    
    while ee or ee_timers:
        fdset = map(lambda e: e.e_fd, ee)
        timeout = 0
        rlist = wlist = xlist = []
        if len(ee_timers) > 0:
            e = ee_timers[0]
            curtime = time.time()
            timeout = e.e_time - curtime
            if timeout > 0:
                (rlist, wlist, xlist) = select.select(fdset, [], [], timeout)
        else:
                (rlist, wlist, xlist) = select.select(fdset, [], [], 0)
        if (rlist == wlist == xlist == []):
            # Timeout 
            e = ee_timers[0]
            ee_timers.remove(e)
            e.e_fun(e.e_arg)
        elif len(rlist) > 0:
            events = filter(lambda e: e.e_fd in rlist, ee)
            # File event
            for e in events:
                e.e_fun(e.e_fd, e.e_arg)
    
        
