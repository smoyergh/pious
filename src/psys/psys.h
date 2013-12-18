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




/* PIOUS System Call Interface (PSYS)
 *
 * @(#)psys.h	2.2  28 Apr 1995  Moyer
 *
 * The PIOUS system call interface (PSYS) module exports an interface to
 * common, but potentially non-portable, native OS calls.  All functions
 * implemented here exist in some form on most systems, thus porting the
 * PSYS should be trivial.
 *
 * Function Summary:
 *
 *   SYS_gethostid();
 *   SYS_getpid();
 *   SYS_getuid();
 *   SYS_gettimeofday();
 */




/*
 * SYS_gethostid()
 *
 * Parameters:
 *
 *   hostid - unique host id
 *
 * Returns in 'hostid' a unique arithmetic identifier for the host of the
 * calling process.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - SYS_gethostid() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP - unexpected error condition
 */

#ifdef __STDC__
int SYS_gethostid(unsigned long *hostid);
#else
int SYS_gethostid();
#endif




/*
 * SYS_getpid()
 *
 * Parameters:
 *
 * Returns a unique arithmetic host identifier for the calling process.
 *
 * Returns:
 *
 *   long - unique host process id
 */

#ifdef __STDC__
long SYS_getpid(void);
#else
long SYS_getpid();
#endif




/*
 * SYS_getuid()
 *
 * Parameters:
 *
 * Returns a unique arithmetic host identifier for the user id of the calling
 * process.
 *
 * Returns:
 *
 *   long - unique host user id
 */

#ifdef __STDC__
long SYS_getuid(void);
#else
long SYS_getuid();
#endif




/*
 * SYS_gettimeofday()
 *
 * Parameters:
 *
 *   seconds  - current time, seconds portion
 *   useconds - current time, microseconds portion
 *
 * Returns the current time expressed in elapsed seconds and microseconds
 * since 00:00 Universal Coordinated Time, January 1, 1970; results
 * are placed in 'seconds' and 'useconds'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - SYS_gettimeofday() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EUNXP - unexpected error condition
 */

#ifdef __STDC__
int SYS_gettimeofday(long *seconds,
		     long *useconds);
#else
int SYS_gettimeofday();
#endif
