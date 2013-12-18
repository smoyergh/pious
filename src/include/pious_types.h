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




/* PIOUS: Scalar Data Types
 *
 * @(#)pious_types.h	2.2  28 Apr 1995  Moyer
 *
 * This file defines a set of scalar data types used in implementing PIOUS.
 *
 * Many of the defined types correspond to common POSIX.1 file system
 * interface types, which have a base arithmetic type that is implementation
 * defined. To support heterogeneity, it is necessary that all components
 * of the PIOUS system represent these types as equivalent arithmetic
 * types; hence these definitions. Furthermore, for each type all PIOUS
 * components must impose the same value constraints. Thus, minimum and maximum
 * type values must be defined that are the most restrictive supported by ANY
 * machine hosting PIOUS.  Making use of the standard <limits.h> is not
 * sufficient as values may vary on different machines.
 *
 * The default limit configuration of this file is consistent with the range
 * of data values supported on most 32-bit architectures; value ranges
 * are also consistent with current XDR protocol (RFC1014).
 */


#ifndef PIOUS_TYPES_H
#define PIOUS_TYPES_H


/* Base type limits */

#define PIOUS_INT_MIN     (-2147483647-1)  /* minimum value for int */
#define PIOUS_INT_MAX       2147483647     /* maximum value for int */

#define PIOUS_UINT_MAX      4294967295     /* maximum value unsigned int */

#define PIOUS_LONG_MIN    (-2147483647-1L) /* minimum value for long */
#define PIOUS_LONG_MAX      2147483647L    /* maximum value for long */

#define PIOUS_ULONG_MAX     4294967295L    /* maximum value unsigned long */

#define PIOUS_SCALAR_DIGITS_MAX     10     /* maximum digits in any scalar */


/* Derived types */

typedef long             pious_offt;       /* file offset */
#define PIOUS_OFFT_MIN   PIOUS_LONG_MIN    /* minimum value for pious_offt */
#define PIOUS_OFFT_MAX   PIOUS_LONG_MAX    /* maximum value for pious_offt */

typedef unsigned long    pious_sizet;      /* bytes requested */
#define PIOUS_SIZET_MAX  PIOUS_ULONG_MAX   /* maximum value for pious_sizet */

typedef long             pious_ssizet;     /* bytes returned */
#define PIOUS_SSIZET_MAX PIOUS_LONG_MAX    /* maximum value for pious_ssizet */

typedef unsigned long    pious_modet;      /* file permission and type bits */




/* Implementation Notes:
 *
 * 1) derived types pious_offt and pious_ssizet must be signed.
 *
 * 2) if derived type pious_offt must be larger than type long, then
 *    the data server routines that read/write/fetch&add integer array files,
 *    see pds/pds.h, must be modified to operate on the same extended integer
 *    type as pious_offt is declared to be, since shared file pointers are
 *    stored in integer array files.
 *
 * 3) it must be true that PIOUS_SIZET_MAX > PIOUS_OFFT_MAX; this allows
 *    the pds/pds_daemon.c routines to place a lock on all referenceable
 *    bytes in a file when necessary.
 *
 * 4) it must be true that PIOUS_SSIZET_MAX >= PIOUS_OFFT_MAX; generally
 *    these derived types will have the same base type.
 *
 * 5) pdce/pdce.h exports message pack/unpack macros that require
 *    knowledge of the underlying types for scalar derived types.
 *    If the above derived types are altered, or additional types are
 *    defined, pdce.h macros must be updated accordingly.
 */


#endif /* PIOUS_TYPES_H */




