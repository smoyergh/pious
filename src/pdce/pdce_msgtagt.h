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




/* PIOUS Distributed Computing Environment (PDCE): Message Tag ADT
 *
 * @(#)pdce_msgtagt.h	2.2  28 Apr 1995  Moyer
 *
 * Definition and implementation of the dce_msgtagt abstract data type.
 * Objects of this type are used to identify messages within the DCE.
 *
 * Function Summary:
 *
 *   msgtag_encode();
 *   msgtag_decode();
 *   msgtag_valid();
 */


/* Message tag type definition */

typedef int dce_msgtagt;

/* Specify any message tag in a receive */

#define DCE_ANY_TAG  ((dce_msgtagt) -1)



/* Message tag range allocated to PIOUS.
 *
 *   PIOUS is allocated (DCE_MSGTAGT_MAX + 1) consecutive message tags.
 *   The tag range may be adjusted via DCE_MSGTAGT_BASE, but must insure that:
 *
 *          DCE_MSGTAGT_BASE + DCE_MSGTAGT_MAX <= max_value_of(dce_msgtagt)
 *
 *   Assigning PIOUS a very restricted message tag range is necessary
 *   because PIOUS *may* share a single tag space with user code; such
 *   is the case with PVM and other common DCE.  MPI, among others,
 *   defines message contexts to eliminate this problem.
 */


/* Set DCE_MSGTAGT_MAX to accomodate constants that identify PDS and PSC
 * service request operation codes, as defined in pds/pds_msg_exchange.h
 * and psc/psc_msg_exchange.h, respectively.  It must be true that:
 *
 *          DCE_MSGTAGT_MAX == Max(PDS_OPCODE_MAX, PSC_OPCODE_MAX)
 */

#define DCE_MSGTAGT_MAX 17

#define DCE_MSGTAGT_BASE (PIOUS_INT_MAX - DCE_MSGTAGT_MAX)




/* Message tag operations */


/*
 * msgtag_encode()
 *
 * Parameters:
 *
 *   val  - value to encode
 * 
 * Encode a message tag with value 'val'; 'val' must be in the
 * range 0..DCE_MSGTAGT_MAX.
 *
 * Note: 'val' is assumed to be valid and is not checked.
 *
 * Returns:
 *
 *   dce_msgtagt - encoded message tag
 */

/*
dce_msgtagt msgtag_encode(int val);
*/

#define msgtag_encode(val) \
((dce_msgtagt)(DCE_MSGTAGT_BASE + (int)(val)))




/*
 * msgtag_decode()
 *
 * Parameters:
 *
 *   msgtag - a message tag to decode
 * 
 * Decode a message tag 'msgtag'; if the message tag is properly encoded,
 * the decoded value is in the range 0..DCE_MSGTAGT_MAX.
 *
 * Returns:
 *
 *   int - decoded message tag
 */

/*
int msgtag_decode(dce_msgtagt msgtag);
*/

#define msgtag_decode(msgtag) \
((int)((int)msgtag - DCE_MSGTAGT_BASE))




/*
 * msgtag_valid()
 *
 * Parameters:
 *
 *   msgtag - a message tag
 * 
 * Determine if 'msgtag' is a properly encoded message tag.
 *
 * Returns:
 *
 *   TRUE -  if msgtag is properly encoded
 *   FALSE - if msgtag is NOT properly encoded
 */

/*
int msgtag_valid(dce_msgtagt msgtag);
*/

#define msgtag_valid(msgtag) \
((int) \
 ((int)msgtag >= DCE_MSGTAGT_BASE && \
  (int)msgtag <= (DCE_MSGTAGT_BASE + DCE_MSGTAGT_MAX)))




/* Implementation Notes:
 *
 * 1) It is of course possible to have DCE_MSGTAGT_MAX updated automatically.
 *    However, doing so forces all files which include pdce_msgtagt.h
 *    to become dependent on both pds/pds_msg_exchange.h and
 *    psc/psc_msg_exchange.h, which is somewhat kludgy.
 *
 *    An alternative is to define functions instead of macros and write
 *    a pdce_msgtagt.c; then this file alone could be dependent on
 *    psc/psc_msg_exchange.h and pds/pds_msg_exchange.h.  However, I hate the
 *    idea of a function call for a simple addition and subtraction.
 */
