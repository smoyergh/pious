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




/* PIOUS Service Coordinator (PSC): Message Exchange Routines
 *
 * @(#)psc_msg_exchange.h	2.2  28 Apr 1995  Moyer
 *
 * The psc_msg_exchange routines define request/reply message structures
 * and message exchange routines for implementing PSC remote procedure
 * calls.
 *
 * Function Summary:
 *
 * PSCMSG_req_send();
 * PSCMSG_req_recv();
 * PSCMSG_reply_send();
 * PSCMSG_reply_recv();
 *
 */




/* PSC operation codes and macros */


/* WARNING: Operation codes MUST be consecutive integers in the range
 *          0..PSC_OPCODE_MAX.  This is due to the manner in which operation
 *          codes are embedded in message tags in pdce/pdce_msgtagt.h; see
 *          discussion there.
 *
 *          If PSC_OPCODE_MAX changes then the file pdce/pdce_msgtagt.h MUST
 *          be updated accordingly.
 */

#define PSC_OPEN_OP         0    /* standard operations */
#define PSC_CLOSE_OP        1
#define PSC_CHMOD_OP        2
#define PSC_UNLINK_OP       3
#define PSC_MKDIR_OP        4
#define PSC_RMDIR_OP        5
#define PSC_CHMODDIR_OP     6
#define PSC_CONFIG_OP       7
#define PSC_SHUTDOWN_OP     8

#define PSC_SOPEN_OP        9    /* server version of standard operations */
#define PSC_SCLOSE_OP      10
#define PSC_SCHMOD_OP      11
#define PSC_SUNLINK_OP     12
#define PSC_SMKDIR_OP      13
#define PSC_SRMDIR_OP      14
#define PSC_SCHMODDIR_OP   15

#define PSC_OPCODE_MAX     15    /* maximum operation code */

/* valid PSC operation test macro */
#define PscOp(OPcode) ((OPcode) >= 0 && (OPcode) <= PSC_OPCODE_MAX)

/* valid PSC server operation test macro */
#define PscServerOp(OPcode) ((OPcode) >= 9 && (OPcode) <= PSC_OPCODE_MAX)




/* PSC Control Operation Request/Reply Message Structure Types */


typedef struct{

  /* common header information */

                              /* PDS info for "server" version of fns only */
  int pds_cnt;                /* PDS count */
  char **pds_hname;           /* PDS host names */
  char **pds_spath;           /* PDS search root paths */
  char **pds_lpath;           /* PDS log directory paths */

  /* request dependent body information */
  union{

    /* open request */
    struct{
      int view;               /* parafile view */
      int oflag;              /* parafile open flags */
      int faultmode;          /* parafile fault-tolerance mode */
      pious_modet mode;       /* parafile creation access permission bits */
      int seg;                /* parafile creation segment count */
      char *group;            /* process group */
      char *path;             /* path name */
    } open;

    /* close request */
    struct{
      char *group;            /* process group */
      char *path;             /* path name */
    } close;

    /* mkdir request */
    struct{
      pious_modet mode;       /* file access permission bits */
      char *path;             /* path name */
    } mkdir;

    /* rmdir request */
    struct{
      char *path;             /* path name */
    } rmdir;

    /* chmoddir request */
    struct{
      pious_modet mode;       /* file access permission bits */
      char *path;             /* path name */
    } chmoddir;

    /* unlink request */
    struct{
      char *path;             /* path name */
    } unlink;

    /* chmod request */
    struct{
      pious_modet mode;       /* file access permission bits */
      char *path;             /* path name */
    } chmod;

  } body;
} pscmsg_reqt;


typedef struct {

  /* common header information */
  long rcode;                    /* result code */

  /* reply dependent body information */
  union{

    /* open reply */
    struct{
      int pds_cnt;               /* parafile PDS count */
      int seg_cnt;               /* parafile data segment count */
      pds_fhandlet sptr_fhandle; /* parafile shared pointer file fhandle */
      pious_offt sptr_idx;       /* parafile shared pointer file index */
      pds_fhandlet *seg_fhandle; /* parafile data segment file handles */
      dce_srcdestt *pds_id;      /* parafile PDS message passing ids */
    } open;

    /* config reply */
    struct{
      int def_pds_cnt;           /* default PDS count */
    } config;

  } body;
} pscmsg_replyt;




/*
 * PSCMSG_req_send()
 *
 * Parameters:
 *
 *   reqop   - requested operation
 *   reqmsg  - request message
 *
 * Request PSC to perform operation 'reqop'; request parameters are defined
 * in request message 'reqmsg'.
 *
 * Note: 'reqmsg' parameters are presumed to be correct.
 *
 * Returns:
 *
 *   PIOUS_OK - PSCMSG_req_send() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - unable to locate PSC
 *       PIOUS_EINVAL   - invalid 'reqop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PSCMSG_req_send(int reqop,
		    pscmsg_reqt *reqmsg);
#else
int PSCMSG_req_send();
#endif




/*
 * PSCMSG_req_recv()
 *
 * Parameters:
 *
 *   clientid - client id
 *   reqop    - requested operation
 *   reqmsg   - request message
 *   timeout  - time-out period (in milliseconds)
 *
 * Receive a client request. Requesting client id is placed in 'clientid',
 * with requested operation identifier and arguments placed in 'reqop' and
 * 'reqmsg', respectively.
 *
 * PSCMSG_req_recv() is a blocking call with a time-out period of 'timeout'
 * milliseconds.  A negative 'timeout' value will cause the function to
 * block waiting without time-out.
 *
 * Returns:
 *
 *   PIOUS_OK - PSCMSG_req_recv() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETIMEOUT - function timed-out prior to completion
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PSCMSG_req_recv(dce_srcdestt *clientid,
		    int *reqop,
		    pscmsg_reqt *reqmsg,
		    int timeout);
#else
int PSCMSG_req_recv();
#endif




/*
 * PSCMSG_reply_send()
 *
 * Parameters:
 *
 *   clientid - clientid
 *   replyop  - replied operation
 *   replymsg - reply message
 *
 * Reply to client 'clientid' with result of operation 'replyop'; reply
 * arguments are defined in reply message 'replymsg'.
 *
 * Note: 'replymsg' parameters are presumed to be correct.
 *
 * Returns:
 *
 *   PIOUS_OK - PSCMSG_reply_send() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'clientid' argument
 *       PIOUS_EINVAL   - invalid 'replyop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PSCMSG_reply_send(dce_srcdestt clientid,
		      int replyop,
		      pscmsg_replyt *replymsg);
#else
int PSCMSG_reply_send();
#endif




/*
 * PSCMSG_reply_recv()
 *
 * Parameters:
 *
 *   replyop   - replied operation
 *   replymsg  - reply message
 *
 * Receive reply from PSC for operation 'replyop'; reply arguments are
 * placed in message 'replymsg'.
 *
 * Returns:
 *
 *   PIOUS_OK - PSCMSG_reply_recv() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - unable to locate PSC
 *       PIOUS_EINVAL   - invalid 'replyop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PSCMSG_reply_recv(int replyop,
		      pscmsg_replyt *replymsg);
#else
int PSCMSG_reply_recv();
#endif
