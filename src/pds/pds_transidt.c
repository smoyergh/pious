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
 * @(#)pds_transidt.c	2.2  28 Apr 1995  Moyer
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

/* Include Files */

#include "gpmacro.h"
#include "psys.h"

#include "pious_errno.h"

#include "pds_transidt.h"


/*
 * Private Variable Definitions
 */

static transid_initialized = FALSE;

static pds_transidt transid_last;


/*
 * Function Definitions - Transaction ID ADT
 */


/*
 * transid_assign() - See pds_transidt.h for description
 */

#ifdef __STDC__
int transid_assign(pds_transidt *transid)
#else
int transid_assign(transid)
     pds_transidt *transid;
#endif
{
  int rcode;
  pds_transidt transid_tmp;

  /* Unique transaction ids are assigned by using a unique host id, a unique
   * process id, and a monotonically increasing time stamp.
   */

  /* first transaction id can simply be assigned; guaranteed unique */

  if (!transid_initialized)
    { /* determine host id and process id and assign time stamp */
      transid_tmp.procid = SYS_getpid();

      if (SYS_gethostid(&transid_tmp.hostid) == PIOUS_OK &&
	  SYS_gettimeofday(&transid_tmp.sec, &transid_tmp.usec) == PIOUS_OK)
	{
	  rcode               = PIOUS_OK;
	  transid_initialized = TRUE;
	}
      else
	rcode = PIOUS_EUNXP;
    }

  /* successive transaction ids must have a time stamp greater than previous */

  else
    { /* insure that time stamp is different from that previously assigned */
      transid_tmp.hostid = transid_last.hostid;
      transid_tmp.procid = transid_last.procid;

      rcode = PIOUS_OK;

      do
	if (SYS_gettimeofday(&transid_tmp.sec, &transid_tmp.usec) != PIOUS_OK)
	  rcode = PIOUS_EUNXP;

      while (rcode == PIOUS_OK &&
	     transid_tmp.sec  == transid_last.sec &&
	     transid_tmp.usec == transid_last.usec);
    }

  /* transid_tmp is a unique transaction id; save and return */
  if (rcode == PIOUS_OK)
    *transid = transid_last = transid_tmp;

  return rcode;
}
