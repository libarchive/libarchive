#!/bin/sh

#
# Simple script to repopulate the 'doc' tree from
# the mdoc man pages stored in each project.
#

USAGE="\n\
Simple script to repopulate the 'doc' tree from\n\
the mdoc man pages stored in each project.\n\
\n\
 -h, --help                  Display this help message\n\
 --mediawiki                 Use mediawiki markup for wiki pages.\n"

# Loop that parses options passed to script
while [ "$#" -gt "0" ]; do
  case "$1" in
    --mediawiki)
      if [ -x "$(which html2wiki)" >/dev/null 2>&1 ]; then
          USE_MEDIAWIKI=1
      else
          echo "html2wiki program not found or is not executable."
          exit 1
      fi
      shift
      ;;
    -h|--help|*)
      echo -e "${USAGE}"
      exit 1
      ;;
  esac
done

# Collect list of man pages, relative to my subdirs
test -d man || mkdir man
cd man
MANPAGES=`for d in libarchive tar cpio;do ls ../../$d/*.[135];done | grep -v '\.so\.'`
cd ..

# Build Makefile in 'man' directory
cd man
chmod +w .
rm -f *.[135] Makefile
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for f in $MANPAGES; do
    outname="`basename $f`"
    echo >> Makefile
    echo $outname: ../mdoc2man.awk $f >> Makefile
    echo "	awk -f ../mdoc2man.awk < $f > $outname" >> Makefile
    all="$all $outname"
done
echo $all >>Makefile
cd ..

# Rebuild Makefile in 'text' directory
test -d text || mkdir text
cd text
chmod +w .
rm -f *.txt Makefile
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for f in $MANPAGES; do
    outname="`basename $f`.txt"
    echo >> Makefile
    echo $outname: $f >> Makefile
    echo "	nroff -mdoc $f | col -b > $outname" >> Makefile
    all="$all $outname"
done
echo $all >>Makefile
cd ..

# Rebuild Makefile in 'pdf' directory
test -d pdf || mkdir pdf
cd pdf
chmod +w .
rm -f *.pdf Makefile
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for f in $MANPAGES; do
    outname="`basename $f`.pdf"
    echo >> Makefile
    echo $outname: $f >> Makefile
    echo "	groff -mdoc -T ps $f | ps2pdf - - > $outname" >> Makefile
    all="$all $outname"
done
echo $all >>Makefile
cd ..

# Build Makefile in 'html' directory
test -d html || mkdir html
cd html
chmod +w .
rm -f *.html Makefile
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for f in $MANPAGES; do
    outname="`basename $f`.html"
    echo >> Makefile
    echo $outname: $f >> Makefile
    echo "	groff -mdoc -T html $f > $outname" >> Makefile
    all="$all $outname"
done
echo $all >>Makefile
cd ..

# Build Makefile in 'wiki' directory
test -d wiki || mkdir wiki
cd wiki
chmod +w .
rm -f *.wiki Makefile
echo > Makefile
echo "default: all" >>Makefile
echo >>Makefile
all="all:"
for f in $MANPAGES; do
    outname="`basename $f | awk '{ac=split($0,a,"[_.-]");o="ManPage";for(w=0;w<=ac;++w){o=o toupper(substr(a[w],1,1)) substr(a[w],2)};print o}'`.wiki"
    echo >> Makefile
    if [ -z "$USE_MEDIAWIKI" ]; then
        echo $outname: ../mdoc2wiki.awk $f >> Makefile
        echo "	awk -f ../mdoc2wiki.awk < $f > $outname" >> Makefile
    else
        inname="../html/$(basename $f).html"
        echo $outname: $inname $f >> Makefile
        echo "	html2wiki --dialect MediaWiki --encoding utf-8 $inname > $outname" >> Makefile
    fi
    all="$all $outname"
done
echo $all >>Makefile
cd ..

# Convert all of the manpages to -man format
(cd man && make)
# Format all of the manpages to text
(cd text && make)
# Format all of the manpages to PDF
(cd pdf && make)
# Format all of the manpages to HTML
(cd html && make)
# Format all of the manpages to wiki syntax
(cd wiki && make)
