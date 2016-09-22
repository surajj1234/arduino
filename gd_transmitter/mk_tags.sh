#!/bin/bash

export LC_COLLATE=C
ctags -f tags.ino --langmap=c++:.ino `find . -name "*.ino"`
cat tags.ino > tags
sort tags -o tags
rm -f tags.*
