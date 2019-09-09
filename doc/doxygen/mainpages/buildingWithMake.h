/**
@page building_with_make Building Protolib with Make

@li @ref building_with_make
@li @ref building_with_make_the_library
@li @ref building_with_make_cleaning

<hr>
@section building_with_make Building Protolib with Make

Protolib can be built using the Make build tool.  Makefile configuration files
for a variety of platforms are included in the distribution in the makefiles 
directory.

<hr>
@section building_with_make_the_library Building the Library

To build the library, simply run:

@verbatim
    make -f Makefile.<os>
@endverbatim

To build examples along with the library, run:

@verbatim
    make -f Makefile.<os> <exampleApp>
@endverbatim

Where XXX is the name of the example you want to build.

<hr>
@section building_with_make_cleaning Cleaning

@verbatim
    make -f Makefile.<os> clean
@endverbatim

will delete all compiled files.

*/

