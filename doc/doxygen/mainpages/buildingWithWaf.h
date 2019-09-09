/**
@page building_with_waf Building Protolib with WAF

@li @ref building_with_waf
@li @ref building_configuring_waf
@li @ref building_the_library
@li @ref building_installing
@li @ref building_uninstalling
@li @ref building_cleaning

<hr>
@section building_with_waf Building Protolib with WAF

Protolib can be built using the Waf build tool, included in the distribution.
To see a full list of options, run:

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
@section building_the_library Building the Library

To build the library, simply run:

@verbatim
    ./waf
@endverbatim

To build examples along with the library, run:

@verbatim
    ./waf --ex-XXX
@endverbatim

Where XXX is the name of the example you want to build (see ./waf -h).
Additionally, you can add the --ex-all flag to build all the example programs.

<hr>
@section building_installing Installing

To install, run:

@verbatim
    ./waf install
@endverbatim

This will install the compiled library and headers to wherever your prefix was
specified.  (See configure flags)

<hr>
@section building_uninstalling Uninstalling

Waf tracks the files it installs, so run:

@verbatim
    ./waf uninstall
@endverbatim

to remove all files from a previous ./waf install

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

