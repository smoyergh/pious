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




/* PIOUS Data Server (PDS): Data Manager
 *
 * @(#)pds_data_manager.h	2.2  28 Apr 1995  Moyer
 *
 * The pds_data_manager exports all functions necessary to perform read,
 * write, prepare (to commit), commit, and abort operations on behalf
 * of a transaction.
 *
 * The data manager presumes that the scheduler, via the Lock Manager,
 * implements a Strict Two-Phase locking protocol (2PL) for transaction data
 * access operations.  The data manager insures that such operations are
 * carried out in a consistent and fault-tolerant (recoverable) manner.
 *
 * The data manager also presumes the use of a Two-Phase commit protocol (2PC)
 * in determing the final outcome of a transaction, hence the need to prepare
 * a transaction for committement, via DM_prepare(), prior to actually
 * committing (DM_commit()).  Prepared transaction are recoverable and
 * are referred to here as stable transactions (PIOUS_STABLE); transactions
 * that are not prepared prior to committement are not recoverable and are
 * referred to here as volatile (PIOUS_VOLATILE).
 *
 * Note that in general the data manager maintains state for a given
 * transaction until that transaction is committed or aborted, at which point
 * all state is discarded.  There are two important exceptions to this rule
 * implemented by the DM_prepare() operation:
 *
 *   1) if DM_prepare() votes to abort a transaction all effects are undone
 *      and the state of the transaction is discarded, and
 *
 *   2) if the transaction is read-only and DM_prepare() votes to commit,
 *      which will always be the case, then the state of the transaction is
 *      discarded; this is known in the literature as the 2PC read-only
 *      optimization as the commitment is no longer required.
 *
 * Function Summary:
 *
 * DM_read();
 * DM_write();
 * DM_prepare();
 * DM_commit();
 * DM_abort();
 */


/*
 * DM_read()
 *
 * Parameters:
 *
 *   transid - transaction id
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
 *       PIOUS_EINVAL - 'offset' or 'nbyte' argument not a proper value
 *                      or exceeds SYSTEM constraints
 *       PIOUS_EINSUF - insufficient system resources; retry operation
 *       PIOUS_EPROTO - attempted read after prepare; 2PC protocol error
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_ERECOV - recovery required for PDS to continue
 *       PIOUS_EFATAL - fatal error; check PDS error log
 *
 * Note: Assumes offset, nbyte, and (offset + (nbyte - 1)) have been checked
 *       and are confirmed to be valid and within limits defined in
 *       include/pious_types.h.
 *
 */

#ifdef __STDC__
pious_ssizet DM_read(pds_transidt transid,
		     pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf);
#else
pious_ssizet DM_read();
#endif




/*
 * DM_write()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   fhandle - file handle
 *   offset  - starting offset
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Write file 'fhandle' starting at 'offset' bytes from the beginning
 * and proceeding for 'nbyte' bytes the data in buffer 'buf'.
 *
 * WARNING: Data in 'buf' NOT copied; 'buf' can NOT be deallocated by caller
 *          if DM_write() is successful, i.e. returns PIOUS_OK.
 *          DM_prepare(), DM_commit(), and DM_abort() will deallocate 'buf'
 *          when appropriate.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - write operation processed without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES - write is invalid access mode for 'fhandle'
 *       PIOUS_EINVAL - 'offset' or 'nbyte' argument not proper value
 *       PIOUS_EINSUF - insufficient system resources; retry operation
 *       PIOUS_EPROTO - attempted write after prepare; 2PC protocol error
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_EFATAL - fatal error; check PDS error log
 *
 * Note: Assumes offset, nbyte, and (offset + (nbyte - 1)) have been checked
 *       and are confirmed to be valid and within limits defined in
 *       include/pious_types.h.
 */

#ifdef __STDC__
int DM_write(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte,
	     char *buf);
#else
int DM_write();
#endif




/*
 * DM_prepare()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Prepare to commit transaction 'transid'.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - transaction prepared to commit
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ECHCKPT - transaction NOT prepared to commit; check-point
 *                       required for PDS to continue
 *       PIOUS_EFATAL  - fatal error; check PDS error log
 *
 * Note: PIOUS_OK is a vote to COMMIT. An error code is a vote to ABORT, in
 *       which case all effects of writes are undone and the transaction
 *       state is discarded.
 *
 *       DM_prepare() implements the 2PC read-only optimization; i.e. for
 *       read-only transactions, no further action need be taken by the
 *       data manager after performing DM_prepare().
 */

#ifdef __STDC__
int DM_prepare(pds_transidt transid);
#else
int DM_prepare();
#endif




/*
 * DM_commit()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Commit transaction 'transid'.  Transaction MUST have previously been
 * prepared for committement via DM_prepare() or will NOT be recoverable.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - transaction committed
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ENOTLOG  - commit NOT logged for prepared transaction
 *       PIOUS_ERECOV   - recovery required for PDS to continue
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Note: In the event of an error, must presume transaction result (commit)
 *       not logged for prepared transaction.
 */

#ifdef __STDC__
int DM_commit(pds_transidt transid);
#else
int DM_commit();
#endif




/*
 * DM_abort()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Abort transaction 'transid', undoing any effects.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - transaction aborted
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ENOTLOG  - abort NOT logged for prepared transaction
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Note: In the event of an error, must presume transaction result (abort)
 *       not logged for prepared transaction.
 */

#ifdef __STDC__
int DM_abort(pds_transidt transid);
#else
int DM_abort();
#endif
