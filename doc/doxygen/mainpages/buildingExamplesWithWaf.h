/**
@page building_examples_with_waf Building Protolib Examples with WAF

@li @ref building_with_waf
@li @ref building_configuring_waf
@li @ref building_the_examples
@li @ref building_cleaning

<hr>
@section building_with_waf Building  with WAF

Protolib examples can be built using the Waf build tool, included in the distribution.
To see a full list of example programs, run:

@verbatim
    ./waf -h
@endverbatim

<hr>
@section building_configuring_waf Configuring WAF

To perform the configure checks, run:

@verbatim
    ./waf configure
@endverbatim

Some options for the configure step:

@verbatim
    --prefix=<DIR> - Directory to install files to (Default - /usr/local)

    --debug - Builds a debug build (with debugging symbols), otherwise an
            optimized library is built.
    --static - Builds a statically linked library, otherwise a shared library
            is built.

    --build-python - Builds the Python ProtoPipe extension
    --build-java - Builds the Java ProtoPipe extension
        You must set the JAVA_HOME environment variable to the location of your
        JDK directory
    --build-ns3 - Builds the NS3 extensions
        You must set the NS3_HOME environment variable to the location of your
        NS3 directory

    --disable-X - Disables the feature X (See ./waf -h for descriptions)
@endverbatim

<hr>
@section building_the_examples Building the Examples

To build all the examples, simply run:

@verbatim
    ./waf -ex-all
@endverbatim

To build a specific example, run:

@verbatim
    ./waf --ex-XXX
@endverbatim

Where XXX is the name of the example you want to build (see ./waf -h).

<hr>
@section building_cleaning Cleaning

@verbatim
    ./waf clean
@endverbatim

will delete all compiled files and configuration settings.

@verbatim
	./waf distclean
@endverbatim

will do the same as clean, and additionally delete the waf cache files.

*/

