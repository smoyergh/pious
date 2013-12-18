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




/* PIOUS Data Server (PDS): Stable Storage Manager
 *
 * @(#)pds_sstorage_manager.h	2.2  28 Apr 1995  Moyer
 *
 * The pds_sstorage_manager exports all functions necessary for accessing,
 * querying, updating, and maintaining stable storage.
 *
 * Function Summary:
 *
 *   SS_init();
 *
 *   SS_lookup();
 *   SS_read();
 *   SS_write();
 *   SS_faccess();
 *   SS_stat();
 *   SS_rename();    [not implemented]
 *   SS_chmod();
 *   SS_unlink();
 *
 *   SS_mkdir();
 *   SS_rmdir();
 *
 *   SS_logread();
 *   SS_logwrite();
 *   SS_logsync();
 *   SS_logtrunc();
 *
 *   SS_errlog();
 *
 */


/* Export global flags indicating PDS stable storage state */

extern int SS_fatalerror;  /* fatal error has occured; PDS can not continue */
extern int SS_recover;     /* recovery required before PDS can continue */
extern int SS_checkpoint;  /* checkpoint required before PDS can continue */




/*
 * SS_init()
 *
 * Parameters:
 *
 *   logpath - log directory pathname
 *
 * Initialize the stable storage manager, with system log files stored
 * in the directory 'logpath'.
 *
 * Must be called prior to any other stable storage operations, except
 * SS_errlog(), or those operations will return a fatal error.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - storage manager initialized and ready for operation
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ERECOV - recovery required prior to normal PDS operation
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_init(char *logpath);
#else
int SS_init();
#endif




/*
 * SS_lookup()
 *
 * Parameters:
 *
 *   path      - file path name string
 *   fhandle   - file handle
 *   cflag     - file creation flag
 *   mode      - file creation access control (permission) bits
 *
 * Obtain a file handle for regular file 'path' and place in 'fhandle'.
 * If 'path' does not exist and 'cflag' indicates file creation, then create
 * file with access control (permission) bits set to 'mode'.
 * If 'path' does exist and 'cflag' indicates file truncation, then the
 * file length is set to zero.
 *
 * 'cflag' is the inclusive OR of:
 *    exactly one of:     PIOUS_NOCREAT, PIOUS_CREAT and
 *    any combination of: PIOUS_TRUNC
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * NOTE: If a file is successfully truncated then MUST insure that
 *       cached data from file 'path' is invalidated to prevent read
 *       or write of stale data.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully located/created file
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid 'cflag' argument
 *       PIOUS_EACCES       - path search permission denied or write permission
 *                            to create or truncate file denied.
 *       PIOUS_ENOENT       - file does not exist and PIOUS_NOCREAT, or
 *                            PIOUS_CREAT and path prefix component does not
 *                            exist or path argument empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a dir
 *       PIOUS_ENOTREG      - file is not a regular file
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_lookup(char *path,
	      pds_fhandlet *fhandle,
	      int cflag,
	      pious_modet mode);
#else
int SS_lookup();
#endif




/*
 * SS_read()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *   offset  - starting offset
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Read file 'fhandle' starting at 'offset' bytes from the beginning
 * and proceeding for 'nbyte' bytes; place results in buffer 'buf'.
 *
 * Returns:
 *
 *   >= 0 - number of bytes read and placed in buffer (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES - read is invalid access mode for 'fhandle'
 *       PIOUS_EINVAL - file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints
 *       PIOUS_EINSUF - insufficient system resources to perform operation
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
pious_ssizet SS_read(pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf);
#else
pious_ssizet SS_read();
#endif




/*
 * SS_write()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *   offset    - starting offset
 *   nbyte     - byte count
 *   buf       - buffer
 *   faultmode - PIOUS_VOLATILE or PIOUS_STABLE
 *
 * Write file 'fhandle' starting at 'offset' bytes from the beginning
 * and proceeding for 'nbyte' bytes the data in buffer 'buf'.
 *
 * The faultmode indicates either an asynchronous update (PIOUS_VOLATILE)
 * that is NOT forced to disk, or a synchronous update (PIOUS_STABLE) that
 * is not complete until the data resides on disk.
 *
 * Returns:
 *
 *   >= 0 - number of bytes written (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES - write is invalid access mode for 'fhandle'
 *       PIOUS_EINVAL - file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints, or 'faultmode' not valid
 *       PIOUS_EFBIG  - write would cause file size to exceed SYSTEM constraint
 *       PIOUS_ENOSPC - no free space remains on device 
 *       PIOUS_EINSUF - insufficient system resources to perform operation
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
pious_ssizet SS_write(pds_fhandlet fhandle,
		      pious_offt offset,
		      pious_sizet nbyte,
		      char *buf,
		      int faultmode);
#else
pious_ssizet SS_write();
#endif




/*
 * SS_faccess()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *   amode   - file accessability
 *
 * Determine accessability of file 'fhandle' and place in 'amode'.
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *   PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 * Returns:
 *
 *   PIOUS_OK (0) - 'amode' indicates file accessibility
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - invalid/stale 'fhandle' argument
 *       PIOUS_EINSUF - insufficient system resources to perform operation
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_faccess(pds_fhandlet fhandle,
	       int *amode);
#else
int SS_faccess();
#endif




/*
 * SS_stat()
 *
 * Parameters:
 *
 *   path    - file path name
 *   mode    - file mode bits (permission and type bits)
 *
 * Obtains mode of file 'path' and places it in 'mode'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully obtained status information
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist or path empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EUNXP        - unexpected error encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_stat(char *path,
	    pious_modet *mode);
#else
int SS_stat();
#endif




/*
 * SS_chmod()
 *
 * Parameters:
 *
 *   path    - file path name
 *   mode    - file permission bits
 *   amode   - file accessability
 *
 * Set the file permission bits of file 'path' to the value specified
 * by 'mode'.  File accessability, after performing SS_chmod(), is
 * returned in 'amode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *   PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 *
 * NOTE: Prior to performing SS_chmod(), MUST insure that cached
 *       data from file 'path' is flushed AND invalidated so as not to
 *       allow/inhibit operations that were invalid/valid prior to
 *       changing the file permission bits.
 *
 *       If 'path' is a directory file, then cached data from files
 *       contained in directory 'path' can also be effected.
 *   
 * Returns:
 *
 *   PIOUS_OK (0) - file mode successfully updated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_chmod(char *path,
	     pious_modet mode,
	     int *amode);
#else
int SS_chmod();
#endif




/*
 * SS_unlink()
 *
 * Parameters:
 *
 *   path    - file path name
 *
 * Remove non-directory file 'path'.
 *
 * NOTE: Upon successfull completion of SS_unlink(), MUST insure that
 *       cached data from file 'path' is invalidated to prevent read
 *       from or write-back to a non-existent file.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix is not a directory
 *       PIOUS_EPERM        - 'path' is a directory; operation denied
 *       PIOUS_EINSUF       - insufficient system resources
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_unlink(char *path);
#else
int SS_unlink();
#endif




/*
 * SS_mkdir()
 *
 * Parameters:
 *
 *   path    - file path name
 *   mode    - file permission bits
 *
 * Create directory file 'path' with access control bits set according 
 * to the value of 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully created
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EEXIST       - the named file exists
 *       PIOUS_ENOENT       - path prefix component does not exist or 'path'
 *                            arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_mkdir(char *path,
	     pious_modet mode);
#else
int SS_mkdir();
#endif




/*
 * SS_rmdir()
 *
 * Parameters:
 *
 *   path    - file path name
 *
 * Remove directory file 'path', if empty.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EBUSY        - directory in use
 *       PIOUS_ENOTEMPTY    - directory not empty
 *       PIOUS_ENOENT       - directory file does not exist or 'path' arg empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_rmdir(char *path);
#else
int SS_rmdir();
#endif




/*
 * SS_logread()
 *
 * Parameters:
 *
 *   offset  - starting offset
 *   whence  - PIOUS_SEEK_SET or PIOUS_SEEK_END
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Read from log file starting at 'offset' bytes from the position indicated
 * by 'whence', PIOUS_SEEK_SET for beginning of file and PIOUS_SEEK_END for
 * end of file, and proceeding for 'nbyte' bytes; place result in buffer 'buf'.
 *
 * Returns:
 *
 *   >= 0 - number of bytes read and placed in buffer (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - resulting file offset or nbyte is not a proper
 *                      value or exceeds SYSTEM constraints, or 'whence'
 *                      argument is invalid.
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
pious_ssizet SS_logread(pious_offt offset,
			int whence,
			pious_sizet nbyte,
			char *buf);
#else
pious_ssizet SS_logread();
#endif




/*
 * SS_logwrite()
 *
 * Parameters:
 *
 *   offset  - starting offset
 *   whence  - PIOUS_SEEK_SET or PIOUS_SEEK_END
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Write the data in buffer 'buf' to the log file starting at 'offset' bytes
 * fron the position indicated by 'whence', PIOUS_SEEK_SET for beginning of
 * file and PIOUS_SEEK_END for end of file, and proceeding for 'nbyte' bytes.
 *
 * NOTE: SS_logwrite() is NOT a STABLE update operation. Stabalize log
 *       operations via SS_logsync().
 *
 * Returns:
 *
 *   >= 0 - number of bytes written (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - resulting file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints, or 'whence' arg invalid
 *       PIOUS_EFBIG  - write would cause file size to exceed SYSTEM constraint
 *       PIOUS_ENOSPC - no free space remains on device 
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
pious_ssizet SS_logwrite(pious_offt offset,
			 int whence,
			 pious_sizet nbyte,
			 char *buf);
#else
pious_ssizet SS_logwrite();
#endif




/*
 * SS_logsync()
 *
 * Parameters:
 *
 * Force the result of all update operations generated by SS_logwrite() to
 * stable storage (disk).
 *
 * Returns:
 *
 *   PIOUS_OK (0) - write operations forced to disk
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_logsync(void);
#else
int SS_logsync();
#endif




/*
 * SS_logtrunc()
 *
 * Parameters:
 *
 * Truncates transaction log file; for use in performing check-pointing
 * and recovery.
 *
 * NOTE: Prior to calling SS_logtrunc(), must insure that cached data from
 *       ALL files is flushed AND invalidated; invalidation is required
 *       due to the manner in which the pds_sstorage_manager maintains
 *       file handles.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - transaction log file truncated without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int SS_logtrunc(void);
#else
int SS_logtrunc();
#endif




/*
 * SS_errlog()
 *
 * Parameters:
 *
 *   module  - module name
 *   func    - function name
 *   errno   - PIOUS error code
 *   errmsg  - error message text
 *
 * Output error message to error log, if possible.  Otherwise, error
 * message is written to stderr.  Error message components include
 * the module name, function name, PIOUS error code, and a message
 * from the calling function.
 *
 * Note: if no error code is associated with message, the errno should be
 *       set to zero (0).
 *
 * Returns:
 */

#ifdef __STDC__
void SS_errlog(char *module,
	       char *func,
	       int errno,
	       char *errmsg);
#else
void SS_errlog();
#endif
