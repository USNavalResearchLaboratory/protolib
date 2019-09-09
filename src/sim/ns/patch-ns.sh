#!/bin/sh

# Before running this patch, build your ns-allinone distribution.

# After running this patch, go to the NS_SRC_DIR
# and "./configure; make ns"

NS_SRC_DIR=/home/adamson/ns/ns-allinone-2.29/ns-2.29

PROTOLIB_SRC_DIR=/home/adamson/PROTOLIB/protolib

MDP_SRC_DIR=/home/adamson/MDP/mdp

NORM_SRC_DIR=/home/adamson/NORM/norm

MGEN_SRC_DIR=/home/adamson/MGEN/mgen

CWD=`pwd`

# 1) Point to modified version of ns-2 Makefile.in

rm -i $NS_SRC_DIR/Makefile.in
ln -s $CWD/ns229-Makefile.in $NS_SRC_DIR/Makefile.in

# 2) Add a link to our Protolib source code directory
rm -rif $NS_SRC_DIR/protolib
ln -s $PROTOLIB_SRC_DIR $NS_SRC_DIR/protolib

# 3) Add a link to our MDP source code directory
rm -rif $NS_SRC_DIR/mdp
ln -s $MDP_SRC_DIR $NS_SRC_DIR/mdp

# 4) Add a link to our NORM source code directory
rm -rif $NS_SRC_DIR/norm
ln -s $NORM_SRC_DIR $NS_SRC_DIR/norm

# 5) Add a link to our MGEN source code directory
rm -rif $NS_SRC_DIR/mgen
ln -s $MGEN_SRC_DIR $NS_SRC_DIR/mgen

