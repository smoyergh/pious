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




/* PIOUS Data Server (PDS): Remote Procedure Call Interface
 *
 * @(#)pds.h	2.2  28 Apr 1995  Moyer
 *
 *
 * INTRODUCTION:
 *
 *   The pds module provides a remote procedure call interface to operations
 *   provided by the PDS daemon.  Client requests fall into two catagories:
 *   transaction operations and control operations.
 *
 *   Transaction operations, as the name implies, are operations performed
 *   within the context of a transaction, and hence are guaranteed to be atomic
 *   with respect to concurrency and (if specified) failures. PDS daemon
 *   operate under a Two-Phase Commit protocol (2PC), allowing multiple PDS
 *   daemons to participate in a distributed transaction.
 *
 *   Control operations do not operate within the context of a transaction and
 *   are provided to query and/or alter the PDS operational state.
 *
 *
 * TRANSACTION TYPES:
 *
 *   A PDS supports two distinct transaction types: stable and volatile.
 *   A stable transaction is a "traditional" transaction requiring data
 *   coherence be maintained in the event of a system failure. A volatile
 *   transaction is a "light-weight" transaction that does not require
 *   fault-tolerance; only serializability of access must be guaranteed.
 *   Providing two distinct transaction types allows a client to choose
 *   between fault-tolerance, via stable transactions, and high-performance,
 *   via volatile transactions; serializability of access is provided in
 *   either case.
 *
 *   Distinguishing transaction types is simple: stable transactions are
 *   those which have been prepared, i.e. have issued a prepare operation
 *   to log transaction updates.
 *
 *   It is easily proven that 2PC (preparing) is not required for volatile
 *   transactions:
 *
 *   Lemma:
 *     A PDS never aborts non-blocked (completed) transaction operations.
 *
 *   Lemma:
 *     A non-logging prepare is a no-op that by definition succeeds.
 *
 *   Corollary:
 *     If all transaction operations have completed, a non-logging prepare
 *     operation is guaranteed to succeed.
 *
 *   Theorem:
 *     Volatile transactions, not requring fault-tolerance via logging, do not
 *     need to be prepared, i.e. do not need to perform 2PC.
 *
 *   Hence, a client makes a transaction stable by issuing a prepare operation.
 *   Otherwise, the transaction is volatile.
 *
 *
 * USAGE:
 *
 *   The following demonstrates the generic PDS usage for performing stable
 *   and volatile transactions:
 *
 *   Volatile:
 *
 *     begin transaction (implied)
 *       PDS_read/write() 
 *       PDS_read/write()
 *            ...
 *       PDS_read/write()
 *
 *       PDS_commit()
 *     end transaction (implied)
 *
 *   Stable:
 *
 *     begin transaction (implied)
 *       PDS_read/write() 
 *       PDS_read/write()
 *            ...
 *       PDS_read/write()
 *
 *       PDS_prepare()
 *       PDS_commit/abort()
 *     end transaction (implied)
 *
 *
 *   In performing a transaction, individual read/write operations may fail
 *   for various reasons, e.g. attempting to write to a file without
 *   sufficient permission.  Such a failure does NOT abort the transaction.
 *   If a client wishes to abort a transaction because of the result of
 *   a particular operation, an abort operation must be performed.
 *
 *   Note that the PDS implements a common 2PC optimization for stable
 *   transactions: the read-only optimization.
 *   
 *   The read-only optimization acknowledges the fact that if data is not
 *   updated at a given PDS, then after performing a prepare operation
 *   a commit is a no-op and need not be performed.  Thus a prepare operation
 *   can return a result of PIOUS_READONLY, indicating both a vote to commit
 *   and acknowledging that a commit/abort operation is not expected for the
 *   transaction.
 *
 *
 * MIXING TRANSACTION TYPES:
 *
 *   Transaction types can be mixed; i.e. a stable transaction can access
 *   data written by a volatile transaction.  However, this can easily lead
 *   to semantics different than those a client mostly likely expects.
 *   For example, client A might write to a file via a volatile
 *   transaction.  Client B might then read the data written by A and,
 *   based on the value, make an update within a stable transaction.
 *   Subsequent system failure and recovery would then guarantee that the
 *   write of client B would be reflected in the restored file state, but no
 *   such guarantee holds for the write of client A that client B's update
 *   was based on.
 *
 *   There is no reasonable way for a PDS to prevent such a sequence of
 *   events from occuring.  Furthermore, it is not clear that it is
 *   necessary in all cases to prevent the mixing of transaction types;
 *   perhaps an application expects this, or perhaps the semantics of the
 *   result will not be effected.
 *
 *   Regardless, it is the responsibility of a higher-level mechanism to
 *   prevent the mixing of transaction types if the side-effects are
 *   considered undesireable.
 *
 *
 * OPERATION PROTOCOLS:
 *
 *   Transaction operation protocol:
 *
 *   Transaction operations are subject to a simple transaction request
 *   protocol implemented by a PDS daemon and its clients.  Each transaction
 *   is assigned a unique id (transid), and each operation within a transaction
 *   is given a consecutive sequence number (transsn), with zero (0) indicating
 *   the beginning of a transaction. A particular transaction operation must
 *   be completed before another can be requested, or a protocol violation
 *   occurs.  Sequencing transaction operations in this manner allows for the
 *   detection of client/server crashes and restarts and provides a means for
 *   dealing with failures in the underlying message system.  Further protocol
 *   details are provided in pds_daemon.c.
 *
 *   Note that the ABORT operation is special in that it is not subject to
 *   the transaction operation protocol.  An abort operation may be performed
 *   prior to the completion of the previous transaction operation and does
 *   not require a transaction sequence number.
 *
 *   For performance, the PDS RPC interface implements non-blocking
 *   request/blocking receive calls, allowing a client to have one outstanding
 *   transaction request at each PDS daemon.
 *
 *
 *   Control operation protocol:
 *
 *   Control operations are tagged with a client supplied identifier (cmsgid),
 *   again for fault-tolerance.  A client is allowed one outstanding control
 *   operation of a given type at any PDS, with results of that operation
 *   type distinguished via the client supplied operation id.  Control
 *   operations are idempotent and thus may be repeated, though the result
 *   code may differ from one execution of the operation to the next; e.g. a
 *   request to unlink a file may be repeated, however after the first
 *   execution the result code will indicate that the file to unlink does
 *   not exist.
 *
 *   Resulting Semantics:
 *
 *   A given client thread of control can have at most one outstanding
 *   transaction operation and one outstanding control operation of each type
 *   per PDS daemon, as discussed above.  Thus when receiving server results,
 *   the PDS interface routines may discard messages from the specified
 *   PDS destined for the calling client thread that do not match in
 *   (transid, transsn), for transaction operations, or (operation, cmsgid),
 *   for control operations.  If the client thread is obeying the protocol,
 *   such messages are no longer of interest.
 *
 *
 * TRANSACTION ID'S:
 *
 *   As discussed above, each transaction must be assigned a unique transid;
 *   this is done via the function transid_assign() from the pds_transidt
 *   abstract data type.  The single exception is a transaction that is
 *   timed-out by the PDS due to the inability to obtain the required lock,
 *   usually as the result of deadlock.  Such a transaction should be
 *   re-tried with the same transid; doing so will result in a higher
 *   priority for that transaction.  A transid should not be reused in
 *   any other case, as doing so will degrade the performance of the PDS
 *   scheduling algorithm.
 *
 *
 * FILE HANDLES:
 *
 *   File access operations are performed using the file handle returned
 *   by the PDS_lookup() function.  File handles provided by a PDS will not
 *   go stale during PDS operation, but will become stale when the PDS is
 *   shutdown and later restarted.  Thus PDS clients do not benefit from
 *   caching file handles between PDS instantiations.
 *
 *
 * VECTOR BUFFER DESCRIPTORS:
 *
 *   Regular byte-oriented read and write functions take as a parameter a
 *   buffer descriptor for accessing potentially non-contiguous regions of
 *   a user buffer, allowing these regions to be read/written via a single
 *   PDS_read()/PDS_write() call and without extraneous data copying.
 *
 *   The buffer descriptor defines a regular vector of fixed size byte blocks
 *   to be accessed at a fixed stride.  Access begins at a specified
 *   offset in the first block and, after accessing up to the end of the
 *   first block, continues using the specified block size and stride until
 *   all required data has been read/written.  Thus the user buffer is accessed
 *   in fixed size blocks, at a fixed block stride, excepting potentially the
 *   first and last block accessed.
 *
 *   To illustrate this concept, the following diagram shows a vector of byte
 *   blocks accessed at a stride of 2.  The first block is accessed beginning
 *   at the specified offset.  The total number of bytes to be read/written is
 *   such that 4 byte blocks are accessed, with the last byte read/written
 *   occuring somewhere prior to the end of the fourth block.
 *
 *                   |--- effective access range ---|
 *
 *                 +----+----+----+----+----+----+----+
 *                 | B0 |    | B1 |    | B2 |    | B3 |
 *                 +----+----+----+----+----+----+----+
 *                   ^                              ^
 *                   |                              |__ last byte accessed
 *                   |
 *                   |__ specified first block offset; i.e. first byte accessed
 *
 *
 *-----------------------------------------------------------------------------
 *
 *
 * Transaction Operation Summary:
 *
 * PDS_read{_send, _recv}();
 * PDS_write{_send, _recv}();
 * PDS_read_sint{_send, _recv}();
 * PDS_write_sint{_send, _recv}();
 * PDS_fa_sint{_send, _recv}();
 * PDS_prepare{_send, _recv}();
 * PDS_commit{_send, _recv}();
 * PDS_abort{_send, _recv}();
 *
 * Control Operation Summary:
 *
 * PDS_lookup{_send, _recv}();
 * PDS_cacheflush{_send, _recv}();
 * PDS_mkdir{_send, _recv}();
 * PDS_rmdir{_send, _recv}();
 * PDS_unlink{_send, _recv}();
 * PDS_chmod{_send, _recv}();
 * PDS_stat{_send, _recv}();
 * PDS_ping{_send, _recv}();
 * PDS_reset{_send, _recv}();
 * PDS_shutdown{_send, _recv}();
 *
 *
 * A detailed description of each of the above operations is presented below.
 * Note that most operations have both a blocking and (pseudo) non-blocking
 * version. Descriptions apply to the blocking version of each operation;
 * non-blocking request/blocking receive versions are the obvious extensions.
 */




/* Read/Write user vector buffer descriptor */

struct PDS_vbuf_dscrp{
  pious_sizet blksz;            /* vector buffer block size */
  pious_offt stride;            /* vector buffer access stride */
  char *firstblk_ptr;           /* ptr into first block for initial access */
  pious_sizet firstblk_netsz;   /* net first block size from ptr to end */

  /* the following parameters are for system use only */

  pds_transidt transid;         /* trans id              - read reply only */
  int transsn;                  /* trans sequence number - read reply only */
};




/*
 * PDS_read()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *   fhandle - file handle
 *   offset  - starting offset
 *   nbyte   - byte count
 *   lock    - lock type; PDS_READLK or PDS_WRITELK
 *   vbuf    - vector buffer
 *
 * Read file 'fhandle' starting at 'offset' bytes from the beginning
 * and proceeding for 'nbyte' bytes; place results in buffer 'vbuf'.
 *
 * Returns: PDS_read(), PDS_read_recv()
 *
 *   >= 0 - number of bytes read and returned (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted
 *       PIOUS_EBADF    - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES   - read is invalid access mode for 'fhandle'
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - 'offset', 'nbyte', or 'vbuf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_read_send()
 *
 *   PIOUS_OK (0) - PDS_read_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#define PDS_READLK    0  /* lock type symbolic constants */
#define PDS_WRITELK   1


#ifdef __STDC__
pious_ssizet PDS_read(dce_srcdestt pdsid,
		      pds_transidt transid,
		      int transsn,
		      pds_fhandlet fhandle,
		      pious_offt offset,
		      pious_sizet nbyte,
		      int lock,
		      struct PDS_vbuf_dscrp *vbuf);

int PDS_read_send(dce_srcdestt pdsid,
		  pds_transidt transid,
		  int transsn,
		  pds_fhandlet fhandle,
		  pious_offt offset,
		  pious_sizet nbyte,
		  int lock);

pious_ssizet PDS_read_recv(dce_srcdestt pdsid,
			   pds_transidt transid,
			   int transsn,
			   struct PDS_vbuf_dscrp *vbuf);
#else
pious_ssizet PDS_read();

int PDS_read_send();

pious_ssizet PDS_read_recv();
#endif




/*
 * PDS_write()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *   fhandle - file handle
 *   offset  - starting offset
 *   nbyte   - byte count
 *   vbuf    - vector buffer
 *
 * Write file 'fhandle' starting at 'offset' bytes from the beginning
 * and proceeding for 'nbyte' bytes the data in buffer 'vbuf'.
 *
 * Returns: PDS_write(), PDS_write_recv()
 *
 *   >= 0 - number of bytes written (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted
 *       PIOUS_EBADF    - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES   - write is invalid access mode for 'fhandle'
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - 'offset', 'nbyte', or 'vbuf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_write_send()
 *
 *   PIOUS_OK (0) - PDS_write_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'nbyte' or 'vbuf' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
pious_ssizet PDS_write(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       pds_fhandlet fhandle,
		       pious_offt offset,
		       pious_sizet nbyte,
		       struct PDS_vbuf_dscrp *vbuf);

int PDS_write_send(dce_srcdestt pdsid,
		   pds_transidt transid,
		   int transsn,
		   pds_fhandlet fhandle,
		   pious_offt offset,
		   pious_sizet nbyte,
		   struct PDS_vbuf_dscrp *vbuf);

pious_ssizet PDS_write_recv(dce_srcdestt pdsid,
			    pds_transidt transid,
			    int transsn);
#else
pious_ssizet PDS_write();

int PDS_write_send();

pious_ssizet PDS_write_recv();
#endif




/*
 * PDS_read_sint()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *   fhandle - file handle
 *   offset  - starting offset
 *   nint    - integer count
 *   buf     - integer buffer
 *
 * Read 'nint' integer values from file 'fhandle' starting at index position
 * 'offset', placing result in 'buf'.
 *
 * PDS_read_sint() treats a file as an integer array indexed starting at 0.
 * The primary function is to work in conjunction with PDS_fa_sint().
 *
 * Returns: PDS_read_sint(), PDS_read_sint_recv()
 *
 *   >= 0 - number of integers read and returned (<= nint)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted
 *       PIOUS_EBADF    - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES   - read is invalid access mode for 'fhandle'
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - 'offset', 'nint', or 'buf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_read_sint_send()
 *
 *   PIOUS_OK (0) - PDS_read_sint_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_read_sint(dce_srcdestt pdsid,
		  pds_transidt transid,
		  int transsn,
		  pds_fhandlet fhandle,
		  pious_offt offset,
		  int nint,
		  long *buf);

int PDS_read_sint_send(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       pds_fhandlet fhandle,
		       pious_offt offset,
		       int nint);

int PDS_read_sint_recv(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       long *buf);
#else
int PDS_read_sint();

int PDS_read_sint_send();

int PDS_read_sint_recv();
#endif




/*
 * PDS_write_sint()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *   fhandle - file handle
 *   offset  - starting offset
 *   nint    - integer count
 *   buf     - integer buffer
 *
 * Write 'nint' integers from 'buf' to file 'fhandle' starting at index
 * position 'offset'.
 *
 * PDS_write_sint() treats a file as an integer array indexed starting at 0.
 * The primary function is to work in conjunction with PDS_fa_sint().
 *
 * Returns: PDS_write_sint(), PDS_write_sint_recv()
 *
 *   >= 0 - number of integers written (<= nint)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted
 *       PIOUS_EBADF    - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES   - write is invalid access mode for 'fhandle'
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - 'offset', 'nint', or 'buf' argument not a proper
 *                        value or exceeds PIOUS system constraints
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_write_sint_send()
 *
 *   PIOUS_OK (0) - PDS_write_sint_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_write_sint(dce_srcdestt pdsid,
		   pds_transidt transid,
		   int transsn,
		   pds_fhandlet fhandle,
		   pious_offt offset,
		   int nint,
		   long *buf);

int PDS_write_sint_send(dce_srcdestt pdsid,
			pds_transidt transid,
			int transsn,
			pds_fhandlet fhandle,
			pious_offt offset,
			int nint,
			long *buf);

int PDS_write_sint_recv(dce_srcdestt pdsid,
			pds_transidt transid,
			int transsn);
#else
int PDS_write_sint();

int PDS_write_sint_send();

int PDS_write_sint_recv();
#endif




/*
 * PDS_fa_sint() (fetch and add signed int)
 *
 * Parameters:
 *
 *   pdsid     - PDS id
 *   transid   - transaction id
 *   transsn   - transaction sequence number
 *   fhandle   - file handle
 *   offset    - integer array index
 *   increment - increment
 *   rvalue    - return value
 *
 * Read integer value from file 'fhandle' at index position 'offset',
 * increment value by 'increment', and write result back to the file at
 * position read. Return value of accessed integer prior to update
 * in 'rvalue'.
 *
 * PDS_fa_sint() is an optimization for the extremely common case that a
 * shared integer value is updated by many competing processes.  The file
 * is treated as an integer array indexed starting at 0, thus avoiding
 * the problem of different integer sizes in a heterogeneous environment.
 *
 * PIOUS uses this function to implement shared file pointers.
 *
 * Returns: PDS_fa_sint(), PDS_fa_sint_recv()
 *
 *   PIOUS_OK (0) - value of accessed integer, prior to increment, is returned
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted
 *       PIOUS_EBADF    - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES   - read/write is invalid access mode for 'fhandle'
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - improper 'rvalue', 'offset', or 'increment' argument
 *       PIOUS_EINSUF   - insufficient system resources; retry operation
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_fa_sint_send()
 *
 *   PIOUS_OK (0) - PDS_fa_sint_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_fa_sint(dce_srcdestt pdsid,
		pds_transidt transid,
		int transsn,
		pds_fhandlet fhandle,
		pious_offt offset,
		long increment,
		long *rvalue);

int PDS_fa_sint_send(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn,
		     pds_fhandlet fhandle,
		     pious_offt offset,
		     long increment);

int PDS_fa_sint_recv(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn,
		     long *rvalue);
#else
int PDS_fa_sint();

int PDS_fa_sint_send();

int PDS_fa_sint_recv();
#endif




/*
 * PDS_prepare()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *
 * Prepare to commit transaction 'transid'.
 *
 * Returns: PDS_prepare(), PDS_prepare_recv()
 *
 *   PIOUS_READONLY - transaction prepared to commit; vote to commit
 *   PIOUS_OK (0)   - transaction prepared to commit; vote to commit
 *   <  0           - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - transaction is aborted; vote to abort
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Note: In the event of an error, other than PIOUS_EABORT, must presume
 *       a vote to abort and perform an abort operation at this PDS.
 *
 * Returns: PDS_prepare_send()
 *
 *   PIOUS_OK (0) - PDS_prepare_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_prepare(dce_srcdestt pdsid,
		pds_transidt transid,
		int transsn);

int PDS_prepare_send(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn);

int PDS_prepare_recv(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn);
#else
int PDS_prepare();

int PDS_prepare_send();

int PDS_prepare_recv();
#endif




/*
 * PDS_commit()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *   transsn - transaction sequence number
 *
 * Commit transaction 'transid'; transaction MUST have previously been
 * prepared for committement via PDS_prepare() or will NOT be recoverable.
 *
 * Returns: PDS_commit(), PDS_commit_recv()
 *
 *   PIOUS_OK (0) - transaction committed without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_ENOTLOG  - commit action not logged
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Note: In the event of an error, other than PIOUS_ENOTLOG, must presume
 *       transaction result (commit) not logged for prepared transaction.
 *
 * Returns: PDS_commit_send()
 *
 *   PIOUS_OK (0) - PDS_commit_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_commit(dce_srcdestt pdsid,
	       pds_transidt transid,
	       int transsn);

int PDS_commit_send(dce_srcdestt pdsid,
		    pds_transidt transid,
		    int transsn);

int PDS_commit_recv(dce_srcdestt pdsid,
		    pds_transidt transid,
		    int transsn);
#else
int PDS_commit();

int PDS_commit_send();

int PDS_commit_recv();
#endif




/*
 * PDS_abort()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   transid - transaction id
 *
 * Abort transaction 'transid', undoing any effects.
 *
 * Returns: PDS_abort(), PDS_abort_recv()
 *
 *   PIOUS_OK (0) - transaction aborted without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_ENOTLOG  - abort action not logged
 *       PIOUS_EUNXP    - unexpected error encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Note: In the event of an error, other than PIOUS_ENOTLOG, must presume
 *       transaction result (abort) not logged for prepared transaction.
 *
 * Returns: PDS_abort_send()
 *
 *   PIOUS_OK (0) - PDS_abort_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_abort(dce_srcdestt pdsid,
	      pds_transidt transid);

int PDS_abort_send(dce_srcdestt pdsid,
		   pds_transidt transid);

int PDS_abort_recv(dce_srcdestt pdsid,
		   pds_transidt transid);
#else
int PDS_abort();

int PDS_abort_send();

int PDS_abort_recv();
#endif




/*
 * PDS_lookup()
 *
 * Parameters:
 *
 *   pdsid     - PDS id
 *   cmsgid    - control message id
 *   path      - file path name
 *   cflag     - file creation flag
 *   mode      - file creation access control (permission) bits
 *   fhandle   - file handle
 *   amode     - file accessability
 *
 * Obtain a file handle for regular file 'path' and place in 'fhandle';
 * file accessability is placed in 'amode'.
 * If 'path' does not exist and 'cflag' indicates file creation, then create
 * file with access control (permission) bits set to 'mode'.
 * If 'path' does exist and 'cflag' indicates file truncation, then the
 * file length is set to zero.
 *
 * 'cflag' is the inclusive OR of:
 *    exactly one of:     PIOUS_NOCREAT, PIOUS_CREAT and
 *    any combination of: PIOUS_TRUNC
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *    PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 * Returns: PDS_lookup(), PDS_lookup_recv()
 *
 *   PIOUS_OK (0) - successfully located/created file; file handle returned
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path', 'cflag', 'mode', or 'fhandle' arg
 *       PIOUS_EACCES       - path search permission denied or write permission
 *                            to create or truncate file denied.
 *       PIOUS_ENOENT       - file does not exist and PIOUS_NOCREAT, or
 *                            PIOUS_CREAT and path prefix component does not
 *                            exist or 'path' argument empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a dir
 *       PIOUS_ENOTREG      - file is not a regular file
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_EBUSY        - file is currently in use and PIOUS_TRUNC
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_lookup_send()
 *
 *   PIOUS_OK (0) - PDS_lookup_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_lookup(dce_srcdestt pdsid,
	       int cmsgid,
	       char *path,
	       int cflag,
	       pious_modet mode,
	       pds_fhandlet *fhandle,
	       int *amode);

int PDS_lookup_send(dce_srcdestt pdsid,
		    int cmsgid,
		    char *path,
		    int cflag,
		    pious_modet mode);

int PDS_lookup_recv(dce_srcdestt pdsid,
		    int cmsgid,
		    pds_fhandlet *fhandle,
		    int *amode);
#else
int PDS_lookup();

int PDS_lookup_send();

int PDS_lookup_recv();
#endif




/*
 * PDS_cacheflush()
 *
 * Parameters:
 *
 *   pdsid  - PDS id
 *   cmsgid - control message id
 *
 * Flush contents of PDS cache to stable storage.
 *
 * Note: Data written by stable transactions is flushed via synchronous
 *       writes, while data written by volatile transactions is flush
 *       via asynchronous writes to disk.
 *
 *       This function is primarily for testing purposes so that a PDS
 *       correctness test can force a cache flush to check the contents
 *       of a file.  It is not used during the course of normal system
 *       operation; i.e. the fact that the PDS maintains a cache should
 *       be transparent to the rest of the system.
 *
 * Returns: PDS_cacheflush(), PDS_cacheflush_recv()
 *
 *   PIOUS_OK (0) - cache contents successfully flushed to disk
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; assume cache flush incomplete
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_cacheflush_send()
 *
 *   PIOUS_OK (0) - PDS_cacheflush_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; assume cache flush incomplete
 */


#ifdef __STDC__
int PDS_cacheflush(dce_srcdestt pdsid,
		   int cmsgid);

int PDS_cacheflush_send(dce_srcdestt pdsid,
			int cmsgid);

int PDS_cacheflush_recv(dce_srcdestt pdsid,
			int cmsgid);
#else
int PDS_cacheflush();

int PDS_cacheflush_send();

int PDS_cacheflush_recv();
#endif




/*
 * PDS_mkdir()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *   path    - file path name
 *   mode    - file permission bits
 *
 * Create directory file 'path' with access control bits set according 
 * to the value of 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * Returns: PDS_mkdir(), PDS_mkdir_recv()
 *
 *   PIOUS_OK (0) - directory file successfully created
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path' argument
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EEXIST       - the named file exists
 *       PIOUS_ENOENT       - path prefix component does not exist or 'path'
 *                            arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_mkdir_send()
 *
 *   PIOUS_OK (0) - PDS_mkdir_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_mkdir(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path,
	      pious_modet mode);

int PDS_mkdir_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path,
		   pious_modet mode);

int PDS_mkdir_recv(dce_srcdestt pdsid,
		   int cmsgid);
#else
int PDS_mkdir();

int PDS_mkdir_send();

int PDS_mkdir_recv();
#endif




/*
 * PDS_rmdir()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *   path    - file path name
 *
 * Remove directory file 'path', if empty.
 *
 * Returns: PDS_rmdir(), PDS_rmdir_recv()
 *
 *   PIOUS_OK (0) - directory file successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path' argument
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EBUSY        - directory in use
 *       PIOUS_ENOTEMPTY    - directory not empty
 *       PIOUS_ENOENT       - directory file does not exist or 'path' arg empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_rmdir_send()
 *
 *   PIOUS_OK (0) - PDS_rmdir_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_rmdir(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path);

int PDS_rmdir_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path);

int PDS_rmdir_recv(dce_srcdestt pdsid,
		   int cmsgid);
#else
int PDS_rmdir();

int PDS_rmdir_send();

int PDS_rmdir_recv();
#endif




/*
 * PDS_unlink()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *   path    - file path name
 *
 * Remove non-directory file 'path'.
 *
 * NOTE: Upon successfull completion of PDS_unlink(), MUST insure that
 *       cached data from file 'path' is invalidated to prevent read
 *       from or write-back to a non-existent file.
 *
 * Returns: PDS_unlink(), PDS_unlink_recv()
 *
 *   PIOUS_OK (0) - file successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path' argument
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix is not a directory
 *       PIOUS_EPERM        - 'path' is a directory; operation denied
 *       PIOUS_EBUSY        - file is currently in use; retry
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_unlink_send()
 *
 *   PIOUS_OK (0) - PDS_unlink_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_unlink(dce_srcdestt pdsid,
	       int cmsgid,
	       char *path);

int PDS_unlink_send(dce_srcdestt pdsid,
		    int cmsgid,
		    char *path);

int PDS_unlink_recv(dce_srcdestt pdsid,
		    int cmsgid);
#else
int PDS_unlink();

int PDS_unlink_send();

int PDS_unlink_recv();
#endif




/*
 * PDS_chmod()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *   path    - file path name
 *   mode    - file permission bits
 *   amode   - file accessability
 *
 * Set the file permission bits of file 'path' to the value specified
 * by 'mode'.  File accessability, after performing SS_chmod(), is
 * returned in 'amode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *    PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 *
 * NOTE: Prior to performing SS_chmod(), MUST insure that cached
 *       data from file 'path' is flushed AND invalidated so as not to
 *       allow/inhibit operations that were invalid/valid prior to
 *       changing the file permission bits.
 *
 *       If 'path' is a directory file, then cached data from files
 *       contained in directory 'path' can also be effected.
 *
 *   
 * Returns: PDS_chmod(), PDS_chmod_recv()
 *
 *   PIOUS_OK (0) - file mode successfully updated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path' or 'amode' argument
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EBUSY        - file is currently in use; retry
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_chmod_send()
 *
 *   PIOUS_OK (0) - PDS_chmod_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_chmod(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path,
	      pious_modet mode,
	      int *amode);

int PDS_chmod_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path,
		   pious_modet mode);

int PDS_chmod_recv(dce_srcdestt pdsid,
		   int cmsgid,
		   int *amode);
#else
int PDS_chmod();

int PDS_chmod_send();

int PDS_chmod_recv();
#endif




/*
 * PDS_stat()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *   path    - file path name
 *   mode    - file mode bits (permission and type bits)
 *
 * Obtains mode of file 'path' and places it in 'mode'.
 *
 * 'mode' is the bitwise inclusive OR of any of the permission bits:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * and at most one of the file type bits:
 *    PIOUS_ISREG, PIOUS_ISDIR
 *
 *
 * Returns: PDS_stat(), PDS_stat_recv()
 *
 *   PIOUS_OK (0) - file mode successfully obtained
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINVAL       - invalid 'path' or 'mode' argument
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist or 'path' arg is empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix is not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 *
 * Returns: PDS_stat_send()
 *
 *   PIOUS_OK (0) - PDS_stat_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINVAL   - invalid 'path' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_stat(dce_srcdestt pdsid,
	     int cmsgid,
	     char *path,
	     pious_modet *mode);

int PDS_stat_send(dce_srcdestt pdsid,
		  int cmsgid,
		  char *path);

int PDS_stat_recv(dce_srcdestt pdsid,
		  int cmsgid,
		  pious_modet *mode);
#else
int PDS_stat();

int PDS_stat_send();

int PDS_stat_recv();
#endif




/*
 * PDS_ping()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   cmsgid  - control message id
 *
 * Ping the data server.  This function is a no-op.
 *
 *
 * Returns: PDS_ping(), PDS_ping_recv()
 *
 *   PIOUS_OK (0) - data server responded to ping
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *
 * Returns: PDS_ping_send()
 *
 *   PIOUS_OK (0) - PDS_ping_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
int PDS_ping(dce_srcdestt pdsid,
	     int cmsgid);

int PDS_ping_send(dce_srcdestt pdsid,
		  int cmsgid);

int PDS_ping_recv(dce_srcdestt pdsid,
		  int cmsgid);
#else
int PDS_ping();

int PDS_ping_send();

int PDS_ping_recv();
#endif




/*
 * PDS_reset()
 *
 * Parameters:
 *
 *   pdsid  - PDS id
 *   cmsgid - control message id
 *
 * Reset the PDS, checkpointing all access operations.  The PDS remains
 * active and can continue servicing requests.
 *
 * NOTE: the PDS will only reset if it has no pending transaction or control
 *       operations to be performed.
 *
 * Returns: PDS_reset(), PDS_reset_recv()
 *
 *   PIOUS_OK (0) - PDS reset and prepared for shutdown.
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EBUSY    - PDS has pending transaction/control operations
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; assume reset incomplete
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_reset_send()
 *
 *   PIOUS_OK (0) - PDS_reset_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; assume reset incomplete
 */


#ifdef __STDC__
int PDS_reset(dce_srcdestt pdsid,
	      int cmsgid);

int PDS_reset_send(dce_srcdestt pdsid,
		   int cmsgid);

int PDS_reset_recv(dce_srcdestt pdsid,
		   int cmsgid);
#else
int PDS_reset();

int PDS_reset_send();

int PDS_reset_recv();
#endif




/*
 * PDS_shutdown()
 *
 * Parameters:
 *
 *   pdsid  - PDS id
 *   cmsgid - control message id
 *
 * Shutdown the PDS: aborts active (unprepared) transactions, completes
 * blocked control operations, and checkpoints access operations.
 * If the PDS is unable to flush internal buffers, or if there are
 * transactions in an uncertainty period (ie prepared), then the PDS will
 * perform recovery the next time it is executed.
 *
 * Returns: PDS_shutdown(), PDS_shutdown_recv()
 *
 *   PIOUS_OK (0) - PDS shutdown complete; no recovery will be required
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; recovery will be required
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 *
 * Returns: PDS_shutdown_send()
 *
 *   PIOUS_OK (0) - PDS_shutdown_send() completed successfully
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete; retry
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error; assume shutdown incomplete
 */


#ifdef __STDC__
int PDS_shutdown(dce_srcdestt pdsid,
		 int cmsgid);

int PDS_shutdown_send(dce_srcdestt pdsid,
		      int cmsgid);

int PDS_shutdown_recv(dce_srcdestt pdsid,
		      int cmsgid);
#else
int PDS_shutdown();

int PDS_shutdown_send();

int PDS_shutdown_recv();
#endif
