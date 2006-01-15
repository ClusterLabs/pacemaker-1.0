#!/usr/bin/env python

'''Heartbeat related classes.

What we have here is a handful of classes related to the
heartbeat cluster membership services.

These classes are:
    ha_msg:  The heartbeat messaging class
    hb_api:  The heartbeat API class
 '''

__copyright__='''
Copyright (C) 2000 Alan Robertson <alanr@unix.sh>
Licensed under the GNU GPL.
'''

#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

import types, string, os, sys
from UserDict import UserDict
import select

class ha_msg (UserDict): 

    '''ha_msg is the Heartbeat messaging class.  It is the bottle into
    which you put messages before throwing them out onto the sea
    of cluster :-)  Not surprisingly, it is also the bottle which you
    receive them in.  It is also the way you communicate with heartbeat
    itself using its API

    All heartbeat messages are name value pairs (~Python dicts)
    Not too surprisingly, this is much nicer in python than in 'C'

    These objects are the fundamental units of heartbeat communication,
    and also the fundamental units of communication with heartbeat itself
    (via the heartbeat API).

    This class is basically a restricted dictionary type with a few
    minor twists to make it fit a little better into the heartbeat
    message paradigm.

    These twists are:
        We only allow strings as names and values
        We require a particular canonical string representation
            so we can transport them compatibly on the network
        We allow a wide variety of __init__() and update() args
            including strings in our canonical network format
        See the update member function for more details.
        We are picky about what kinds of things you want to shove into
            our bottle.  Everything needs to be strings, and need to be
            somewhat restricted in content from the Python point of view.
            For example, no nulls, no newlines, etc.

    Constructor arguments:
        dictionaries, ha_msg objects, 2-element lists/tuples, files
	strings (in canonical msg format)

    Exceptions raised:

    ValueError:
       For every bad parameter we see, we raise a ValueError.
       This can happen when the string you've given us doesn't
       meet our expectations in various ways.  Be prepared to deal with it
       when you give us messages you can't guarantee are perfect.
    '''

    #	Field names start with F_...

    F_TYPE="t"
    F_ORIG="src"
    F_NODE="node"
    F_TO="dest"
    F_FROMID="from_id"
    F_IFNAME="ifname"
    F_NODENAME="node"
    F_TOID="to_id"
    F_PID="pid"
    F_STATUS="st"
    F_APIREQ="reqtype"
    F_APIRESULT="result"
    F_COMMENT="info"

    #	Message types start with T_...

    T_APIREQ="hbapi-req"
    T_APIRESP="hbapi-resp"
    T_TESTREQ="cltest-req"
    T_TESTRSP="cltest-rsp"


    #
    #   Things we need for making network-compatible strings
    #   from ha_msg objects
    #
    max_reprlen = 1024	# Maximum length string for an ha_msg
    startstr=">>>\n"
    endstr="<<<\n"
    __str__ = UserDict.__repr__	 # use default __str__ function


    def __init__(self, *args):

    	'''Initialize the ha_msg according to the parameters we're given'''

        self.data = {}
	for arg in args:
            self.update(arg)

    def update(self, *args):

        '''Update the message from info in our arguments
           We currently allow these kinds of arguments:
             dictionary, ha_msg, tuple, list, string, file...
	'''
#
#	It would be nice to check for type attributes rather than
#	for specific types...
#
	for arg in args:

            # Do we have a String?
            if isinstance(arg, types.StringType):
                self.fromstring(arg)

            # Do we have a 2-element Tuple/List?
            elif (isinstance(arg, types.TupleType)
            or    isinstance(arg, types.ListType)):

                if len(arg) != 2: raise ValueError("wrong size tuple/list")
                self[arg[0]] = arg[1]

            # Do we have a dictionary or ha_msg object?
            elif (isinstance(arg, types.DictType)
            or   (isinstance(arg, types.InstanceType)
                  and issubclass(arg.__class__, UserDict))):

                for key in arg.keys():
		    self[key] = arg[key]

            # How about a file?
            elif isinstance(arg, types.FileType):
    		self.fromfile(arg)

            elif isinstance(arg, types.FileType):
    		self.fromfile(arg)
            else: 
	      raise ValueError("bad type in update")

#	I can imagine more validation being useful...
#	The strings have more constraints than this code enforces...
#	They can't contain NULLs, or \r or \n
#
#	The names should be legitimate environment var names
#	(for example, can't contain '=')
#	etc...

    def __setitem__(self, k, value):
        if (not isinstance(k, types.StringType)
        or  not isinstance(k, types.StringType)):
		raise ValueError("non-string data")
        self.data[k] = value

    def __repr__(self):

        '''Convert to the canonical network-format string
           that heartbeat expects us to use.
        '''

	ret = ha_msg.startstr
        for i in self.items():
            ret = ret + i[0] + "=" + i[1] + "\n"
	ret = ret + ha_msg.endstr

	if len(ret) <= ha_msg.max_reprlen:
            return ret
        raise ValueError("message length error")


    #   Convert from canonical-message-string to ha_msg

    def fromstring(self, s):

        '''Update an ha_msg from a string
           The string must be in our "well-known" network format
           (like comes from heartbeat or __repr__())
        '''

	#
	# It should start w/ha_msg.startstr, and end w/ha_msg.endstr
	#
	if  (s[:len(ha_msg.startstr)] != ha_msg.startstr
	or   s[-len(ha_msg.endstr):] != ha_msg.endstr) :
		raise ValueError("message format error")
        #
        # Split up the string into lines, and process each
	# line as a name=value pair
        #
	strings = string.split(s, '\n')[1:-2]
        for astring in strings:
            # Update-from-list is handy here...
	    self.update(string.split(astring, '='))

    def fromfile(self, f):

        '''Read an ha_msg from a file.
           This means that we read from the file until we find an ha_msg
           string, then plop it into 'self'
        '''

        delimfound=0
        while not delimfound: 
            line = f.readline()
            if line == "" : raise ValueError("EOF")
            delimfound = (line == ha_msg.startstr)

        delimfound=0

        line="?"
        while not delimfound and line != "":
            line = f.readline()
            if line == "" : raise ValueError("EOF")
            delimfound = (line == ha_msg.endstr)
	    if not delimfound: self.update(string.split(line[:-1], '='))

    def tofile(self, f):
        '''Write an ha_msg to a file, and flush it.'''
	f.write(repr(self))
	f.flush()
        return 1

class hb_api:
    '''The heartbeat API class.
    This interesting and useful class is a python client side implementation
    of the heartbeat API.  It allows one to inquire concerning the valid
    set of nodes and interfaces, and in turn allows one to inquire about the
    status of these things.  Additionally, it allows one to send messages to
    the cluster, and to receive messages from the cluster.
    '''
#
#	Probably the exceptions we trap should have messages that
#	go along with them, since they shouldn't happen.
#

    FIFO_BASE_DIR = "/var/lib/heartbeat/"
#
#	Various constants that are part of the heartbeat API
#
    SIGNON="signon"
    SIGNOFF="signoff"
    SETFILTER="setfilter"
    SETSIGNAL="setsignal"
    NODELIST="nodelist"
    NODESTATUS="nodestatus"
    IFLIST="iflist"
    IFSTATUS="ifstatus"
    ActiveStatus="active"

    OK="OK"
    FAILURE="fail"
    BADREQ="badreq"
    MORE="ok/more"
    _pid=os.getpid()
    API_REGFIFO     = FIFO_BASE_DIR + "register"
    NAMEDCLIENTDIR  = FIFO_BASE_DIR + "api"
    CASUALCLIENTDIR = FIFO_BASE_DIR + "casual"

    def __init__(self):
        self.SignedOn=0
        self.iscasual=1
        self.MsgQ = []
        self.Callbacks = {}
        self.NodeCallback = None
        self.IFCallback = None

    def __del__(self):
        '''hb_api class destructor.
        NOTE: If you're going to let an hb_api object go out of scope, and
        not sign off, then don't let it go out of scope from the highest
        level but instead make sure it goes out of scope from a function.
        This is because some of the classes this destructor needs may have
        already disappeared if you wait until the bitter end to __del__ us :-(
        '''
        print "Destroying hb_api object"
        self.signoff()

    def __api_msg(self, msgtype):

        '''Create a standard boilerplate API message'''

	return ha_msg(
           { ha_msg.F_TYPE   : ha_msg.T_APIREQ,
             ha_msg.F_APIREQ : msgtype,
             ha_msg.F_PID    : repr(hb_api._pid),
             ha_msg.F_FROMID : self.OurClientID
           })

    def __get_reply(self):

        '''Return the reply to the current API request'''

        try:

            while 1:
                reply = ha_msg(self.ReplyFIFO)
                if reply[ha_msg.F_TYPE] == ha_msg.T_APIRESP:
                    return reply
                # Not an API reply.  Queue it up for later...
                self.MsgQ.append(reply)

        except (KeyError,ValueError):
            return None

    def __CallbackCall(self, msg):
        '''Perform the callback calls (if any) associated with the given
           message.  We never do more than one callback per message.
           and we return true if we did any callbacks, and None otherwise.
        '''

        msgtype = msg[ha_msg.F_TYPE]

        if self.NodeCallback and (msgtype == ha_msg.T_STATUS
        or                        msgtype == T_NS_STATUS):
            node=msg[ha_msg.F_ORIG]
            self.NodeCallback[0](node, self.NodeCallback[1])
            return 1

        if self.IFCallback and msgtype == ha_msg.T_IFSTATUS:
            node=msg[ha_msg.F_ORIG]
            stat=msg[ha_msg.F_STATUS]
            self.IFCallback[0](node, stat, self.IFCallback[1])
            return 1

        if self.Callbacks.has_key(msgtype):
            entry = self.Callbacks[msgtype]
            entry[0](msg, entry[1])
            return 1

        return None

    def __read_hb_msg(self, blocking):

        '''Return the next message from heartbeat.'''

        if len(self.MsgQ) > 0:
            return self.MsgQ.pop(0)

        if not blocking and not self.msgready():
            return None

        try:
            return ha_msg(self.ReplyFIFO)
        except (ValueError):
            return None

        
    def readmsg(self, blocking):

        '''Return the next message to the caller for which there were no active
           callbacks.  Call the callbacks for those messages which might
           have been read along the way that *do* have callbacks.
           Because this is Python, and this member function also replaces
           the 'rcvmsg' function in the 'C' code.
        '''

        while(1):
            rc=self.__read_hb_msg(blocking)

            if rc == None: return None
            
            if not self.__CallbackCall(rc):
                return rc

    def signoff(self):

        '''Sign off of the heartbeat API.'''

        if self.SignedOn:
            msg = self.__api_msg(hb_api.SIGNOFF)
	    msg.tofile(self.MsgFIFO)
        self.SignedOn=0


    def signon(self, service=None):

        '''Sign on to heartbeat (register as a client)'''

        if service == None:
            self.OurClientID = repr(hb_api._pid)
            self.iscasual = 1
        else:
            self.OurClientID = service
            self.iscasual = 0
        msg = self.__api_msg(hb_api.SIGNON)

        # Compute FIFO directory

        if self.iscasual:
            self.FIFOdir = hb_api.CASUALCLIENTDIR
        else:
            self.FIFOdir = hb_api.NAMEDCLIENTDIR

        self.ReqFIFOName = self.FIFOdir + os.sep + self.OurClientID + ".req"
        self.ReplyFIFOName = self.FIFOdir + os.sep + self.OurClientID + ".rsp"
        self.OurNode = lower(os.uname()[1])
           
        #
        # For named clients, lock the request/response fifos
	# (FIXME!!)
        #
        if self.iscasual:
            # Make the registration, request FIFOs
            os.mkfifo(self.ReqFIFOName, 0600)
            os.mkfifo(self.ReplyFIFOName, 0600)
        #
        # Open the reply FIFO with fdopen...
	#	(this keeps it from hanging)

	fd = os.open(self.ReplyFIFOName, os.O_RDWR)
        self.ReplyFIFO = os.fdopen(fd, "r")
        
	msg = hb_api.__api_msg(self, hb_api.SIGNON)

        # Open the registration FIFO
        RegFIFO = open(hb_api.API_REGFIFO, "w");

        # Send the registration request
	msg.tofile(RegFIFO)
	RegFIFO.close()
        
        try:
            # Read the reply
            reply = self.__get_reply()

	    # Read the return code
            rc =  reply[ha_msg.F_APIRESULT]

            if rc == hb_api.OK :
                self.SignedOn=1
                self.MsgFIFO = open(self.ReqFIFOName, "w")
                return 1
            return None

        except (KeyError,ValueError):
            return None

    def setfilter(self, fmask):

        '''Set message reception filter mask
        This is the 'raw' interface.  I guess I should implement
        a higher-level one, too... :-)
        '''

        msg = hb_api.__api_msg(self, hb_api.SETFILTER)
        msg[ha_msg.F_FILTERMASK] = "%x" % fmask
	msg.tofile(self.MsgFIFO)

        try:
            reply = self.__get_reply()
            rc =  reply[ha_msg.F_APIRESULT]

            if rc == hb_api.OK:
                return 1
            return None

        except (KeyError, ValueError):
            return None

    def setsignal(self, signal):

        '''Set message notification signal (0 to cancel)'''

        msg = hb_api.__api_msg(self, hb_api.SETSIGNAL)
        msg[ha_msg.F_SIGNAL] = "%d" % signal

	msg.tofile(self.MsgFIFO)

        try:
            reply = self.__get_reply()

            rc =  reply[ha_msg.F_APIRESULT]

            if rc == hb_api.OK :
                return 1
            return None

        except (KeyError, ValueError):
            return None

    def nodelist(self):

        '''Retrieve the list of nodes in the cluster'''

        Nodes = []
	msg = hb_api.__api_msg(self, hb_api.NODELIST)

	msg.tofile(self.MsgFIFO)

        try:
            while 1:
                reply = self.__get_reply()
                rc =  reply[ha_msg.F_APIRESULT]
                if rc != hb_api.OK and rc != hb_api.MORE:
                    return None

                Nodes.append(reply[ha_msg.F_NODENAME])

                if rc == hb_api.OK :
                   return Nodes
                elif rc == hb_api.MORE:
                   continue
                else:
                  return None
        except (KeyError, ValueError):
            return None


    def iflist(self, node):

        '''Retrieve the list of interfaces to the given node'''

        Interfaces = []
	msg = hb_api.__api_msg(self, hb_api.IFLIST)
        msg[ha_msg.F_NODENAME] = node

	msg.tofile(self.MsgFIFO)

        try:
            while 1:
                reply = self.__get_reply()
                rc =  reply[ha_msg.F_APIRESULT]
                if rc != hb_api.OK and rc != hb_api.MORE :
                    return None

                Interfaces.append(reply[ha_msg.F_IFNAME])

                if rc == hb_api.OK :
                   return Interfaces
                elif rc == hb_api.MORE:
                   continue
                else:
                  return None
        except (KeyError, ValueError):
            return None


    def nodestatus(self, node):

        '''Retrieve the status of the given node'''

	msg = hb_api.__api_msg(self, hb_api.NODESTATUS)
	msg[ha_msg.F_NODENAME]=node


	msg.tofile(self.MsgFIFO)

        try:

            reply = self.__get_reply()
            rc =  reply[ha_msg.F_APIRESULT]

            if rc == hb_api.FAILURE : return None
  
            return reply[ha_msg.F_STATUS]

        except (KeyError, ValueError):
            return None

    def ifstatus(self, node, interface):

        '''Retrieve the status of the given interface on the given node'''

	msg = hb_api.__api_msg(self, hb_api.IFSTATUS)
	msg[ha_msg.F_NODENAME]=node
	msg[ha_msg.F_IFNAME]=interface

	msg.tofile(self.MsgFIFO)

        try:

            reply = self.__get_reply()
            rc =  reply[ha_msg.F_APIRESULT]

            if rc == hb_api.FAILURE : return None
  
            return reply[ha_msg.F_STATUS]

        except (KeyError, ValueError):
            return None

    def cluster_config(self):

        '''Return the whole current cluster configuration.
        This call not present in the 'C' API.
        It could probably give a better structured return value.
        '''

        ret = {}
        for node in self.nodelist():
            nstat = {}
            nstat["status"] = self.nodestatus(node)
            interfaces={}
            for intf in self.iflist(node):
                interfaces[intf] =  self.ifstatus(node, intf)
            nstat["interfaces"] = interfaces
            ret[node] = nstat
        return ret

    def nodes_with_status(self, status=None):
        '''Return the list of nodes with the given status.  Default status is
        hb_api.ActiveStatus (i.e., "active")
        '''
        if status == None: status=hb_api.ActiveStatus
        ret = []
        for node in self.nodelist():
            if self.nodestatus(node) == status:
                ret.append(node)
        return ret

    def get_inputfd(self):

        '''Return the input file descriptor associated with this object'''

        if not self.SignedOn: return None

        return self.ReplyFIFO.fileno()

    def fileno(self):
        return self.get_inputfd()

    def msgready(self):

        '''Returns TRUE if a message is waiting to be read.'''

        if len(self.MsgQ) > 0:
            return 1

        ifd = self.get_inputfd()
 
        inp, out, exc = select.select([ifd,], [], [], 0)

        if len(inp) > 0 : return 1
        return None

    def sendclustermsg(self, origmsg):

        '''Send a message to all cluster members.

         This is not allowed for casual clients.'''
        if not self.SignedOn or self.iscasual: return None

        msg =ha_msg(origmsg)
        msg[ha_msg.F_ORIG] = self.OurNode
	return msg.tofile(self.MsgFIFO)

    def sendnodemsg(self, origmsg, node):

        '''Send a message to a specific node in the cluster.
         This is not allowed for casual clients.'''

        if not self.SignedOn or self.iscasual: return None

        msg = ha_msg(origmsg)
        msg[ha_msg.F_ORIG] = self.OurNode
        msg[ha_msg.F_TO] = node

	return msg.tofile(self.MsgFIFO)


    def set_msg_callback(self, msgtype, callback, data):
     
        '''Define a callback for a specific message type.
           It returns the previous (callback,data) for
           that particular message type.
        '''

        if self.Callbacks.has_key(msgtype) :
            ret=self.Callbacks[msgtype]
        else:
            ret=None

        if callback == None :
            if self.Callbacks.has_key(msgtype) :
                del self.Callbacks[msgtype]
            return ret

        self.Callbacks[msgtype] = (callback, data)
        return ret

    def set_nstatus_callback(self, callback, data):

        '''Define a callback for node status changes.
           It returns the previous (callback,data) for
           the previous nstatus_callback.
        '''

        ret = self.NodeCallback
        if callback == None:
            self.NodeCallback = None
            return ret
           
        self.NodeCallback = (callback, data)
        return ret


    def set_ifstatus_callback(self, callback, data):

        '''Define a callback for interface status changes.
           It returns the previous (callback,data) for
           the previous ifstatus_callback.
        '''

        ret = self.IFCallback
        if callback == None:
            self.IFCallback = None
            return ret
           
        self.IFCallback = (callback, data)
        return ret


#
#   A little test code...
#
if __name__ == '__main__':

    hb = hb_api()
    hb.signon()
    print "Now signed on to heartbeat API..."
    print "Nodes in cluster:", hb.nodelist()

    for node in hb.nodelist():
        print "\nStatus of %s: %s" %  (node,  hb.nodestatus(node))
        print "\tInterfaces to %s: %s" % (node, hb.iflist(node))
        for intf in hb.iflist(node):
            print "\tInterface %s: %s" % (intf, hb.ifstatus(node, intf))

    print "\nCluster Config:"
    config = hb.cluster_config()
    print config

    print "\n"
    print config["localhost"]["interfaces"]["localhost"], ":-)"
    print config["kathyamy"]["interfaces"]["/dev/ttyS0"]

