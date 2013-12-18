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




/* PIOUS Data Server (PDS): Message Exchange Routines
 *
 * @(#)pds_msg_exchange.h	2.2  28 Apr 1995  Moyer
 *
 * The pds_msg_exchange routines define request/reply message structures
 * and message exchange routines for implementing PDS remote procedure
 * calls.
 *
 * Function Summary:
 *
 * PDSMSG_req_send();
 * PDSMSG_req_recv();
 * PDSMSG_reply_send();
 * PDSMSG_reply_recv();
 *
 */




/* PDS transaction/control operation codes and macros */


/* WARNING: Operation codes MUST be consecutive integers in the range
 *          0..PDS_OPCODE_MAX.  This is due to the manner in which operation
 *          codes are embedded in message tags in pdce/pdce_msgtagt.h; see
 *          discussion there.
 *
 *          If PDS_OPCODE_MAX changes then the file pdce/pdce_msgtagt.h must
 *          be updated accordingly.
 */

#define PDS_READ_OP         0    /* transaction operations */
#define PDS_WRITE_OP        1
#define PDS_READ_SINT_OP    2
#define PDS_WRITE_SINT_OP   3
#define PDS_FA_SINT_OP      4
#define PDS_PREPARE_OP      5
#define PDS_COMMIT_OP       6
#define PDS_ABORT_OP        7

#define PDS_LOOKUP_OP       8    /* control operations */
#define PDS_CACHEFLUSH_OP   9
#define PDS_MKDIR_OP       10
#define PDS_RMDIR_OP       11
#define PDS_UNLINK_OP      12
#define PDS_CHMOD_OP       13
#define PDS_STAT_OP        14
#define PDS_PING_OP        15
#define PDS_RESET_OP       16
#define PDS_SHUTDOWN_OP    17

#define PDS_OPCODE_MAX     17    /* maximum operation code */

/* valid PDS operation test macro */
#define PdsOp(OPcode) ((OPcode) >= 0 && (OPcode) <= PDS_OPCODE_MAX)

/* valid transaction operation test macro */
#define PdsTransop(OPcode) ((OPcode) >= 0 && (OPcode) <= 7)

/* valid control operation test macro */
#define PdsControlop(OPcode) ((OPcode) >= 8 && (OPcode) <= PDS_OPCODE_MAX)




/* PDS Transaction Operation Request/Reply Message Structures */


struct PDSMSG_transop_req{

  /* common header information */
  pds_transidt transid;       /* transaction id */
  int transsn;                /* transaction sequence number */

  /* request dependent body information */
  union{

    /* read request */
    struct{
      pds_fhandlet fhandle;   /* file handle */
      pious_offt offset;      /* file offset */
      pious_sizet nbyte;      /* byte count */
      int lock;               /* lock type */
    } read;

    /* write request */
    struct{
      pds_fhandlet fhandle;   /* file handle */
      pious_offt offset;      /* file offset */
      pious_sizet nbyte;      /* byte count */
      char *buf;              /* write data buffer */
    } write;

    /* read signed int request */
    struct{
      pds_fhandlet fhandle;   /* file handle */
      pious_offt offset;      /* file offset */
      int nint;               /* int count */
    } read_sint;

    /* write signed int request */
    struct{
      pds_fhandlet fhandle;   /* file handle */
      pious_offt offset;      /* file offset */
      int nint;               /* int count */
      long *buf;              /* write data buffer */
    } write_sint;

    /* fetch & add signed int request */
    struct{
      pds_fhandlet fhandle;   /* file handle */
      pious_offt offset;      /* file offset */
      long increment;         /* increment */
    } fa_sint;

  } body;
};


struct PDSMSG_transop_reply{

  /* common header information */
  pds_transidt transid;       /* transaction id */
  int transsn;                /* transaction sequence number */
  long rcode;                 /* result code */

  /* reply dependent body information */
  union{

    /* read reply */
    struct{
      char *buf;              /* data buffer */
    } read;

    /* read signed int reply */
    struct{
      long *buf;              /* data buffer */
    } read_sint;

    /* fetch & add signed int reply */
    struct{
      long rvalue;            /* result value */
    } fa_sint;

  } body;
};




/* PDS Control Operation Request/Reply Message Structures */


struct PDSMSG_cntrlop_req{

  /* common header information */
  int cmsgid;                 /* control operation id */

  /* request dependent body information */
  union{

    /* lookup request */
    struct{
      int cflag;              /* file creation flag */
      pious_modet mode;       /* file creation access permission bits */
      char *path;             /* path name */
    } lookup;

    /* mkdir request */
    struct{
      pious_modet mode;       /* file access permission bits */
      char *path;             /* path name */
    } mkdir;

    /* rmdir request */
    struct{
      char *path;             /* path name */
    } rmdir;

    /* unlink request */
    struct{
      char *path;             /* path name */
    } unlink;

    /* chmod request */
    struct{
      pious_modet mode;       /* file access permission bits */
      char *path;             /* path name */
    } chmod;

    /* stat request */
    struct{
      char *path;             /* path name */
    } stat;

  } body;
};


struct PDSMSG_cntrlop_reply{

  /* common header information */
  int cmsgid;                 /* control operation id */
  long rcode;                 /* result code */

  /* reply dependent body information */
  union{

    /* lookup reply */
    struct{
      pds_fhandlet fhandle;   /* returned file handle */
      int amode;              /* returned file accessability */
    } lookup;

    /* chmod reply */
    struct{
      int amode;              /* returned file accessability */
    } chmod;

    /* stat reply */
    struct{
      pious_modet mode;       /* returned file mode */
    } stat;

  } body;
};




/* PDS Request/Reply Message Types */


typedef union{
  struct PDSMSG_transop_req transop;      /* transaction operation request */
  struct PDSMSG_cntrlop_req cntrlop;      /* control operation request */
} pdsmsg_reqt;


typedef union{
  struct PDSMSG_transop_reply transop;    /* transaction operation reply */
  struct PDSMSG_cntrlop_reply cntrlop;    /* control operation request */
} pdsmsg_replyt;




/* Macros for accessing transaction operation message components */

#define TransopHead    transop

#define ReadHead       transop
#define ReadBody       transop.body.read

#define WriteHead      transop
#define WriteBody      transop.body.write

#define ReadsintHead   transop
#define ReadsintBody   transop.body.read_sint

#define WritesintHead  transop
#define WritesintBody  transop.body.write_sint

#define FasintHead     transop
#define FasintBody     transop.body.fa_sint

#define PrepareHead    transop

#define CommitHead     transop

#define AbortHead      transop


/* Macros for accessing control operation message components */

#define CntrlopHead    cntrlop

#define LookupHead     cntrlop
#define LookupBody     cntrlop.body.lookup

#define CacheflushHead cntrlop

#define MkdirHead      cntrlop
#define MkdirBody      cntrlop.body.mkdir

#define RmdirHead      cntrlop
#define RmdirBody      cntrlop.body.rmdir

#define UnlinkHead     cntrlop
#define UnlinkBody     cntrlop.body.unlink

#define ChmodHead      cntrlop
#define ChmodBody      cntrlop.body.chmod

#define StatHead       cntrlop
#define StatBody       cntrlop.body.stat

#define PingHead       cntrlop

#define ResetHead      cntrlop

#define ShutdownHead   cntrlop




/*
 * PDSMSG_req_send()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   reqop   - requested operation
 *   reqmsg  - request message
 *   vbuf    - vector buffer management information
 *
 * Request PDS 'pdsid' to perform operation 'reqop'; request parameters
 * are defined in request message 'reqmsg'.
 *
 * For regular byte-oriented write requests, i.e. reqop == PDS_WRITE_OP,
 * 'vbuf' must define the vector buffer descriptor for extracting potentially
 * non-contiguous write data from the user buffer.
 *
 * For integer-oriented write requests, i.e. reqop == PDS_WRITE_SINT_OP,
 * 'vbuf' must define a contiguous buffer via 'vbuf->firstblk_ptr'; all other
 * vector buffer descriptor parameters are ignored.
 *
 * Vector buffer descriptor format is defined in pds/pds.h.
 *
 * Note: 'reqmsg' and 'vbuf' parameters are presumed to be correct.
 *
 * Returns:
 *
 *   PIOUS_OK - PDSMSG_req_send() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'reqop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDSMSG_req_send(dce_srcdestt pdsid,
		    int reqop,
		    pdsmsg_reqt *reqmsg,
		    struct PDS_vbuf_dscrp *vbuf);
#else
int PDSMSG_req_send();
#endif




/*
 * PDSMSG_req_recv()
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
 * PDSMSG_req_recv() is a blocking call with a time-out period of 'timeout'
 * milliseconds.  A negative 'timeout' value will cause the function to
 * block waiting without time-out.
 *
 * Returns:
 *
 *   PIOUS_OK - PDSMSG_req_recv() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETIMEOUT - function timed-out prior to completion
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDSMSG_req_recv(dce_srcdestt *clientid,
		    int *reqop,
		    pdsmsg_reqt *reqmsg,
		    int timeout);
#else
int PDSMSG_req_recv();
#endif




/*
 * PDSMSG_reply_send()
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
 *   PIOUS_OK - PDSMSG_reply_send() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'clientid' argument
 *       PIOUS_EINVAL   - invalid 'replyop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDSMSG_reply_send(dce_srcdestt clientid,
		      int replyop,
		      pdsmsg_replyt *replymsg);
#else
int PDSMSG_reply_send();
#endif




/*
 * PDSMSG_reply_recv()
 *
 * Parameters:
 *
 *   pdsid     - PDS id
 *   replyop   - replied operation
 *   replymsg  - reply message
 *   vbuf      - vector buffer management information
 *
 * Receive reply from PDS 'pdsid' for operation 'replyop'; reply arguments
 * are placed in message 'replymsg'.
 *
 * For regular byte-oriented read replies, i.e. replyop == PDS_READ_OP,
 * 'vbuf' must define the vector buffer descriptor for filling potentially
 * non-contiguous regions of a user buffer.
 *
 * For integer-oriented read replies, i.e. replyop == PDS_READ_SINT_OP,
 * 'vbuf' must define a contiguous buffer via 'vbuf->firstblk_ptr'; all other
 * vector buffer descriptor parameters are ignored.
 *
 * Vector buffer descriptor format is defined in pds/pds.h.
 *
 * For both read types, 'vbuf' must also define the expected values for transid
 * and transsn.  Data is placed in the user buffer as specified only if the
 * reply message matches in (transid, transsn); otherwise, the read data is
 * discarded and the function returns a value of PIOUS_EPERM with the
 * 'replymsg' fields set to the values received.
 *
 * Utilizing 'vbuf' is an optimization that allows data in the reply message
 * to be received directly into the user buffer without copying.
 *
 * Returns:
 *
 *   PIOUS_OK - PDSMSG_reply_recv() completed without error
 *   <  0     - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'replyop' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPERM    - read data not received; (transid, transsn) mismatch
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDSMSG_reply_recv(dce_srcdestt pdsid,
		      int replyop,
		      pdsmsg_replyt *replymsg,
		      struct PDS_vbuf_dscrp *vbuf);
#else
int PDSMSG_reply_recv();
#endif
