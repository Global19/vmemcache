// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2019, Intel Corporation */

/*
 * os_posix.c -- abstraction layer for basic Posix functions
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <stdarg.h>
#include <sys/file.h>
#ifdef __FreeBSD__
#include <sys/mount.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "util.h"
#include "out.h"
#include "os.h"

/*
 * os_open -- open abstraction layer
 */
int
os_open(const char *pathname, int flags, ...)
{
	int mode_required = (flags & O_CREAT) == O_CREAT;

#ifdef O_TMPFILE
	mode_required |= (flags & O_TMPFILE) == O_TMPFILE;
#endif

	if (mode_required) {
		va_list arg;
		va_start(arg, flags);
		/* Clang requires int due to auto-promotion */
		int mode = va_arg(arg, int);
		va_end(arg);
		return open(pathname, flags, (mode_t)mode);
	} else {
		return open(pathname, flags);
	}
}

#if 0
/*
 * os_fsync -- fsync abstraction layer
 */
int
os_fsync(int fd)
{
	return fsync(fd);
}

/*
 * os_fsync_dir -- fsync the directory
 */
int
os_fsync_dir(const char *dir_name)
{
	int fd = os_open(dir_name, O_RDONLY | O_DIRECTORY);
	if (fd < 0)
		return -1;

	int ret = os_fsync(fd);

	os_close(fd);

	return ret;
}
#endif

/*
 * os_stat -- stat abstraction layer
 */
int
os_stat(const char *pathname, os_stat_t *buf)
{
	return stat(pathname, buf);
}

/*
 * os_unlink -- unlink abstraction layer
 */
int
os_unlink(const char *pathname)
{
	return unlink(pathname);
}

/*
 * os_access -- access abstraction layer
 */
int
os_access(const char *pathname, int mode)
{
	return access(pathname, mode);
}

/*
 * os_fopen -- fopen abstraction layer
 */
FILE *
os_fopen(const char *pathname, const char *mode)
{
	return fopen(pathname, mode);
}

#if 0
/*
 * os_fdopen -- fdopen abstraction layer
 */
FILE *
os_fdopen(int fd, const char *mode)
{
	return fdopen(fd, mode);
}

/*
 * os_chmod -- chmod abstraction layer
 */
int
os_chmod(const char *pathname, mode_t mode)
{
	return chmod(pathname, mode);
}
#endif

/*
 * os_mkstemp -- mkstemp abstraction layer
 */
int
os_mkstemp(char *temp)
{
	return mkstemp(temp);
}

/*
 * os_posix_fallocate -- posix_fallocate abstraction layer
 */
int
os_posix_fallocate(int fd, os_off_t offset, off_t len)
{

#ifdef __FreeBSD__
	struct stat fbuf;
	struct statfs fsbuf;
/*
 * XXX Workaround for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=223287
 *
 *	FreeBSD implements posix_fallocate with a simple block allocation/zero
 *	loop. If the requested size is unreasonably large, this can result in
 *	an uninterruptable system call that will suck up all the space in the
 *	file system and could take hours to fail. To avoid this, make a crude
 *	check to see if the requested allocation is larger than the available
 *	space in the file system (minus any blocks already allocated to the
 *	file), and if so, immediately return ENOSPC. We do the check only if
 *	the offset is 0; otherwise, trying to figure out how many additional
 *	blocks are required is too complicated.
 *
 *	This workaround is here mostly to fail "absurdly" large requests for
 *	testing purposes; however, it is coded to allow normal (albeit slow)
 *	operation if the space can actually be allocated. Because of the way
 *	PMDK uses posix_fallocate, supporting Linux-style fallocate in
 *	FreeBSD should be considered.
 */
	if (offset == 0) {
		if (fstatfs(fd, &fsbuf) == -1 || fstat(fd, &fbuf) == -1)
			return errno;

		size_t reqd_blocks =
			(((size_t)len + (fsbuf.f_bsize - 1)) / fsbuf.f_bsize)
				- (size_t)fbuf.st_blocks;
		if (reqd_blocks > (size_t)fsbuf.f_bavail)
			return ENOSPC;
	}
#endif

	return posix_fallocate(fd, offset, len);
}

#if 0
/*
 * os_ftruncate -- ftruncate abstraction layer
 */
int
os_ftruncate(int fd, os_off_t length)
{
	return ftruncate(fd, length);
}

/*
 * os_flock -- flock abstraction layer
 */
int
os_flock(int fd, int operation)
{
	int opt = 0;
	if (operation & OS_LOCK_EX)
		opt |= LOCK_EX;
	if (operation & OS_LOCK_SH)
		opt |= LOCK_SH;
	if (operation & OS_LOCK_UN)
		opt |= LOCK_UN;
	if (operation & OS_LOCK_NB)
		opt |= LOCK_NB;

	return flock(fd, opt);
}

/*
 * os_writev -- writev abstraction layer
 */
ssize_t
os_writev(int fd, const struct iovec *iov, int iovcnt)
{
	return writev(fd, iov, iovcnt);
}
#endif

/*
 * os_clock_gettime -- clock_gettime abstraction layer
 */
int
os_clock_gettime(int id, struct timespec *ts)
{
	return clock_gettime(id, ts);
}

#if 0
/*
 * os_rand_r -- rand_r abstraction layer
 */
unsigned
os_rand_r(unsigned *seedp)
{
	return (unsigned)rand_r(seedp);
}

/*
 * os_unsetenv -- unsetenv abstraction layer
 */
int
os_unsetenv(const char *name)
{
	return unsetenv(name);
}

/*
 * os_setenv -- setenv abstraction layer
 */
int
os_setenv(const char *name, const char *value, int overwrite)
{
	return setenv(name, value, overwrite);
}
#endif

/*
 * secure_getenv -- provide GNU secure_getenv for FreeBSD
 */
#ifndef __USE_GNU
static char *
secure_getenv(const char *name)
{
	if (issetugid() != 0)
		return NULL;

	return getenv(name);
}
#endif

/*
 * os_getenv -- getenv abstraction layer
 */
char *
os_getenv(const char *name)
{
	return secure_getenv(name);
}

#if 0
/*
 * os_strsignal -- strsignal abstraction layer
 */
const char *
os_strsignal(int sig)
{
	return strsignal(sig);
}

int
os_execv(const char *path, char *const argv[])
{
	return execv(path, argv);
}
#endif
