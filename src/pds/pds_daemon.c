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




/* PIOUS Data Server (PDS): Server Daemon
 *
 * @(#)pds_daemon.c	2.2  28 Apr 1995  Moyer
 *
 * The pds_daemon is the main server loop that processes client requests.
 * Client requests fall into two catagories: transaction operations and
 * control operations.
 *
 * The pds_daemon operates under a Two-Phase Commit protocol (2PC), allowing
 * multiple PDS daemons to participate in a distributed transaction. The
 * pds_daemon also enforces a Strict Two-Phase Locking protocol (2PL)
 * for serializability and recoverability.
 *
 * To guarantee progress and provide fair scheduling of transaction operations,
 * the following constraints are placed on the scheduling and deadlock
 * avoidance algorithms:
 *
 *   2PL scheduling constraint: Conflicting lock requests for overlapping
 *     data regions are satisfied in the order received.
 *
 *   Deadlock avoidance/recovery constraint: A transaction Ti is aborted when
 *     an associated lock request has been delayed for longer than the timeout
 *     period if and only if there exists an active transaction Tj (known
 *     to this PDS) such that i > j.
 *
 *
 * NOTE:
 *
 * The file pds/pds.h provides a full discussion of PDS services and operation.
 * This is a must read prior to examining this code.
 *
 *
 * Transaction Operation Summary:
 *
 * PDS_read();
 * PDS_write();
 * PDS_read_sint();
 * PDS_write_sint();
 * PDS_fa_sint();
 * PDS_prepare();
 * PDS_commit();
 * PDS_abort();
 *
 * Control Operation Summary:
 *
 * PDS_lookup();
 * PDS_cacheflush();
 * PDS_mkdir();
 * PDS_rmdir();
 * PDS_unlink();
 * PDS_chmod();
 * PDS_stat();
 * PDS_ping();
 * PDS_reset();
 * PDS_shutdown();
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) The method of scanning/re-trying blocked operations after freeing
 *      locks, as performed in the main server loop, is inherently inefficient.
 *      A really efficient implementation will share state between the
 *      pds_lock_manager and the pds_daemon, allowing the lock manager to
 *      unblock operations as locks are released.
 *
 *   2) Neither recovery nor checkpointing is implemented, allowing only
 *      volatile transactions to be properly executed.  Stable transactions
 *      can be executed for benchmarking purposes, as logging does take
 *      place, but are not recoverable in the event of a system failure.
 *
 *   3) The function PDS_fa_sint() avoids the heterogeneity problem by
 *      operating on files that are treated as arrays of integers, with
 *      the specified file offset being the array index.  The functions
 *      PDS_write_sint() and PDS_read_sint() are provided for initializing
 *      and reading values operated on by PDS_fa_sint(); these two functions
 *      can be eliminated if a general purpose version of PDS_fa_sint()
 *      is implemented to operate on integers stored in XDR or some other
 *      "universal" format.
 *
 *   4) Non-blocked transaction operations are never aborted by the PDS,
 *      as discussed in pds/pds.h.  This allows volatile transactions to
 *      be implemented without 2PC.  However a problem arises in that a
 *      client can die and leave data locked.
 *
 *      Even if it is known that an unprepared transaction is volatile, via
 *      tagging at the start of the transaction, it is not possible to
 *      assertain if the next transaction operation will be a data access
 *      or a commit. Thus it is never safe to time-out a lock for volatile
 *      transactions.  Leaving data locked for a dead client is preferable
 *      to aborting an unblocked volatile transaction for a slow client
 *      because, while both actions are undesireable, the former only
 *      occurs in the case of a system failure, and volatile transactions
 *      are not guaranteed to correctly handle system failures.
 *
 * ----------------------------------------------------------------------------
 * Procedure for Adding PDS Functions:
 *
 *   The following is a step-by-step procedure for properly adding new PDS
 *   functions with consistent documentation.  Each step lists the file to
 *   be modified, and what those modifications must be.  Most modifications
 *   are very obvious extensions of existing code; basically adding more of
 *   the same.
 *
 *
 *   1) pds/pds_msg_exchange.h -
 *        a) add op code symbolic constant for new function and update
 *           operation type test macros.
 *
 *        b) add message structures defining request/reply parameters for
 *           new function; add message access macros.
 *
 *   2) pds/pds_msg_exchange.c - add new function to switch statements
 *        that control the packing/unpacking of message structures.
 *
 *   3) pds/pds_daemon.c -
 *        a) add new function to summary at top of file (documentation)
 *        b) add new function to private function declarations (for both
 *           ANSI/non-ANSI C) in either transaction operation or control
 *           operation sections.
 *        c) control operation:
 *             - add new function to do_cntrlop() switch
 *             - add new function definition in control op section
 *             - update cntrlreq_ack(), if necessary
 *             - update cntrlreq_dealloc(), if necessary
 *           transaction operation:
 *             - add new function to do_transop() switch
 *             - add new function definition in transaction op section
 *             - update init_transreq(), if necessary
 *             - update transreq_ack(), if necessary
 *             - update transreq_dealloc(), if necessary
 *             - update transreply_dealloc(), if necessary
 *
 *   4) pds/pds.h - RPC stub declarations; add new function to summary near
 *        top (documentation) and add complete function documentation and
 *        declaration(s) where appropriate.
 *
 *   5) pds/pds.c - RPC stub definitions; add new function to summary at
 *        top (documentation) and add function definition(s) where
 *        appropriate.
 *
 */


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#else
#include "nonansi.h"
#include <memory.h>
#endif

#include <string.h>
#include <stdio.h>

#include "gpmacro.h"
#include "gputil.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "pious_sysconfig.h"

#include "psys.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"

#include "pdce_srcdestt.h"
#include "pdce_msgtagt.h"
#include "pdce.h"

#include "pds_sstorage_manager.h"
#include "pds_data_manager.h"
#include "pds_cache_manager.h"
#include "pds_lock_manager.h"
#include "pds.h"
#include "pds_msg_exchange.h"




/*
 * Private Declaration - Types, and Constants
 */


#define INSERT    0 /* perform insert during transid lookup operation */
#define NOINSERT  1 /* do NOT perform insert during transid lookup operation */

/* Transaction Id hash table size; choose prime not near a power of 2 */
#define TI_TABLE_SZ 103


#ifdef PDSPROFILE
/* Transaction profile file name */
#define PROF_NAME  "PIOUS.DS.PROFILE"
#endif


/* Transaction Table Entry
 *
 *   Contains the state of the most recent client transaction operation
 *   request and the result of the most recent transaction operation reply.
 *
 *   The transaction table is maintained as two doubly linked lists so that
 *   either all transaction entries or just those blocked waiting on a lock
 *   can be scanned. The transaction table is also threaded by transaction id,
 *   so that a particular transaction can be located via a hash function.
 */

#define ACTIVE     0  /* transaction operation is active (newly received) */
#define BLOCKED    1  /* transaction operation is blocked waiting on lock */
#define COMPLETED  2  /* transaction operation is completed */


/* define request information record */

typedef struct {                       /* transaction op. auxiliary storage */
                                       /* 1) computed values */
  pious_sizet nobj_prime;              /*      - adjusted object count */
                                       /* 2) scheduling parameters */
  pds_fhandlet lk_fhandle;             /*      - fhandle to lock */
  int lk_type;                         /*      - lock type */
  pious_offt lk_start;                 /*      - effective lock lower-bound */
  pious_offt lk_stop;                  /*      - effective lock upper-bound */
} req_auxstoret;

typedef struct {
  dce_srcdestt clientid;               /* client id */
  int reqop;                           /* requested operation */
  pdsmsg_reqt reqmsg;                  /* request message */
  util_clockt tstamp;                  /* request time stamp */
  req_auxstoret aux;                   /* transaction op. auxiliary storage */
} req_infot;


/* define reply information record */

typedef struct {
  int replyop;                         /* replied operation */
  pdsmsg_replyt replymsg;              /* reply message */
} reply_infot;

/* define transaction table entry */

typedef struct trans_entry{
  pds_transidt transid;                /* transaction id */
  int transop_state;                   /* transaction request state */
  int readlk;                          /* transaction read lock flag */
  int writelk;                         /* transaction write lock flag */
  int readonly;                        /* transaction read-only flag */
  int prepared;                        /* transaction prepared flag */
  req_infot transop_req;               /* transaction op. request - current */
  reply_infot transop_reply;           /* transaction op. reply   - last */
  struct trans_entry *tblnext;         /* next entry in transaction table */
  struct trans_entry *tblprev;         /* prev entry in transaction table */
  struct trans_entry *tinext;          /* next entry in transid hash chain */
  struct trans_entry *tiprev;          /* prev entry in transid hash chain */

#ifdef PDSPROFILE
  unsigned long prof_opcum;            /* transaction op. cumulative time */
#endif
} trans_entryt;


/* Blocked Control Operation Table Entry
 *
 *   Contains information regarding control operation requests that
 *   are blocked pending an operation specific change in state.
 */

typedef struct cntrl_entry{
  req_infot cntrlop_req;               /* control operation request */
  struct cntrl_entry *tblnext;         /* next entry in control op table */
  struct cntrl_entry *tblprev;         /* prev entry in control op table */
} cntrl_entryt;




/*
 * Private Variable Definitions
 */


/* SCCS version information */
static char VersionID[] = "@(#)pds_daemon.c	2.2  28 Apr 1995  Moyer";


/* Transaction Table
 *
 *   transtable.ready            : is the set of non-blocked transactions.
 *   transtable.block_{head/tail}: is the set of blocked transactions.
 */

static struct {
  trans_entryt *ready;
  trans_entryt *block_head;
  trans_entryt *block_tail;
} transtable;


/* Transaction ID Hash Table - for locating transaction table entries */
static trans_entryt *ti_table[TI_TABLE_SZ];


/* Blocked Control Operation Table */
static struct {
  cntrl_entryt *block_head;
  cntrl_entryt *block_tail;
} cntrltable;


#ifdef PDSPROFILE
/* Transaction profile file stream pointer and timer clock */
static FILE *prof_stream;
static util_clockt prof_clock;
#endif




/*
 * Private Function Declarations
 */

#ifdef __STDC__


/* Control Operation Function Declarations */


static int do_cntrlop(req_infot *request);

static void retry_blk_cntrlop(void);

static int PDS_lookup_(req_infot *request);

static int PDS_cacheflush_(req_infot *request);

static int PDS_mkdir_(req_infot *request);

static int PDS_rmdir_(req_infot *request);

static int PDS_unlink_(req_infot *request);

static int PDS_chmod_(req_infot *request);

static int PDS_stat_(req_infot *request);

static int PDS_ping_(req_infot *request);

static int PDS_reset_(req_infot *request);

static void PDS_shutdown_(req_infot *request);

static cntrl_entryt *block_cntrlop(req_infot *request);

static void rm_cntrlrec(cntrl_entryt *cntrlrec);

static void cntrlreq_ack(req_infot *request,
			 int rcode);

static void cntrlreq_dealloc(req_infot *request);


/* Transaction Operation Function Declarations */


static void do_transop(trans_entryt *transrec);

static void retry_blk_transop(void);

static void PDS_read_(trans_entryt *transrec);

static void PDS_write_(trans_entryt *transrec);

static void PDS_read_sint_(trans_entryt *transrec);

static void PDS_write_sint_(trans_entryt *transrec);

static void PDS_fa_sint_(trans_entryt *transrec);

static void PDS_prepare_(trans_entryt *transrec);

static void PDS_commit_(trans_entryt *transrec);

static void PDS_abort_(trans_entryt *transrec);

static int init_transreq(req_infot *request,
			 trans_entryt **transrec);

static void complete_transop(trans_entryt *transrec);

static void block_transop(trans_entryt *transrec);

static int fcfs_conflict(register trans_entryt *transrec);

static trans_entryt *ti_lookup(pds_transidt transid,
			       int action);

static void rm_transrec(trans_entryt *transrec);

static void transreq_ack(req_infot *request,
			 int rcode);

static void transreq_dealloc(req_infot *request);

static void transreply_dealloc(reply_infot *reply);

#ifdef PDSPROFILE
static void prof_init(char *logpath);
static void prof_print(trans_entryt *transrec);
#endif


#else /* ifndef __STDC__ */


/* Control Operation Function Declarations */


static int do_cntrlop();

static void retry_blk_cntrlop();

static int PDS_lookup_();

static int PDS_cacheflush_();

static int PDS_mkdir_();

static int PDS_rmdir_();

static int PDS_unlink_();

static int PDS_chmod_();

static int PDS_stat_();

static int PDS_ping_();

static int PDS_reset_();

static void PDS_shutdown_();

static cntrl_entryt *block_cntrlop();

static void rm_cntrlrec();

static void cntrlreq_ack();

static void cntrlreq_dealloc();


/* Transaction Operation Function Declarations */


static void do_transop();

static void retry_blk_transop();

static void PDS_read_();

static void PDS_write_();

static void PDS_read_sint_();

static void PDS_write_sint_();

static void PDS_fa_sint_();

static void PDS_prepare_();

static void PDS_commit_();

static void PDS_abort_();

static int init_transreq();

static void complete_transop();

static void block_transop();

static int fcfs_conflict();

static trans_entryt *ti_lookup();

static void rm_transrec();

static void transreq_ack();

static void transreq_dealloc();

static void transreply_dealloc();

#ifdef PDSPROFILE
static void prof_init();
static void prof_print();
#endif
#endif




/* ---------------------------- PDS Daemon Loop --------------------------- */


/*
 * main()
 *
 * Parameters:
 *
 *   logpath - PDS host log directory path
 *
 * Returns:
 */

#ifdef __STDC__
int main(int argc, char **argv)
#else
int main(argc, argv)
     int argc;
     char **argv;
#endif
{
  char *logpath;
  int rcode;
  req_infot request;
  trans_entryt *transrec, *transrec_next;
  cntrl_entryt *cntrlrec, *cntrlrec_next;
  util_clockt deadlock_timer;
  pds_transidt min_transid;
  int transtimedout;


  /* The following global stable storage flags are exported by the stable
   * storage manager pds_sstorage_manager:
   *
   *   SS_fatalerror - a fatal error has occured, PDS can not continue.
   *   SS_recover    - recovery required before PDS can continue.
   *   SS_checkpoint - checkpoint required before PDS can continue
   *
   * See pds_sstorage_manager.c for information on where these flags are set.
   *
   * NOTE: SS_fatalerror is TRUE by default prior to performing stable storage
   *       initialization (SS_init()).
   */


  /* Set the pathname of the log directory */

  if (argc > 1)
    logpath = argv[1];
  else
    logpath = NULL;


#ifdef PDSPROFILE
  /* Initialize transaction profiling */
  prof_init(logpath);
#endif


  /* Initialize Stable Storage Manager */

  if (SS_init(logpath) == PIOUS_ERECOV)
    { /* stable storage recovery required; not currently implemented */
      SS_errlog("pds_daemon", "main()", PIOUS_EFATAL,
		"stable storage recovery required; not implemented");

      SS_fatalerror = TRUE;
    }


  /* Start deadlock avoidance interval timer */

  UTIL_clock_mark(&deadlock_timer);


  /* Service Client Requests */

  while (!SS_fatalerror)
    { /* receive client request */
      do
	rcode = PDSMSG_req_recv(&request.clientid,
				&request.reqop,
				&request.reqmsg,
				(PDS_TDEADLOCK / 2));
      while (rcode != PIOUS_OK && rcode != PIOUS_ETIMEOUT);

      /* perform requested transaction or control operation */

      if (rcode == PIOUS_OK)
	{
	  /* control operation request
	   *
	   *   initiate control operation request; block if unable to
	   *   complete immediately.
	   */

	  if (PdsControlop(request.reqop))
	    { /* attempt control operation */

	      if (do_cntrlop(&request) == COMPLETED)
		{ /* control operation completed; deallocate request */
		  cntrlreq_dealloc(&request);
		}

	      /* block control operation if unable to complete */

	      else if (block_cntrlop(&request) == NULL)
		{ /* unable to alloc storage in blocked control op table */
		  cntrlreq_ack(&request, PIOUS_EINSUF);
		  cntrlreq_dealloc(&request);
		}
	    }


	  /* transaction operation request
	   *
	   *   determine if transaction operation begins a new transaction
	   *   or is the next logical operation of an active transaction.
	   *   if meets either criteria, then initiate operation.
	   */

	  else if (PdsTransop(request.reqop))
	    { /* determine if trans. op. is to be initiated */

	      if (!init_transreq(&request, &transrec))
		{ /* do not initiate; deallocate request */
		  transreq_dealloc(&request);
		}

	      /* attempt transaction operation */

	      else
		{
		  do_transop(transrec);

		  /* if requested operation released locks, then scan the
		   * control and transaction operation tables for blocked
		   * operations that can now be performed and do them.
		   *
		   * blocked control operations are scanned first since
		   * control ops do not hold locks over multiple requests.
		   *
		   * only PDS_prepare(), PDS_commit(), and PDS_abort() release
		   * locks; since these operations are NEVER blocked, a single
		   * scan of the control and transaction tables is sufficient.
		   */

		  if (request.reqop == PDS_PREPARE_OP ||
		      request.reqop == PDS_COMMIT_OP  ||
		      request.reqop == PDS_ABORT_OP)
		    {
		      /* scan blocked control operations */
		      retry_blk_cntrlop();

		      /* scan blocked transaction operations */
		      retry_blk_transop();
		    }
		}
	    }
	}


      /* all transaction/control ops that can be performed are now complete */


      if (!SS_fatalerror)
	{
	  /* case: SS_recover - dynamic recovery required
	   *
	   *   1) complete blocked transaction operations, responding with
	   *      PIOUS_EABORT; guaranteed to be non-prepared since prepare
	   *      operation NEVER blocks.
	   *   2) complete blocked control operations, responding with
	   *      PIOUS_EBUSY.
	   *   3) abort all transactions.
	   *   4) invalidate cache
	   *   5) perform recovery as if crashed.
	   *   6) continue operation.
	   */

	  if (SS_recover)
	    {
	      SS_errlog("pds_daemon", "main()", PIOUS_EFATAL,
			"stable storage recovery required; not implemented");

	      SS_fatalerror = TRUE;
	    }


	  /* case: SS_checkpoint - dynamic checkpoint required
	   *
	   *   1) continue operation under following rules:
	   *        a) respond to all prepare requests with PIOUS_EABORT.
	   *        b) continue in this mode until no transactions are
	   *           prepared, then proceed to step 2).
	   *   2) flush and invalidate cache.
	   *   3) truncate transaction log.
	   *   4) continue operation.
	   */

	  else if (SS_checkpoint)
	    {
	      SS_errlog("pds_daemon", "main()", PIOUS_EFATAL,
			"stable storage checkpoint required; not implemented");

	      SS_fatalerror = TRUE;
	    }

	  /* case: !SS_recover && !SS_checkpoint - deadlock/postponement check
	   *
	   *   Perform transaction operation deadlock avoidance/recovery and
           *   control operation indefinite-postponement recovery.
	   *
	   *   1) complete "old" blocked transaction operations, responding
	   *      with PIOUS_EABORT; abort same transactions, which are
	   *      guaranteed to be non-prepared as prepare NEVER blocks.
	   *
	   *      NOTE: NEVER abort non-blocked transactions.  This condition
	   *            is required to implement volatile transactions
	   *            without 2PC; see discussion in pds/pds.h.
	   *
	   *   2) complete "old" blocked control operations, responding with
	   *      PIOUS_EBUSY.
	   *
	   *   3) if any transactions were aborted in step 1 then scan the
	   *      control and transaction operation tables for blocked
	   *      operations that can now be performed and do them.
	   *
	   *      NOTE: this MUST be done for transaction operations or
	   *            transaction with minimum transid can get "stuck".
	   */

	  else if (UTIL_clock_delta(&deadlock_timer,
				    UTIL_MSEC) >= (PDS_TDEADLOCK / 2))
	    { /* abort "old" blocked transaction operations */

	      transtimedout = FALSE;

	      if (transtable.block_head != NULL)
		{ /* if blocked trans. ops., determine minimum transid */
		  min_transid = transtable.block_head->transid;

		  for (transrec =  transtable.block_head->tblnext;
		       transrec != NULL;
		       transrec =  transrec->tblnext)
		    if (transid_gt(min_transid, transrec->transid))
		      min_transid = transrec->transid;

		  for (transrec =  transtable.ready;
		       transrec != NULL;
		       transrec =  transrec->tblnext)
		    if (transid_gt(min_transid, transrec->transid))
		      min_transid = transrec->transid;
		}

	      transrec = transtable.block_head;

	      while (transrec != NULL && !SS_fatalerror)
		{ /* determine next, as aborting removes from table */
		  transrec_next = transrec->tblnext;

		  /* abort transaction operation, and hence transaction, if
                   *   1) it has timed-out, and
		   *   2) it does not have the minimum transid.
		   */

		  if (UTIL_clock_delta(&(transrec->transop_req.tstamp),
				       UTIL_MSEC) >= PDS_TDEADLOCK &&
		      transid_gt(transrec->transid, min_transid))
		    { /* issue abort to data manager */
		      if (DM_abort(transrec->transid) != PIOUS_EFATAL)
			{
			  /* free ALL locks held by the transaction */
			  if (transrec->readlk)
			    LM_rfree(transrec->transid);

			  if (transrec->writelk)
			    LM_wfree(transrec->transid);

			  /* reply to client */
			  transreq_ack(&(transrec->transop_req), PIOUS_EABORT);

			  /* remove transaction from transaction table */
			  rm_transrec(transrec);

			  /* flag that transaction was timed-out */
			  transtimedout = TRUE;
			}
		    }

		  transrec = transrec_next;
		}

	      /* complete "old" blocked control operations */

	      cntrlrec = cntrltable.block_head;

	      while (cntrlrec != NULL && !SS_fatalerror)
		{ /* determine next, as completing removes from table */
		  cntrlrec_next = cntrlrec->tblnext;

		  /* determine if control operation has timed-out */
		  if (UTIL_clock_delta(&(cntrlrec->cntrlop_req.tstamp),
				       UTIL_MSEC) >= PDS_TDEADLOCK)
		    { /* reply to client */
		      cntrlreq_ack(&(cntrlrec->cntrlop_req), PIOUS_EBUSY);

		      /* remove control operation from blocked table */
		      rm_cntrlrec(cntrlrec);
		    }

		  cntrlrec = cntrlrec_next;
		}

	      /* if any transaction timed-out and was aborted, then scan the
	       * control and transaction operation tables for blocked
	       * operations that can now be performed and do them.
	       *
	       * blocked control operations are scanned first since
	       * control ops do not hold locks over multiple requests.
	       *
	       * only PDS_prepare(), PDS_commit(), and PDS_abort() release
	       * locks; since these operations are NEVER blocked, a single
	       * scan of the control and transaction tables is sufficient.
	       */

	      if (transtimedout)
		{ /* scan blocked control operations */
		  retry_blk_cntrlop();

		  /* scan blocked transaction operations */
		  retry_blk_transop();
		}


	      /* mark time at which deadlock detection completed */

	      UTIL_clock_mark(&deadlock_timer);
	    }
	}
    }




  /* fatal error detected
   *
   *   1) complete ALL blocked operations, responding with PIOUS_EFATAL.
   *   2) do not initiate any new requests; respond to all new requests
   *      with PIOUS_EFATAL.
   */

  /* complete blocked control operations */
  cntrlrec = cntrltable.block_head;

  while (cntrlrec != NULL)
    { /* determine next, as completing removes from table */
      cntrlrec_next = cntrlrec->tblnext;

      /* reply to client */
      cntrlreq_ack(&(cntrlrec->cntrlop_req), PIOUS_EFATAL);

      /* remove control operation from blocked table */
      rm_cntrlrec(cntrlrec);

      cntrlrec = cntrlrec_next;
    }


  /* complete blocked transaction operations */
  transrec = transtable.block_head;

  while (transrec != NULL)
    { /* determine next, as completing removes from table */
      transrec_next = transrec->tblnext;

      /* reply to client */
      transreq_ack(&(transrec->transop_req), PIOUS_EFATAL);

      /* remove transaction from table */
      rm_transrec(transrec);

      transrec = transrec_next;
    }


  /* respond to all new requests with PIOUS_EFATAL */
  while(TRUE)
    {
      while (PDSMSG_req_recv(&request.clientid,
			     &request.reqop,
			     &request.reqmsg,
			     DCE_BLOCK) != PIOUS_OK);

      if (PdsControlop(request.reqop))
	{ /* respond to control operation request */
	  cntrlreq_ack(&request, PIOUS_EFATAL);

	  /* if request is for shutdown then exit */
	  if (request.reqop == PDS_SHUTDOWN_OP)
	    { /* exit from DCE, then exit process */
	      DCE_exit();
	      exit(0);
	    }
	  else
	    cntrlreq_dealloc(&request);
	}

      else if (PdsTransop(request.reqop))
	{ /* respond to transaction operation request */
	  transreq_ack(&request, PIOUS_EFATAL);
	  transreq_dealloc(&request);
	}
    }
}




/*
 * Private Function Definitions - Control Operation Functions
 */




/*
 * do_cntrlop()
 *
 * Parameters:
 *
 *   request - requested control operation information record
 *
 * Perform the requested control operation. Control operations do not take
 * place within the context of a transaction, but they may effect a
 * transacation; e.g. by forcing a transaction to abort.
 *
 * Returns:
 *
 *   COMPLETED - the control operation is completed
 *   BLOCKED   - control operation blocked; unable to complete
 */

#ifdef __STDC__
static int do_cntrlop(req_infot *request)
#else
static int do_cntrlop(request)
     req_infot *request;
#endif
{
  int rcode;

  rcode = COMPLETED;

  switch(request->reqop)
    {
    case PDS_LOOKUP_OP:
      rcode = PDS_lookup_(request);
      break;
    case PDS_CACHEFLUSH_OP:
      rcode = PDS_cacheflush_(request);
      break;
    case PDS_MKDIR_OP:
      rcode = PDS_mkdir_(request);
      break;
    case PDS_RMDIR_OP:
      rcode = PDS_rmdir_(request);
      break;
    case PDS_UNLINK_OP:
      rcode = PDS_unlink_(request);
      break;
    case PDS_CHMOD_OP:
      rcode = PDS_chmod_(request);
      break;
    case PDS_STAT_OP:
      rcode = PDS_stat_(request);
      break;
    case PDS_PING_OP:
      rcode = PDS_ping_(request);
      break;
    case PDS_RESET_OP:
      rcode = PDS_reset_(request);
      break;
    case PDS_SHUTDOWN_OP:
      /* does not return */
      PDS_shutdown_(request);
      break;
    }

  return rcode;
}




/*
 * retry_blk_cntrlop()
 *
 * Parameters:
 *
 * Scan the control operation table for blocked operations that can
 * now be performed and do them.
 *
 * NOTE: Will not attempt further operations if any operation generates a
 *       SS_fatalerror, SS_recover, or SS_checkpoint flag.
 *
 * Returns:
 */

#ifdef __STDC__
static void retry_blk_cntrlop(void)
#else
static void retry_blk_cntrlop()
#endif
{
  cntrl_entryt *cntrlrec, *cntrlrec_next;

  /* scan blocked control operations */
  cntrlrec = cntrltable.block_head;

  while (cntrlrec != NULL &&
	 !SS_fatalerror && !SS_recover && !SS_checkpoint)
    { /* determine next, as completing removes operation from the table */
      cntrlrec_next = cntrlrec->tblnext;

      /* attempt blocked control operation */
      if (do_cntrlop(&(cntrlrec->cntrlop_req)) == COMPLETED)
	rm_cntrlrec(cntrlrec);

      cntrlrec = cntrlrec_next;
    }
}




/*
 * PDS_lookup() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_lookup_(req_infot *request)
#else
static int PDS_lookup_(request)
     req_infot *request;
#endif
{
  int lcode, dolookup, completed, gotlock;
  pdsmsg_replyt reply;
  pds_fhandlet fhandle;
  pds_transidt transid;

  /* if truncating an existing file, obtain exclusive access via lock mgr */

  gotlock = FALSE;

  if (!(request->reqmsg.LookupBody.cflag & PIOUS_TRUNC) ||
      (lcode = SS_lookup(request->reqmsg.LookupBody.path,
			 &fhandle,
			 PIOUS_NOCREAT, (pious_modet)0)) == PIOUS_ENOENT)

    { /* no file truncation or file does not exist; can perform lookup */
      dolookup  = TRUE;
      completed = FALSE;
    }

  else if (lcode != PIOUS_OK)
    { /* file truncation requested, but can not access file */
      dolookup  = FALSE;
      completed = TRUE;
    }

  else
    { /* file truncation requested and file exists; obtain exclusive access */

      if (transid_assign(&transid) != PIOUS_OK)
	{ /* unable to obtain a transaction id */
	  lcode     = PIOUS_EUNXP;
	  dolookup  = FALSE;
	  completed = TRUE;
	}

      else if (LM_wlock(transid, fhandle,
			(pious_offt)0,
			(pious_sizet)PIOUS_OFFT_MAX + 1) != LM_GRANT)
	{ /* unable to get exclusive access to file */
	  dolookup  = FALSE;
	  completed = FALSE;
	}

      else
	{ /* obtained exclusive access to file */
	  gotlock   = TRUE;
	  dolookup  = TRUE;
	  completed = FALSE;
	}
    }


  /* perform lookup operation */

  if (dolookup)
    {
      if ((lcode = SS_lookup(request->reqmsg.LookupBody.path,
			     &reply.LookupBody.fhandle,
			     request->reqmsg.LookupBody.cflag,
			     request->reqmsg.LookupBody.mode)) == PIOUS_OK)
	{ /* lookup successful */

	  if (gotlock)
	    /* truncated file; invalidate cache */
	    CM_finvalidate(reply.LookupBody.fhandle);

	  /* determine file accessability */

	  lcode = SS_faccess(reply.LookupBody.fhandle,
			     &reply.LookupBody.amode);
	}

      if (gotlock)
	/* requested truncation; hold file lock */
	LM_wfree(transid);

      completed = TRUE;
    }


  /* set result code and reply to client */

  if (completed)
    {
      switch(lcode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_ENOTREG:
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_EFATAL:
	  reply.LookupHead.rcode = lcode;
	  break;
	default:
	  reply.LookupHead.rcode = PIOUS_EUNXP;
	  break;
	}

      /* set control message id */
      reply.LookupHead.cmsgid = request->reqmsg.LookupHead.cmsgid;

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(request->clientid, PDS_LOOKUP_OP, &reply);

      /* indicate completion of control operation */
      return COMPLETED;
    }

  else
    return BLOCKED;
}




/*
 * PDS_cacheflush() - see pds.h for description
 */

#ifdef __STDC__
static int PDS_cacheflush_(req_infot *request)
#else
static int PDS_cacheflush_(request)
     req_infot *request;
#endif
{
  int fcode;
  pdsmsg_replyt reply;

  /* perform cacheflush operation */
  fcode = CM_flush();

  /* set result code. if the cache manager indicates that recovery
   * is required, inform client that cache flush may be incomplete.
   *
   * the main daemon loop will detect that recovery is required via
   * the global stable storage flag SS_recover and take action.
   */

  switch(fcode)
    {
    case PIOUS_OK:
    case PIOUS_EFATAL:
      reply.CacheflushHead.rcode = fcode;
      break;
    default:
      reply.CacheflushHead.rcode = PIOUS_EUNXP;
      break;
    }

  /* set control message id */
  reply.CacheflushHead.cmsgid = request->reqmsg.CacheflushHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_CACHEFLUSH_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_mkdir() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_mkdir_(req_infot *request)
#else
static int PDS_mkdir_(request)
     req_infot *request;
#endif
{
  int rcode;
  pdsmsg_replyt reply;

  /* perform mkdir operation */
  rcode = SS_mkdir(request->reqmsg.MkdirBody.path,
		   request->reqmsg.MkdirBody.mode);

  /* set result code appropriately */
  switch(rcode)
    {
    case PIOUS_OK:
    case PIOUS_EACCES:
    case PIOUS_EEXIST:
    case PIOUS_ENOENT:
    case PIOUS_ENAMETOOLONG:
    case PIOUS_ENOTDIR:
    case PIOUS_ENOSPC:
    case PIOUS_EINSUF:
    case PIOUS_EFATAL:
      reply.MkdirHead.rcode = rcode;
      break;
    default:
      reply.MkdirHead.rcode = PIOUS_EUNXP;
      break;
    }

  /* set control message id */
  reply.MkdirHead.cmsgid = request->reqmsg.MkdirHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_MKDIR_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_rmdir() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_rmdir_(req_infot *request)
#else
static int PDS_rmdir_(request)
     req_infot *request;
#endif
{
  int rcode;
  pdsmsg_replyt reply;

  /* perform rmdir operation */
  rcode = SS_rmdir(request->reqmsg.RmdirBody.path);

  /* set result code appropriately */
  switch(rcode)
    {
    case PIOUS_OK:
    case PIOUS_EACCES:
    case PIOUS_EBUSY:
    case PIOUS_ENOTEMPTY:
    case PIOUS_ENOENT:
    case PIOUS_ENAMETOOLONG:
    case PIOUS_ENOTDIR:
    case PIOUS_EFATAL:
      reply.RmdirHead.rcode = rcode;
      break;
    default:
      reply.RmdirHead.rcode = PIOUS_EUNXP;
      break;
    }

  /* set control message id */
  reply.RmdirHead.cmsgid = request->reqmsg.RmdirHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_RMDIR_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_unlink() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_unlink_(req_infot *request)
#else
static int PDS_unlink_(request)
     req_infot *request;
#endif
{
  int rcode, completed, dounlink, regular;
  pdsmsg_replyt reply;
  pds_fhandlet fhandle;
  pds_transidt transid;


  rcode = SS_lookup(request->reqmsg.UnlinkBody.path,
		    &fhandle,
		    PIOUS_NOCREAT, (pious_modet)0);

  /* case: file exists and is regular */

  if (rcode == PIOUS_OK)
    { /* obtain exclusive access to file utilizing the lock manager */

      regular = TRUE;

      if (transid_assign(&transid) != PIOUS_OK)
	{ /* unable to obtain a transaction id */
	  reply.UnlinkHead.rcode = PIOUS_EUNXP;
	  dounlink               = FALSE;
	  completed              = TRUE;
	}

      else if (LM_wlock(transid, fhandle,
			(pious_offt)0,
			(pious_sizet)PIOUS_OFFT_MAX + 1) != LM_GRANT)
	{ /* unable to get exclusive access to file */
	  dounlink  = FALSE;
	  completed = FALSE;
	}

      else
	{ /* obtained exclusive access to file */
	  dounlink  = TRUE;
	  completed = FALSE;
	}
    }

  /* case: file exists but is NOT regular */

  else if (rcode == PIOUS_ENOTREG)
    {
      regular   = FALSE;
      dounlink  = TRUE;
      completed = FALSE;
    }

  /* case: error performing file lookup */

  else
    {
      switch (rcode)
	{
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_EFATAL:
	  reply.UnlinkHead.rcode = rcode;
	  break;
	default:
	  reply.UnlinkHead.rcode = PIOUS_EUNXP;
	  break;
	}

      regular   = FALSE;
      dounlink  = FALSE;
      completed = TRUE;
    }


  /* perform unlink operation */

  if (dounlink)
    {
      if ((rcode = SS_unlink(request->reqmsg.UnlinkBody.path))
	  == PIOUS_OK)
	/* unlink successful; invalidate cached data for regular file */
	if (regular)
	  CM_finvalidate(fhandle);

      /* set result code appropriately */
      switch(rcode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_EFATAL:
	  reply.UnlinkHead.rcode = rcode;
	  break;
	default:
	  reply.UnlinkHead.rcode = PIOUS_EUNXP;
	  break;
	}

      /* free file lock for regular file and mark operation as completed */
      if (regular)
	LM_wfree(transid);

      completed = TRUE;
    }


  /* reply to client if operation completed */

  if (completed)
    { /* set control message id */
      reply.UnlinkHead.cmsgid = request->reqmsg.UnlinkHead.cmsgid;

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(request->clientid, PDS_UNLINK_OP, &reply);

      /* indicate completion of control operation */
      return COMPLETED;
    }
  else
    /* indicate inability to complete operation */
    return BLOCKED;
}




/*
 * PDS_chmod() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_chmod_(req_infot *request)
#else
static int PDS_chmod_(request)
     req_infot *request;
#endif
{
  int rcode, completed, dochmod, regular, badflush;
  pdsmsg_replyt reply;
  pds_fhandlet fhandle;
  pds_transidt transid;


  rcode = SS_lookup(request->reqmsg.ChmodBody.path,
		    &fhandle,
		    PIOUS_NOCREAT, (pious_modet)0);

  /* case: file exists and is regular */

  if (rcode == PIOUS_OK)
    { /* obtain exclusive access to file utilizing the lock manager */

      regular = TRUE;

      if (transid_assign(&transid) != PIOUS_OK)
	{ /* unable to obtain a transaction id */
	  reply.ChmodHead.rcode = PIOUS_EUNXP;
	  dochmod               = FALSE;
	  completed             = TRUE;
	}

      else if (LM_wlock(transid, fhandle,
			(pious_offt)0,
			(pious_sizet)PIOUS_OFFT_MAX + 1) != LM_GRANT)
	{ /* unable to get exclusive access to file */
	  dochmod   = FALSE;
	  completed = FALSE;
	}

      else
	{ /* obtained exclusive access to file */
	  dochmod   = TRUE;
	  completed = FALSE;
	}
    }

  /* case: file exists but is NOT regular */

  else if (rcode == PIOUS_ENOTREG)
    {
      regular   = FALSE;
      dochmod   = TRUE;
      completed = FALSE;
    }

  /* case: error performing file lookup */

  else
    {
      switch (rcode)
	{
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_EFATAL:
	  reply.ChmodHead.rcode = rcode;
	  break;
	default:
	  reply.ChmodHead.rcode = PIOUS_EUNXP;
	  break;
	}

      regular   = FALSE;
      dochmod   = FALSE;
      completed = TRUE;
    }


  /* perform chmod operation */

  if (dochmod)
    { /* flush/invalidate cached data for regular file */

      badflush = FALSE;

      if (regular)
	{
	  if ((rcode = CM_fflush(fhandle)) != PIOUS_OK)
	    { /* flush failed, set error code appropriately.  if the cache
	       * manager indicates that recovery is required, the main
	       * daemon loop will detect this condition via the global
	       * stable storage flag SS_recover and take action.
	       */

	      if (rcode == PIOUS_EFATAL)
		reply.ChmodHead.rcode = PIOUS_EFATAL;
	      else
		reply.ChmodHead.rcode = PIOUS_EUNXP;

	      badflush = TRUE;
	    }

	  else
	    CM_finvalidate(fhandle);
	}

      /* perform chmod operation; set result code appropriately */

      if (!badflush)
	{
	  rcode = SS_chmod(request->reqmsg.ChmodBody.path,
			   request->reqmsg.ChmodBody.mode,
			   &reply.ChmodBody.amode);

	  switch(rcode)
	    {
	    case PIOUS_OK:
	    case PIOUS_EACCES:
	    case PIOUS_ENOENT:
	    case PIOUS_ENAMETOOLONG:
	    case PIOUS_ENOTDIR:
	    case PIOUS_EPERM:
	    case PIOUS_EFATAL:
	      reply.ChmodHead.rcode = rcode;
	      break;
	    default:
	      reply.ChmodHead.rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      /* free file lock for regular file and mark operation as completed */

      if (regular)
	LM_wfree(transid);

      completed = TRUE;
    }


  /* reply to client if operation completed */

  if (completed)
    { /* set control message id */
      reply.ChmodHead.cmsgid = request->reqmsg.ChmodHead.cmsgid;

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(request->clientid, PDS_CHMOD_OP, &reply);

      /* indicate completion of control operation */
      return COMPLETED;
    }
  else
    /* indicate inability to complete operation */
    return BLOCKED;
}




/*
 * PDS_stat() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_stat_(req_infot *request)
#else
static int PDS_stat_(request)
     req_infot *request;
#endif
{
  int rcode;
  pdsmsg_replyt reply;

  /* perform stat operation */
  rcode = SS_stat(request->reqmsg.StatBody.path,
		  &reply.StatBody.mode);

  /* set result code appropriately */
  switch(rcode)
    {
    case PIOUS_OK:
    case PIOUS_EACCES:
    case PIOUS_ENOENT:
    case PIOUS_ENAMETOOLONG:
    case PIOUS_ENOTDIR:
    case PIOUS_EFATAL:
      reply.StatHead.rcode = rcode;
      break;
    default:
      reply.StatHead.rcode = PIOUS_EUNXP;
      break;
    }

  /* set control message id */
  reply.StatHead.cmsgid = request->reqmsg.StatHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_STAT_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_ping() - See pds.h for description.
 */

#ifdef __STDC__
static int PDS_ping_(req_infot *request)
#else
static int PDS_ping_(request)
     req_infot *request;
#endif
{
  pdsmsg_replyt reply;

  /* ping is a no-op that requires no further processing */
  reply.PingHead.rcode  = PIOUS_OK;
  reply.PingHead.cmsgid = request->reqmsg.PingHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_PING_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_reset() - see pds.h for description
 */

#ifdef __STDC__
static int PDS_reset_(req_infot *request)
#else
static int PDS_reset_(request)
     req_infot *request;
#endif
{
  int fcode;
  pdsmsg_replyt reply;

  /* determine if there are active transactions or blocked control ops */

  if (cntrltable.block_head != NULL ||
      transtable.block_head != NULL || transtable.ready != NULL)
    reply.ResetHead.rcode = PIOUS_EBUSY;

  /* flush cache */

  else if ((fcode = CM_flush()) != PIOUS_OK)
    { /* error flushing cache. inform client that reset operation is
       * incomplete.
       *
       * if the cache manager indicates that recovery is required,
       * the main daemon loop will detect this condition via the
       * global stable storage flag SS_recover and take action.
       */

      if (fcode == PIOUS_EFATAL)
	reply.ResetHead.rcode = PIOUS_EFATAL;
      else
	reply.ResetHead.rcode = PIOUS_EUNXP;
    }

  /* invalidate cache and truncate transaction log file */

  else
    {
      CM_invalidate();

      fcode = SS_logtrunc();

      switch(fcode)
	{
	case PIOUS_OK:
	case PIOUS_EFATAL:
	  reply.ResetHead.rcode = fcode;
	  break;
	default:
	  reply.ResetHead.rcode = PIOUS_EUNXP;
	  break;
	}
    }


  /* set control message id */
  reply.ResetHead.cmsgid = request->reqmsg.ResetHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_RESET_OP, &reply);

  /* indicate completion of control operation */
  return COMPLETED;
}




/*
 * PDS_shutdown() - see pds.h for description
 */

#ifdef __STDC__
static void PDS_shutdown_(req_infot *request)
#else
static void PDS_shutdown_(request)
     req_infot *request;
#endif
{
  int fcode;
  pdsmsg_replyt reply;
  trans_entryt *transrec;
  cntrl_entryt *cntrlrec;


  /* complete blocked control operations */
  cntrlrec = cntrltable.block_head;

  while (cntrlrec != NULL)
    { /* reply to client */
      cntrlreq_ack(&(cntrlrec->cntrlop_req), PIOUS_EBUSY);

      cntrlrec = cntrlrec->tblnext;
    }

  /* abort blocked transaction operations */
  transrec = transtable.block_head;

  while (transrec != NULL)
    { /* reply to client */
      transreq_ack(&(transrec->transop_req), PIOUS_EABORT);

      transrec = transrec->tblnext;
    }


  /* flush the data cache */

  if ((fcode = CM_flush()) != PIOUS_OK)
    { /* error flushing cache. inform client that shutdown operation is
       * incomplete; i.e. is essentially a "crash".
       */

      if (fcode == PIOUS_EFATAL)
	reply.ShutdownHead.rcode = PIOUS_EFATAL;
      else
	reply.ShutdownHead.rcode = PIOUS_EUNXP;
    }

  /* truncate transaction log file if NO prepared transactions.
   *
   * performing a shutdown while a transaction is in the uncertainty
   * period (i.e. is prepared) is equivalent to a crash and thus the log
   * file must be retained to uphold the guarantee of fault-tolerance, since
   * a prepared transaction is a "stable" transaction (see pds.h).
   *
   * however, shutting down with active but un-prepared transactions allows
   * the log file to be truncated without violating semantics since no
   * committed updates by stable transactions are lost, given that the
   * cache has been flushed.
   */

  else
    {
      transrec = transtable.ready;

      while (transrec != NULL && transrec->prepared == FALSE)
	transrec = transrec->tblnext;

      if (transrec != NULL)
	/* found a prepared transaction */
	reply.ShutdownHead.rcode = PIOUS_EUNXP;

      else
	{ /* NO prepared transactions; truncate transaction log */
	  fcode = SS_logtrunc();

	  switch(fcode)
	    {
	    case PIOUS_OK:
	    case PIOUS_EFATAL:
	      reply.ShutdownHead.rcode = fcode;
	      break;
	    default:
	      reply.ShutdownHead.rcode = PIOUS_EUNXP;
	      break;
	    }
	}
    }


  /* set control message id */
  reply.ShutdownHead.cmsgid = request->reqmsg.ShutdownHead.cmsgid;

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, PDS_SHUTDOWN_OP, &reply);

  /* exit from the DCE, then exit process */
  DCE_exit();

  exit(0);
}




/*
 * block_cntrlop()
 *
 * Parameters:
 *
 *   request  - requested control operation information record
 *
 * Insert the control operation 'request' into the table of blocked control
 * operations.
 *
 * Returns:
 *
 *   NULL           - unable to allocate control operation table record
 *   cntrl_entryt * - blocked control operation table record
 */

#ifdef __STDC__
static cntrl_entryt *block_cntrlop(req_infot *request)
#else
static cntrl_entryt *block_cntrlop(request)
     req_infot *request;
#endif
{
  cntrl_entryt *op_entry;

  /* allocate table space */
  op_entry = (cntrl_entryt *)malloc((unsigned)sizeof(cntrl_entryt));

  /* set table fields appropriately */
  if (op_entry != NULL)
    {
      op_entry->cntrlop_req.clientid = request->clientid;
      op_entry->cntrlop_req.reqop    = request->reqop;
      op_entry->cntrlop_req.reqmsg   = request->reqmsg;

      UTIL_clock_mark(&(op_entry->cntrlop_req.tstamp));

      /* insert at tail of blocked list */
      op_entry->tblnext = NULL;
      op_entry->tblprev = cntrltable.block_tail;

      if (op_entry->tblprev != NULL)
	op_entry->tblprev->tblnext = op_entry;
      else
	cntrltable.block_head = op_entry;

      cntrltable.block_tail = op_entry;
    }

  return op_entry;
}




/*
 * rm_cntrlrec()
 *
 * Parameters:
 *
 *   cntrlrec - blocked control operation table record
 *
 * Complete a control operation by deallocating request and removing
 * 'cntrlrec' from the blocked control operation table.
 *
 * Returns:
 */ 

#ifdef __STDC__
static void rm_cntrlrec(cntrl_entryt *cntrlrec)
#else
static void rm_cntrlrec(cntrlrec)
     cntrl_entryt *cntrlrec;
#endif
{
  /* deallocate request */
  cntrlreq_dealloc(&(cntrlrec->cntrlop_req));

  /* remove control operation record from blocked table */
  if (cntrlrec->tblnext != NULL)
    cntrlrec->tblnext->tblprev = cntrlrec->tblprev;
  else
    cntrltable.block_tail = cntrlrec->tblprev;

  if (cntrlrec->tblprev != NULL)
    cntrlrec->tblprev->tblnext = cntrlrec->tblnext;
  else
    cntrltable.block_head = cntrlrec->tblnext;

  /* deallocate storage */
  free((char *)cntrlrec);
}




/*
 * cntrlreq_ack()
 *
 * Parameters:
 *
 *   request - control operation request information record
 *   rcode   - result code
 *
 * Send client a null reply message for requested control operation that
 * has a result code 'rode'.
 *
 * Note: Assumes parameters are valid
 *
 * Returns:
 */

#ifdef __STDC__
static void cntrlreq_ack(req_infot *request,
			 int rcode)
#else
static void cntrlreq_ack(request, rcode)
     req_infot *request;
     int rcode;
#endif
{
  pdsmsg_replyt reply;

  /* set reply message header */
  reply.CntrlopHead.cmsgid = request->reqmsg.CntrlopHead.cmsgid;
  reply.CntrlopHead.rcode  = rcode;

  /* set reply message body - NULL any expected pointers */

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, request->reqop, &reply);
}




/*
 * cntrlreq_dealloc()
 *
 * Parameters:
 *
 *   request - control operation request information record
 *
 * Deallocate any extra space allocated to control operation request
 * 'request'.  Space for 'request' itself is NOT deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void cntrlreq_dealloc(req_infot *request)
#else
static void cntrlreq_dealloc(request)
     req_infot *request;
#endif
{
  /* space to deallocate is dependent on the operation type */
  switch(request->reqop)
    {
    case PDS_LOOKUP_OP:
      /* deallocate pathname space */
      if (request->reqmsg.LookupBody.path != NULL)
	{
	  free(request->reqmsg.LookupBody.path);
	  request->reqmsg.LookupBody.path = NULL;
	}
      break;

    case PDS_MKDIR_OP:
      /* deallocate pathname space */
      if (request->reqmsg.MkdirBody.path != NULL)
	{
	  free(request->reqmsg.MkdirBody.path);
	  request->reqmsg.MkdirBody.path = NULL;
	}
      break;

    case PDS_RMDIR_OP:
      /* deallocate pathname space */
      if (request->reqmsg.RmdirBody.path != NULL)
	{
	  free(request->reqmsg.RmdirBody.path);
	  request->reqmsg.RmdirBody.path = NULL;
	}
      break;

    case PDS_UNLINK_OP:
      /* deallocate pathname space */
      if (request->reqmsg.UnlinkBody.path != NULL)
	{
	  free(request->reqmsg.UnlinkBody.path);
	  request->reqmsg.UnlinkBody.path = NULL;
	}
      break;

    case PDS_CHMOD_OP:
      /* deallocate pathname space */
      if (request->reqmsg.ChmodBody.path != NULL)
	{
	  free(request->reqmsg.ChmodBody.path);
	  request->reqmsg.ChmodBody.path = NULL;
	}
      break;

    case PDS_STAT_OP:
      /* deallocate pathname space */
      if (request->reqmsg.StatBody.path != NULL)
	{
	  free(request->reqmsg.StatBody.path);
	  request->reqmsg.StatBody.path = NULL;
	}
      break;
    }
}




/*
 * Private Function Definitions - Transaction Operation Functions
 */



  
/*
 * do_transop()
 *
 * Parameters:
 *
 *   transrec - transaction table record
 *
 * Attempt to perform the most recently requested transaction operation
 * for the indicated transaction.  The transaction operation state must
 * be either ACTIVE, indicating the most recently received request, or
 * BLOCKED, indicating an operation that previously blocked waiting on a lock.
 *
 * Returns:
 */

#ifdef __STDC__
static void do_transop(trans_entryt *transrec)
#else
static void do_transop(transrec)
     trans_entryt *transrec;
#endif
{
#ifdef PDSPROFILE
  /* mark operation start time for profiling time accumulation */
  UTIL_clock_mark(&prof_clock);
#endif

  if (transrec->transop_state == ACTIVE || transrec->transop_state == BLOCKED)
    switch (transrec->transop_req.reqop)
      {
      case PDS_READ_OP:
	PDS_read_(transrec);
	break;
      case PDS_WRITE_OP:
	PDS_write_(transrec);
	break;
      case PDS_READ_SINT_OP:
	PDS_read_sint_(transrec);
	break;
      case PDS_WRITE_SINT_OP:
	PDS_write_sint_(transrec);
	break;
      case PDS_FA_SINT_OP:
	PDS_fa_sint_(transrec);
	break;
      case PDS_PREPARE_OP:
	PDS_prepare_(transrec);
	break;
      case PDS_COMMIT_OP:
	PDS_commit_(transrec);
	break;
      case PDS_ABORT_OP:
	PDS_abort_(transrec);
	break;
      }
}




/*
 * retry_blk_transop()
 *
 * Parameters:
 *
 * Scan the transaction table for blocked operations that can now be
 * performed and do them.
 *
 * NOTE: Will not attempt further operations if any operation generates a
 *       SS_fatalerror, SS_recover, or SS_checkpoint flag.
 *
 * Returns:
 */

#ifdef __STDC__
static void retry_blk_transop(void)
#else
static void retry_blk_transop()
#endif
{
  trans_entryt *transrec, *transrec_next;

  /* scan blocked transaction operations */
  transrec = transtable.block_head;

  while (transrec != NULL &&
	 !SS_fatalerror && !SS_recover && !SS_checkpoint)
    { /* determine next, as completing operation removes transaction from
       * the blocked list and places it on the ready list.
       */
      transrec_next = transrec->tblnext;

      /* attempt blocked transaction operation */
      do_transop(transrec);

      transrec = transrec_next;
    }
}




/*
 * PDS_read - See pds.h for description
 */

#ifdef __STDC__
static void PDS_read_(trans_entryt *transrec)
#else
static void PDS_read_(transrec)
     trans_entryt *transrec;
#endif
{
  int completed, lcode;
  pious_ssizet dmcode, rcode;
  pious_sizet nbyte_prime;
  char *rbuf;
  pdsmsg_reqt *request;
  req_auxstoret *reqaux;

  /* if new request then validate params & compute nbyte_prime/sched values */

  completed = FALSE;
  rbuf      = NULL;
  request   = &(transrec->transop_req.reqmsg);
  reqaux    = &(transrec->transop_req.aux);

  if (transrec->transop_state == ACTIVE)
    { /* succeed immediately if number of bytes to read is zero (0) */
      if (request->ReadBody.nbyte == 0)
	{
	  rcode     = 0;
	  completed = TRUE;
	}

      /* check parameter bounds */
      else if (request->ReadBody.offset < 0 ||
	       request->ReadBody.offset > PIOUS_OFFT_MAX  ||
	       request->ReadBody.nbyte  < 0 ||
	       request->ReadBody.nbyte  > PIOUS_SIZET_MAX ||
	       (request->ReadBody.lock != PDS_READLK &&
		request->ReadBody.lock != PDS_WRITELK))
	{
	  rcode     = PIOUS_EINVAL;
	  completed = TRUE;
	}

      /* compute nbyte_prime and scheduling values */
      else
	{ /* adjust byte count to upper bound of range, if necessary;
	   * can NOT assign to request message as init_transreq() may require
	   * client parameters to implement transaction operation protocol.
	   */

	  if (PIOUS_OFFT_MAX - request->ReadBody.offset -
	      (request->ReadBody.nbyte - 1) >= 0)
	    reqaux->nobj_prime = request->ReadBody.nbyte;
	  else
	    reqaux->nobj_prime = ((PIOUS_OFFT_MAX - request->ReadBody.offset)
				  + 1);

	  reqaux->lk_fhandle = request->ReadBody.fhandle;
	  reqaux->lk_type    = request->ReadBody.lock;
	  reqaux->lk_start   = request->ReadBody.offset;
	  reqaux->lk_stop    = reqaux->lk_start + (reqaux->nobj_prime - 1);
	}
    }

  /* perform read operation (of greater than zero (0) bytes) */

  if (!completed && !fcfs_conflict(transrec))
    { /* retrieve computed value of nbyte_prime */
      nbyte_prime = reqaux->nobj_prime;

      /* request appropriate lock */
      if (request->ReadBody.lock == PDS_READLK)
	lcode = LM_rlock(request->ReadHead.transid,
			 request->ReadBody.fhandle,
			 request->ReadBody.offset,
			 nbyte_prime);
      else
	lcode = LM_wlock(request->ReadHead.transid,
			 request->ReadBody.fhandle,
			 request->ReadBody.offset,
			 nbyte_prime);

      /* allocate a read buffer and perform read operation if lock obtained */

      if (lcode == LM_GRANT)
	{ /* allocate a read buffer */
	  if ((rbuf = (char *)malloc((unsigned)nbyte_prime)) == NULL)
	    { /* insufficient buffer space for operation */
	      rcode = PIOUS_EINSUF;
	    }

	  /* read data into buffer */
	  else
	    { 
	      dmcode = DM_read(request->ReadHead.transid,
			       request->ReadBody.fhandle,
			       request->ReadBody.offset,
			       nbyte_prime,
			       rbuf);

	      if (dmcode >= 0)
		/* read successful, return number of bytes read */
		rcode = dmcode;
	      else
		/* read failed, set error code appropriately.  if the data
		 * manager indicates that recovery is required, inform client
		 * that this transaction is aborted.
		 *
		 * the main daemon loop will detect that recovery is required
		 * via the global stable storage flag SS_recover and take
		 * action.
		 */

		switch(dmcode)
		  {
		  case PIOUS_EBADF:
		  case PIOUS_EACCES:
		  case PIOUS_EINVAL:
		  case PIOUS_EINSUF:
		  case PIOUS_EPROTO:
		  case PIOUS_EFATAL:
		    rcode = dmcode;
		    break;
		  case PIOUS_ERECOV:
		    rcode = PIOUS_EABORT;
		    break;
		  default:
		    rcode = PIOUS_EUNXP;
		    break;
		  }
	    }

	  /* flag completion of operation; mark transaction as holding a
	   * read or write lock, as appropriate.
	   */

	  if (request->ReadBody.lock == PDS_READLK)
	    transrec->readlk = TRUE;
	  else
	    transrec->writelk = TRUE;

	  completed = TRUE;
	}
    }

  if (completed)
    { /* read operation completed; deallocate read buffer if error occured
       * or if no data is to be returned.
       */

      if (rcode <= 0 && rbuf != NULL)
	{
	  free(rbuf);
	  rbuf = NULL;
	}

      /* set reply message */
      transrec->transop_reply.replyop                   = PDS_READ_OP;

      transrec->transop_reply.replymsg.ReadHead.transid =
	request->ReadHead.transid;
      transrec->transop_reply.replymsg.ReadHead.transsn =
	request->ReadHead.transsn;
      transrec->transop_reply.replymsg.ReadHead.rcode   = rcode;

      transrec->transop_reply.replymsg.ReadBody.buf     = rbuf;

      /* mark operation as completed */
      complete_transop(transrec);

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_READ_OP,
			&transrec->transop_reply.replymsg);
    }

  else
    { /* mark operation as blocked */
      block_transop(transrec);
    }
}




/*
 * PDS_write() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_write_(trans_entryt *transrec)
#else
static void PDS_write_(transrec)
     trans_entryt *transrec;
#endif
{
  int completed, dmcode;
  pious_ssizet rcode;
  pious_sizet nbyte_prime;
  pdsmsg_reqt *request;
  req_auxstoret *reqaux;

  /* if new request then validate params & compute nbyte_prime/sched values */

  completed = FALSE;
  request   = &(transrec->transop_req.reqmsg);
  reqaux    = &(transrec->transop_req.aux);

  if (transrec->transop_state == ACTIVE)
    { /* succeed immediately if number of bytes to write is zero (0) */
      if (request->WriteBody.nbyte == 0)
	{
	  rcode     = 0;
	  completed = TRUE;
	}

      /* check parameter bounds */
      else if (request->WriteBody.offset < 0 ||
	       request->WriteBody.offset > PIOUS_OFFT_MAX  ||
	       request->WriteBody.nbyte  < 0 ||
	       request->WriteBody.nbyte  > PIOUS_SIZET_MAX)
	{
	  rcode     = PIOUS_EINVAL;
	  completed = TRUE;
	}

      /* compute nbyte_prime and scheduling values */
      else
	{ /* adjust byte count to upper bound of range, if necessary;
	   * can NOT assign to request message as init_transreq() may require
	   * client parameters to implement transaction operation protocol.
	   */

	  if (PIOUS_OFFT_MAX - request->WriteBody.offset -
	      (request->WriteBody.nbyte - 1) >= 0)
	    reqaux->nobj_prime = request->WriteBody.nbyte;
	  else
	    reqaux->nobj_prime = ((PIOUS_OFFT_MAX - request->WriteBody.offset)
				  + 1);

	  reqaux->lk_fhandle = request->WriteBody.fhandle;
	  reqaux->lk_type    = PDS_WRITELK;
	  reqaux->lk_start   = request->WriteBody.offset;
	  reqaux->lk_stop    = reqaux->lk_start + (reqaux->nobj_prime - 1);
	}
    }

  /* perform write operation (of greater than zero (0) bytes) */

  if (!completed && !fcfs_conflict(transrec))
    { /* retrieve computed value of nbyte_prime */
      nbyte_prime = reqaux->nobj_prime;

      /* request write lock */
      if (LM_wlock(request->WriteHead.transid,
		   request->WriteBody.fhandle,
		   request->WriteBody.offset,
		   nbyte_prime) == LM_GRANT)
	{ /* lock obtained; perform write */
	  dmcode = DM_write(request->WriteHead.transid,
			    request->WriteBody.fhandle,
			    request->WriteBody.offset,
			    nbyte_prime,
			    request->WriteBody.buf);

	  if (dmcode == PIOUS_OK)
	    /* write successful, return byte count */
	    rcode = nbyte_prime;
	  else
	    /* write unsuccessful, set result code appropriately */
	    switch(dmcode)
	      {
	      case PIOUS_EBADF:
	      case PIOUS_EACCES:
	      case PIOUS_EINVAL:
	      case PIOUS_EINSUF:
	      case PIOUS_EPROTO:
	      case PIOUS_EFATAL:
		rcode = dmcode;
		break;
	      default:
		rcode = PIOUS_EUNXP;
		break;
	      }

	  /* flag completion of operation; mark transaction as holding
	   * a write lock; if write successful, transaction is not readonly
	   */

	  if (rcode >= 0)
	    transrec->readonly = FALSE;

	  transrec->writelk = TRUE;
	  completed         = TRUE;
	}
    }


  if (completed)
    { /* write operation is complete; deallocate write buffer if error occured
       * or if no data written.
       */

      if (rcode <= 0 && request->WriteBody.buf != NULL)
	free((char *)request->WriteBody.buf);

      /* mark write buf as NULL so that transreq_dealloc() will not free.
       * if an error occured, or no write took place, storage freed above;
       * otherwise storage will be deallocated by the data manager at the
       * appropriate time.
       */

      request->WriteBody.buf = NULL;

      /* set reply message */
      transrec->transop_reply.replyop                    = PDS_WRITE_OP;

      transrec->transop_reply.replymsg.WriteHead.transid =
	request->WriteHead.transid;
      transrec->transop_reply.replymsg.WriteHead.transsn =
	request->WriteHead.transsn;
      transrec->transop_reply.replymsg.WriteHead.rcode   = rcode;

      /* mark operation as complete */
      complete_transop(transrec);

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_WRITE_OP,
			&transrec->transop_reply.replymsg);
    }

  else
    { /* mark operation as blocked */
      block_transop(transrec);
    }
}




/*
 * PDS_read_sint() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_read_sint_(trans_entryt *transrec)
#else
static void PDS_read_sint_(transrec)
     trans_entryt *transrec;
#endif
{
  int completed, rcode;
  long *rbuf;
  int nint_prime;
  pious_ssizet dmcode;
  pdsmsg_reqt *request;
  req_auxstoret *reqaux;

  /* if new request then validate params & compute nint_prime/sched values */

  completed = FALSE;
  rbuf      = NULL;
  request   = &(transrec->transop_req.reqmsg);
  reqaux    = &(transrec->transop_req.aux);

  if (transrec->transop_state == ACTIVE)
    { /* succeed immediately if number of integers to read is zero (0) */
      if (request->ReadsintBody.nint == 0)
	{
	  rcode     = 0;
	  completed = TRUE;
	}

      /* check parameter bounds; note: offset is index into integer array */
      else if (request->ReadsintBody.offset < 0 ||

	       request->ReadsintBody.offset >
	       PIOUS_OFFT_MAX / sizeof(long) - 1 ||

	       request->ReadsintBody.nint   < 0 ||
	       request->ReadsintBody.nint   > PIOUS_INT_MAX)
	{
	  rcode     = PIOUS_EINVAL;
	  completed = TRUE;
	}

      /* compute nint_prime and scheduling values */
      else
	{ /* adjust integer count to upper bound of range, if necessary;
	   * can NOT assign to request message as init_transreq() may require
	   * client parameters to implement transaction operation protocol.
	   */

	  if ((PIOUS_OFFT_MAX / sizeof(long)) - request->ReadsintBody.offset <
	      request->ReadsintBody.nint)
	    reqaux->nobj_prime = ((PIOUS_OFFT_MAX / sizeof(long)) -
				  request->ReadsintBody.offset);
	  else
	    reqaux->nobj_prime = request->ReadsintBody.nint;

	  reqaux->lk_fhandle = request->ReadsintBody.fhandle;
	  reqaux->lk_type    = PDS_READLK;
	  reqaux->lk_start   = request->ReadsintBody.offset * sizeof(long);
	  reqaux->lk_stop    = (reqaux->lk_start +
				((reqaux->nobj_prime * sizeof(long)) - 1));
	}
    }


  /* perform read_sint operation (of greater than zero (0) integers) */

  if (!completed && !fcfs_conflict(transrec))
    { /* retrieve computed value of nint_prime */
      nint_prime = reqaux->nobj_prime;

      /* obtain read lock */
      if (LM_rlock(request->ReadsintHead.transid,
		   request->ReadsintBody.fhandle,
		   (pious_offt)(request->ReadsintBody.offset * sizeof(long)),
		   (pious_sizet)(nint_prime * sizeof(long))) == LM_GRANT)

	{ /* lock obtained; alloc a read buffer and perform read operation */

	  if ((rbuf = (long *)
	       malloc((unsigned)(nint_prime * sizeof(long)))) == NULL)
	    { /* insufficinet buffer space for operation */
	      rcode = PIOUS_EINSUF;
	    }

	  else
	    { /* read data into buffer */
	      dmcode = DM_read(request->ReadsintHead.transid,
			       request->ReadsintBody.fhandle,
			       (pious_offt)(request->ReadsintBody.offset *
					    sizeof(long)),
			       (pious_sizet)(nint_prime * sizeof(long)),
			       (char *)rbuf);

	      /* set result code appropriately. if the data manager
	       * indicates that recovery is required, inform client
	       * that this transaction is aborted.
	       *
	       * the main daemon loop will detect that recovery is required
	       * via the global stable storage flag SS_recover and take action.
	       */

	      if (dmcode >= 0)
		/* read successful, return number of integers read */
		rcode = dmcode / sizeof(long);

	      else
		switch(dmcode)
		  {
		  case PIOUS_EBADF:
		  case PIOUS_EACCES:
		  case PIOUS_EINVAL:
		  case PIOUS_EINSUF:
		  case PIOUS_EPROTO:
		  case PIOUS_EFATAL:
		    rcode = dmcode;
		    break;
		  case PIOUS_ERECOV:
		    rcode = PIOUS_EABORT;
		    break;
		  default:
		    rcode = PIOUS_EUNXP;
		    break;
		  }
	    }


	  /* flag completion of operation; mark transaction as holding
	   * a read lock
	   */

	  transrec->readlk = TRUE;
	  completed        = TRUE;
	}
    }


  if (completed)
    { /* read_sint operation complete; deallocate read buffer if error occured
       * or if no data is to be returned
       */

      if (rcode <= 0 && rbuf != NULL)
	{
	  free((char *)rbuf);
	  rbuf = NULL;
	}


      /* set reply message */
      transrec->transop_reply.replyop                       = PDS_READ_SINT_OP;

      transrec->transop_reply.replymsg.ReadsintHead.transid =
	request->ReadsintHead.transid;
      transrec->transop_reply.replymsg.ReadsintHead.transsn =
	request->ReadsintHead.transsn;
      transrec->transop_reply.replymsg.ReadsintHead.rcode   = rcode;

      transrec->transop_reply.replymsg.ReadsintBody.buf     = rbuf;

      /* mark operation as completed */
      complete_transop(transrec);

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_READ_SINT_OP,
			&transrec->transop_reply.replymsg);
    }

  else
    { /* mark operation as blocked */
      block_transop(transrec);
    }
}




/*
 * PDS_write_sint() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_write_sint_(trans_entryt *transrec)
#else
static void PDS_write_sint_(transrec)
     trans_entryt *transrec;
#endif
{
  int rcode, completed;
  int nint_prime;
  pious_ssizet dmcode;
  pdsmsg_reqt *request;
  req_auxstoret *reqaux;

  /* if new request then validate params & compute nint_prime/sched values */

  completed = FALSE;
  request   = &(transrec->transop_req.reqmsg);
  reqaux    = &(transrec->transop_req.aux);

  if (transrec->transop_state == ACTIVE)
    { /* succeed immediately if number of integers to write is zero (0) */
      if (request->WritesintBody.nint == 0)
	{
	  rcode     = 0;
	  completed = TRUE;
	}

      /* check parameter bounds; note: offset is index into integer array */
      else if (request->WritesintBody.offset < 0 ||

	       request->WritesintBody.offset >
	       PIOUS_OFFT_MAX / sizeof(long) - 1  ||

	       request->WritesintBody.nint   < 0 ||
	       request->WritesintBody.nint   > PIOUS_INT_MAX)
	{
	  rcode     = PIOUS_EINVAL;
	  completed = TRUE;
	}

      /* compute nint_prime and scheduling values */
      else
	{ /* adjust integer count to upper bound of range, if necessary;
	   * can NOT assign to request message as init_transreq() may require
	   * client parameters to implement transaction operation protocol.
	   */

	  if ((PIOUS_OFFT_MAX / sizeof(long)) - request->WritesintBody.offset <
	      request->WritesintBody.nint)
	    reqaux->nobj_prime = ((PIOUS_OFFT_MAX / sizeof(long)) -
				  request->WritesintBody.offset);
	  else
	    reqaux->nobj_prime = request->WritesintBody.nint;

	  reqaux->lk_fhandle = request->WritesintBody.fhandle;
	  reqaux->lk_type    = PDS_WRITELK;
	  reqaux->lk_start   = request->WritesintBody.offset * sizeof(long);
	  reqaux->lk_stop    = (reqaux->lk_start +
				((reqaux->nobj_prime * sizeof(long)) - 1));
	}
    }


  /* perform write_sint operation (of greater than zero (0) integers) */

  if (!completed && !fcfs_conflict(transrec))
    { /* retrieve computed value of nint_prime */
      nint_prime = reqaux->nobj_prime;

      /* obtain write lock */
      if (LM_wlock(request->WritesintHead.transid,
		   request->WritesintBody.fhandle,
		   (pious_offt)(request->WritesintBody.offset * sizeof(long)),
		   (pious_sizet)(nint_prime * sizeof(long))) == LM_GRANT)

	{ /* lock obtained; perform write operation */
	  dmcode = DM_write(request->WritesintHead.transid,
			    request->WritesintBody.fhandle,
			    (pious_offt)(request->WritesintBody.offset *
					 sizeof(long)),
			    (pious_sizet)(nint_prime * sizeof(long)),
			    (char *)request->WritesintBody.buf);

	  if (dmcode == PIOUS_OK)
	    /* write successful, return integer count */
	    rcode = nint_prime;
	  else
	    /* write failed, set result code appropriately */
	    switch(dmcode)
	      {
	      case PIOUS_EBADF:
	      case PIOUS_EACCES:
	      case PIOUS_EINVAL:
	      case PIOUS_EINSUF:
	      case PIOUS_EPROTO:
	      case PIOUS_EFATAL:
		rcode = dmcode;
		break;
	      default:
		rcode = PIOUS_EUNXP;
		break;
	      }

	  /* flag completion of operation; mark transaction as holding
	   * a write lock; if write successful, transaction is not readonly
	   */

	  if (rcode >= 0)
	    transrec->readonly = FALSE;

	  transrec->writelk = TRUE;
	  completed         = TRUE;
	}
    }


  if (completed)
    { /* write_sint operation complete; deallocate int buffer if error occured
       * or if no data written.
       */

      if (rcode <= 0 && request->WritesintBody.buf != NULL)
	free((char *)request->WritesintBody.buf);

      /* mark write buf as NULL so that transreq_dealloc() will not free.
       * if an error occured, or no write took place, storage freed above;
       * otherwise storage will be deallocated by the data manager at the
       * appropriate time.
       */

      request->WritesintBody.buf = NULL;

      /* set reply message */
      transrec->transop_reply.replyop = PDS_WRITE_SINT_OP;

      transrec->transop_reply.replymsg.WritesintHead.transid =
	request->WritesintHead.transid;
      transrec->transop_reply.replymsg.WritesintHead.transsn =
	request->WritesintHead.transsn;
      transrec->transop_reply.replymsg.WritesintHead.rcode   = rcode;

      /* mark operation as completed */
      complete_transop(transrec);

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_WRITE_SINT_OP,
			&transrec->transop_reply.replymsg);
    }

  else
    { /* mark operation as blocked */
      block_transop(transrec);
    }
}




/*
 * PDS_fa_sint() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_fa_sint_(trans_entryt *transrec)
#else
static void PDS_fa_sint_(transrec)
     trans_entryt *transrec;
#endif
{
  int nowrite, completed, rcode;
  long *intbuf, rvalue;
  pious_offt intidx;
  pious_ssizet dmcode;
  pdsmsg_reqt *request;
  req_auxstoret *reqaux;

  /* if new request then validate params & compute scheduling values */

  completed = FALSE;
  intbuf    = NULL;
  request   = &(transrec->transop_req.reqmsg);
  reqaux    = &(transrec->transop_req.aux);

  if (transrec->transop_state == ACTIVE)
    { /* note: offset is index into integer array; test bounds accordingly */
      if (request->FasintBody.offset < 0 ||
	  request->FasintBody.offset > PIOUS_OFFT_MAX / sizeof(long) - 1 ||
	  request->FasintBody.increment > PIOUS_LONG_MAX)
	{
	  rcode     = PIOUS_EINVAL;
	  completed = TRUE;
	}

      /* compute scheduling values */
      else
	{
	  reqaux->nobj_prime = 1;
	  reqaux->lk_fhandle = request->FasintBody.fhandle;
	  reqaux->lk_type    = PDS_WRITELK;
	  reqaux->lk_start   = request->FasintBody.offset * sizeof(long);
	  reqaux->lk_stop    = reqaux->lk_start + (sizeof(long) - 1);
	}
    }

  /* perform fa_sint operation */

  if (!completed && !fcfs_conflict(transrec))
    { /* calculate integer index */
      intidx = request->FasintBody.offset * sizeof(long);

      /* obtain write lock */
      if (LM_wlock(request->FasintHead.transid,
		   request->FasintBody.fhandle,
		   intidx,
		   (pious_sizet)sizeof(long)) == LM_GRANT)
	{ /* lock obtained; allocate read/modify/write buffer */

	  if ((intbuf = (long *)malloc((unsigned)sizeof(long))) == NULL)
	    /* unable to allocate a write buffer */
	    rcode = PIOUS_EINSUF;
	  else
	    { /* perform read operation */
	      nowrite = TRUE;

	      if ((dmcode = DM_read(request->FasintHead.transid,
				    request->FasintBody.fhandle,
				    intidx,
				    (pious_sizet)sizeof(long),
				    (char *)intbuf)) == sizeof(long))
		{ /* value read; set return value, increment and write back */
		  rvalue   = *intbuf;
		  *intbuf += request->FasintBody.increment;

		  dmcode = DM_write(request->FasintHead.transid,
				    request->FasintBody.fhandle,
				    intidx,
				    (pious_sizet)sizeof(long),
				    (char *)intbuf);
		  nowrite = FALSE;
		}

	      /* set result code appropriately. if the data manager
	       * indicates that recovery is required, inform client
	       * that this transaction is aborted.
	       *
	       * the main daemon loop will detect that recovery is required
               * via the global stable storage flag SS_recover and take action.
	       */

	      if (nowrite && dmcode >= 0)
		/* insufficient data in file */
		rcode = PIOUS_EINVAL;
	      else
		switch(dmcode)
		  {
		  case PIOUS_OK:
		  case PIOUS_EBADF:
		  case PIOUS_EACCES:
		  case PIOUS_EINVAL:
		  case PIOUS_EINSUF:
		  case PIOUS_EPROTO:
		  case PIOUS_EFATAL:
		    rcode = dmcode;
		    break;
		  case PIOUS_ERECOV:
		    rcode = PIOUS_EABORT;
		    break;
		  default:
		    rcode = PIOUS_EUNXP;
		    break;
		  }
	    }

	  /* flag completion of operation; mark transaction as holding
	   * a write lock; if write successful, transaction is not readonly
	   */

	  if (rcode == PIOUS_OK)
	    transrec->readonly = FALSE;

	  transrec->writelk = TRUE;
	  completed         = TRUE;
	}
    }


  if (completed)
    { /* fa_sint operation complete; deallocate int buffer if error occured,
       * otherwise storage will be deallocated by the data manager at the
       * appropriate time.
       */

      if (rcode < 0 && intbuf != NULL)
	free((char *)intbuf);

      /* set reply message */
      transrec->transop_reply.replyop                     = PDS_FA_SINT_OP;

      transrec->transop_reply.replymsg.FasintHead.transid =
	request->FasintHead.transid;
      transrec->transop_reply.replymsg.FasintHead.transsn =
	request->FasintHead.transsn;
      transrec->transop_reply.replymsg.FasintHead.rcode   = rcode;

      transrec->transop_reply.replymsg.FasintBody.rvalue  = rvalue;

      /* mark operation as completed */
      complete_transop(transrec);

      /* reply to client; inability to send is equivalent to a lost message */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_FA_SINT_OP,
			&transrec->transop_reply.replymsg);
    }

  else
    { /* mark operation as blocked */
      block_transop(transrec);
    }
}




/*
 * PDS_prepare() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_prepare_(trans_entryt *transrec)
#else
static void PDS_prepare_(transrec)
     trans_entryt *transrec;
#endif
{
  int rcode, dmcode;

  /* issue prepare to data manager and set result code */
  dmcode = DM_prepare(transrec->transid);

  /* any error is a vote to abort. if the data manager result code indicates
   * that checkpointing is required, the main daemon loop will detect this
   * via the global stable storage flag SS_checkpoint.
   *
   * note that upon a successful prepare, readonly transactions are
   * distinguished as such.
   */

  switch(dmcode)
    {
    case PIOUS_OK:
      if (transrec->readonly)
	rcode = PIOUS_READONLY;
      else
	rcode = PIOUS_OK;
      break;
    case PIOUS_EFATAL:
      rcode = PIOUS_EFATAL;
      break;
    default:
      rcode = PIOUS_EABORT;
      break;
    }

  /* if vote to commit a transaction that has successfully written, then
   * free read locks and complete prepare.
   */

  if (rcode == PIOUS_OK)
    { /* free read locks ONLY (Strict 2PL) */
      if (transrec->readlk)
	{
	  LM_rfree(transrec->transid);
	  transrec->readlk = FALSE;
	}
    }

  /* if vote to abort, or if a read-only transaction, then free all locks
   * and complete prepare, as transaction will be removed from the
   * transaction table; the later case is referred to as the 2PC read-only
   * optimization.
   *
   * note that read-only transactions may hold write locks from writes
   * that failed.
   */

  else
    { /* free ALL locks */
      if (transrec->readlk)
	LM_rfree(transrec->transid);

      if (transrec->writelk)
	LM_wfree(transrec->transid);
    }


  /* set reply message */
  transrec->transop_reply.replyop                      = PDS_PREPARE_OP;

  transrec->transop_reply.replymsg.PrepareHead.transid =
    transrec->transop_req.reqmsg.PrepareHead.transid;
  transrec->transop_reply.replymsg.PrepareHead.transsn =
    transrec->transop_req.reqmsg.PrepareHead.transsn;
  transrec->transop_reply.replymsg.PrepareHead.rcode   = rcode;

  /* mark operation as complete */
  complete_transop(transrec);

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(transrec->transop_req.clientid,
		    PDS_PREPARE_OP,
		    &transrec->transop_reply.replymsg);


  /* if vote to abort or read-only transaction, remove from table */
  if (rcode != PIOUS_OK)
    rm_transrec(transrec);
  else
    transrec->prepared = TRUE;
}




/*
 * PDS_commit() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_commit_(trans_entryt *transrec)
#else
static void PDS_commit_(transrec)
     trans_entryt *transrec;
#endif
{
  int rcode, dmcode;

  /* issue commit to data manager and set result code */
  dmcode = DM_commit(transrec->transid);

  /* if recovery required, inform client that commit not logged if transaction
   * is prepared, ie is a stable transaction, otherwise transaction is
   * volatile so inform client that an unexpected error has occured.
   *
   * the main daemon loop will detect the need for recovery via the global
   * stable storage flag SS_recover.
   */

  switch(dmcode)
    {
    case PIOUS_OK:
    case PIOUS_ENOTLOG:
    case PIOUS_EFATAL:
      rcode = dmcode;
      break;
    case PIOUS_ERECOV:
      if (transrec->prepared)
	rcode = PIOUS_ENOTLOG;
      else
	rcode = PIOUS_EUNXP;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  /* free ALL locks held by the transaction */
  if (transrec->readlk)
    LM_rfree(transrec->transid);

  if (transrec->writelk)
    LM_wfree(transrec->transid);

  /* set reply message */
  transrec->transop_reply.replyop                     = PDS_COMMIT_OP;

  transrec->transop_reply.replymsg.CommitHead.transid =
    transrec->transop_req.reqmsg.CommitHead.transid;
  transrec->transop_reply.replymsg.CommitHead.transsn =
    transrec->transop_req.reqmsg.CommitHead.transsn;
  transrec->transop_reply.replymsg.CommitHead.rcode   = rcode;

  /* mark operation as complete */
  complete_transop(transrec);

  /* reply to client; inability to send is equivalent to a lost message */

#ifdef VTCOMMITNOACK
  if (transrec->prepared)
    { /* ack commit for stable (ie prepared) transactions only */
      PDSMSG_reply_send(transrec->transop_req.clientid,
			PDS_COMMIT_OP,
			&transrec->transop_reply.replymsg);
    }
#else
  /* ack commit for both stable and volatile transactions */
  PDSMSG_reply_send(transrec->transop_req.clientid,
		    PDS_COMMIT_OP,
		    &transrec->transop_reply.replymsg);
#endif

  /* remove transaction from transaction table */
  rm_transrec(transrec);
}




/*
 * PDS_abort() - See pds.h for description
 */

#ifdef __STDC__
static void PDS_abort_(trans_entryt *transrec)
#else
static void PDS_abort_(transrec)
     trans_entryt *transrec;
#endif
{
  int rcode, dmcode;

  /* issue abort to data manager and set result code */
  dmcode = DM_abort(transrec->transid);

  switch(dmcode)
    {
    case PIOUS_OK:
    case PIOUS_ENOTLOG:
    case PIOUS_EFATAL:
      rcode = dmcode;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  /* free ALL locks held by the transaction */
  if (transrec->readlk)
    LM_rfree(transrec->transid);

  if (transrec->writelk)
    LM_wfree(transrec->transid);

  /* set reply message */
  transrec->transop_reply.replyop                    = PDS_ABORT_OP;

  transrec->transop_reply.replymsg.AbortHead.transid =
    transrec->transop_req.reqmsg.AbortHead.transid;
  transrec->transop_reply.replymsg.AbortHead.transsn =
    transrec->transop_req.reqmsg.AbortHead.transsn;
  transrec->transop_reply.replymsg.AbortHead.rcode   = rcode;

  /* mark operation as complete */
  complete_transop(transrec);

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(transrec->transop_req.clientid,
		    PDS_ABORT_OP,
		    &transrec->transop_reply.replymsg);

  /* remove transaction from transaction table */
  rm_transrec(transrec);
}




/*
 * init_transreq()
 *
 * Parameters:
 *
 *   request  - requested transaction operation information record
 *   transrec - transaction table record
 *
 * If the transaction operation 'request' begins a new transaction or is the
 * next logical operation of an active transaction, place request in the ready
 * list of the transaction table, mark transaction operation state as ACTIVE,
 * and set 'transrec' to that transaction record.  Otherwise, respond to client
 * as appropriate.
 *
 * Note: This routine implements the transaction operation protocol discussed
 *       in pds/pds.h
 *
 *       Accepting the next logical operation of an active transaction
 *       implies that the reply message for the previous transaction
 *       operation is no longer needed; hence residual space allocated
 *       in the transaction table for retaining operation results is
 *       freed (e.g. the data buffer of a read operation).
 *
 * Returns:
 *
 *   TRUE  - initiate request; 'transrec' is transaction table record
 *           containing request.
 *   FALSE - no further action to be taken on this request
 */

#ifdef __STDC__
static int init_transreq(req_infot *request,
			 trans_entryt **transrec)
#else
static int init_transreq(request, transrec)
     req_infot *request;
     trans_entryt **transrec;
#endif
{
  int do_request;
  trans_entryt *ti_entry;
  pds_transidt req_transid;
  int req_transsn, prev_req_transsn;


#ifdef PDSPROFILE
  /* mark operation start time for profiling time accumulation */
  UTIL_clock_mark(&prof_clock);
#endif

  /* dereference request (transid, transsn) */

  req_transid = request->reqmsg.TransopHead.transid;
  req_transsn = request->reqmsg.TransopHead.transsn;

  /* locate transid in transaction table and dereference previous transsn */

  ti_entry = ti_lookup(req_transid, NOINSERT);

  if (ti_entry != NULL)
    prev_req_transsn = ti_entry->transop_req.reqmsg.TransopHead.transsn;


  /* check for transaction operation protocol error or re-sent request */

  do_request = FALSE;


  /* CASE I) handle special case ABORT operation */

  if (request->reqop == PDS_ABORT_OP)
    {
      /*   client requests a transaction abort.  as discussed in pds/pds.h,
       *   the abort operation can occur asynchronously, i.e. before the
       *   previous operation completes, and is not subject to the transaction
       *   operation protocol.
       */

      if (ti_entry == NULL)
	{ /* transid not active; conservatively assume transaction final
	   * state not logged, as discussed in similar cases below.
	   */

	  transreq_ack(request, PIOUS_ENOTLOG);
	}

      else
	{ /* transid active so abort */

	  if (ti_entry->transop_state != COMPLETED)
	    { /* complete previous transaction operation w/o sending reply */

	      /* note: initializing reply message for profiling */

	      ti_entry->transop_reply.replyop                      =
		ti_entry->transop_req.reqop;
	      ti_entry->transop_reply.replymsg.TransopHead.transid =
		req_transid;
	      ti_entry->transop_reply.replymsg.TransopHead.transsn =
		prev_req_transsn;
	      ti_entry->transop_reply.replymsg.TransopHead.rcode   =
		PIOUS_EABORT;

	      /* mark operation as completed */
	      complete_transop(ti_entry);

	      /* dealloc previous request; no reply storage to dealloc */
	      transreq_dealloc(&(ti_entry->transop_req));
	    }

	  else
	    { /* previous transaction operation already completed */

	      transreq_dealloc(&(ti_entry->transop_req));
	      transreply_dealloc(&(ti_entry->transop_reply));
	    }

	  /* insert abort request into transaction table */

	  ti_entry->transop_state        = ACTIVE;

	  ti_entry->transop_req.clientid = request->clientid;
	  ti_entry->transop_req.reqop    = request->reqop;
	  ti_entry->transop_req.reqmsg   = request->reqmsg;

	  /* time stamp request */
	  UTIL_clock_mark(&(ti_entry->transop_req.tstamp));

	  do_request = TRUE;
	}
    }


  /* CASE II) handle all other transaction operations */

  else if (req_transsn < 0)
    { /* protocol error; log and reply to client */
      transreq_ack(request, PIOUS_EPROTO);

      SS_errlog("pds_daemon", "init_transreq()", PIOUS_EPROTO,
		"transaction sequence number is invalid");
    }

  else if (ti_entry == NULL)
    {
      /* case: transid not active and transsn starts a new transaction
       *
       *   client requests the start of a new transaction, beginning with
       *   the requested operation.  insert an entry into the transaction
       *   table and flag that request is to be performed.
       */

      if (req_transsn == 0)
	{
	  ti_entry = ti_lookup(req_transid, INSERT);

	  if (ti_entry != NULL)
	    { /* ACTIVE entry allocated; set request fields appropriately */
	      ti_entry->transop_req.clientid = request->clientid;
	      ti_entry->transop_req.reqop    = request->reqop;
	      ti_entry->transop_req.reqmsg   = request->reqmsg;

	      /* time stamp request */
	      UTIL_clock_mark(&(ti_entry->transop_req.tstamp));

	      do_request = TRUE;
	    }

	  else
	    { /* insufficient resources to allocate entry; reply with abort
	       * to all operations except prepare and commit, as can
	       * consider these operations to have completed successfully.
	       */

	      if (request->reqop == PDS_PREPARE_OP)
		transreq_ack(request, PIOUS_READONLY);

#ifdef VTCOMMITNOACK
	      else if (request->reqop == PDS_COMMIT_OP)
		;
#else
	      else if (request->reqop == PDS_COMMIT_OP)
	        transreq_ack(request, PIOUS_OK);
#endif

	      else
		transreq_ack(request, PIOUS_EABORT);
	    }
	}

      /* case: transid not active and transsn does not begin transaction
       *
       *   one of the following has occured:
       *
       *     1) transaction previously completed (including read-only
       *        transaction that prepared) and reply message was lost,
       *
       *     2) transaction was aborted between client transaction operations
       *        as a result of checkpoint or dynamic recovery.
       *
       *     3) server died and recovered while client remained active, or
       *
       *     4) a transaction operation protocol error has occured.
       *
       *   can not distinguish cases so server response must be conservative
       *   to correctly handle all cases, as well as take in to consideration
       *   that an apparently lost reply message may actually be delayed and
       *   hence may be received prior to this reponse.
       *
       *   read{_sint}/write{_sint}/fa_sint
       *
       *                      - respond with PIOUS_EABORT.
       *
       *   prepare            - respond with PIOUS_EABORT, though this may have
       *                        been a read only transaction that completed
       *                        via an earlier prepare.
       *
       *   commit             - conservatively assume transaction final state
       *                        not logged; respond with PIOUS_ENOTLOG.
       *
       *
       */

      else
	{
	  if (request->reqop == PDS_COMMIT_OP)
	    transreq_ack(request, PIOUS_ENOTLOG);
	  else
	    transreq_ack(request, PIOUS_EABORT);
	}
    }

  else
    { /* ti_entry != NULL */

      /* case: transid active and transsn that of previous request
       *
       *   client re-sent request implying either timeout resulting from
       *   a delayed request or a lost reply from the server. if operation
       *   is delayed then discard request; if operation is completed then
       *   re-send reply.
       *
       *   Note: should probably check that requests are identical and signal
       *         a transaction operation protocol error (PIOUS_EPROTO) if
       *         they are not.
       */

      if (req_transsn == prev_req_transsn)
	{
	  if (ti_entry->transop_state == COMPLETED)
	    /* reply; inability to send is equivalent to a lost message */
	    PDSMSG_reply_send(request->clientid, request->reqop,
			      &(ti_entry->transop_reply.replymsg));
	}


      /* case: transid active and transsn next logical sequence number
       *
       *   client requests another transaction operation for a given
       *   transaction; update transaction table to reflect request.
       *
       *   Note: if the previous request has not been completed then a
       *         transaction operation protocol error has occured, implying
       *         a bug in the PDS daemon or the client library routines.
       *
       *         if the previous request has completed and is a prepare
       *         operation, then current request must be a commit operation
       *         or a 2PC protocol error has occured.
       */

      else if (req_transsn == prev_req_transsn + 1)
	{
	  if (ti_entry->transop_state == COMPLETED &&
	      (!ti_entry->prepared || request->reqop == PDS_COMMIT_OP))
	    { /* next operation OK and previous complete */

	      /* deallocate previous extra request/reply storage */
	      transreq_dealloc(&(ti_entry->transop_req));
	      transreply_dealloc(&(ti_entry->transop_reply));

	      /* insert new request into table entry and mark as ACTIVE */
	      ti_entry->transop_state        = ACTIVE;

	      ti_entry->transop_req.clientid = request->clientid;
	      ti_entry->transop_req.reqop    = request->reqop;
	      ti_entry->transop_req.reqmsg   = request->reqmsg;

	      /* time stamp request */
	      UTIL_clock_mark(&(ti_entry->transop_req.tstamp));

	      do_request = TRUE;
	    }
	  else
	    { /* protocol error; log and reply to client */
	      transreq_ack(request, PIOUS_EPROTO);

	      if (ti_entry->transop_state != COMPLETED)
		SS_errlog("pds_daemon", "init_transreq()", PIOUS_EPROTO,
			  "next transaction op sent before previous complete");
	      else
		SS_errlog("pds_daemon", "init_transreq()", PIOUS_EPROTO,
			  "operation other than commit followed prepare");
	    }
	}

      /* case: transid active and transsn is out of sequence
       *
       *   transaction operation sequence number is out of sequence;
       *   indicates a bug in the PDS daemon or client library routines.
       */

      else
	{ /* protocol error; log and reply to client */
	  transreq_ack(request, PIOUS_EPROTO);

	  SS_errlog("pds_daemon", "init_transreq()", PIOUS_EPROTO,
		    "transaction sequence number is invalid");
	}
    }


  if (do_request)
    { /* transaction operation is to be performed */
      *transrec = ti_entry;

#ifdef PDSPROFILE
      /* charge processing time to transaction operation */
      ti_entry->prof_opcum = UTIL_clock_delta(&prof_clock, UTIL_USEC);
#endif
    }

  return do_request;
}




/*
 * complete_transop()
 *
 * Parameters:
 *
 *   transrec - transaction table record
 *
 * Mark transaction operation as completed. If transaction is on the blocked
 * list, move to the ready list in the transaction table.
 *
 * NOTE: For profiling purposes, complete_transop() MUST be called after
 *       the transop_reply field is set in transrec.  To avoid counting
 *       reply transport time towards transaction operation processing
 *       time, complete_transop() must be called prior to sending
 *       reply to client.
 *
 * Returns:
 */ 

#ifdef __STDC__
static void complete_transop(trans_entryt *transrec)
#else
static void complete_transop(transrec)
     trans_entryt *transrec;
#endif
{
  /* if transaction blocked, move to ready list */
  if (transrec->transop_state == BLOCKED)
    { /* remove from blocked list */
      if (transrec->tblnext != NULL)
	transrec->tblnext->tblprev = transrec->tblprev;
      else
	transtable.block_tail = transrec->tblprev;

      if (transrec->tblprev != NULL)
	transrec->tblprev->tblnext = transrec->tblnext;
      else
	transtable.block_head = transrec->tblnext;

      /* insert into ready list */
      transrec->tblnext = transtable.ready;
      transrec->tblprev = NULL;
      transtable.ready  = transrec;
      if (transrec->tblnext != NULL)
	transrec->tblnext->tblprev = transrec;
    }

  /* mark operation as completed */
  transrec->transop_state = COMPLETED;

#ifdef PDSPROFILE
  /* charge processing time to transaction operation */
  transrec->prof_opcum += UTIL_clock_delta(&prof_clock, UTIL_USEC);

  /* output profile information */
  prof_print(transrec);
#endif
}




/*
 * block_transop()
 *
 * Parameters:
 *
 *   transrec - transaction table record
 *
 * Mark transaction operation as blocked.  If operation was not previously
 * blocked then move to the tail of the blocked list in the transaction table.
 *
 * NOTE: For profiling purposes, block_transop() MUST be called to
 *       re-block an already blocked operation so that processing time
 *       can be charged; the operation's position in the blocked list
 *       will not be effected.
 *
 * Returns:
 */ 

#ifdef __STDC__
static void block_transop(trans_entryt *transrec)
#else
static void block_transop(transrec)
     trans_entryt *transrec;
#endif
{
  /* if transaction not blocked, move to blocked list */
  if (transrec->transop_state != BLOCKED)
    { /* remove from ready list */
      if (transrec->tblnext != NULL)
	transrec->tblnext->tblprev = transrec->tblprev;

      if (transrec->tblprev != NULL)
	transrec->tblprev->tblnext = transrec->tblnext;
      else
	transtable.ready = transrec->tblnext;

      /* insert at tail of blocked list */
      transrec->tblnext = NULL;
      transrec->tblprev = transtable.block_tail;

      if (transrec->tblprev != NULL)
	transrec->tblprev->tblnext = transrec;
      else
	transtable.block_head = transrec;

      transtable.block_tail = transrec;

      /* mark transaction state as blocked */
      transrec->transop_state = BLOCKED;
    }

#ifdef PDSPROFILE
  /* charge processing time to transaction operation */
  transrec->prof_opcum += UTIL_clock_delta(&prof_clock, UTIL_USEC);
#endif
}


	  

/*
 * fcfs_conflict()
 *
 * Parameters:
 *
 *   transrec - transaction table record
 *
 * Determine if this transaction operation conflicts with any currently
 * blocked operations received prior to this operation.
 *
 * Fair scheduling and freedom from live-lock requires that conflicting lock
 * requests for overlapping data regions be satisfied in the order received.
 *
 * Returns:
 *
 *   FALSE (0) - there is NO scheduling conflict
 *   TRUE  (1) - there IS a scheduling conflict
 */ 

#ifdef __STDC__
static int fcfs_conflict(register trans_entryt *transrec)
#else
static int fcfs_conflict(transrec)
     register trans_entryt *transrec;
#endif
{
  int conflict;
  register trans_entryt *priorrec;

  conflict = FALSE;

  /* check ops on blocked list ahead of this op to determine if conflict */

  if (transrec->transop_state != COMPLETED)
    { /* determine portion of blocked list to search */
      
      if (transrec->transop_state == ACTIVE)
	priorrec = transtable.block_tail;
      else
	priorrec = transrec->tblprev;

      /* search towards head of list for conflicting operation */

      while (priorrec != NULL && !conflict)
	{
	  conflict =
	    ((!fhandle_eq(transrec->transop_req.aux.lk_fhandle,
			  priorrec->transop_req.aux.lk_fhandle)) ? FALSE :

	     (transrec->transop_req.aux.lk_type == PDS_READLK &&
	      priorrec->transop_req.aux.lk_type == PDS_READLK)   ? FALSE :

	     ((transrec->transop_req.aux.lk_stop <
	       priorrec->transop_req.aux.lk_start)  ||
	      (transrec->transop_req.aux.lk_start >
	       priorrec->transop_req.aux.lk_stop))               ? FALSE :

	     TRUE);

	  priorrec = priorrec->tblprev;
	}
    }

  return conflict;
}




/*
 * ti_lookup()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   action   - INSERT | NOINSERT
 *
 * Locate 'transid' in the transaction table. If transaction id not
 * found and INSERT, then create an ACTIVE table entry for transid.
 *
 * Returns:
 *
 *   trans_entryt * - pointer to trans_entry if transid found/inserted
 *   NULL           - transid not found/inserted
 */

#ifdef __STDC__
static trans_entryt *ti_lookup(pds_transidt transid,
			       int action)
#else
static trans_entryt *ti_lookup(transid, action)
     pds_transidt transid;
     int action;
#endif
{
  int index;
  register trans_entryt *ti_entry;

  /* perform hash function on transid */
  index = transid_hash(transid, TI_TABLE_SZ);

  ti_entry = ti_table[index];

  /* search appropriate transaction id hash chain for transid */
  while (ti_entry != NULL && !transid_eq(transid, ti_entry->transid))
    ti_entry = ti_entry->tinext;

  if (ti_entry == NULL && action == INSERT)
    { /* transid not located; insert into the transaction table */
      ti_entry = (trans_entryt *)malloc((unsigned)sizeof(trans_entryt));

      if (ti_entry != NULL)
	{ /* initialize entry; put in trans table and on transid hash chain */
	  ti_entry->transid                  = transid;
	  ti_entry->transop_state            = ACTIVE;
	  ti_entry->readlk                   = FALSE;
	  ti_entry->writelk                  = FALSE;
	  ti_entry->readonly                 = TRUE;
	  ti_entry->prepared                 = FALSE;

	  ti_entry->tblnext                  = transtable.ready;
	  ti_entry->tblprev                  = NULL;
	  transtable.ready                   = ti_entry;
	  if (ti_entry->tblnext != NULL)
	    ti_entry->tblnext->tblprev = ti_entry;

	  ti_entry->tinext                   = ti_table[index];
	  ti_entry->tiprev                   = NULL;
	  ti_table[index]                    = ti_entry;
	  if (ti_entry->tinext != NULL)
	    ti_entry->tinext->tiprev = ti_entry;

#ifdef PDSPROFILE
	  ti_entry->prof_opcum               = 0;
#endif
	}
    }

  return(ti_entry);
}




/*
 * rm_transrec()
 *
 * Parameters:
 *
 *   transrec - transaction table entry
 *
 * Remove and deallocate transaction table entry.
 *
 * Returns:
 */

#ifdef __STDC__
static void rm_transrec(trans_entryt *transrec)
#else
static void rm_transrec(transrec)
     trans_entryt *transrec;
#endif
{
  if (transrec != NULL)
    { /* deallocate extra request storage */
      transreq_dealloc(&(transrec->transop_req));

      /* deallocate extra reply storage, if extant (trans op completed) */
      if (transrec->transop_state == COMPLETED)
	transreply_dealloc(&(transrec->transop_reply));

      /* remove transrec from associated transaction id hash chain */

      if (transrec->tinext != NULL)
	transrec->tinext->tiprev = transrec->tiprev;

      if (transrec->tiprev != NULL)
	transrec->tiprev->tinext = transrec->tinext;
      else
	ti_table[transid_hash(transrec->transid, TI_TABLE_SZ)] =
	  transrec->tinext;

      /* remove transrec from transaction table */

      if (transrec->transop_state == BLOCKED)
	{ /* remove from blocked list */

	  if (transrec->tblnext != NULL)
	    transrec->tblnext->tblprev = transrec->tblprev;
	  else
	    transtable.block_tail = transrec->tblprev;

	  if (transrec->tblprev != NULL)
	    transrec->tblprev->tblnext = transrec->tblnext;
	  else
	    transtable.block_head = transrec->tblnext;
	}
      else
	{ /* remove from ready list */

	  if (transrec->tblnext != NULL)
	    transrec->tblnext->tblprev = transrec->tblprev;

	  if (transrec->tblprev != NULL)
	    transrec->tblprev->tblnext = transrec->tblnext;
	  else
	    transtable.ready = transrec->tblnext;
	}

      /* deallocate storage */
      free((char *)transrec);
    }
}




/*
 * transreq_ack()
 *
 * Parameters:
 *
 *   request - transaction operation request information record
 *   rcode   - result code
 *
 * Send client a null reply message for requested transaction operation that
 * has a result code 'rode'.
 *
 * Note: Assumes parameters are valid
 *
 * Returns:
 */

#ifdef __STDC__
static void transreq_ack(req_infot *request,
			 int rcode)
#else
static void transreq_ack(request, rcode)
     req_infot *request;
     int rcode;
#endif
{
  pdsmsg_replyt reply;

  /* set reply message header */
  reply.TransopHead.transid = request->reqmsg.TransopHead.transid;
  reply.TransopHead.transsn = request->reqmsg.TransopHead.transsn;
  reply.TransopHead.rcode   = rcode;

  /* set reply message body - NULL any expected pointers */
  switch (request->reqop)
    {
    case PDS_READ_OP:
      reply.ReadBody.buf = NULL;
      break;

    case PDS_READ_SINT_OP:
      reply.ReadsintBody.buf = NULL;
      break;
    }

  /* reply to client; inability to send is equivalent to a lost message */
  PDSMSG_reply_send(request->clientid, request->reqop, &reply);
}




/*
 * transreq_dealloc()
 *
 * Parameters:
 *
 *   request - transaction operation request information record
 *
 * Deallocate any extra space allocated to transaction operation request
 * 'request'.  Space for 'request' itself is NOT deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void transreq_dealloc(req_infot *request)
#else
static void transreq_dealloc(request)
     req_infot *request;
#endif
{ /* space to deallocate is dependent on the operation type */
  switch(request->reqop)
    {
    case PDS_WRITE_OP:
      /* deallocate write buffer */
      if (request->reqmsg.WriteBody.buf != NULL)
	{
	  free(request->reqmsg.WriteBody.buf);
	  request->reqmsg.WriteBody.buf = NULL;
	}
      break;

    case PDS_WRITE_SINT_OP:
      /* deallocate integer write buffer */
      if (request->reqmsg.WritesintBody.buf != NULL)
	{
	  free((char *)request->reqmsg.WritesintBody.buf);
	  request->reqmsg.WritesintBody.buf = NULL;
	}
      break;
    }
}




/*
 * transreply_dealloc()
 *
 * Parameters:
 *
 *   reply - transaction operation reply information record
 *
 * Deallocate any extra space allocated to transaction operation reply
 * 'reply'.  Space for 'reply' itself is NOT deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void transreply_dealloc(reply_infot *reply)
#else
static void transreply_dealloc(reply)
     reply_infot *reply;
#endif
{ /* space to deallocate is dependent on the operation type */
  switch(reply->replyop)
    {
    case PDS_READ_OP:
      /* deallocate read buffer */
      if (reply->replymsg.ReadBody.buf != NULL)
	{
	  free(reply->replymsg.ReadBody.buf);
	  reply->replymsg.ReadBody.buf = NULL;
	}
      break;

    case PDS_READ_SINT_OP:
      /* deallocate integer read buffer */
      if (reply->replymsg.ReadsintBody.buf != NULL)
	{
	  free((char *)reply->replymsg.ReadsintBody.buf);
	  reply->replymsg.ReadsintBody.buf = NULL;
	}
      break;
    }
}




#ifdef PDSPROFILE
/*
 * Private Function Definitions - Transaction Profiling Facilities
 */


/*
 * prof_init()
 *
 * Parameters:
 *
 *   logpath - log directory path
 *
 * Open transaction profiling file in directory 'logpath'.
 *
 * Returns:
 */

#ifdef __STDC__
static void prof_init(char *logpath)
#else
static void prof_init(logpath)
     char *logpath;
#endif
{
  unsigned long myhostid, myuid;
  int idwidth;
  char *prof_path;

  prof_stream = NULL;

  if (logpath != NULL)
    { /* determine user id and host id for differentiating profile files */

      myuid = (unsigned long)SYS_getuid();

      if (SYS_gethostid(&myhostid) == PIOUS_OK)
	{ /* Open file for writing transaction profiling information */

	  idwidth = sizeof(unsigned long) * CHAR_BIT / 4;

	  if ((prof_path = malloc((unsigned)(strlen(logpath) +
					     strlen(PROF_NAME) +
					     (2 * idwidth) +
					     4))) != NULL)
	    {
	      sprintf(prof_path, "%s/%s.%lx.%lx",
		      logpath, PROF_NAME, myuid, myhostid);

	      prof_stream = fopen(prof_path, "w");

	      free(prof_path);
	    }
	}
    }
}




/*
 * prof_print()
 *
 * Parameters:
 *
 *   transrec - transaction table record
 *
 * Output profiling information for most recently completed transaction
 * operation request in transaction table entry 'transrec'.
 *
 * Returns:
 */

#ifdef __STDC__
static void prof_print(trans_entryt *transrec)
#else
static void prof_print(transrec)
     trans_entryt *transrec;
#endif
{
  pds_transidt transid;
  int transsn;
  int replyop, rcode;
  unsigned long timecum;

  /* check for open profile stream and completed transaction operation */
  if (prof_stream != NULL && transrec->transop_state == COMPLETED)
    {
      transid = transrec->transop_reply.replymsg.TransopHead.transid;
      transsn = transrec->transop_reply.replymsg.TransopHead.transsn;
      replyop = transrec->transop_reply.replyop;
      rcode   = transrec->transop_reply.replymsg.TransopHead.rcode;
      timecum = transrec->prof_opcum;

      /* print profiling information */

      fprintf(prof_stream,
	      "\nTID/SN:\t(%12lu, %12ld, %12ld, %12ld) / (%12d)\n",
	      transid.hostid, transid.procid, transid.sec, transid.usec,
	      transsn);

      fprintf(prof_stream,
	      "OP/R/T:\t(%12d, %12d, %12ld)\n",
	      replyop, rcode, timecum);

      fflush(prof_stream);
    }
}
#endif
