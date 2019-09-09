
import protokit
import sys
import time
# Usage: python pipeExample {send | recv} <pipeName>


# Take your pick of which pipe type you want to try
# (Note that "STREAM" is currently limited to a single
#  accepted connection)

#pipeType = "MESSAGE"
pipeType = "STREAM"

mode = sys.argv[1]
pipeName = sys.argv[2]

protoPipe = protokit.Pipe(pipeType)

if "recv" == mode:
    protoPipe.Listen(pipeName)
    if "STREAM" == pipeType:
        protoPipe.Accept()
    while True:
        buf = protoPipe.Recv(8192)
        print buf
else:
    protoPipe.Connect(pipeName)
    count = 0
    while True:
        count += 1
        protoPipe.Send("Hello, ProtoPipe %s (%d) ..." % (pipeName, count))
        time.sleep(1.0)
