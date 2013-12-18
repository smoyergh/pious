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




/* PIOUS Service Coordinator (PSC): Remote Procedure Call Interface
 *
 * @(#)psc.h	2.2  28 Apr 1995  Moyer
 *
 *
 * INTRODUCTION:
 *
 *   The psc module provides a remote procedure call interface to operations
 *   provided by the PSC daemon.  The PSC daemon implements functions
 *   for managing the PIOUS environment and operating on parafile objects.
 *
 *
 * PARAFILE OBJECTS:
 *
 *   The PSC implements "parafiles", the basic file abstraction for the PIOUS
 *   system.  Parafiles are logically single files composed of one or
 *   more physically disjoint data segments.  Each data segment is composed of
 *   a linear sequence of zero or more bytes.  The number of data segments in
 *   a parafile is set at the time of creation and does not change for the life
 *   of the file.
 *
 *   Each parafile data segment resides on a single PIOUS data server (PDS).
 *   If the number of segments exceeds the maximum number of PDS for
 *   declustering the file, then data segments are mapped to data servers in a
 *   round-robin fashion.
 *
 *   Each parafile data segment is implemented as a file accessed via a PDS.
 *   In addition to data segment files, a parafile contains a single metadata
 *   file and a single shared pointer file, each of which are stored on the
 *   low order PDS (i.e. the PDS containing data segment file zero (0)).
 *   The metadata file contains information regarding the parafile structure.
 *   The shared pointer file is essentially a file of integers that serve
 *   as shared pointers into the parafile.
 *
 *   As described above, parafiles are implemented as a collection of "regular"
 *   files.  At each data server (PDS) on which a parafile is declustered,
 *   a directory is created that contains the files associated with a single
 *   parafile; the directory path name is the logical file path name at the
 *   given server.  Organizing parafiles in this manner helps alleviate file
 *   system name space pollution.
 *
 *   Note that the logical parafile structure directly reflects the physical
 *   structure that naturally results from declustering file data.
 *
 *
 * ACCESS CONTROL:
 *
 *   Access control to parafiles is performed via the standard Unix-style
 *   permission bits.  Directories containing parafile component files are
 *   given read/execute permission for all (user, group, other) and write
 *   permission for user only. Parafile metadata files are given read
 *   permission for all and write permission for user only, and parafile
 *   shared pointer files are give read/write permission for all.
 *   Parafile data segment files are given access permission as specified
 *   by the user.
 *
 *   The result is essentially a parafile with UFS access control, except
 *   that only the owner can unlink a parafile, and the parafile is always
 *   unlink-able since the parafile directory is owner writeable.
 *
 *
 * STATEFUL SERVICE:
 *
 *   The PSC has a notion of open files, unlike the PDS.  Client processes
 *   request the PSC to open files prior to performing file access, and then
 *   request that the files be closed after access is concluded. This allows
 *   the system to determine if all processes in a group open a file with
 *   the same view, controls file truncation, prevents the unlinking of open
 *   files, etc.
 *
 *   Unfortunately, stateful servers present serious fault-tolerance problems
 *   in a distributed system.  If a system is to be dynamically reconfigurable
 *   servers must be stateless, or must write the state to permanent storage.
 *
 *   To make PIOUS a continuously available and dynamically reconfigurable
 *   service, the PSC should be made stateless; i.e. the notion of open files
 *   must be eliminated.  To do so and still provide the same or similar
 *   service semantics will require implementing user library functions
 *   such as as open and unlink as collective operations, requiring processes
 *   to synchronize so that function parameters, system state, etc. can be
 *   checked.
 *
 *   Note that since the primary purpose of the PSC is to maintain state
 *   for the PIOUS system, eliminating the need for this state via the methods
 *   mentioned above eliminates the need for the PSC.  PSC functions can then
 *   be moved into the user library routines.
 *
 *
 * OPERATION PROTOCOL:
 *
 *   Because the PSC is statefull and not all operations are idempotent,
 *   e.g. open and close, the PSC does NOT use an operating protocol like
 *   that of the PDS control operation protocol to detect client/server crashes
 *   and restarts and provide a means for dealing with failures in the
 *   underlying message system.
 *
 *
 * STANDARD VS SERVER FUNCTIONS:
 *
 *   Most PSC functions have both a standard and "server" version.  The
 *   standard version presumes operations apply to files on a default
 *   set of data servers running on hosts specified at start-up.  The server
 *   version allows the user to specify a set of data server hosts that are to
 *   be utilized for a given function.  For the server version of functions,
 *   a data server (PDS) is spawned on any specified host that does not
 *   already have an active PDS.
 *
 *   Data server information is supplied to functions in a set of corresponding
 *   arrays that specify host names, host search root paths, and host log
 *   directory paths.  Host information *must* be specified in lexicographic
 *   order by host name, with no duplicates.  Furthermore, all host search
 *   root and log directory paths must be non-empty with '/' as the first
 *   character.  For performance this is not checked, since the PSC functions
 *   are called by PIOUS library routines that are presumed to validate/order
 *   user supplied host information.
 *
 *-----------------------------------------------------------------------------
 *
 *
 * Function Summary:
 *
 * PSC_{s}open();
 * PSC_{s}close();
 * PSC_{s}chmod();
 * PSC_{s}unlink();
 * PSC_{s}mkdir();
 * PSC_{s}rmdir();
 * PSC_{s}chmoddir();
 * PSC_config();
 * PSC_shutdown();
 *
 *
 * A detailed description of each of the above operations is presented below.
 * Descriptions apply to the "server" version of each operation; standard
 * versions are the obvious derivative.
 */




/* Data server information for "server" version of functions */

struct PSC_dsinfo{
  int cnt;          /* PDS count */
  char **hname;     /* PDS host names */
  char **spath;     /* PDS search root paths */
  char **lpath;     /* PDS log directory paths */
};




/*
 * PSC_sopen()
 * PSC_open()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   group     - process' group
 *   path      - parafile path name
 *   view      - parafile view
 *   faultmode - parafile faultmode
 *   oflag     - parafile open flags
 *   mode      - parafile creation permission bits
 *   seg       - parafile creation segment count
 *
 *   pf_info   - parafile information record
 *
 * Open parafile 'path', with view 'view' and fault tolerance mode 'faultmode',
 * on default or specified data servers, for a process in group 'group', with
 * access mode determined by 'oflag'.  If the file does not exist and
 * 'oflag' specifies file creation, then a parafile with 'seg' data segments
 * is created, with permission (access control) bits set according to 'mode'.
 * If the file exists and 'oflag' indicates truncation, then all data segments
 * in the parafile are truncated IF the process is the FIRST to open the file.
 *
 * Upon successful completion, information required for directly accessing the
 * parafile on the data servers over which it is declustered is returned
 * in 'pf_info'.
 *
 * Note that the 'view' argument is not interpreted, it is merely retained
 * to insure that all processes in group 'group' specify the same view.
 * Likewise, the 'faultmode' argument does not alter the state of the
 * file in any way; it is merely retained to insure that all processes in
 * the system share a file utilizing the same fault tolerance mode.
 *
 *
 * 'oflag' is the bitwise inclusive OR of:
 *    exactly one of:     PIOUS_RDONLY, PIOUS_WRONLY, PIOUS_RDWR and
 *    any combination of: PIOUS_CREAT, PIOUS_TRUNC
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'faultmode' is one of: PIOUS_STABLE, PIOUS_VOLATILE
 *
 *
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile successfully opened/created
 *   < 0          - error code defined in pious_errno.h; possible codes are:
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
 *       PIOUS_EINSUF       - file table full, or too many groups with parafile
 *                            open, or too many procs in group, or insufficient
 *                            system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

struct PSC_pfinfo {
  int pds_cnt;                /* parafile PDS count; degree of declustering */
  int seg_cnt;                /* parafile data segment count */
  pds_fhandlet sptr_fhandle;  /* parafile shared pointer file fhandle */
  pious_offt sptr_idx;        /* parafile shared pointer file group index */
  pds_fhandlet *seg_fhandle;  /* parafile data segment file handles */
  dce_srcdestt *pds_id;       /* parafile PDS message passing ids */
};

#ifdef __STDC__
int PSC_sopen(struct PSC_dsinfo *pds,
	      char *group,
	      char *path,
	      int view,
	      int faultmode,
	      int oflag,
	      pious_modet mode,
	      int seg,

	      struct PSC_pfinfo *pf_info);
#else
int PSC_sopen();
#endif

#define PSC_open(group, path, view, faultmode, oflag, mode, seg, pf_info) \
PSC_sopen((struct PSC_dsinfo *)NULL, \
	  group, path, view, faultmode, oflag, mode, seg, pf_info)




/*
 * PSC_sclose()
 * PSC_close()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   group     - process' group
 *   path      - parafile path name
 *
 * Close parafile 'path', on default or specified data servers, for a process
 * in group 'group'.
 *
 *
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile successfully closed
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument value
 *       PIOUS_ENOENT       - file not open by 'group' or 'path' is the
 *                            empty string
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int PSC_sclose(struct PSC_dsinfo *pds,
	       char *group,
	       char *path);
#else
int PSC_sclose();
#endif

#define PSC_close(group, path) \
PSC_sclose((struct PSC_dsinfo *)NULL, group, path)




/*
 * PSC_schmod()
 * PSC_chmod()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   path      - parafile path name
 *   mode      - parafile permission bits
 *
 * Set the file permission bits of parafile 'path', on default or specified
 * data servers, to the value specified by 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * NOTE: PSC_chmod() operates on parafiles ONLY; use PSC_chmoddir() to set
 *       the file permission bits of directory files.
 *
 *
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile mode successfully updated
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
int PSC_schmod(struct PSC_dsinfo *pds,
	       char *path,
	       pious_modet mode);
#else
int PSC_schmod();
#endif

#define PSC_chmod(path, mode) \
PSC_schmod((struct PSC_dsinfo *)NULL, path, mode)




/*
 * PSC_sunlink()
 * PSC_unlink()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   path      - parafile path name
 *
 * Unlink parafile 'path' on default or specified data servers.
 *
 *
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile successfully removed
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
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int PSC_sunlink(struct PSC_dsinfo *pds,
		char *path);
#else
int PSC_sunlink();
#endif

#define PSC_unlink(path) \
PSC_sunlink((struct PSC_dsinfo *)NULL, path)




/*
 * PSC_smkdir()
 * PSC_mkdir()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   path      - directory file path name
 *   mode      - directory file permission bits
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
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
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
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int PSC_smkdir(struct PSC_dsinfo *pds,
	       char *path,
	       pious_modet mode);
#else
int PSC_smkdir();
#endif

#define PSC_mkdir(path, mode) \
PSC_smkdir((struct PSC_dsinfo *)NULL, path, mode)




/*
 * PSC_srmdir()
 * PSC_rmdir()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   path      - directory file path name
 *
 * Remove directory file 'path' on all default or specified data servers.
 *
 *
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
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
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
int PSC_srmdir(struct PSC_dsinfo *pds,
	       char *path);
#else
int PSC_srmdir();
#endif

#define PSC_rmdir(path) \
PSC_srmdir((struct PSC_dsinfo *)NULL, path)




/*
 * PSC_schmoddir()
 * PSC_chmoddir()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   path      - directory file path name
 *   mode      - directory file permission bits
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
 * WARNING: In specifying a set of data servers, hosts must be specified
 *          in lexicographic order by host name, all host names must be
 *          unique, and host search root and log directory paths must be
 *          non-empty with '/' as the first character. This is not checked!
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
int PSC_schmoddir(struct PSC_dsinfo *pds,
		  char *path,
		  pious_modet mode);
#else
int PSC_schmoddir();
#endif

#define PSC_chmoddir(path, mode) \
PSC_schmoddir((struct PSC_dsinfo *)NULL, path, mode)




/*
 * PSC_config()
 *
 * Parameters:
 *
 *   buf - configuration information buffer
 *
 * Query service coordinator configuration information.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - configuration information successfully returned
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid argument
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

struct PSC_configinfo {
  int def_pds_cnt;       /* default PDS count */
};

#ifdef __STDC__
int PSC_config(struct PSC_configinfo *buf);
#else
int PSC_config();
#endif




/*
 * PSC_shutdown()
 *
 * Parameters:
 *
 * Request service coordinator to shutdown the PIOUS system, itself included.
 * The PIOUS system will only be shutdown if all files open by all processes
 * have been closed.
 *
 * Note that the service coordinator does NOT reply to a shutdown request;
 * this prevents blocking in the case that multiple shutdown requests are
 * sent while the service coordinator is still alive.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - shutdown request successfully sent OR system not active
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
int PSC_shutdown(void);
#else
int PSC_shutdown();
#endif
