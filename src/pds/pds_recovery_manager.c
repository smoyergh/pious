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




/* PIOUS Data Server (PDS): Recovery Manager
 *
 * @(#)pds_recovery_manager.c	2.2 28 Apr 1995 Moyer
 *
 * The pds_recovery_manager exports all functions necessary for logging and
 * checkpointing transactions during normal operation, and for performing
 * recovery (returning files to a consistent state) after a system failure.
 *
 * The recovery manager, together with the data manager, implements a
 * No-Undo/Redo recovery algorithm.  Thus, only the write operations issued
 * by a given transaction must be logged; such a log is refered to as an
 * "intentions list".
 *
 * Function Summary:
 *
 *   RM_trans_log();
 *   RM_trans_state();
 *   RM_checkpt();      [not yet implemented]
 *   RM_recover();      [not yet implemented]
 *
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) RM_trans_log() and RM_trans_state() are currently "dummy" routines
 *      implemented solely for the purposes of benchmarking performance.
 *
 *   2) When implementing recovery, it is necessary to undo a number of
 *      performance/convenience hacks in pds_sstorage_manager routines.
 *      These hacks are well documented in the pds_sstorage_manager
 *      implementation notes and only require un-commenting to undo.
 */


/* Include Fildes */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"
#include "pds_sstorage_manager.h"

#include "pds_recovery_manager.h"


/*
 * Function Definitions - Recovery Manager Operations
 */


/*
 * RM_trans_log() - See pds_recovery_manager.h for description
 */

#ifdef __STDC__
pious_offt RM_trans_log(pds_transidt transid,
			struct RM_wbuf *wbuf)
#else
pious_offt RM_trans_log(transid, wbuf)
     pds_transidt transid;
     struct RM_wbuf *wbuf;
#endif
{
  pious_offt rcode;
  int tempstate;

  /* check for previous stable storage fatal error or log check-point flags */

  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else if (SS_checkpoint)
    rcode = PIOUS_ECHCKPT;

  /* write log record */

  else
    {
      tempstate = RM_TRANS_UNKNOWN;

      /* dummy log record header */
      SS_logwrite((pious_offt)0, PIOUS_SEEK_SET,
		  (pious_sizet)sizeof(pds_transidt), (char *)&transid);

      SS_logwrite((pious_offt)0, PIOUS_SEEK_SET, (pious_sizet)sizeof(int),
		  (char *)&tempstate);


      /* dummy record body */
      while (!SS_fatalerror && wbuf != NULL)
	{
	  SS_logwrite((pious_offt)0, PIOUS_SEEK_SET,
		      (pious_sizet)(sizeof(pds_fhandlet) +
				    sizeof(pious_offt) +
				    sizeof(pious_sizet)),
		      (char *)&(wbuf->fhandle));

	  SS_logwrite((pious_offt)0, PIOUS_SEEK_SET,
		      (pious_sizet)(wbuf->nbyte), wbuf->buf);

	  wbuf = wbuf->next;
	}

      SS_logsync();

      if (SS_fatalerror)
	rcode = PIOUS_EFATAL;
      else
	rcode = 0;
    }

  return rcode;
}




/*
 * RM_trans_state() - See pds_recovery_manager.h for description
 */

#ifdef __STDC__
int RM_trans_state(pious_offt lhandle,
		   int state)
#else
int RM_trans_state(lhandle, state)
     pious_offt lhandle;
     int state;
#endif
{
  int rcode, tempstate;

  /* dummy transaction log routine; write data to disk for accurate timing */

  /* check for previous fatal error */

  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* write log record */

  else
    {
      tempstate = RM_TRANS_COMMIT;

      SS_logwrite((pious_offt)0, PIOUS_SEEK_SET, (pious_sizet)sizeof(int),
		  (char *)&tempstate);

      SS_logsync();

      if (SS_fatalerror)
	rcode = PIOUS_EFATAL;
      else
	rcode = PIOUS_OK;
    }

  return rcode;
}
