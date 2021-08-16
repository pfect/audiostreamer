CC=gcc

EXTRA_WARNINGS=-Wall -W -Wformat-nonliteral -Wcast-align -Wpointer-arith \
	       -Wbad-function-cast -Wmissing-prototypes -Wstrict-prototypes \
	       -Wmissing-declarations -Winline -Wundef -Wnested-externs -Wcast-qual \
	       -Wshadow -Wwrite-strings -Wno-unused-parameter -Wfloat-equal -pedantic -ansi -std=c99 -export-dynamic

GST_LIBS=`pkg-config --libs gstreamer-1.0`
GST_CFLAGS=`pkg-config --cflags gstreamer-1.0`

CFLAGS=-ggdb $(EXTRA_WARNINGS)

BINS=audiostreamer


audiostreamer:	audiostreamer.c
	 $(CC) $+ $(CFLAGS) $(GST_CFLAGS) $(GST_LIBS) -o $@ -I.

clean:
	rm -rf $(BINS)

