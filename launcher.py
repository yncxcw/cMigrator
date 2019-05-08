import zmq
import sys
port = "1127"

if __name__ == "__main__":

    command = ""
    if len(sys.argv) == 2 and sys.argv[1] == "quit":
        command = "QUIT"
    elif len(sys.argv) == 3 and sys.argv[1] == "laun":
        command = "LAUN "+str(sys.argv[2])
    else:
        print("Usage python launch [quit|laun pid]")


    context = zmq.Context()
    print "Connecting to server..."
    socket = context.socket(zmq.REQ)
    socket.connect ("tcp://localhost:%s" % port)
    socket.send(command)
        
    


