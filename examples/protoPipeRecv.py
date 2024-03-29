#!/usr/bin/env python
import sys
import protokit

if len(sys.argv) != 2:
  print("Usage: %s <pipeName>" % sys.argv[0])
  sys.exit(1)

pipe = protokit.Pipe("MESSAGE")
pipe.Listen(sys.argv[1])
while True:
    msg = pipe.Recv(1024)
    if msg:
        print("received \"%s\"" % msg.decode('UTF-8'))
    else:
        break

print("Exiting...")
