#!/bin/sh

#
# Simple script to repopulate the 'doc' tree from
# the mdoc man pages stored in each project.
#

# Remove existing manpages from the doc tree
chmod -R +w man text
rm -f man/*.[135]
rm -f text/*.[135]

# Build Makefile in 'man' directory
cd man
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for d in libarchive tar cpio; do
    for f in ../../$d/*.[135]; do
	echo >> Makefile
	echo `basename $f`: ../mdoc2man.awk $f >> Makefile
	echo "	awk -f ../mdoc2man.awk < $f > `basename $f`" >> Makefile
        all="$all `basename $f`"
    done
done
echo $all >>Makefile
cd ..

# Rebuild Makefile in 'text' directory
cd text
echo > Makefile
echo "$all" >>Makefile
echo >>Makefile
for d in libarchive tar cpio; do
    for f in ../../$d/*.[135]; do
	echo >> Makefile
	echo `basename $f`: $f >> Makefile
	echo "	nroff -mdoc $f | col -b > `basename $f`" >> Makefile
    done
done
cd ..

# Convert all of the manpages to -man format.
(cd man && make)
# Format all of the manpages to text
(cd text && make)
