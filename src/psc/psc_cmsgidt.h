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




/* PIOUS Service Coordinator (PSC): Control Message Id Macros
 *
 * @(#)psc_cmsgidt.h	2.2  28 Apr 1995  Moyer
 *
 * Definition of control message ID macros used by PSC in making PDS
 * control operation requests.
 *
 */


/* global PSC control message id - defined in psc/psc_daemon.c */

extern int psc_global_cmsgid;

#define CntrlIdNext \
((psc_global_cmsgid == PIOUS_INT_MAX) ? \
 (psc_global_cmsgid = 0) : (++psc_global_cmsgid))

#define CntrlIdLast    psc_global_cmsgid
