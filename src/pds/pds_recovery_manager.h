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




/* PIOUS Data Server (PDS): Recovery Manager
 *
 * @(#)pds_recovery_manager.h	2.2 28 Apr 1995 Moyer
 *
 * The pds_recovery_manager exports all functions necessary for logging and
 * checkpointing transactions during normal operation, and for performing
 * recovery (returning files to a consistent state) after a system failure.
 *
 * Function Summary:
 *
 *   RM_trans_log();
 *   RM_trans_state();
 *   RM_checkpt();      [not implemented]
 *   RM_recover();      [not implemented]
 */




/* Symbolic constants for defining transaction final state */

#define RM_TRANS_COMMIT  0    /* transaction committed */
#define RM_TRANS_ABORT   1    /* transaction aborted */
#define RM_TRANS_UNKNOWN 2    /* transaction in uncertainty period */




/*
 * RM_trans_log()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   wbuf    - write buffer list
 *
 * Log write operations for transaction 'transid'. Write operations are
 * specified via 'wbuf', a linked list of records of type 'struct RM_wbuf'.
 *
 * Since RM_trans_log() is called in preparation for committement, the log
 * record's final committement state is unknown; this state is updated
 * to commit/abort via RM_trans_state() once the final committement state
 * is decided.
 *
 * NOTE: It is assumed that in the 'wbuf' write buffer list, write records
 *       reflect the temporal order of write operations, beginning with the
 *       first and proceeding to the last.
 *
 * Returns:
 *
 *   >= 0 - transaction write operations successfully logged; value returned
 *          is the log entry handle (lhandle) required by RM_trans_state().
 *
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ECHCKPT - transaction NOT logged; check-point required
 *                       for PDS to continue
 *       PIOUS_EFATAL  - fatal error; check PDS error log
 */

struct RM_wbuf{
  pds_fhandlet fhandle;   /* file handle */
  pious_offt offset;      /* starting offset */
  pious_sizet nbyte;      /* byte count */
  char *buf;              /* buffer */
  struct RM_wbuf *next;   /* pointer to next RM_wbuf record in list */
};

#ifdef __STDC__
pious_offt RM_trans_log(pds_transidt transid,
			struct RM_wbuf *wbuf);
#else
pious_offt RM_trans_log();
#endif




/*
 * RM_trans_state()
 *
 * Parameters:
 *
 *   lhandle - log handle (returned by RM_trans_log())
 *   state   - RM_TRANS_COMMIT or RM_TRANS_ABORT
 *
 * For log record 'lhandle', set final committement state to 'state'.
 *
 * WARNING: RM_trans_state() can not validate 'lhandle'; bad values will
 *          result in corruption of the transaction log file.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - transaction final committement state logged
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - 'state' argument is invalid
 *       PIOUS_EUNXP  - state NOT logged; unexpected error
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
int RM_trans_state(pious_offt lhandle,
		   int state);
#else
int RM_trans_state();
#endif
