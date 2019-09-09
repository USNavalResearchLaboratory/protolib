#! /bin/csh

# Copy *.cpp files to *.ex.cpp to make OPNET happy
# Note: MUST be run from the protolib/opnet directory!

# This "dirList" can be added to as necessary
set dirList = "opnet common"

foreach dir ($dirList)
set fileList = `ls ../$dir/*.cpp`
foreach file ($fileList)
cp $file $file:r.ex.cpp
echo "Created $file:r.ex.cpp"
end
end
