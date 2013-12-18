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




/* PIOUS Data Server (PDS): Cache Manager
 *
 * @(#)pds_cache_manager.h	2.2  28 Apr 1995  Moyer
 *
 * The pds_cache_manager exports all functions necessary to access
 * data from a cached data file.  All stable storage access operations,
 * except those required for logging, must pass through the pds_cache_manager
 * to maintain cache coherence.
 *
 * Function Summary:
 *
 * CM_read();
 * CM_write();
 * CM_flush();
 * CM_fflush();
 * CM_invalidate();
 * CM_finvalidate();
 */


/*
 * CM_read()
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
 *       PIOUS_EINSUF - insufficient system resources; retry operation
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
pious_ssizet CM_read(pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf);
#else
pious_ssizet CM_read();
#endif




/*
 * CM_write()
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
 * NOTE: CM_write(), in the absence of a fatal error,  either MUST succeed
 *       or MUST indicate that recovery is required.  This results from
 *       the fact that, in the No Undo/Redo recovery algorithm, write
 *       operations must be logged and committed prior to calling CM_write(),
 *       and once committed can not later be rejected.
 *
 *       The faultmode indicates the transaction type performing the write.
 *       PIOUS_STABLE is a "traditional" transaction requiring coherence be
 *       maintained in the event of system failure.  PIOUS_VOLATILE indicates
 *       a transaction that does not require fault-tolerance; such a
 *       transaction requires only serializability of access be guaranteed.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - write completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int CM_write(pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte,
	     char *buf,
	     int faultmode);
#else
int CM_write();
#endif




/*
 * CM_flush()
 *
 * Parameters:
 *
 * Flush contents of entire cache to stable storage.
 *
 * Note: Only data written with a faultmode of PIOUS_STABLE is flushed
 *       to disk via synchronous writes.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - cache contents successfully flushed to disk
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP  - unexpected error condition encountered;
 *                      cache flush incomplete
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int CM_flush(void);
#else
int CM_flush();
#endif




/*
 * CM_fflush()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *
 * Flush all cache entries associated with file 'fhandle'.
 *
 * Note: Only data written with a faultmode of PIOUS_STABLE is flushed
 *       to disk via synchronous writes.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - cache contents successfully flushed to disk
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP  - unexpected error condition encountered;
 *                      cache flush incomplete
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int CM_fflush(pds_fhandlet fhandle);
#else
int CM_fflush();
#endif




/*
 * CM_invalidate()
 *
 * Parameters:
 *
 * Invalidate all cache entries.
 *
 * NOTE: Dirty cache entries are NOT flushed to stable storage.
 *
 *       To facilitate dynamic recovery, CM_invalidate() may be performed
 *       regardless of the state of global stable storage flags
 *       SS_fatalerror, SS_recover, and SS_checkpoint.
 *
 * Returns:
 */

#ifdef __STDC__
void CM_invalidate(void);
#else
void CM_invalidate();
#endif




/*
 * CM_finvalidate()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *
 * Invalidate all cache entries associated with file 'fhandle'.
 *
 * NOTE: Dirty cache entries are NOT flushed to stable storage.
 *
 *       To facilitate dynamic recovery, CM_invalidate() may be performed
 *       regardless of the state of global stable storage flags
 *       SS_fatalerror, SS_recover, and SS_checkpoint.
 *
 * Returns:
 */

#ifdef __STDC__
void CM_finvalidate(pds_fhandlet fhandle);
#else
void CM_finvalidate();
#endif
