# $FreeBSD: src/usr.bin/tar/Makefile,v 1.35 2008/05/18 06:24:47 cperciva Exp $

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.5.3b
SRCS=	bsdtar.c getdate.y matching.c read.c siginfo.c tree.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${.CURDIR}
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1

.PHONY: check test
check test: $(PROG) bsdtar.1.gz
	cd ${.CURDIR}/test && make clean test

.include <bsd.prog.mk>
