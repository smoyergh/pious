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
 * @(#)psys.c	2.2  28 Apr 1995  Moyer
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



/* Include Files */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include "gpmacro.h"

#include "pious_errno.h"

#include "psys.h"




/*
 * Private Variable Definitions
 */

static int  hostid_initialized = FALSE;

static unsigned long myhostid;




/*
 * SYS_gethostid() - See psys.h for description.
 */

#ifdef __STDC__
int SYS_gethostid(unsigned long *hostid)
#else
int SYS_gethostid(hostid)
     unsigned long *hostid;
#endif
{
  int rcode;
  struct utsname name;
  struct hostent *hp;

  /* if hostid has previously been determined, return that value */
  if (hostid_initialized)
    {
      *hostid = myhostid;
      rcode   = PIOUS_OK;
    }

  /* determine my hostid */
  else
    { /* get host name */
      if (uname(&name) == -1)
	rcode = PIOUS_EUNXP;

      /* get host internet id */
      else if ((hp = gethostbyname(name.nodename)) == NULL)
	rcode = PIOUS_EUNXP;

      /* check that primary internet id is set */
      else if (hp->h_addr_list[0] == NULL)
	rcode = PIOUS_EUNXP;

      /* my hostid is primary internet id */
      else
	{ /* convert internet id from network to host byte-order */
	  *hostid = myhostid = ntohl(*((unsigned long *)hp->h_addr_list[0]));

	  hostid_initialized = TRUE;
	  rcode              = PIOUS_OK;
	}
    }

  return rcode;
}




/*
 * SYS_getpid() - See psys.h for description.
 */

#ifdef __STDC__
long SYS_getpid(void)
#else
long SYS_getpid()
#endif
{
  return (long)getpid();
}




/*
 * SYS_getuid() - See psys.h for description.
 */

#ifdef __STDC__
long SYS_getuid(void)
#else
long SYS_getuid()
#endif
{
  return (long)getuid();
}




/*
 * SYS_gettimeofday() - See psys.h for description.
 */

#ifdef __STDC__
int SYS_gettimeofday(long *seconds,
		     long *useconds)
#else
int SYS_gettimeofday(seconds, useconds)
     long *seconds;
     long *useconds;
#endif
{
  int rcode;
  struct timeval tval;

  if (gettimeofday(&tval, NULL) == -1)
    /* unable to determine time of day */
    rcode = PIOUS_EUNXP;
  else
    { /* got time; set results */
      *seconds  = tval.tv_sec;
      *useconds = tval.tv_usec;

      rcode     = PIOUS_OK;
    }

  return rcode;
}
