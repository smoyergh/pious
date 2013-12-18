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




/* PIOUS Data Server (PDS): Transaction ID Data Type
 *
 * @(#)pds_transidt.h	2.2  28 Apr 1995  Moyer
 *
 * Definition and implementation of the pds_transidt abstract data type.
 * Objects of this type are used to uniquely identify transactions.
 *
 * Function Summary:
 *
 *   transid_assign();
 *   transid_eq();
 *   transid_gt();
 *   transid_hash();
 */


/* Transaction id type definition */

typedef struct {
  unsigned long hostid;    /* unique host id */
  long procid;             /* unique process id */
  long sec;                /* time stamp: seconds */
  long usec;               /* time stamp: useconds */
} pds_transidt;




/*
 * transid_assign()
 *
 * Parameters:
 *
 *   transid - transaction id
 *
 * Assign 'transid' a unique transaction id.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - transid_assign() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP - unexpected error condition
 */

#ifdef __STDC__
int transid_assign(pds_transidt *transid);
#else
int transid_assign();
#endif




/*
 * transid_eq()
 *
 * Parameters:
 *
 *   transid_1 - transaction id
 *   transid_2 - transaction id
 *
 * Returns:
 *
 *   FALSE (0) - transid_1 != transid_2
 *   TRUE  (1) - transid_1 == transid_2
 */

/*
int transid_eq(pds_transidt transid_1,
	       pds_transidt transid_2);
*/

/* Check in order likely to minimize comparisons when not equal */

#define transid_eq(transid_1, transid_2) \
((transid_1).usec   == (transid_2).usec   && \
 (transid_1).sec    == (transid_2).sec    && \
 (transid_1).procid == (transid_2).procid && \
 (transid_1).hostid == (transid_2).hostid)




/*
 * transid_gt()
 *
 * Parameters:
 *
 *   transid_1 - transaction id
 *   transid_2 - transaction id
 *
 * Returns:
 *
 *   FALSE (0) - transid_1 <= transid_2
 *   TRUE  (1) - transid_1 >  transid_2
 *
 * Tuple Ordering (time stamp first for fair scheduling):
 *
 *   (sec, usec, hostid, procid)
 */

/*
int transid_gt(pds_transidt transid_1,
	       pds_transidt transid_2);
*/

#define transid_gt(transid_1, transid_2) \
(((transid_1).sec    > (transid_2).sec)    ? 1 :    \
 ((transid_1).sec    < (transid_2).sec)    ? 0 :    \
 ((transid_1).usec   > (transid_2).usec)   ? 1 :    \
 ((transid_1).usec   < (transid_2).usec)   ? 0 :    \
 ((transid_1).hostid > (transid_2).hostid) ? 1 :    \
 ((transid_1).hostid < (transid_2).hostid) ? 0 :    \
 ((transid_1).procid > (transid_2).procid) ? 1 : 0)




/*
 * transid_hash()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   maxval  - hash value bound
 *
 * Calculates hash value for transid in the range 0..(maxval - 1).
 *
 * Returns:
 *
 *   0..(maxval - 1) - if maxval > 0
 *   0               - if maxval <= 0
 */

/*
int transid_hash(pds_transidt transid,
		 int maxval);
*/

#define transid_hash(transid, maxval) \
(((int)(maxval) <= 0) ? (int)0 : (int)(((transid).usec) % (int)(maxval)))




/* Implementation Notes:
 *
 * 1) pdce/pdce.c exports message pack/unpack functions that require
 *    knowledge of the underlying pds_transidt type implementation.
 *    If the pds_transidt definition is altered, pdce/pdce.c must be
 *    updated accordingly.
 *
 * 2) pds/pds_daemon.c outputs profiling information that requires knowledge
 *    of the underlying pds_transidt type implementation. If the pds_transidt
 *    definition is altered, pds/pds_daemon.c must be updated accordingly.
 *
 * pdce/pdce.c and pds/pds_daemon.c are "friends" of the pds_fhandlet ADT in
 * the C++ sense.
 */
