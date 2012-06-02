#ifndef _MTPFS_H_
#define _MTPFS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>

#include <usb.h>
#include <libmtp.h>
#include <glib.h>
#include <sys/mman.h>
#include <strings.h>

#endif /* _MTPFS_H_ */
