#!/usr/bin/env python
import sys
import protokit

if len(sys.argv) <= 2:
  print "Usage: %s <pipeName> <message>" % sys.argv[0]
  sys.exit(1)

pipe = protokit.Pipe("MESSAGE")
pipe.Connect(sys.argv[1])

message = ' '.join(sys.argv[2:])
pipe.Send(message)

print "Sent message '%s'" % message
