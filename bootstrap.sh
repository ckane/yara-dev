#!/bin/sh

# Quick and dirty script to reset everything
# regarding libtool, autoconf, automake, etc.


if [ ! -d m4 ]; then mkdir m4; fi
if [ ! -d libyara/m4 ]; then mkdir libyara/m4; fi

autoreconf -vif
