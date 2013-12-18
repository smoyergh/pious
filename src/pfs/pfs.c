/* PIOUS1 Parallel Input/OUtput System
 * Copyright (C) 1994,1995 by Steven A. Moyer and V. S. Sunderam
 *
 * PIOUS1 is a software system distributed under the terms of the
 * GNU Library General Public License Version 2.  All PIOUS1 software,
 * including PIOUS1 code that is not intended to be directly linked with
 * non PIOUS1 code, is considered to be part of a single logical software
 * library for the purposes of licensing and distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */




/* PIOUS File System Interface (PFS)
 *
 * @(#)pfs.c	2.2  28 Apr 1995  Moyer
 *
 * The PIOUS file system interface (PFS) module exports an interface to the
 * native OS file system.  PFS routines provide essentially POSIX 1003.1-19xx
 * equivalent semantics for file model/operations, with read() and write()
 * operations incorporating the functionality of the traditional lseek()
 * function.
 *
 * PIOUS depends on the PFS providing POSIX.1 file model and file operation
 * semantics; e.g. guaranteeing that reading in "holes" left by writes
 * returns zero bytes, etc. In porting PIOUS to a non POSIX.1 platform, the PFS
 * routines must be implemented to support POSIX.1 semantics.
 *
 * Porting PIOUS to various native file systems is simplified as only PFS
 * functions (pds.c) need be updated.
 *
 *
 * Function Summary:
 *
 *   FS_open();
 *   FS_write();
 *   FS_read();
 *   FS_close();
 *   FS_fsync();
 *   FS_stat();
 *   FS_fstat();
 *   FS_access();
 *   FS_open_max();
 *   FS_rename();
 *   FS_chmod();
 *   FS_unlink();
 *   FS_mkdir();
 *   FS_rmdir();
 *
 *
 * ---------------------------------------------------------------------------
 * Implementation Notes:
 * 
 *   1) This implementation of the PFS interface conforms to the
 *      IEEE POSIX 1003.1-1988/1990. The single exception is the
 *      FS_fsync() operation; see function documentation for details.
 */



/* Definition required to get correct POSIX error codes on some machines */

#define _POSIX_SOURCE 1


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"

#include "pfs.h"


/* Local function declarations */

#ifdef __STDC__
static mode_t pious2posix_perm(pious_modet mode);

static pious_modet posix2pious_perm(mode_t mode);
#else
static mode_t pious2posix_perm();

static pious_modet posix2pious_perm();
#endif



/* PFS function definitions */


/*
 * FS_open() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_open(char *path,
	    int oflag,
	    pious_modet mode)
#else
int FS_open(path, oflag, mode)
     char *path;
     int oflag;
     pious_modet mode;
#endif
{
  int rcode, ocode;
  int o_oflag, tmpflag;
  mode_t o_mode;

  /* check that only one access mode is specified in oflag */
  tmpflag = ((oflag & PIOUS_RDONLY) | (oflag & PIOUS_WRONLY) |
	     (oflag & PIOUS_RDWR));

  if (tmpflag != PIOUS_RDONLY && tmpflag != PIOUS_WRONLY &&
      tmpflag != PIOUS_RDWR)
    rcode = PIOUS_EINVAL;

  /* check if PIOUS_RDONLY specified with PIOUS_TRUNC */
  else if ((oflag & PIOUS_RDONLY) && (oflag & PIOUS_TRUNC))
    rcode = PIOUS_EINVAL;

  /* perform open operation */
  else
    { /* set o_oflag to requested access mode */
      o_oflag = 0;

      if (oflag & PIOUS_RDONLY)
	o_oflag |= O_RDONLY;
      else if (oflag & PIOUS_WRONLY)
	o_oflag |= O_WRONLY;
      else
	o_oflag |= O_RDWR;

      if (oflag & PIOUS_CREAT)
	o_oflag |= O_CREAT;

      if (oflag & PIOUS_TRUNC)
	o_oflag |= O_TRUNC;

      /* set o_mode to requested file permission bits */
      o_mode = pious2posix_perm(mode);

      /* set file creation mask to not interfere with o_mode permission bits */
      umask((mode_t)0);

      /* open/create file for access - guard against signal interrupts */
      while ((ocode = open(path, o_oflag, o_mode)) == -1 && errno == EINTR);

      if (ocode != -1)
	/* open/created without error - return fildes */
	rcode = ocode;
      else
	/* error - return appropriate code */
	switch (errno)
	  {
	  case EACCES:
	  case EISDIR:
	  case EROFS:
	    rcode = PIOUS_EACCES;
	    break;
	  case EMFILE:
	  case ENFILE:
	    rcode = PIOUS_EINSUF;
	    break;
	  case ENAMETOOLONG:
	    rcode = PIOUS_ENAMETOOLONG;
	    break;
	  case ENOENT:
	    rcode = PIOUS_ENOENT;
	    break;
	  case ENOTDIR:
	    rcode = PIOUS_ENOTDIR;
	    break;
	  case ENOSPC:
	    rcode = PIOUS_ENOSPC;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * FS_write() - See pfs.h for description.
 */

#ifdef __STDC__
pious_ssizet FS_write(int fildes,
		      pious_offt offset,
		      int whence,
		      pious_sizet nbyte,
		      char *buf)
#else
pious_ssizet FS_write(fildes, offset, whence, nbyte, buf)
     int fildes;
     pious_offt offset;
     int whence;
     pious_sizet nbyte;
     char *buf;
#endif
{
  pious_ssizet rcode, acode;
  off_t pos;

  /* validate 'whence' and 'nbyte' arguments */
  if ((whence != PIOUS_SEEK_SET && whence != PIOUS_SEEK_END) || nbyte < 0)
    return (PIOUS_EINVAL);

  /* attempt seek to starting offset -- guard against signal interrupts */
  if (whence == PIOUS_SEEK_END)
    while ((pos = lseek(fildes, (off_t)offset, SEEK_END)) == -1 &&
	   errno == EINTR);
  else
    while ((pos = lseek(fildes, (off_t)offset, SEEK_SET)) == -1 &&
	   errno == EINTR);

  if (pos == -1)
    /* error occured during seek */
    switch (errno)
      {
      case EBADF:
	rcode = PIOUS_EBADF;
	break;
      case EINVAL:
	rcode = PIOUS_EINVAL;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  else
    { /* attempt to write data; during write() operation guard against
       * partial transfers of data resulting from signal interrupt.
       *
       * don't need to check lseek() return code; 'pos' must be valid and
       * if fildes becomes invalid (which should never happen), write()
       * will catch this.
       */

#if (!defined(_POSIX_VERSION)) || (_POSIX_VERSION == 198808L)
      while ((acode = write(fildes, buf,
			    (unsigned)Min(nbyte, PIOUS_UINT_MAX))) == -1 &&
	     errno == EINTR)
	lseek(fildes, pos, SEEK_SET);

#else
      while ((acode = write(fildes, buf,
			    (size_t)Min(nbyte, PIOUS_UINT_MAX))) == -1 &&
	     errno == EINTR)
	lseek(fildes, pos, SEEK_SET);
#endif

      /* check result of write and return appropriate value */
      if (acode != -1)
	rcode = acode;

      else /* error - set rcode appropriately */
	switch (errno)
	  {
	  case EBADF:
	    rcode = PIOUS_EBADF;
	    break;
	  case EFBIG:
	    rcode = PIOUS_EFBIG;
	    break;
	  case ENOSPC:
	    rcode = PIOUS_ENOSPC;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * FS_read() - See pfs.h for description.
 */

#ifdef __STDC__
pious_ssizet FS_read(int fildes,
		     pious_offt offset,
		     int whence,
		     pious_sizet nbyte,
		     char *buf)
#else
pious_ssizet FS_read(fildes, offset, whence, nbyte, buf)
     int fildes;
     pious_offt offset;
     int whence;
     pious_sizet nbyte;
     char *buf;
#endif
{
  pious_ssizet rcode, acode;
  off_t pos;

  /* validate 'whence' and 'nbyte' argument */
  if ((whence != PIOUS_SEEK_SET && whence != PIOUS_SEEK_END) || nbyte < 0)
    return (PIOUS_EINVAL);

  /* attempt seek to starting offset -- guard against signal interrupts */
  if (whence == PIOUS_SEEK_END)
    while ((pos = lseek(fildes, (off_t)offset, SEEK_END)) == -1 &&
	   errno == EINTR);
  else
    while ((pos = lseek(fildes, (off_t)offset, SEEK_SET)) == -1 &&
	   errno == EINTR);

  if (pos == -1)
    /* error occured during seek */
    switch (errno)
      {
      case EBADF:
	rcode = PIOUS_EBADF;
	break;
      case EINVAL:
	rcode = PIOUS_EINVAL;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  else
    { /* attempt to read data; during read() operation guard against
       * partial transfers of data resulting from signal interrupt.
       *
       * don't need to check lseek() return code; 'pos' must be valid and
       * if fildes becomes invalid (which should never happen), read()
       * will catch this.
       */

#if (!defined(_POSIX_VERSION)) || (_POSIX_VERSION == 198808L)
      while ((acode = read(fildes, buf,
			   (unsigned)Min(nbyte, PIOUS_UINT_MAX))) == -1 &&
	     errno == EINTR)
	lseek(fildes, pos, SEEK_SET);

#else
      while ((acode = read(fildes, buf,
			   (size_t)Min(nbyte, PIOUS_UINT_MAX))) == -1 &&
	     errno == EINTR)
	lseek(fildes, pos, SEEK_SET);
#endif

      /* check result of read() and return appropriate value */
      if (acode != -1) /* no error */
	rcode = acode;
      else /* error - set rcode appropriately */
	switch (errno)
	  {
	  case EBADF:
	    rcode = PIOUS_EBADF;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * FS_close() - See pfs.h for description.
 */

#ifdef __STDC__
void FS_close(int fildes)
#else
void FS_close(fildes)
     int fildes;
#endif
{
  /* during close() operation, guard against failure due to signal interrupts;
   * EBADF is ignored since, in a sense, fildes is closed.
   */

  while (close(fildes) == -1 && errno == EINTR);
}




/*
 * FS_fsync() - See pfs.h for description.
 *
 * WARNING: NOT POSIX 1003.1-19xx compliant but MUST be implemented somehow.
 *          This implementation is based on the commonly available fsync()
 *          operation.
 */

#ifdef __STDC__
int FS_fsync(int fildes)
#else
int FS_fsync(fildes)
     int fildes;
#endif
{
  int scode, rcode;

  /* during fsync() operation, guard against partial transfers of data
   * due to signal interrupt.
   */

  while ((scode = fsync(fildes)) == -1 && errno == EINTR);

  if (scode != -1) /* no error */
    rcode = PIOUS_OK;

  else /* error - set rcode appropriately */
    switch (errno)
      {
      case EBADF:
	rcode = PIOUS_EBADF;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return rcode;
}




/*
 * FS_stat() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_stat(char *path,
	    struct FS_stat *status)
#else
int FS_stat(path, status)
     char *path;
     struct FS_stat *status;
#endif
{
  int rcode, scode;
  struct stat fstatus;

  /* obtain file status -- guard against signal interrupts */
  while ((scode = stat(path, &fstatus)) == -1 && errno == EINTR);

  if (scode != -1)
    { /* no errors accessing 'path' status; set file permission bits */
      status->mode = posix2pious_perm(fstatus.st_mode);

      /* set file type bits */
      if (S_ISREG(fstatus.st_mode))
	status->mode |= PIOUS_ISREG;
      else if (S_ISDIR(fstatus.st_mode))
	status->mode |= PIOUS_ISDIR;

      /* set file serial (inode) number and device ID */
      status->ino = fstatus.st_ino;
      status->dev = fstatus.st_dev;

      /* set file size for regular files, 0 for others */
      if (S_ISREG(fstatus.st_mode))
	status->size = fstatus.st_size;
      else
	status->size = 0;

      rcode = PIOUS_OK;
    }

  else
    /* error accessing 'path'; set rcode appropriately */
    switch (errno)
      {
      case EACCES:
	rcode = PIOUS_EACCES;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return (rcode);
}




/*
 * FS_fstat() - See pfs.h for description
 */

#ifdef __STDC__
int FS_fstat(int fildes,
	     struct FS_stat *status)
#else
int FS_fstat(fildes, status)
     int fildes;
     struct FS_stat *status;
#endif
{
  int rcode, scode;
  struct stat fstatus;

  /* obtain file status -- guard against signal interrupts */
  while ((scode = fstat(fildes, &fstatus)) == -1 && errno == EINTR);

  if (scode != -1)
    { /* no errors accessing 'fildes' status; set file permission bits */
      status->mode = posix2pious_perm(fstatus.st_mode);

      /* set file type bits */
      if (S_ISREG(fstatus.st_mode))
	status->mode |= PIOUS_ISREG;
      else if (S_ISDIR(fstatus.st_mode))
	status->mode |= PIOUS_ISDIR;

      /* set file serial (inode) number and device ID */
      status->ino = fstatus.st_ino;
      status->dev = fstatus.st_dev;

      /* set file size for regular files, 0 for others */
      if (S_ISREG(fstatus.st_mode))
	status->size = fstatus.st_size;
      else
	status->size = 0;

      rcode = PIOUS_OK;
    }

  else
    /* error accessing 'fildes'; set rcode appropriately */
    switch (errno)
      {
      case EBADF:
	rcode = PIOUS_EBADF;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return (rcode);
}




/*
 * FS_access() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_access(char *path,
	      int amode)
#else
int FS_access(path, amode)
     char *path;
     int  amode;
#endif
{
  int rcode, acode;
  int a_amode;

  /* validate 'amode' argument */

  if (amode != PIOUS_F_OK &&
      (amode & (~(PIOUS_R_OK | PIOUS_W_OK | PIOUS_X_OK))) != 0)
    /* amode is invalid */
    rcode = PIOUS_EINVAL;

  else
    { /* set a_amode to desired accessability */
      a_amode = 0;

      if (amode & PIOUS_F_OK)
	a_amode |= F_OK;
      else
	{
	  if (amode & PIOUS_R_OK)
	    a_amode |= R_OK;

	  if (amode & PIOUS_W_OK)
	    a_amode |= W_OK;

	  if (amode & PIOUS_X_OK)
	    a_amode |= X_OK;
	}

      /* check 'path' for 'amode' accessability -- guard against signal intr */
      while ((acode = access(path, a_amode)) == -1 && errno == EINTR);

      if (acode != -1)
	/* 'path' is 'amode' accessible */
	rcode = PIOUS_OK;

      else
	/* error accessing 'path'; set rcode appropriately */
	switch (errno)
	  {
	  case EACCES:
	  case EROFS:
	    rcode = PIOUS_EACCES;
	    break;
	  case ENAMETOOLONG:
	    rcode = PIOUS_ENAMETOOLONG;
	    break;
	  case ENOENT:
	    rcode = PIOUS_ENOENT;
	    break;
	  case ENOTDIR:
	    rcode = PIOUS_ENOTDIR;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * FS_open_max() - See pfs.h for description
 */

#ifdef __STDC__
long FS_open_max(void)
#else
long FS_open_max()
#endif
{
#ifdef OPEN_MAX
return (OPEN_MAX);
#else
return (sysconf(_SC_OPEN_MAX));
#endif
}




/*
 * FS_rename() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_rename(char *old,
	      char *new)
#else
int FS_rename(old, new)
     char *old;
     char *new;
#endif
{
  int rcode, ocode;
  
  /* rename file - guard against signal interrupts */
  while ((ocode = rename(old, new)) == -1 && errno == EINTR);
  
  if (ocode != -1)
    /* file renamed without error */
    rcode = PIOUS_OK;
  else
    /* error - return appropriate code */
    switch (errno)
      {
      case EACCES:
      case EROFS:
	rcode = PIOUS_EACCES;
	break;
      case EBUSY:
	rcode = PIOUS_EBUSY;
	break;
      case EEXIST:
      case ENOTEMPTY:
	rcode = PIOUS_ENOTEMPTY;
	break;
      case EINVAL:
	rcode = PIOUS_EINVAL;
	break;
      case EISDIR:
	rcode = PIOUS_EISDIR;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOSPC:
	rcode = PIOUS_ENOSPC;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      case EXDEV:
	rcode = PIOUS_EXDEV;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return rcode;
}




/*
 * FS_chmod() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_chmod(char *path,
	     pious_modet mode)
#else
int FS_chmod(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode, ocode;
  mode_t o_mode;

  /* set o_mode to requested file permission bits */
  o_mode = pious2posix_perm(mode);

  /* set file creation mask to not interfere with o_mode permission bits */
  umask((mode_t)0);
  
  /* change file mode - guard against signal interrupts */
  while ((ocode = chmod(path, o_mode)) == -1 && errno == EINTR);
  
  if (ocode != -1)
    /* mode changed without error */
    rcode = PIOUS_OK;
  else
    /* error - return appropriate code */
    switch (errno)
      {
      case EACCES:
	rcode = PIOUS_EACCES;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      case EPERM:
      case EROFS:
	rcode = PIOUS_EPERM;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return rcode;
}




/*
 * FS_unlink() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_unlink(char *path)
#else
int FS_unlink(path)
     char *path;
#endif
{
  int rcode, ocode, scode;
  struct stat fstatus;

  /* obtain file status -- guard against signal interrupts */

  while ((scode = stat(path, &fstatus)) == -1 && errno == EINTR);

  if (scode == -1)
    /* error accessing 'path'; set rcode appropriately */
    switch (errno)
      {
      case EACCES:
	rcode = PIOUS_EACCES;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  /* determine if file is a directory; if so, forbid unlinking */

  else if (S_ISDIR(fstatus.st_mode))
    /* file is a directory; do not permit unlinking */
    rcode = PIOUS_EPERM;

  /* remove (unlink) file -- guard against signal interrupts */

  else
    {
      while ((ocode = unlink(path)) == -1 && errno == EINTR);
  
      if (ocode != -1)
	/* file removed without error */
	rcode = PIOUS_OK;
      else
	/* error - return appropriate code */
	switch (errno)
	  {
	  case EACCES:
	  case EROFS:
	    rcode = PIOUS_EACCES;
	    break;
	  case EPERM:
	    rcode = PIOUS_EPERM;
	    break;
	  case ENAMETOOLONG:
	    rcode = PIOUS_ENAMETOOLONG;
	    break;
	  case ENOENT:
	    rcode = PIOUS_ENOENT;
	    break;
	  case ENOTDIR:
	    rcode = PIOUS_ENOTDIR;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * FS_mkdir() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_mkdir(char *path,
	     pious_modet mode)
#else
int FS_mkdir(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode, ocode;
  mode_t o_mode;

  /* set o_mode to requested file permission bits */
  o_mode = pious2posix_perm(mode);

  /* set file creation mask to not interfere with o_mode permission bits */
  umask((mode_t)0);
  
  /* make directory - guard against signal interrupts */
  while ((ocode = mkdir(path, o_mode)) == -1 && errno == EINTR);
  
  if (ocode != -1)
    /* directory created without error */
    rcode = PIOUS_OK;
  else
    /* error - return appropriate code */
    switch (errno)
      {
      case EACCES:
      case EROFS:
	rcode = PIOUS_EACCES;
	break;
      case EEXIST:
	rcode = PIOUS_EEXIST;
	break;
      case EMLINK:
	rcode = PIOUS_EINSUF;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      case ENOSPC:
	rcode = PIOUS_ENOSPC;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return rcode;
}




/*
 * FS_rmdir() - See pfs.h for description.
 */

#ifdef __STDC__
int FS_rmdir(char *path)
#else
int FS_rmdir(path)
     char *path;
#endif
{
  int rcode, ocode;
  
  /* remove directory - guard against signal interrupts */
  while ((ocode = rmdir(path)) == -1 && errno == EINTR);
  
  if (ocode != -1)
    /* directory removed without error */
    rcode = PIOUS_OK;
  else
    /* error - return appropriate code */
    switch (errno)
      {
      case EACCES:
      case EROFS:
	rcode = PIOUS_EACCES;
	break;
      case EBUSY:
	rcode = PIOUS_EBUSY;
	break;
      case EEXIST:
      case ENOTEMPTY:
	rcode = PIOUS_ENOTEMPTY;
	break;
      case ENAMETOOLONG:
	rcode = PIOUS_ENAMETOOLONG;
	break;
      case ENOENT:
	rcode = PIOUS_ENOENT;
	break;
      case ENOTDIR:
	rcode = PIOUS_ENOTDIR;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return rcode;
}




/* Local function definitions */

/*
 * pious2posix_perm()
 *
 * Parameters:
 *
 *   mode - PIOUS permission bits
 *
 * Convert PIOUS permission bits to the POSIX equivalent.
 */

#ifdef __STDC__
static mode_t pious2posix_perm(pious_modet mode)
#else
static mode_t pious2posix_perm(mode)
     pious_modet mode;
#endif
{
  mode_t posix_mode;

  posix_mode = 0;

  if (mode & PIOUS_IRUSR)
    posix_mode |= S_IRUSR;

  if (mode & PIOUS_IWUSR)
    posix_mode |= S_IWUSR;

  if (mode & PIOUS_IXUSR)
    posix_mode |= S_IXUSR;


  if (mode & PIOUS_IRGRP)
    posix_mode |= S_IRGRP;

  if (mode & PIOUS_IWGRP)
    posix_mode |= S_IWGRP;

  if (mode & PIOUS_IXGRP)
    posix_mode |= S_IXGRP;


  if (mode & PIOUS_IROTH)
    posix_mode |= S_IROTH;

  if (mode & PIOUS_IWOTH)
    posix_mode |= S_IWOTH;

  if (mode & PIOUS_IXOTH)
    posix_mode |= S_IXOTH;

  return posix_mode;
}




/*
 * posix2pious_perm()
 *
 * Parameters:
 *
 *   mode - POSIX permission bits
 *
 * Convert POSIX permission bits to the PIOUS equivalent.
 */

#ifdef __STDC__
static pious_modet posix2pious_perm(mode_t mode)
#else
static pious_modet posix2pious_perm(mode)
     mode_t mode;
#endif
{
  pious_modet pious_mode;

  pious_mode = 0;

  if (mode & S_IRUSR)
    pious_mode |= PIOUS_IRUSR;

  if (mode & S_IWUSR)
    pious_mode |= PIOUS_IWUSR;

  if (mode & S_IXUSR)
    pious_mode |= PIOUS_IXUSR;


  if (mode & S_IRGRP)
    pious_mode |= PIOUS_IRGRP;

  if (mode & S_IWGRP)
    pious_mode |= PIOUS_IWGRP;

  if (mode & S_IXGRP)
    pious_mode |= PIOUS_IXGRP;


  if (mode & S_IROTH)
    pious_mode |= PIOUS_IROTH;

  if (mode & S_IWOTH)
    pious_mode |= PIOUS_IWOTH;

  if (mode & S_IXOTH)
    pious_mode |= PIOUS_IXOTH;

  return pious_mode;
}
