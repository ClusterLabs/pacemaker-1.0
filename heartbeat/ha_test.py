#!/usr/bin/env python

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

from hb_api import *
import select

#
# This is an abstract class, you need to derive a class from it and set
# RequestType and ResponseType in the derived class
#
class RequestMsg (ha_msg):
    #RequestType  = ha_msg.T_TESTREQ
    #ResponseType = ha_msg.T_TESTRSP

    '''This class is a CTS request packet.  It is an 'abstract' class.
    To make a derived class from it, set derived_class.RequestType and
    and derived_class.ResponseType.
    '''

    def __init__(self, type):
        '''We initialize the packet type and request type.'''
        ha_msg.__init__(self)
        self.reqtype=type

        # Set the packet type and request type
        self.update(
           { ha_msg.F_TYPE  : self.__class__.RequestType,
             ha_msg.F_APIREQ: type
           })

    def WaitForReplies(self, api, nodelist, timeout):

        '''Wait for reply messages, and return them.
        The return is a 3-element tuple.  The first element of the tuple
        is a list of reply messages.  The second element of the tuple is
        the list of nodes that timed out.  The third element of the tuple
        is the list of replies that we didn't expect.  Unexpected replies are
        from machines not in 'nodes', or duplicates.
        '''

        nodes = {}
        for node in nodelist:
           nodes[node] = None
        fd = api.get_inputfd()
        replies=[]
        extras=[]

        while 1:
            #    Are there any messages waiting?
            inp,out,exc = select.select([fd,], [], [], timeout)

            if len(inp) > 0:

                # Read it.
                msg = api.readmsg(0) # May return None...

                try:
                    msgtype = msg[ha_msg.F_TYPE]
                    fromnode = msg[ha_msg.F_ORIG]
                    reqtype = msg[ha_msg.F_APIREQ]
                except (TypeError, KeyError):		continue

                if (msgtype != self.__class__.ResponseType
                or  reqtype != self.reqtype):
                    continue

                # Remember this message

                if nodes.has_key(fromnode):
                    del nodes[fromnode]
                    replies.append(msg)
                else:
                    extras.append(msg)

                # Return if we've gotten replies from each node
                if len(nodes) == 0:
                    return replies, [], extras
            else:
              return replies, nodes.keys(), extras
        
    def sendall(self, api, timeout, participants=None):

        '''Send the request packet to every node.
        We return the messages we get in reply.  The list of expected
        participants is either the value of the 'participants' argument,
        or the list of 'up' machines.  We return when all expected
        participants reply, or timeout expires.
        See WaitForReplies() for an explanation of the return value.
        '''

        if participants == None:   participants=api.nodes_with_status()

        api.sendclustermsg(self)

        return self.WaitForReplies(api, participants, timeout)


    def sendnode(self, api, node, timeout):

        '''Send the request to the given node.
        We return the messages we get in reply.
        See WaitForReplies() for an explanation of the return value.
        '''

        if api.nodestatus(node) != hb_api.ActiveStatus:
            print "Attempt to send request to bad/down node"
        api.sendnodemsg(self, node)
        return self.WaitForReplies(api, [node], timeout)

class ReplyMsg(ha_msg):

    '''This is an CTS reply packet.  It is an "abstract" class.
    To make a derived class from it, set derived_class.RequestType and
    and derived_class.ResponseType.
    '''

    #RequestType  = ha_msg.T_TESTREQ
    #ResponseType = ha_msg.T_TESTRSP

    def __init__(self, req, result):

        '''Pass the constructor the packet you're responding to.'''

        self.data = req.data
        __str__ = UserDict.__repr__	 # use default __str__ function

        if self[ha_msg.F_TYPE] != self.__class__.RequestType:
            raise ValueError("Inappropriate initialization packet")

	#
	#	Change message type, add return code, and return to sender
	#
        self.update(req,
           { ha_msg.F_TYPE       : self.__class__.ResponseType,
             ha_msg.F_TO         : self[ha_msg.F_ORIG],
             ha_msg.F_TOID       : self[ha_msg.F_FROMID],
             ha_msg.F_APIRESULT  : result
           })

    def send(self, api):
        #	We've already set the return address above.
        #	This means this is now effectively sendnodemsg ;-)
        api.sendclustermsg(self)


class TestMappings(UserDict):

    '''A class to call the right function with the right arguments when
    presented with a message.  Each data item in the mapping is a 2-element
    tuple of the form: (function, argument-to-function).
    When the function given in the mapping is actually called, it is called with
    three arguments:
            (message-to-handle, API object, argument-to-function)
    Argument-to-function was the second element of the 2-tuple originally
    associated with the message type (the first element was the function).

    In this class, everything depends on messages having an F_APIREQ field
    to be used as the message type when processing messages.
    '''

    def __init__(self, api):
        self.Api = api
        self.data = {}

    def __setitem__(self, key, value):

        '''The values you assign to go with keys need to be 2-element tuples or
        lists.   The first item has to be a callable thing (function),
        and the second can be anything that the function likes for an argument.
        This function is all about error checking.
        '''

        if ((not isinstance(value, types.ListType)
             and  not isinstance(value, types.TupleType))
        or   len(value) != 2)				:
            raise ValueError("inappropriate TestMappings tuple")

        if (not callable(value[0])) :
            raise ValueError("Non-callable TestMappings 'function'")

        self.data[key]=value
   
    def __call__(self, msg, dummyarg):

        '''Process the request that goes with the given message.
        We use the F_APIREQ field to determine the type of request
        we're processing.
        '''

        reqtype=msg[ha_msg.F_APIREQ]
        if self.has_key(reqtype):
            self[reqtype][0](msg, self.Api, self[reqtype][1])
        elif self.has_key(self.Api.BADREQ):
            self[self.Api.BADREQ][0](msg, self.Api, self[self.Api.BADREQ][1])
        else:
            #	It would be nice to do something better ;-)
            print "No handler for request type %s" % reqtype

class CTSRequest (RequestMsg):
    '''A CTS request message.  This class can be further subclassed to
    good effect.
    '''
    RequestType  = ha_msg.T_TESTREQ
    ResponseType = ha_msg.T_TESTRSP

class CTSReply(ReplyMsg):
    '''A CTS reply message.'''
    RequestType  = ha_msg.T_TESTREQ
    ResponseType = ha_msg.T_TESTRSP
#
#   A little test code...
#
#   This is a trivial 'ping' application...
#
#   pingreply is called when a ping "request" is received
#
if __name__ == '__main__':

    class PingRequest(CTSRequest):

        '''A Ping request message'''

        def __init__(self): CTSRequest.__init__(self, "ping")

    class SpamRequest(CTSRequest):

        '''A Spam request message (which we won't handle)'''

        def __init__(self): CTSRequest.__init__(self, "spam")

    #	Function to perform a ping reply...
    def pingreply(pingmsg, api, arg):

        '''Construct and send a ping reply message.'''

        reply=CTSReply(pingmsg, api.OK)
        reply.send(api)
        
    #	Function to perform a Bad Request reply...
    def BadReq(badmsg, api, arg):

        '''Give 'em the bad news...'''

        reply=CTSReply(badmsg, api.BADREQ)
        reply[ha_msg.F_COMMENT]=arg
        reply.send(api)

    hb = hb_api()
    hb.signon()

    #	Set up response functions to automatically reply to pings when
    #	they arrive.

    testmap = TestMappings(hb)
    testmap["ping"]    = (pingreply, None)
    testmap[hb.BADREQ] = (BadReq, "Invalid CTS request (we don't like spam)")

    # Set up our function to respond to CTSRequest packets

    hb.set_msg_callback(ha_msg.T_TESTREQ, testmap, None)

    print hb.cluster_config()

    req = PingRequest()  # Same as CTSRequest("ping")
    print req.sendnode(hb, "kathyamy", 5)

    spam = SpamRequest()  # Same as CTSRequest("spam")
    print spam.sendnode(hb, "kathyamy", 5)

    print req.sendall(hb, 5)
    hb.signoff()
