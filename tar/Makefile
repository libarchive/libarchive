# $FreeBSD: src/usr.bin/tar/Makefile,v 1.40 2008/12/06 07:38:14 kientzle Exp $

PROG=	bsdtar
BSDTAR_VERSION_STRING=2.6.900a
SRCS=	bsdtar.c cmdline.c getdate.y matching.c read.c siginfo.c subst.c tree.c util.c write.c
WARNS?=	5
DPADD=	${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD=	-larchive -lbz2 -lz
CFLAGS+=	-DBSDTAR_VERSION_STRING=\"${BSDTAR_VERSION_STRING}\"
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I${.CURDIR}
SYMLINKS=	bsdtar ${BINDIR}/tar
MLINKS=	bsdtar.1 tar.1
DEBUG_FLAGS=-g

.PHONY: check test
check test: $(PROG) bsdtar.1.gz
	cd ${.CURDIR}/test && make test

.include <bsd.prog.mk>
