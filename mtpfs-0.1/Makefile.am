bin_PROGRAMS = mtpfs
mtpfs_SOURCES = mtpfs.c 
mtpfs_CPPFLAGS = -DFUSE_USE_VERSION=22 $(FUSE_CFLAGS) $(GLIB_CFLAGS) $(MTP_CFLAGS)
mtpfs_LDADD = $(FUSE_LIBS) $(GLIB_LIBS) $(MTP_LIBS)

