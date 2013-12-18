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




/* PIOUS Data Server (PDS): Data Manager
 *
 * @(#)pds_data_manager.c	2.2  28 Apr 1995  Moyer
 *
 * The pds_data_manager exports all functions necessary to perform read,
 * write, prepare (to commit), commit, and abort operations on behalf
 * of a transaction.
 *
 * The data manager presumes that the scheduler, via the Lock Manager,
 * implements a Strict Two-Phase locking protocol (2PL) for transaction data
 * access operations.  The data manager insures that such operations are
 * carried out in a consistent and fault-tolerant (recoverable) manner.
 *
 * The data manager also presumes the use of a Two-Phase commit protocol (2PC)
 * in determing the final outcome of a transaction, hence the need to prepare
 * a transaction for committement, via DM_prepare(), prior to actually
 * committing (DM_commit()).  Prepared transaction are recoverable and
 * are referred to here as stable transactions (PIOUS_STABLE); transactions
 * that are not prepared prior to committement are not recoverable and are
 * referred to here as volatile (PIOUS_VOLATILE).
 *
 * A No-Undo/Redo recovery algorithm is implemented by the Data and Recovery
 * Managers.  The data manager buffers all writes for a given transaction until
 * such time as the DM_prepare() routine is called, at which point the set of
 * write buffers is passed to the recovery manager for logging; such a log
 * is refered to as an "intentions list".  Once all write operations have
 * been logged, the data manager is prepared to commit (DM_commit()) or
 * abort (DM_abort()) as determined via the Two-Phase Commit (2PC) protocol.
 *
 * Note that in general the data manager maintains state for a given
 * transaction until that transaction is committed or aborted, at which point
 * all state is discarded.  There are two important exceptions to this rule
 * implemented by the DM_prepare() operation:
 *
 *   1) if DM_prepare() votes to abort a transaction all effects are undone
 *      and the state of the transaction is discarded, and
 *
 *   2) if the transaction is read-only and DM_prepare() votes to commit,
 *      which will always be the case, then the state of the transaction is
 *      discarded; this is known in the literature as the 2PC read-only
 *      optimization as the commitment is no longer required.
 *
 *
 * Function Summary:
 *
 * DM_read();
 * DM_write();
 * DM_prepare();
 * DM_commit();
 * DM_abort();
 *
 */


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#include <memory.h>
#endif

#include <string.h>

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"
#include "pds_cache_manager.h"
#include "pds_recovery_manager.h"
#include "pds_sstorage_manager.h"

#include "pds_data_manager.h"


/*
 * Private Declaration - Types, and Constants
 */


#define INSERT    0 /* perform insert during transid lookup operation */
#define NOINSERT  1 /* do NOT perform insert during transid lookup operation */

#define ILHANDLE -1 /* invalid log handle */

/* Transaction Id hash table size; choose prime not near a power of 2 */
#define TI_TABLE_SZ 103


/* Transaction id hash chain entry:
 *
 *   each hash chain entry points to the set of write buffers (struct RM_wbuf)
 *   associated with a given transaction
 */

typedef struct ti_entry{
  pds_transidt transid;        /* transaction id */
  pious_offt lhandle;          /* log handle */
  int prepared;                /* transaction prepared (to commit) flag */
  struct RM_wbuf *wbhead;      /* first write buffer associated with transid */
  struct RM_wbuf *wbtail;      /* last write buffer associated with transid */
  struct ti_entry *tinext;     /* next entry in transid hash chain */
  struct ti_entry *tiprev;     /* previous entry in transid hash chain */
} ti_entryt;




/*
 * Private Variable Definitions
 */

/* Transaction ID hash table */
static ti_entryt *ti_table[TI_TABLE_SZ];


/*
 * Private Function Declarations
 */

#ifdef __STDC__
static ti_entryt *ti_lookup(pds_transidt transid,
			    int action);

static void ti_rm(ti_entryt *ti_entry);
#else
static ti_entryt *ti_lookup();
static void ti_rm();
#endif




/* Function Definitions - Data Manager Operations */




/*
 * DM_read() - See pds_data_manager.h for description
 */

#ifdef __STDC__
pious_ssizet DM_read(pds_transidt transid,
		     pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf)
#else
pious_ssizet DM_read(transid, fhandle, offset, nbyte, buf)
     pds_transidt transid;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
#endif
{
  int famode;
  pious_ssizet acode, rcode;
  pious_offt ovl_start, ovl_end;
  ti_entryt *ti_entry;
  register struct RM_wbuf *wbuf;


  /* determine if is a non-read-only transaction that has already prepared */
  if ((ti_entry = ti_lookup(transid, NOINSERT)) != NULL && ti_entry->prepared)
    rcode = PIOUS_EPROTO;

  /* validate 'offset' and 'nbyte' arguments */
  else if (offset < 0 || nbyte < 0)
    rcode = PIOUS_EINVAL;

  /* determine if file has read accessability */
  else if ((acode = SS_faccess(fhandle, &famode)) != PIOUS_OK)
    /* error in determining accessability */
    switch (acode)
      {
      case PIOUS_EBADF:
      case PIOUS_EINSUF:
      case PIOUS_EFATAL:
	rcode = acode;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  else if (!(famode & PIOUS_R_OK))
    /* file not read accessable */
    rcode = PIOUS_EACCES;

  /* if the 'nbyte' argument is zero, return immediately */
  else if (nbyte == 0)
    rcode = 0;

  else
    { /* read requested data from cache, minus updates by 'transid' */
      acode = CM_read(fhandle, offset, nbyte, buf);

      if (acode < 0)
	/* error occured during read, set return code appropriately */
	switch (acode)
	  {
	  case PIOUS_EBADF:
	  case PIOUS_EACCES:
	  case PIOUS_EINVAL:
	  case PIOUS_EINSUF:
	  case PIOUS_ERECOV:
	  case PIOUS_EFATAL:
	    rcode = acode;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }

      else
	/* augment data read from cache with data written by 'transid' */

	if (ti_entry == NULL)
	  /* no updates by 'transid' */
	  rcode = acode;

	else
	  { /* apply relevant updates in temporal order (first to last) */
	    rcode = acode;
	    wbuf  = ti_entry->wbhead;

	    while (wbuf != NULL)
	      { /* check if write is to same file as read */
		if (fhandle_eq(wbuf->fhandle, fhandle))

		  if (offset + (nbyte - 1) >= wbuf->offset &&
		      offset <= wbuf->offset + (wbuf->nbyte - 1))
		    { /* write EXPLICITLY overlaps read; update data read */
		      ovl_start = Max(offset, wbuf->offset);
		      ovl_end   = Min(offset + (nbyte - 1),
				      wbuf->offset + (wbuf->nbyte - 1));

		      /* fill void between last valid byte in read data and
		       * start of write overlap, if extant.
		       *
		       * note: ovl_start and ovl_end are ABSOLUTE file offsets
		       */

		      if (offset + rcode < ovl_start)
			memset(buf + rcode, 0,
			       (int)(ovl_start - (offset + rcode)));

		      /* copy updated data into read buffer */

		      memcpy(buf + (ovl_start - offset),
			     wbuf->buf + (ovl_start - wbuf->offset),
			     (int)(ovl_end - ovl_start + 1));

		      /* set number of valid bytes read appropriately */
		      rcode = Max(rcode, ovl_end - offset + 1);
		    }
		  else if (rcode < nbyte &&
			   offset + (nbyte - 1) < wbuf->offset)
		    { /* write IMPLICITLY overlaps read by extending file */
		      memset(buf + rcode, 0, (int)(nbyte - rcode));
		      rcode = nbyte;
		    }

		wbuf = wbuf->next;
	      }
	  }
    }
		
  return rcode;
}




/*
 * DM_write() - See pds_data_manager.h for description
 */

#ifdef __STDC__
int DM_write(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte,
	     char *buf)
#else
int DM_write(transid, fhandle, offset, nbyte, buf)
     pds_transidt transid;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
#endif
{
  int acode, rcode, famode;
  ti_entryt *ti_entry;
  struct RM_wbuf *wbuf;

  /* determine if is a non-read-only transaction that has already prepared */
  if ((ti_entry = ti_lookup(transid, NOINSERT)) != NULL && ti_entry->prepared)
    rcode = PIOUS_EPROTO;

  /* validate 'offset' and 'nbyte' arguments */
  else if (offset < 0 || nbyte < 0)
    rcode = PIOUS_EINVAL;

  /* determine if file has write accessability */
  else if ((acode = SS_faccess(fhandle, &famode)) != PIOUS_OK)
    /* error in determining accessability */
    switch (acode)
      {
      case PIOUS_EBADF:
      case PIOUS_EINSUF:
      case PIOUS_EFATAL:
	rcode = acode;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  else if (!(famode & PIOUS_W_OK))
    /* file not write accessable */
    rcode = PIOUS_EACCES;

  /* if 'nbyte' argument is zero, return immediately */
  else if (nbyte == 0)
    rcode = PIOUS_OK;

  else
    { /* locate/insert transaction id entry */
      if ((ti_entry = ti_lookup(transid, INSERT)) == NULL)
	/* unable to allocate storage for transid */
	rcode = PIOUS_EINSUF;

      else
	{ /* allocate/insert a new write buffer entry */
	  wbuf = (struct RM_wbuf *)malloc((unsigned)sizeof(struct RM_wbuf));

	  if (wbuf == NULL)
	    /* unable to allocate storaage for write buffer */
	    rcode = PIOUS_EINSUF;
	  else
	    { /* fill write buffer entry and place at end of buffer list;
	       * note that the write buffer list reflects the temporal
	       * ordering of write operations, with the head being the
	       * earliest and the tail being the latest.
	       *
	       * RM_trans_log() and DM_read() both depend on this ordering.
	       */

	      wbuf->fhandle   = fhandle;
	      wbuf->offset    = offset;
	      wbuf->nbyte     = nbyte;
	      wbuf->buf       = buf;
	      wbuf->next      = NULL;

	      if (ti_entry->wbhead == NULL)
		/* first write buffer entry for transid */
		ti_entry->wbhead = ti_entry->wbtail = wbuf;
	      else
		{ /* place at end of write buffer list */
		  ti_entry->wbtail->next = wbuf;
		  ti_entry->wbtail       = wbuf;
		}

	      /* set return code */
	      rcode = PIOUS_OK;
	    }
	}
    }

  return rcode;
}




/*
 * DM_prepare() - See pds_data_manager.h for description
 */

#ifdef __STDC__
int DM_prepare(pds_transidt transid)
#else
int DM_prepare(transid)
     pds_transidt transid;
#endif
{
  int rcode;
  pious_offt lcode;
  ti_entryt *ti_entry;


  /* determine if is a read-only transaction or if already prepared */
  if ((ti_entry = ti_lookup(transid, NOINSERT)) == NULL || ti_entry->prepared)
    rcode = PIOUS_OK;

  /* if not read-only and not prepared, log write operations */
  else
    { /* log write operations for transaction 'transid' */
      lcode = RM_trans_log(transid, ti_entry->wbhead);

      if (lcode >= 0)
	{ /* write operations successfully logged; set log handle */
	  ti_entry->prepared = TRUE;
	  ti_entry->lhandle  = lcode;
	  rcode              = PIOUS_OK;
	}
      else
	{ /* logging unsuccessful; abort by removing transaction entry */
	  ti_rm(ti_entry);

	  if (lcode == PIOUS_EFATAL)
	    rcode = PIOUS_EFATAL;
	  else
	    rcode = PIOUS_ECHCKPT;
	}
    }

  return rcode;
}




/*
 * DM_commit() - See pds_data_manager.h for description
 */

#ifdef __STDC__
int DM_commit(pds_transidt transid)
#else
int DM_commit(transid)
     pds_transidt transid;
#endif
{
  int rcode, acode, done, faultmode;
  ti_entryt *ti_entry;
  struct RM_wbuf *wbuf;

  /* determine if is a read-only transaction */
  if ((ti_entry = ti_lookup(transid, NOINSERT)) == NULL)
    rcode = PIOUS_OK;

  /* log commit action, issue writes to cache, release buffers */
  else
    { /* log commit action if transaction previously prepared */
      rcode = PIOUS_OK;

      if (ti_entry->prepared)
	{ /* transaction prepared and logged writes; must log commit action */
	  acode = RM_trans_state(ti_entry->lhandle, RM_TRANS_COMMIT);

	  if (acode != PIOUS_OK)
	    if (acode == PIOUS_EFATAL)
	      /* fatal error occured */
	      rcode = PIOUS_EFATAL;
	    else
	      /* indicate that commit not successfully logged */
	      rcode = PIOUS_ENOTLOG;
	}

      if (rcode != PIOUS_EFATAL)
	{ /* issues all write operations to cache. must do this even if can
           * not successfully log commit action, since can not back-out
           * of commit vote as per rules of 2PC. if recovery is required
	   * later, prior to check-pointing, then PDS will have to inquire
	   * as to result.
           */

	  wbuf = ti_entry->wbhead;
	  done = FALSE;

	  if (ti_entry->prepared)
	    /* transaction prepared; fault mode is stable */
	    faultmode = PIOUS_STABLE;
	  else
	    /* transaction NOT prepared; fault mode is volatile */
	    faultmode = PIOUS_VOLATILE;

	  while (wbuf != NULL && !done)
	    { /* issue writes to cache */
	      acode = CM_write(wbuf->fhandle, wbuf->offset, wbuf->nbyte,
			       wbuf->buf, faultmode);

	      if (acode != PIOUS_OK)
		{ /* can not perform write operation; quit indicating error.
                   * this error is serious (PIOUS_ERECOV or PIOUS_EFATAL) and
                   * overrides PIOUS_ENOTLOG.
                   */

		  if (acode == PIOUS_EFATAL)
		    rcode = PIOUS_EFATAL;
		  else
		    rcode = PIOUS_ERECOV;

		  done  = TRUE;
		}

	      wbuf = wbuf->next;
	    }
	}

      /* remove transaction entry buffers */
      ti_rm(ti_entry);
    }

  return rcode;
}




/*
 * DM_abort() - See pds_data_manager.h for description
 */

#ifdef __STDC__
int DM_abort(pds_transidt transid)
#else
int DM_abort(transid)
     pds_transidt transid;
#endif
{
  int rcode, acode;
  ti_entryt *ti_entry;

  /* locate transaction id entry */
  ti_entry = ti_lookup(transid, NOINSERT);

  /* determine if transaction issued writes */
  if (ti_entry == NULL)
    /* transaction did not issue writes; no action required */
    rcode = PIOUS_OK;

  /* log abort action and remove transaction entry/write buffers */
  else
    { /* log abort action if transaction previously prepared */
      rcode = PIOUS_OK;

      if (ti_entry->prepared)
	{ /* transaction prepared and logged writes; must log abort action */
	  acode = RM_trans_state(ti_entry->lhandle, RM_TRANS_ABORT);

	  if (acode != PIOUS_OK)
	    if (acode == PIOUS_EFATAL)
	      /* fatal error occured */
	      rcode = PIOUS_EFATAL;
	    else
	      /* indicate that commit not successfully logged */
	      rcode = PIOUS_ENOTLOG;
	}

      /* remove transaction entry/write buffers */
      ti_rm(ti_entry);
    }

  return rcode;
}




/* Function Definitions - Local Functions */




/*
 * ti_lookup()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   action   - INSERT | NOINSERT
 *
 * Locate transid in the transaction id hash table. If transaction id not
 * found and INSERT, then place transaction id into table.
 *
 * Returns:
 *
 *   ti_entryt * - pointer to ti_entry if transid found/inserted
 *   NULL        - transid not found/inserted
 */

#ifdef __STDC__
static ti_entryt *ti_lookup(pds_transidt transid,
			    int action)
#else
static ti_entryt *ti_lookup(transid, action)
     pds_transidt transid;
     int action;
#endif
{
  int index;
  register ti_entryt *ti_entry;

  /* perform hash function on transid */
  index = transid_hash(transid, TI_TABLE_SZ);

  ti_entry = ti_table[index];

  /* search appropriate transaction id hash chain for transid */
  while (ti_entry != NULL && !transid_eq(transid, ti_entry->transid))
    ti_entry = ti_entry->tinext;

  if (ti_entry == NULL && action == INSERT)
    { /* transid not located; insert into transaction id hash chain */

      ti_entry = (ti_entryt *)malloc((unsigned)sizeof(ti_entryt));

      if (ti_entry != NULL)
	{ /* initialize and place at head of hash chain */
	  ti_entry->transid     = transid;
	  ti_entry->lhandle     = ILHANDLE;
	  ti_entry->prepared    = FALSE;
	  ti_entry->wbhead      = NULL;
	  ti_entry->wbtail      = NULL;
	  ti_entry->tinext      = ti_table[index];
	  ti_entry->tiprev      = NULL;

	  ti_table[index] = ti_entry;

	  /* reset 'previous' pointer of former head, if extant */
	  if (ti_entry->tinext != NULL)
	    ti_entry->tinext->tiprev = ti_entry;
	}
    }

  return(ti_entry);
}




/*
 * ti_rm()
 *
 * Parameters:
 *
 *   ti_entry - transaction id hash chain entry
 *
 * Remove and deallocate transaction id hash chain entry and associated
 * write buffers
 *
 * Returns:
 */

#ifdef __STDC__
static void ti_rm(ti_entryt *ti_entry)
#else
static void ti_rm(ti_entry)
     ti_entryt *ti_entry;
#endif
{
  register struct RM_wbuf *wbuf_pos, *wbuf_tmp;
  if (ti_entry != NULL)
    { /* free write buffers associated with ti_entry */
      wbuf_pos = ti_entry->wbhead;

      while (wbuf_pos != NULL)
	{
	  wbuf_tmp = wbuf_pos->next;
	  free((char *)wbuf_pos->buf);
	  free((char *)wbuf_pos);
	  wbuf_pos = wbuf_tmp;
	}

      /* remove ti_entry from associated hash chain and deallocate storage */

      /* if not last in chain, reset 'previous' pointer of 'next' entry */
      if (ti_entry->tinext != NULL)
	ti_entry->tinext->tiprev = ti_entry->tiprev;

      /* if not first in chain, reset 'next' pointer of 'previous' entry */
      if (ti_entry->tiprev != NULL)
	ti_entry->tiprev->tinext = ti_entry->tinext;
      else
	/* first in chain, reset transaction id hash table entry */
	ti_table[transid_hash(ti_entry->transid, TI_TABLE_SZ)] =
	  ti_entry->tinext;

      free((char *)ti_entry);
    }
}
