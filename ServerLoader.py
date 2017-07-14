import socket
import sys
import os
import time

if len(sys.argv) != 3:
    print ("usage: ServerLoader <scam ip> <scam port>")
    sys.exit();

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
server_address = (sys.argv[1], int(sys.argv[2]))
print >>sys.stderr, 'connecting to %s port %s' % server_address

while True:
    try:
        sock.connect (server_address)
        break
    except:
        print "Oops!  SCAM not found,  Try again..."
        time.sleep(5)

print 'connected!'

request = 'ServerReq'
response = 'RankFinished'
try:
    # Send data
    print >> sys.stderr, 'sending "%s"' % request
    sock.sendall(request)

    # Look for the response
    amount_received = 0
    amount_expected = len(response)

    while amount_received < amount_expected:
        data = sock.recv(16)
        amount_received += len(data)
        print >> sys.stderr, 'received "%s"' % data

    os.system("./GnuTLS_Server")

finally:
    print >> sys.stderr, 'closing socket'
    sock.close()
