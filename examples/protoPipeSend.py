#!/usr/bin/env python
import sys
import protokit

if len(sys.argv) <= 2:
  print("Usage: %s <pipeName> <message>" % sys.argv[0])
  sys.exit(1)

pipe = protokit.Pipe("MESSAGE")
pipe.Connect(sys.argv[1])

text = ' '.join(sys.argv[2:])

# This sends the message as a string
msg = text

# Uncomment these two lines to send as bytearray 
#msg = bytearray()
#msg.extend(map(ord, text))

# Uncomment this one to send as bytes (example/test)
#msg = text.encode('ascii')

pipe.Send(msg)

print("Sent message '%s'" % text)
