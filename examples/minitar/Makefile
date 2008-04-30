CFLAGS=				\
	-DNO_BZIP2_CREATE	\
	-DNO_BZIP2_EXTRACT	\
	-DNO_COMPRESS_EXTRACT	\
	-DNO_CPIO_EXTRACT	\
	-DNO_CREATE		\
	-DNO_GZIP_CREATE	\
	-DNO_GZIP_EXTRACT	\

# Omit 'tree.o' if you're not including create support
#OBJS= minitar.o tree.o
OBJS= minitar.o

all: minitar

minitar: $(OBJS)
	cc -o minitar -static $> -larchive -lz -lbz2
	strip minitar
	ls -l minitar

minitar.o: minitar.c

tree.o: tree.c

clean::
	rm -f *.o
	rm -f minitar
	rm -f *~
