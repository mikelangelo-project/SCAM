#!/usr/bin/env python

import socket
import sys
import os
import time
import ConfigParser

config = ConfigParser.ConfigParser ()
config.read ("ServerLoader.ini")

def ConfigSectionMap(section):
    dict1 = {}
    options = config.options(section)
    for option in options:
        try:
            dict1[option] = config.get(section, option)
            if dict1[option] == -1:
                print("skip: %s" % option)
        except:
            print("exception on %s!" % option)
            dict1[option] = None
    return dict1


def main():
    PATH = ConfigSectionMap("serverloader")['path']
    request = ConfigSectionMap("serverloader")['request_msg']
    response = ConfigSectionMap("serverloader")['response_msg']

    print PATH

    if len(sys.argv) != 3:
        print ("usage: ServerLoader <scam ip> <scam port>")
        sys.exit()

    # Create a TCP/IP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Connect the socket to the port where the server is listening
    server_address = (sys.argv[1], int(sys.argv[2]))
    print 'connecting to %s port %s' % server_address

    while True:
        try:
            sock.connect (server_address)
            break
        except:
            print "Oops! SCAM not found,  Trying again..."
            time.sleep(5)

    print 'Connected!'

    try:
        # Send data
        print 'sending "%s"' % request
        sock.sendall(request)

        # Look for the response
        amount_received = 0
        amount_expected = len(response)

        while True:
            data = sock.recv(16)
#            amount_received = len(data)
            print 'received "%s"' % data
            if data == response:
                sock.send("ack")
                break

        os.system(PATH)

    finally:
        print 'closing socket'
        sock.close()

if __name__ == "__main__":
    sys.exit(main())
