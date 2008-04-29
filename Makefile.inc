BINDIR?= /usr/bin

CFLAGS+=	-fstrict-aliasing
#CFLAGS+=	-pg
#CFLAGS+=        -DDMALLOC
#CFLAGS+=	-include /usr/local/include/dmalloc.h
#LDADD+= -L/usr/local/lib -ldmalloc
#LDADD+= -L../libarchive -larchive_p
#LDADD+= -L/usr/obj/usr/home/tim/projects/FreeBSD/LOCAL/5.1/lib/libc -lc

DEBUG_FLAGS+= -g -static
