VS_MINLEN = 4
VS_FILENAMELENGTH = 128
VS_MAXDATA = 128

VS_TYPE_BEGIN = 1
VS_TYPE_DATA = 2
VS_TYPE_END = 3

import struct

class vsftp(object):
    def __init__(self):
        self.vs_type = None
        self.vs_data = None
        self.vs_filename = None

    def pack(self):
        """
        Pack a message object into a string, for passing to the network.
        """
        if self.vs_type == VS_TYPE_BEGIN:
            ret = struct.pack("!I %ds" % (len(self.vs_filename)), self.vs_type, self.vs_filename)
        elif self.vs_type == VS_TYPE_DATA:
            ret = struct.pack("!I %ds" % (len(self.vs_data)), self.vs_type, self.vs_data)
        elif self.vs_type == VS_TYPE_END:
            ret = struct.pack("!I", self.vs_type)
        else:
            raise ValueError("Packing invalid VSFTP message type (%d)" % (self.vs_type))
        return ret

    def unpack(self, Buf):
        """
        Unpack a string from the network into a message object.
        """
        if len(Buf) < 4:
            raise ValueError("Too short VSFTP message (len %d)" % (len(Buf)))
        hdr=Buf[0:4]
        (type,)=struct.unpack("!I", hdr)
        self.vs_type = type
        if type == VS_TYPE_BEGIN:
            if len(Buf) < 5:
                raise ValueError("Too short VS_TYPE_BEGIN message (len %d)" % (len(Buf)))
            self.vs_filename = Buf[4:]
        elif type == VS_TYPE_DATA:
            self.vs_data = Buf[4:]
        elif type == VS_TYPE_END:
            if len(Buf) > 4:
                raise ValueError("Too long VS_TYPE_END message (len %d)" % (len(Buf)))
            pass
        else:
            raise ValueError("Invalid VSFTP message type (%d)" % (type))
