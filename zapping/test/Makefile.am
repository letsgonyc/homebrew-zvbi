## Process this file with automake to produce Makefile.in

TESTS = guard1 guard2 clear_image
XFAIL_TESTS = guard1 guard2

EXTRA_DIST = \
	guard.h \
	pixel.h

check_PROGRAMS = \
	clear_image \
	guard1 \
	guard2

LDADD = $(top_builddir)/libtv/libtv.a