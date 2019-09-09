/**
@page building_protolibJni_with_waf Building ProtolibJni with WAF

<hr>
@section building_protolibJni_with_waf Building ProtolibJni 

ProtolibJNI is a java native library that allows java applications access 
to the Protolib ProtoPipe class.  The source code is available in the 
protolib/src/java directory.

To build the library, you may find it helpful to load the protolib project 
into Visual Studio (see @ref building_with_vs) and open a Visual Studio command
so that the correct environment variables are set.

First clean the distribution using waf distclean:

@verbatim
waf distclean
@endverbatim

Next configure waf to build the protolibJni library, dll, and
jar files:

@verbatim
waf configure --disable-wx --disable-vif
              --disable-manet --static --build-java
@endverbatim

Now build the library:

@verbatim
waf
@endverbatim

The ProtolibJni.dll, ProtolibJni.jar, and ProtolibJni.lib files will be
created in the build/default directory. 
*/

