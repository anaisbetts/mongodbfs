## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DFUSE_USE_VERSION=27 \
	-I. -Wall -I/usr/local/include \
	$(MONGODBFS_CFLAGS)			

bin_PROGRAMS = mongodbfs

AM_CFLAGS = -std=c99 -g -O0

## HACK HACK HACK: We're adding libevent here, without ever actually checking for it!
mongodbfs_LDADD = $(MONGODBFS_LIBS) -lgthread-2.0 -lpthread -lmongoclient \
		  -lboost_system-mt -lboost_thread-mt -lboost_program_options-mt -lboost_filesystem-mt \
		 #-levent

mongodbfs_SOURCES = \
	gridfs_wrapper.cpp \
	main.cpp \
	mongodbfs.c \
	stats.c \
	queue.c
