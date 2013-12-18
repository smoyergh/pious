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




/* PIOUS Library Functions (PLIB)
 *
 * @(#)plib.h	2.2  28 Apr 1995  Moyer
 *
 *
 * INTRODUCTION:
 *
 *   The PIOUS library functions define the file model and interface (API)
 *   exported to the user.  Essentially, library functions translate user
 *   operations into the appropriate PIOUS Data Server (PDS) and PIOUS
 *   Service Coordinator (PSC) service requests.
 *
 *   The following is a very brief overview of the PIOUS API.
 *
 *
 * FILE MODEL/INTERFACE:
 *
 *   The PIOUS API defines two-dimensional file objects.  Each PIOUS file
 *   is composed of one or more physically disjoint data segments, where
 *   each data segment is a linear sequence of zero or more bytes.  The
 *   number of data segments in a file is specified at the time of creation
 *   and does not change for the life of the file.
 *
 *   An application may open a PIOUS file with any one of the following views:
 *
 *     * global - the file appears as a linear sequence of data bytes; all
 *                processes in a group share a single file pointer
 *
 *     * independent - a file appears as a linear sequence of data bytes; each
 *                     process in a group maintains a local file pointer
 *
 *     * segmented - the segmented nature of the file is exposed; each process
 *                   accesses a specified segment via a local pointer
 *
 *  For the global and independent file views, a linear sequence of bytes
 *  is defined by ordering the bytes in a file by fixed size blocks taken
 *  round-robin from each segment; i.e. data is striped across segments.
 *  The stripe unit size is specified by the application at the time the
 *  file is opened.
 *
 *  Note that a PIOUS file view only defines an access mapping and does not
 *  alter the physical representation.  Thus a file can always be opened
 *  with any view, though all processes in a group must use the same view.
 *
 *  Open PIOUS files are accessed via a traditional read/write interface.
 *  PIOUS provides sequential consistency of access to files opened under
 *  any view, guaranteeing that both data access and file pointer update are
 *  atomic with respect to concurrency.
 *
 *
 * DECLUSTERING
 *
 *  The PIOUS system declusters files on a set of data servers specified
 *  either in the open function or via a configuration file supplied at
 *  system start-up.  File declustering is performed by mapping file
 *  data segments to data servers in a round-robin fashion.
 *
 *  Note that if the number of file data segments is a multiple of the number
 *  of data servers on which the file is declustered, then a linear view of
 *  the file results in an access pattern that is equivalent to traditional
 *  disk striping.
 *
 *
 * USER-TRANSACTIONS
 *
 *  PIOUS files are accessed via read/write functions, as discussed above.
 *  Additional functionality is gained by allowing read/write operations
 *  to be logically grouped within a single transaction.  Transactions are
 *  an atomic unit with repect to concurrency, guaranteeing sequential
 *  consistency with respect to all other transactions and individual
 *  accesses.
 *
 *
 * GENERAL ENVIRONMENT
 *
 *  The PIOUS API provides a set of file access and maintenance functions
 *  that is essentially equivalent to a subset of the POSIX.1 API, but
 *  extended to meet the needs of a distributed environment.  Thus functions
 *  that in POSIX.1 would apply to a single file system, e.g. mkdir, apply
 *  to each file system accessed by a PIOUS data server.
 *
 * ----------------------------------------------------------------------------
 *
 *
 * Function Summary:
 *
 * pious_{s}open()
 * pious_{s}popen()
 * pious_close()
 * pious_fstat()
 * pious_sysinfo()
 *
 * pious_read()
 * pious_oread()
 * pious_pread()
 * pious_write()
 * pious_owrite()
 * pious_pwrite()
 * pious_lseek()
 *
 * pious_tbegin()
 * pious_tabort()
 * pious_tend()
 *
 * pious_setcwd()
 * pious_getcwd()
 * pious_umask()
 *
 * pious_{s}chmod()
 * pious_{s}unlink()
 * pious_{s}mkdir()
 * pious_{s}rmdir()
 * pious_{s}chmoddir()
 *
 * pious_shutdown()
 *
 * A detailed description of each of the above functions is presented below.
 */


#ifndef  PIOUS_LIB_H
#define  PIOUS_LIB_H




/*
 * pious_{s}popen()
 * pious_{s}open()
 *
 * Parameters:
 *
 *   dsv       - data server information vector
 *   dsvcnt    - data server information vector count
 *
 *   group     - process' group
 *   path      - file path
 *   view      - file view
 *   map       - view dependent access mapping
 *   faultmode - access faultmode
 *   oflag     - open flags
 *   mode      - creation permission bits
 *   seg       - creation segment count
 *
 *
 *
 * pious_{s}popen():
 *
 * Open PIOUS file 'path', with view 'view', access mapping 'map', and fault
 * tolerance mode 'faultmode', on default or specified data servers,
 * for a process in group 'group', with access mode determined by 'oflag'.
 * If the file does not exist and 'oflag' specifies file creation, then a file
 * with 'seg' data segments is created, with permission (access control) bits
 * set according to 'mode'.  If the file exists and 'oflag' indicates
 * truncation, then all data segments in the file are truncated IF the process
 * is the FIRST to open the file.
 *
 * Upon successful completion, a non-negative integer representing the
 * lowest numbered unused file descriptor is returned.
 *
 *
 * 'view' is one of: PIOUS_GLOBAL, PIOUS_INDEPENDENT, PIOUS_SEGMENTED
 *
 * 'map' specifies:
 *    stripe unit size    - when view is PIOUS_GLOBAL or PIOUS_INDEPENDENT, and
 *    data segment (>= 0) - when view is PIOUS_SEGMENTED
 *
 * 'faultmode' is one of: PIOUS_STABLE, PIOUS_VOLATILE
 *
 * 'oflag' is the bitwise inclusive OR of:
 *    exactly one of:     PIOUS_RDONLY, PIOUS_WRONLY, PIOUS_RDWR and
 *    any combination of: PIOUS_CREAT, PIOUS_TRUNC
 *
 *    note: PIOUS_TRUNC can not be used in conjunction with PIOUS_RDONLY
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 *
 * pious_{s}open():
 *
 * Equivalent to pious_{s}popen({dsv, dsvcnt,}
 *                              GROUP, path, PIOUS_INDEPENDENT, 1,
 *                              PIOUS_VOLATILE, oflag, mode, SEG)
 *
 * where GROUP is a special unique group name and SEG is equal to the number
 * of default data servers.
 *
 *
 * Returns: pious_{s}popen, pious_{s}open
 *
 *   >= 0  - file descriptor for successfully opened/created file
 *   <  0  - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument value, or PIOUS_TRUNC specified
 *                            with PIOUS_RDONLY, or file does not exist and
 *                            PIOUS_CREAT but access mode in 'oflag' not
 *                            permitted by 'mode', or no PDS executable on host
 *       PIOUS_EACCES       - path search permission denied, or file exists and
 *                            'oflag' access mode denied or 'view' or
 *                            'faultmode' inconsistent, or file does not exist
 *                            and write permission to create denied
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOENT       - file does not exist on data servers and create
 *                            not specified or 'path' is the empty string
 *       PIOUS_EPERM        - create operation not permitted; 'path' name
 *                            conflict on one or more data servers
 *       PIOUS_ENOSPC       - no space to extend directory or file system
 *       PIOUS_EINSUF       - file table full, or too many groups with file
 *                            open, or too many procs in group, or insufficient
 *                            system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_open(char *path,
	       int oflag,
	       pious_modet mode);

int pious_sopen(struct pious_dsvec *dsv,
		int dsvcnt,
		char *path,
		int oflag,
		pious_modet mode);

int pious_popen(char *group,
		char *path,
		int view,
		pious_sizet map,
		int faultmode,
		int oflag,
		pious_modet mode,
		int seg);

int pious_spopen(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *group,
		 char *path,
		 int view,
		 pious_sizet map,
		 int faultmode,
		 int oflag,
		 pious_modet mode,
		 int seg);
#else
int pious_open();

int pious_sopen();

int pious_popen();

int pious_spopen();
#endif




/*
 * pious_close()
 *
 * Parameters:
 *
 *   fd - file descriptor
 *
 * Close file referenced by file descriptor 'fd'
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file closed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF   - invalid file descriptor
 *       PIOUS_EPERM   - file accessed in user-transaction; close not permitted
 *       PIOUS_EINSUF  - insufficient system resources to complete
 *       PIOUS_ETPORT  - error condition in underlying transport system
 *       PIOUS_EUNXP   - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_close(int fd);
#else
int pious_close();
#endif




/*
 * pious_fstat()
 *
 * Parameters:
 *
 *   fd  - file descriptor
 *   buf - status structure pointer
 *
 * Query status of file referenced by descriptor 'fd'
 *
 * Returns:
 *
 *   PIOUS_OK (0) - status information obtained
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF   - invalid file descriptor
 *       PIOUS_EINVAL  - invalid 'buf' argument
 *       PIOUS_EUNXP   - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_fstat(int fd,
		struct pious_stat *buf);
#else
int pious_fstat();
#endif




/*
 * pious_sysinfo()
 *
 * Parameters:
 *
 *   name - configuration/status parameter name
 *
 * Query value of system configuration/status parameter 'name'.  Valid
 * parameters are:
 *
 *   PIOUS_DS_DFLT  - number of default data servers
 *   PIOUS_OPEN_MAX - maximum number of open file descriptors for process
 *   PIOUS_TAG_LOW  - lower bound of message tag range used by PIOUS
 *   PIOUS_TAG_HIGH - upper bound of message tag range used by PIOUS
 *   PIOUS_BADSTATE - 1 if PIOUS system state is inconsistent, 0 otherwise
 *
 * Returns:
 *
 *   >= 0  - value of system configuration/status parameter
 *   <  0  - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument value
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
long pious_sysinfo(int name);
#else
long pious_sysinfo();
#endif




/*
 * pious_{o|p}read()
 *
 * Parameters:
 *
 *   fd     - file descriptor
 *   buf    - buffer
 *   nbyte  - byte count
 *   offset - starting offset
 *
 * pious_read() attempts to read 'nbyte' bytes from file 'fd' into the
 * buffer 'buf'.  Starting offset is determined by the file pointer
 * associated with 'fd'.  Upon successful completion, the file pointer
 * is incremented by the number of bytes read.
 *
 * pious_oread() performs the same action as pious_read() and returns the
 * starting offset, as determined by the file pointer, in 'offset'.
 *
 * pious_pread() performs the same action as pious_read() except that the
 * starting offset is determined by 'offset'; the file pointer associated
 * with 'fd' remains unaffected.
 *
 * Any error in reading implies that the user-transaction/access is aborted
 * or that the PIOUS system state is inconsistent.
 *
 * Returns: pious_read(), pious_oread(), pious_pread()
 *
 *   >= 0 - number of bytes read (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF    - 'fd' is not a valid descriptor open for reading
 *       PIOUS_EINVAL   - 'offset', 'nbyte', or 'buf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EPERM    - file and user-transaction faultmode inconsistent
 *       PIOUS_EABORT   - user-transaction/access aborted normally
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error logs
 */

#ifdef __STDC__
pious_ssizet pious_read(int fd,
			char *buf,
			pious_sizet nbyte);

pious_ssizet pious_oread(int fd,
			 char *buf,
			 pious_sizet nbyte,
			 pious_offt *offset);

pious_ssizet pious_pread(int fd,
			 char *buf,
			 pious_sizet nbyte,
			 pious_offt offset);
#else
pious_ssizet pious_read();

pious_ssizet pious_oread();

pious_ssizet pious_pread();
#endif




/*
 * pious_{o|p}write()
 *
 * Parameters:
 *
 *   fd     - file descriptor
 *   buf    - buffer
 *   nbyte  - byte count
 *   offset - starting offset
 *
 * pious_write() attempts to write 'nbyte' bytes to file 'fd' from the
 * buffer 'buf'.  Starting offset is determined by the file pointer
 * associated with 'fd'.  Upon successful completion, the file pointer is
 * incremented by the number of bytes actually written.
 *
 * pious_owrite() performs the same action as pious_write() and returns the
 * starting offset, as determined by the file pointer, in 'offset'.
 *
 * pious_pwrite() performs the same action as pious_write() except that the
 * starting offset is determined by 'offset'; the file pointer associated
 * with 'fd' remains unaffected.
 *
 * Any error in writing implies that the user-transaction/access is aborted
 * or that the PIOUS system state is inconsistent.
 *
 * Returns: pious_write(), pious_owrite(), pious_pwrite()
 *
 *   >= 0 - number of bytes written (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF    - 'fd' is not a valid descriptor open for writing
 *       PIOUS_EINVAL   - 'offset', 'nbyte', or 'buf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EPERM    - file and user-transaction faultmode inconsistent
 *       PIOUS_EABORT   - user-transaction/access aborted normally
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error logs
 */

#ifdef __STDC__
pious_ssizet pious_write(int fd,
			 char *buf,
			 pious_sizet nbyte);

pious_ssizet pious_owrite(int fd,
			  char *buf,
			  pious_sizet nbyte,
			  pious_offt *offset);

pious_ssizet pious_pwrite(int fd,
			  char *buf,
			  pious_sizet nbyte,
			  pious_offt offset);
#else
pious_ssizet pious_write();

pious_ssizet pious_owrite();

pious_ssizet pious_pwrite();
#endif




/*
 * pious_lseek()
 *
 * Parameters:
 *
 *   fd     - file descriptor
 *   offset - file offset
 *   whence - PIOUS_SEEK_SET or PIOUS_SEEK_CUR
 *
 * pious_lseek() sets the file pointer for file 'fd' as follows:
 *
 * 1) if 'whence' is PIOUS_SEEK_SET then the pointer is set to byte 'offset'
 * 2) if 'whence' is PIOUS_SEEK_CUR then the pointer is set to its current
 *    value plus 'offset'
 *
 * Any error in updating the pointer implies that the user-transaction/access
 * is aborted or that the PIOUS system state is inconsistent.
 *
 * Returns:
 *
 *   >= 0 - resulting file pointer value
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF    - 'fd' is not a valid descriptor
 *       PIOUS_EINVAL   - 'whence' argument not a proper value, or resulting
 *                        pointer offset would be invalid
 *       PIOUS_EABORT   - user-transaction/access aborted normally
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error logs
 */

#ifdef __STDC__
pious_offt pious_lseek(int fd,
		       pious_offt offset,
		       int whence);
#else
pious_offt pious_lseek();
#endif




/*
 * pious_tbegin()
 *
 * Parameters:
 *
 *   faultmode - transaction fault mode (PIOUS_VOLATILE or PIOUS_STABLE)
 *
 * Mark the beginning of a user-transaction.  All accesses performed after
 * a successful pious_tbegin() and prior to a pious_tend()/pious_tabort()
 * are atomic with respect to concurrency.  If 'faultmode' specifies
 * PIOUS_STABLE then the accesses are atomic with respect to system failure
 * as well.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - user-transaction successfully initiated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL  - invalid 'faultmode' argument
 *       PIOUS_EPERM   - user-transaction already in progess
 *       PIOUS_EUNXP   - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_tbegin(int faultmode);
#else
int pious_tbegin();
#endif




/*
 * pious_tend()
 *
 * Parameters:
 *
 * pious_tend() completes a user-transaction.  If successful, all file
 * updates are installed atomically and become visible.  Any error implies
 * that the transaction is aborted or that the PIOUS system state is
 * inconsistent.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - user-transaction successfully completed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - user-transaction aborted normally
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error logs
 */

#ifdef __STDC__
int pious_tend(void);
#else
int pious_tend();
#endif




/*
 * pious_tabort()
 *
 * Parameters:
 *
 * pious_tabort() terminates a user-transaction.  If successful, the effects
 * of all file updates are undone.  Any error implies that the PIOUS system
 * state is inconsistent.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - user-transaction successfully terminated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error logs
 */

#ifdef __STDC__
int pious_tabort(void);
#else
int pious_tabort();
#endif




/*
 * pious_setcwd()
 *
 * Parameters:
 *
 *   path - current working directory path string
 *
 * Set the current working directory path string to 'path'.  The current
 * working directory string is used as the path prefix for all file path names.
 *
 * By default, the current working directory path string is the null string.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - current working directory string assigned
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_setcwd(char *path);
#else
int pious_setcwd();
#endif




/*
 * pious_getcwd()
 *
 * Parameters:
 *
 *   buf  - buffer
 *   size - buffer size
 *
 * Copy the current working directory path string into the buffer 'buf' of
 * size 'size'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - current working directory string copied
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL   - invalid 'buf' or 'size' argument
 *       PIOUS_EINSUF   - buffer size is insufficient
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_getcwd(char *buf, pious_sizet size);
#else
int pious_getcwd();
#endif




/*
 * pious_umask()
 *
 * Parameters:
 *
 *   cmask     - file creation mask
 *   prevcmask - previous file creation mask
 *
 * Set the file mode creation mask to 'cmask', returning the previous value
 * of the mask in 'prevcmask'.  Only the file permission bits of 'cmask' are
 * used, all others are ignored.
 *
 * The file mode creation mask is used by the pious_open(), pious_{s}popen(),
 * and pious_{s}mkdir() functions to turn off permission bits in the 'mode'
 * argument.  Permission bits set in 'cmask' are cleared for a file to be
 * created.
 *
 * By default, the file mode creation mask is zero (0).
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file mode creation mask set
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL   - invalid 'prevcmask' argument
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_umask(pious_modet cmask,
		pious_modet *prevcmask);
#else
int pious_umask();
#endif




/*
 * pious_{s}chmod()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - data server information vector count
 *
 *   path   - file path
 *   mode   - file permission bits
 *
 * Set the permission bits of PIOUS file 'path', on default or specified data
 * servers, to the value specified by 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 *
 * Pious_{s}chmod() operates on PIOUS files ONLY; to set the permission bits
 * of directory files use pious_{s}chmoddir().
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file mode successfully updated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument or no PDS executable on host
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist on data servers or 'path' is
 *                            the empty string
 *       PIOUS_EBUSY        - file in use; operation denied
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_chmod(char *path,
		pious_modet mode);

int pious_schmod(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path,
		 pious_modet mode);
#else
int pious_chmod();

int pious_schmod();
#endif




/*
 * pious_{s}unlink()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - data server information vector count
 *
 *   path   - file path
 *
 * Unlink PIOUS file 'path' on default or specified data servers.
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file successfully removed
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument or no PDS executable on host
 *       PIOUS_EACCES       - path search permission or write permission denied
 *                            or insufficient privileges to perform operation
 *       PIOUS_ENOENT       - file does not exist on data servers or 'path' is
 *                            the empty string
 *       PIOUS_EBUSY        - file in use; operation denied
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_unlink(char *path);

int pious_sunlink(struct pious_dsvec *dsv,
		  int dsvcnt,
		  char *path);
#else
int pious_unlink();

int pious_sunlink();
#endif




/*
 * pious_{s}mkdir()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - data server information vector count
 *
 *   path   - directory file path name
 *   mode   - directory file permission bits
 *
 * Create directory file 'path' with permission bits set according to 'mode'
 * on all default or specified data servers.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully created on all data servers
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument or no PDS executable on host
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EEXIST       - named file exists
 *       PIOUS_ENOENT       - path prefix component does not exist or 'path' is
 *                            the empty string
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_mkdir(char *path,
		pious_modet mode);

int pious_smkdir(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path,
		 pious_modet mode);
#else
int pious_mkdir();

int pious_smkdir();
#endif




/*
 * pious_{s}rmdir()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - data server information vector count
 *
 *   path   - directory file path name
 *
 * Remove directory file 'path' on all default or specified data servers.
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully removed on all data servers
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument or no PDS executable on host
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EBUSY        - directory in use
 *       PIOUS_ENOTEMPTY    - directory not empty
 *       PIOUS_ENOENT       - directory file does not exist or 'path' is the
 *                            empty string
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_rmdir(char *path);

int pious_srmdir(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path);
#else
int pious_rmdir();

int pious_srmdir();
#endif




/*
 * pious_{s}chmoddir()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - data server information vector count
 *
 *   path   - directory file path name
 *   mode   - directory file permission bits
 *
 * Set the file permission bits of directory file 'path' to 'mode' on all
 * default or specified data servers.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 *
 * In specifying a set of data servers, all host names must be unique,
 * and host search root and log directory paths must be non-empty with '/' as
 * the first character.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file mode updated on all data servers
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument or no PDS executable on host
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' is the empty string
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int pious_chmoddir(char *path,
		   pious_modet mode);

int pious_schmoddir(struct pious_dsvec *dsv,
		    int dsvcnt,
		    char *path,
		    pious_modet mode);
#else
int pious_chmoddir();

int pious_schmoddir();
#endif




/*
 * pious_shutdown()
 *
 * Parameters:
 *
 * Request that the PIOUS system be shutdown.  The PIOUS system will only
 * be shutdown if all files open by all processes have been closed; ie if
 * no process has a PIOUS file open.
 *
 * Note that successful completion of the pious_shutdown() operation does
 * not gaurantee that the system has been shutdown, only that a shutdown
 * request has been successfully issued.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - shutdown requested OR system not active
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM   - calling process has open files or a user-transaction
 *                       in progress
 *       PIOUS_EINSUF  - insufficient system resources to complete
 *       PIOUS_ETPORT  - error condition in underlying transport system
 *       PIOUS_EUNXP   - unexpected error condition encountered
 */

#ifdef __STDC__
int pious_shutdown(void);
#else
int pious_shutdown();
#endif




#endif /* PIOUS_LIB_H */
