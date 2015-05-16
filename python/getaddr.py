"""
Parse a "host:port" string, and generate socket address structure.
"""

import socket

def getaddr(str):
    """
    Parse a "host:port" string, and generate socket address structure.

    Returns a sockaddr tuple for a host:port string, otherwise None
    "port" is a decimal port number, or a service name.
    valid formats for "host":
    - domain (DNS) name "host.domain"
    - literal IPv4 address "1.2.3.4"
    - literal IPv6 address within brackets "[20:30:40::1]"
    - empty string"""
    
    import re
    # IPv6 literal
    m = re.compile('\[(.*)\]:(.*)').match(str)
    if  m:
        hoststr, servstr = m.group(1), m.group(2)
    else:
        # Domain name or IPv4 literal
        m = re.compile('(.*):(.*)').match(str)
        if  m:
            hoststr, servstr = m.group(1), m.group(2)
        else:
            return None

    # Request an IPv6 address for a datagram UDP socket address.
    # Both IPv4 and IPV6 addresses are accepted, with IPv4 addresses represented
    # as IPv4-mapped IPv6 addresses
    res = socket.getaddrinfo(hoststr, servstr, socket.AF_INET6, socket.SOCK_DGRAM,
                             socket.IPPROTO_UDP, socket.AI_V4MAPPED)
    # Use first IPv6 address
    for (family, socktype, proto, canonname, sockaddr) in res:
        if family == socket.AF_INET6: return sockaddr
    return None
