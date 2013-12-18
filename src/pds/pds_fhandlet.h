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




/* PIOUS Data Server (PDS): File Handle Abstract Data Type
 *
 * @(#)pds_fhandlet.h	2.2  28 Apr 1995  Moyer
 *
 * Definition and implementation of the pds_fhandlet abstract data type.
 * Objects of this type are used to uniquely identify files managed
 * by the PDS.
 *
 * Function Summary:
 *
 *   fhandle_eq();
 *   fhandle_hash();
 */


/* File handle abstract data type */

typedef struct {
  unsigned long dev;        /* uniquely identifies file system */
  unsigned long ino;        /* uniquely identifies file within file system */
} pds_fhandlet;




/*
 * fhandle_eq()
 *
 * Parameters:
 *
 *   fhandle_1 - file handle
 *   fhandle_2 - file handle
 *
 * Returns:
 *
 *   FALSE (0) - fhandle_1 != fhandle_2
 *   TRUE  (1) - fhandle_1 == fhandle_2
 */

/*
int fhandle_eq(pds_fhandlet fhandle_1,
	       pds_fhandlet fhandle_2);
*/

/* Check in order likely to minimize comparisons when not equal */

#define fhandle_eq(fhandle_1, fhandle_2) \
((fhandle_1).ino == (fhandle_2).ino && \
 (fhandle_1).dev == (fhandle_2).dev)





/*
 * fhandle_hash()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *   maxval  - hash value bound
 *
 * Calculates hash value for fhandle in the range 0..(maxval - 1).
 *
 * Returns:
 *
 *   0..(maxval - 1) - if maxval > 0
 *   0               - if maxval <= 0
 */

/*
int fhandle_hash(pds_fhandlet fhandle,
		 int maxval);
*/

#define fhandle_hash(fhandle, maxval) \
(((int)(maxval) <= 0) ? (int)0 : (int)(((fhandle).ino) % (int)(maxval)))




/* Implementation Notes:
 *
 * 1) pds/pds_sstorage_manager.c assigns all values of type pds_fhandlet
 *    and thus requires knowledge of the underlying type implementation.
 *    If the pds_fhandlet definition is altered, pds/pds_sstorage_manager.c
 *    must be updated accordingly.
 *
 * 2) pdce/pdce.c exports message pack/unpack functions that require
 *    knowledge of the underlying pds_fhandlet type implementation.
 *    If the pds_fhandlet definition is altered, pdce/pdce.c must be
 *    updated accordingly.
 *
 * pds/pds_sstorage_manager.c and pdce/pdce.c are "friends" of the
 * pds_fhandlet ADT in the C++ sense.
 */
