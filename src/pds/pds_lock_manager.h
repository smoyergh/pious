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




/* PIOUS Data Server (PDS): Lock Manager
 *
 * @(#)pds_lock_manager.h	2.2  28 Apr 1995  Moyer
 *
 * The pds_lock_manager exports all locking functions necessary to implement
 * a Strict Two-Phase Locking protocol (2PL) for transaction operations.
 *
 * Function Summary:
 *
 *   LM_rlock();
 *   LM_wlock();
 *   LM_rfree();
 *   LM_wfree();
 */

#define LM_GRANT 0
#define LM_DENY  1

/*
 * LM_rlock()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   fhandle  - file handle
 *   offset   - starting offset
 *   nbyte    - byte count
 *
 * Request read lock for file 'fhandle' starting at 'offset' and
 * extending for 'nbyte'.
 *
 * Returns:
 *
 *   LM_GRANT - lock granted
 *   LM_DENY  - lock denied
 *
 * Note: Assumes offset, nbyte, and (offset + (nbyte - 1)) have been checked
 *       and are confirmed to be valid and within limits defined in
 *       include/pious_types.h.
 */

#ifdef __STDC__
int LM_rlock(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte);
#else
int LM_rlock();
#endif




/*
 * LM_wlock()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   fhandle  - file handle
 *   offset   - starting offset
 *   nbyte    - byte count
 *
 * Request write lock for file 'fhandle' starting at 'offset' and
 * extending for 'nbyte'.
 *
 * Returns:
 *
 *   LM_GRANT - lock granted
 *   LM_DENY  - lock denied
 *
 * Note: Assumes offset, nbyte, and (offset + (nbyte - 1)) have been checked
 *       and are confirmed to be valid and within limits defined in
 *       include/pious_types.h.
 */

#ifdef __STDC__
int LM_wlock(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte);
#else
int LM_wlock();
#endif




/*
 * LM_rfree()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Free all read locks held by transaction 'transid'.
 *
 * Returns:
 *
 */

#ifdef __STDC__
void LM_rfree(pds_transidt transid);
#else
void LM_rfree();
#endif




/*
 * LM_wfree()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Free all write locks held by transaction 'transid'.
 *
 * Returns:
 *
 */

#ifdef __STDC__
void LM_wfree(pds_transidt transid);
#else
void LM_wfree();
#endif
