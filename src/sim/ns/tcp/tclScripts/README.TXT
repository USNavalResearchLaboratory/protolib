Examples
========

This directory contains several example that use the TCPSocketAgent's directly from TCL and also examples that show how this socket API can be used from your own NS-2 applications. 

socketsfromapp.tcl
==================

This is a simple example that uses the TCPSocketAgent and TCPServerSocketAgent directly in order to make a simple connection from the client to the server. This example uses FullTCP.
 
multiplesocketagent.tcl 
=======================

This is a more complex example that uses the TCPSocketAgent and TCPServerSocketAgent directly in order to make a connection from two clients to one server. This is meant to demonstrate the multiple connection caability the TCP server. This example also uses FullTCP.


socketagent.tcl
===============

This is a complex example that shows how a programmer would build their own agents that used the TCPSocketAgent's API in order to dynamically create sockets and connections in Ns-2.  The example given here shows multiple clients (user configurable) that all connection to a single TCPSocketServerAgent to send messages. This example can be expanded easily to create other types of applications that use multiple connections 


The code in this example forms the basis for the Protolib integration and Java bindings that are described in the examples in the protolib/ns directory and AgentJ's tclscript directory. 

 


