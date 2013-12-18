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
 * @(#)pfs.h	2.2  28 Apr 1995  Moyer
 *
 * The PIOUS file system interface (PFS) module exports an interface to the
 * native OS file system.  PFS routines provide essentially POSIX 1003.1-19xx
 * equivalent semantics for file model/operations, with read() and write()
 * operations incorporating the functionality of the traditional lseek()
 * function.
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
 */



/*
 * FS_open()
 *
 * Parameters:
 *
 *   path    - file path name
 *   oflag   - access mode flags
 *   mode    - file permission bits
 *
 * Open file 'path' for access, with access mode determined by 'oflag'.
 * If file is to be created, access control bits are set according 
 * to the value of 'mode'. FS_open() returns a file descriptor
 * for accessing the open file.
 *
 * 'oflag' is the bitwise inclusive OR of:
 *    exactly one of: PIOUS_RDONLY, PIOUS_WRONLY, PIOUS_RDWR and
 *    any combination of: PIOUS_CREAT, PIOUS_TRUNC
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * Returns:
 *
 *   >= 0 - file descriptor; successfully open/created file 'path' for access
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid access mode specified or PIOUS_RDONLY
 *                            specified with PIOUS_TRUNC
 *       PIOUS_EACCES       - path search permission denied or access
 *                            mode (oflag) denied or write permission to
 *                            create or truncate file denied
 *       PIOUS_ENOENT       - file does not exist and PIOUS_CREAT not
 *                            specified, or PIOUS_CREAT and path prefix
 *                            component does not exist or path arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EINSUF       - insufficient system resources; too many
 *                            file descriptors open by process or system
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int FS_open(char *path,
	    int oflag,
	    pious_modet mode);
#else
int FS_open();
#endif




/*
 * FS_write()
 *
 * Parameters:
 *
 *   fildes  - file descriptor
 *   offset  - starting offset
 *   whence  - PIOUS_SEEK_SET or PIOUS_SEEK_END
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Write the data in buffer 'buf' to file 'fildes' starting at 'offset' bytes
 * from the position indicated by 'whence', PIOUS_SEEK_SET for beginning of
 * file and PIOUS_SEEK_END for end of file, and proceeding for 'nbyte' bytes.
 *
 * Write semantics are such that if the write operation begins past the current
 * end of file (EOF), there is an implicit (logical) write of zero (0) to all
 * bytes between the EOF and the start of the write.
 *
 * WARNING: NOT a STABLE write access; data must still be forced to disk
 *          via a FS_fsync() call.  Separating these two functions allows
 *          for performance optimization with no loss in fault tolerance, 
 *          given that FS_fsync() is called at appropriate execution
 *          points.
 *
 * Returns:
 *
 *   >= 0 - number of bytes written (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - 'fildes' not valid file dscrp open for writing
 *       PIOUS_EINVAL - resulting file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints, or 'whence' arg invalid
 *       PIOUS_EFBIG  - write would cause file size to exceed SYSTEM constraint
 *       PIOUS_ENOSPC - no free space remains on device
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *
 */

#ifdef __STDC__
pious_ssizet FS_write(int fildes,
		      pious_offt offset,
		      int whence,
		      pious_sizet nbyte,
		      char *buf);
#else
pious_ssizet FS_write();
#endif




/*
 * FS_read()
 *
 * Parameters:
 *
 *   fildes  - file descriptor
 *   offset  - starting offset
 *   whence  - PIOUS_SEEK_SET or PIOUS_SEEK_END
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Read file 'fildes' starting at 'offset' bytes from the position indicated
 * by 'whence', PIOUS_SEEK_SET for beginning of file and PIOUS_SEEK_END for
 * end of file, and proceeding for 'nbyte' bytes; place result in buffer 'buf'.
 *
 * Read operations at or past the current end of file (EOF) return a value of
 * zero. A read from a file prior to EOF that has not be written returns
 * bytes with value zero (0) in the unwritten positions.
 *
 * Returns:
 *
 *   >= 0 - number of bytes read (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - 'fildes' not valid file dscrp open for reading
 *       PIOUS_EINVAL - resulting file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints, or 'whence' arg invalid
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *
 */

#ifdef __STDC__
pious_ssizet FS_read(int fildes,
		     pious_offt offset,
		     int whence,
		     pious_sizet nbyte,
		     char *buf);
#else
pious_ssizet FS_read();
#endif




/*
 * FS_close()
 *
 * Parameters:
 *
 *   fildes  - file descriptor
 *
 * Deallocates the file descriptor indicated by 'fildes'.
 *
 * Returns:
 */

#ifdef __STDC__
void FS_close(int fildes);
#else
void FS_close();
#endif




/*
 * FS_fsync()
 *
 * Parameters:
 *
 *   fildes  - file descriptor
 *
 * Force to disk any buffered writes to file 'fildes'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - writes forced to disk
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF - 'fildes' not valid file descriptor open for writing
 *       PIOUS_EUNXP - unexpected error condition encountered
 *
 */

#ifdef __STDC__
int FS_fsync(int fildes);
#else
int FS_fsync();
#endif




/*
 * FS_stat()
 *
 * Parameters:
 *
 *   path    - file path name
 *   status  - file status information
 *
 * Obtains information about file 'path' and places it in 'status'
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
 */

struct FS_stat{
  pious_modet mode;      /* file permission and type bits; see pious_std.h */
  unsigned long ino;     /* file serial number */
  unsigned long dev;     /* file device ID */
  pious_offt size;       /* for regular files, size in bytes; otherwise 0 */
};

#ifdef __STDC__
int FS_stat(char *path,
	    struct FS_stat *status);
#else
int FS_stat();
#endif




/*
 * FS_fstat()
 *
 * Parameters:
 *
 *   fildes  - file descriptor
 *   status  - file status information
 *
 * Obtains information about file 'fildes' and places it in 'status'
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully obtained status information
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF - 'fildes' not valid file descriptor
 *       PIOUS_EUNXP - unexpected error encountered
 */

#ifdef __STDC__
int FS_fstat(int fildes,
	     struct FS_stat *status);
#else
int FS_fstat();
#endif




/*
 * FS_access()
 *
 * Parameters:
 *
 *   path    - file path name
 *   amode   - file accessability
 *
 * Determines if file 'path' exists or is accessible in the mode
 * specified in 'amode'.
 *
 * 'amode' is defined by (see pious_std.h):
 *   1) bitwise inclusive OR from set (PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK), or
 *   2) PIOUS_F_OK
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file accessible as specified
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - 'amode' improperly specified
 *       PIOUS_EACCES       - 'amode' access or path search permission denied
 *       PIOUS_ENOENT       - file does not exist or path empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix is not a directory
 *       PIOUS_EUNXP        - unexpected error encountered
 */

#ifdef __STDC__
int FS_access(char *path,
	      int amode);
#else
int FS_access();
#endif




/*
 * FS_open_max()
 *
 * Returns the maximum number of files that the process can have open
 * simultaneously.
 *
 */

#ifdef __STDC__
long FS_open_max(void);
#else
long FS_open_max();
#endif




/*
 * FS_rename()
 *
 * Parameters:
 *
 *   old    - file path name
 *   new    - file path name
 *
 * Rename file 'old' to file 'new'; file 'new' is removed if extant
 * and of the same type prior to call.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EBUSY        - directory named by 'old' or 'new' is in use
 *       PIOUS_ENOTEMPTY    - directory named by 'new' is not empty
 *       PIOUS_EINVAL       - path 'new' contains path 'old'
 *       PIOUS_EISDIR       - 'new' is directory and 'old' is not
 *       PIOUS_ENOENT       - file 'old' does not exist or 'old' or 'new'
 *                            arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of either path prefix not a directory
 *                            or 'old' is a directory and 'new' is not
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EXDEV        - 'new' and 'old' are on different file systems
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int FS_rename(char *old,
	      char *new);
#else
int FS_rename();
#endif




/*
 * FS_chmod()
 *
 * Parameters:
 *
 *   path    - file path name
 *   mode    - file permission bits
 *
 * Set the file permission bits of file 'path' to the value specified
 * by 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
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
 */

#ifdef __STDC__
int FS_chmod(char *path,
	     pious_modet mode);
#else
int FS_chmod();
#endif




/*
 * FS_unlink()
 *
 * Parameters:
 *
 *   path    - file path name
 *
 * Remove non-directory file 'path'.
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
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int FS_unlink(char *path);
#else
int FS_unlink();
#endif




/*
 * FS_mkdir()
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
 *       PIOUS_EINSUF       - insufficient system resources; maximum parent
 *                            link count
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int FS_mkdir(char *path,
	     pious_modet mode);
#else
int FS_mkdir();
#endif




/*
 * FS_rmdir()
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
 */

#ifdef __STDC__
int FS_rmdir(char *path);
#else
int FS_rmdir();
#endif
