/*
  ==============================================================================

    juceNet.cpp
    Created: 13 Apr 2018 10:47:28pm
    Author:  adamson
 
    This file brings in the appropriate platform-specific source files that
    implement the platform-specific aspects of the ProtoNet module.

  ==============================================================================
*/

#ifdef UNIX
#include "../unix/unixNet.cpp"
#endif  // UNIX

#ifdef MACOSX
#include "../bsd/bsdNet.cpp"
#endif // MACOSX

#ifdef LINUX
#include "../linux/linuxNet.cpp"
#endif // LINUX

#ifdef WIN32
#include "../win32/win32Net.cpp"
#endif // WIN32
