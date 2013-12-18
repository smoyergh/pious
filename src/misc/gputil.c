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
 * @(#)gputil.c	2.2  28 Apr 1995  Moyer
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


/* Include Files */
#include "pious_errno.h"
#include "pious_types.h"

#include "psys.h"

#include "gputil.h"




/*
 * UTIL_errno2errtxt() - See gputil.h for description.
 */

#ifdef __STDC__
void UTIL_errno2errtxt(int errno,
			char **errnotxt,
			char **errtxt)
#else
void UTIL_errno2errtxt(errno, errnotxt, errtxt)
     int errno;
     char **errnotxt;
     char **errtxt;
#endif
{
  switch(errno)
    {
    case PIOUS_EACCES:
      *errnotxt = "PIOUS_EACCES";
      *errtxt   = PIOUS_EACCES_TXT;
      break;
    case PIOUS_EBADF:
      *errnotxt = "PIOUS_EBADF";
      *errtxt   = PIOUS_EBADF_TXT;
      break;
    case PIOUS_EBUSY:
      *errnotxt = "PIOUS_EBUSY";
      *errtxt   = PIOUS_EBUSY_TXT;
      break;
    case PIOUS_EEXIST:
      *errnotxt = "PIOUS_EEXIST";
      *errtxt   = PIOUS_EEXIST_TXT;
      break;
    case PIOUS_EFBIG:
      *errnotxt = "PIOUS_EFBIG";
      *errtxt   = PIOUS_EFBIG_TXT;
      break;
    case PIOUS_EINVAL:
      *errnotxt = "PIOUS_EINVAL";
      *errtxt   = PIOUS_EINVAL_TXT;
      break;
    case PIOUS_ENOTREG:
      *errnotxt = "PIOUS_ENOTREG";
      *errtxt   = PIOUS_ENOTREG_TXT;
      break;
    case PIOUS_EINSUF:
      *errnotxt = "PIOUS_EINSUF";
      *errtxt   = PIOUS_EINSUF_TXT;
      break;
    case PIOUS_ENAMETOOLONG:
      *errnotxt = "PIOUS_ENAMETOOLONG";
      *errtxt   = PIOUS_ENAMETOOLONG_TXT;
      break;
    case PIOUS_ENOENT:
      *errnotxt = "PIOUS_ENOENT";
      *errtxt   = PIOUS_ENOENT_TXT;
      break;
    case PIOUS_ENOSPC:
      *errnotxt = "PIOUS_ENOSPC";
      *errtxt   = PIOUS_ENOSPC_TXT;
      break;
    case PIOUS_ENOTDIR:
      *errnotxt = "PIOUS_ENOTDIR";
      *errtxt   = PIOUS_ENOTDIR_TXT;
      break;
    case PIOUS_ENOTEMPTY:
      *errnotxt = "PIOUS_ENOTEMPTY";
      *errtxt   = PIOUS_ENOTEMPTY_TXT;
      break;
    case PIOUS_EISDIR:
      *errnotxt = "PIOUS_EISDIR";
      *errtxt   = PIOUS_EISDIR_TXT;
      break;
    case PIOUS_EPERM:
      *errnotxt = "PIOUS_EPERM";
      *errtxt   = PIOUS_EPERM_TXT;
      break;
    case PIOUS_EXDEV:
      *errnotxt = "PIOUS_EXDEV";
      *errtxt   = PIOUS_EXDEV_TXT;
      break;
    case PIOUS_ETIMEOUT:
      *errnotxt = "PIOUS_ETIMEOUT";
      *errtxt   = PIOUS_ETIMEOUT_TXT;
      break;
    case PIOUS_EPROTO:
      *errnotxt = "PIOUS_EPROTO";
      *errtxt   = PIOUS_EPROTO_TXT;
      break;
    case PIOUS_ENOTLOG:
      *errnotxt = "PIOUS_ENOTLOG";
      *errtxt   = PIOUS_ENOTLOG_TXT;
      break;
    case PIOUS_ESRCDEST:
      *errnotxt = "PIOUS_ESRCDEST";
      *errtxt   = PIOUS_ESRCDEST_TXT;
      break;
    case PIOUS_ETPORT:
      *errnotxt = "PIOUS_ETPORT";
      *errtxt   = PIOUS_ETPORT_TXT;
      break;
    case PIOUS_EABORT:
      *errnotxt = "PIOUS_EABORT";
      *errtxt   = PIOUS_EABORT_TXT;
      break;
    case PIOUS_EUNXP:
      *errnotxt = "PIOUS_EUNXP";
      *errtxt   = PIOUS_EUNXP_TXT;
      break;
    case PIOUS_ECHCKPT:
      *errnotxt = "PIOUS_ECHCKPT";
      *errtxt   = PIOUS_ECHCKPT_TXT;
      break;
    case PIOUS_ERECOV:
      *errnotxt = "PIOUS_ERECOV";
      *errtxt   = PIOUS_ERECOV_TXT;
      break;
    case PIOUS_EFATAL:
      *errnotxt = "PIOUS_EFATAL";
      *errtxt   = PIOUS_EFATAL_TXT;
      break;
    default:
      *errnotxt = "";
      *errtxt   = "unknown error code";
      break;
    }
}




/*
 * UTIL_clock_mark() - See gputil.h for description.
 */

#ifdef __STDC__
void UTIL_clock_mark(util_clockt *clock)
#else
void UTIL_clock_mark(clock)
     util_clockt *clock;
#endif
{
  /* mark clock timer with current time */

  if (SYS_gettimeofday(&(clock->seconds), &(clock->useconds)) != PIOUS_OK)
    /* unable to determine time; set to null clock value */
    clock->seconds = clock->useconds = 0;
}




/*
 * UTIL_clock_delta() - See gputil.h for description.
 */

#ifdef __STDC__
unsigned long UTIL_clock_delta(util_clockt *clock,
			       int resolution)
#else
unsigned long UTIL_clock_delta(clock, resolution)
     util_clockt *clock;
     int resolution;
#endif
{
  long c_sec, c_usec;
  unsigned long c_delta;

  /* calculate delta from clock time */

  if ((clock->seconds == 0 && clock->useconds == 0) ||
      SYS_gettimeofday(&c_sec, &c_usec) != PIOUS_OK)
    /* null clock or unable to determine current time */
    c_delta = PIOUS_ULONG_MAX;

  else
    /* calculate delta at specified resolution */
    switch(resolution)
      {
      case UTIL_SEC:
	c_delta = ((double)(c_sec - clock->seconds) +
		   (double)(c_usec - clock->useconds) * 1.0e-6);
	break;

      case UTIL_USEC:
	c_delta = ((1000000 * (unsigned long)(c_sec - clock->seconds)) +
		   ((unsigned long)(c_usec - clock->useconds)));
	break;

      default:
	/* UTIL_MSEC */
	c_delta = ((double)(c_sec - clock->seconds) * 1.0e3 +
		   (double)(c_usec - clock->useconds) * 1.0e-3);
	break;
      }

  return c_delta;
}
