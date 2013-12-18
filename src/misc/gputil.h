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




/* PIOUS: General Purpose Utilities
 *
 * @(#)gputil.h	2.2  28 Apr 1995  Moyer
 *
 * Defines a set of general purpose utilities useful throughout the PIOUS
 * implementation.
 *
 * Function Summary:
 *
 *   UTIL_errno2errtxt();
 *   UTIL_clock_mark();
 *   UTIL_clock_delta();
 */




/*
 * UTIL_errno2errtxt();
 *
 * Parameters:
 *
 *   errno    - PIOUS integer error code
 *   errnotxt - string representation of error code
 *   errtxt   - error code information string
 * 
 * Convert a given PIOUS error code to a string representation and supply
 * the appropriate error code information string.
 *
 * Note: Strings are in a static data area and must not be overwritten.
 *
 * Returns:
 */

#ifdef __STDC__
void UTIL_errno2errtxt(int errno,
			char **errnotxt,
			char **errtxt);
#else
void UTIL_errno2errtxt();
#endif




/*
 * UTIL_clock_mark()
 *
 * Parameters:
 *
 *   clock - clock timer
 *
 * Reset the timer 'clock'.  If timer can not be properly set, 'clock'
 * is set to the null clock value.
 *
 * Returns:
 */

typedef struct{
  long seconds;
  long useconds;
} util_clockt;


#ifdef __STDC__
void UTIL_clock_mark(util_clockt *clock);
#else
void UTIL_clock_mark();
#endif




/*
 * UTIL_clock_delta()
 *
 * Parameters:
 *
 *   clock      - clock timer
 *   resolution - timer resolution (UTIL_SEC, UTIL_MSEC, UTIL_USEC)
 *
 * Determine elapsed time since last time 'clock' was marked; i.e. since the
 * last call to UTIL_clock_mark(clock).  Elapsed time is computed at the
 * second, millisecond, or microsecond resolution.
 *
 * If 'clock' is the null clock value, or if elapsed time can not be
 * properly determined, then the elapsed time returned is PIOUS_ULONG_MAX.
 *
 * Note that elapsed time accuracy is limited both by the system clock and
 * the finite size of the function return type.
 *
 * If 'resolution' is an improper value, then elapsed time is determined
 * in milliseconds.
 *
 * Returns:
 *
 *   unsigned long - elapsed time or PIOUS_ULONG_MAX
 */

#define UTIL_SEC  0  /* second */
#define UTIL_MSEC 1  /* millisecond */
#define UTIL_USEC 2  /* microsecond */


#ifdef __STDC__
unsigned long UTIL_clock_delta(util_clockt *clock,
			       int resolution);
#else
unsigned long UTIL_clock_delta();
#endif
