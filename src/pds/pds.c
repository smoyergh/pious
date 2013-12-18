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
 * @(#)pds.c	2.2  28 Apr 1995  Moyer
 *
 * The pds module is a remote procedure call interface to operations
 * provided by the PDS daemon.  Client requests fall into two catagories:
 * transaction operations and control operations.
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
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) Only parameters that effect the ability to send/receive messages
 *      or return results are validated; actual function parameters checked
 *      by PDS daemon.
 *
 *   2) PDS_*_recv() functions currently block without time-out, disallowing
 *      the opportunity to handle failures in the message passing layer.
 *      Furthermore, PIOUS components are not dynamically restarted after
 *      a failure.
 *
 *      As a result, PDS RPC receive routines currently do NOT discard old
 *      messages, i.e. messages that do not match in (transid, transsn)
 *      or (cmsgid), as appropriate, as such messages should not exist;
 *      if a non-matching message is received, all functions return a result
 *      code of PIOUS_EUNXP.
 */




/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"

#include "pdce_srcdestt.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"
#include "pds.h"
#include "pds_msg_exchange.h"


/*
 * Private Declaration - Types and Constants
 */


/*
 * Private Variable Definitions
 */


/* Local Function Declarations */

#ifdef __STDC__
#else
#endif



/* Function Definitions - PDS operations */


/*
 * PDS_read() - See pds.h for description.
 */

#ifdef __STDC__
pious_ssizet PDS_read(dce_srcdestt pdsid,
		      pds_transidt transid,
		      int transsn,
		      pds_fhandlet fhandle,
		      pious_offt offset,
		      pious_sizet nbyte,
		      int lock,
		      struct PDS_vbuf_dscrp *vbuf)
#else
pious_ssizet PDS_read(pdsid, transid, transsn, fhandle, offset, nbyte,
		      lock, vbuf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     int lock;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  pious_ssizet rcode;

  /* validate 'vbuf' argument */
  if (vbuf == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS read request */
  else if ((rcode = PDS_read_send(pdsid, transid, transsn, fhandle,
				  offset, nbyte, lock)) == PIOUS_OK)
    /* receive PDS read result */
    rcode = PDS_read_recv(pdsid, transid, transsn, vbuf);

  return rcode;
}


#ifdef __STDC__
int PDS_read_send(dce_srcdestt pdsid,
		  pds_transidt transid,
		  int transsn,
		  pds_fhandlet fhandle,
		  pious_offt offset,
		  pious_sizet nbyte,
		  int lock)
#else
int PDS_read_send(pdsid, transid, transsn, fhandle, offset, nbyte, lock)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     int lock;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.ReadHead.transid = transid;
  reqmsg.ReadHead.transsn = transsn;

  reqmsg.ReadBody.fhandle = fhandle;
  reqmsg.ReadBody.offset  = offset;
  reqmsg.ReadBody.nbyte   = nbyte;
  reqmsg.ReadBody.lock    = lock;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_READ_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
pious_ssizet PDS_read_recv(dce_srcdestt pdsid,
			   pds_transidt transid,
			   int transsn,
			   struct PDS_vbuf_dscrp *vbuf)
#else
pious_ssizet PDS_read_recv(pdsid, transid, transsn, vbuf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  pious_ssizet rcode;
  int mcode;
  pdsmsg_replyt replymsg;

  /* validate 'vbuf' argument */
  if (vbuf == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      /* define extra read buffer information and receive read reply */
      vbuf->transid = transid;
      vbuf->transsn = transsn;

      mcode = PDSMSG_reply_recv(pdsid, PDS_READ_OP, &replymsg, vbuf);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; (transid, transsn) match */
	  rcode = replymsg.ReadHead.rcode;
	  break;

	case PIOUS_EPERM:
	  /* read data not received; (transid, transsn) mismatch */
	  rcode = PIOUS_EUNXP;
	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_write() - See pds.h for description.
 */

#ifdef __STDC__
pious_ssizet PDS_write(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       pds_fhandlet fhandle,
		       pious_offt offset,
		       pious_sizet nbyte,
		       struct PDS_vbuf_dscrp *vbuf)
#else
pious_ssizet PDS_write(pdsid, transid, transsn, fhandle, offset, nbyte, vbuf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  pious_ssizet rcode;

  /* send PDS write request */
  if ((rcode = PDS_write_send(pdsid, transid, transsn, fhandle,
			      offset, nbyte, vbuf)) == PIOUS_OK)
    /* receive PDS write result */
    rcode = PDS_write_recv(pdsid, transid, transsn);

  return rcode;
}


#ifdef __STDC__
int PDS_write_send(dce_srcdestt pdsid,
		   pds_transidt transid,
		   int transsn,
		   pds_fhandlet fhandle,
		   pious_offt offset,
		   pious_sizet nbyte,
		   struct PDS_vbuf_dscrp *vbuf)
#else
int PDS_write_send(pdsid, transid, transsn, fhandle, offset, nbyte, vbuf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'nbyte' and 'vbuf' arguments */
  if (nbyte < 0 || vbuf == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.WriteHead.transid = transid;
      reqmsg.WriteHead.transsn = transsn;

      reqmsg.WriteBody.fhandle = fhandle;
      reqmsg.WriteBody.offset  = offset;
      reqmsg.WriteBody.nbyte   = nbyte;
      reqmsg.WriteBody.buf     = NULL;

      mcode = PDSMSG_req_send(pdsid, PDS_WRITE_OP, &reqmsg, vbuf);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
pious_ssizet PDS_write_recv(dce_srcdestt pdsid,
			    pds_transidt transid,
			    int transsn)
#else
pious_ssizet PDS_write_recv(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  pious_ssizet rcode;
  int mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_WRITE_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (transid, transsn) */
      if (!transid_eq(transid, replymsg.WriteHead.transid) ||
	  transsn != replymsg.WriteHead.transsn)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.WriteHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_read_sint() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_read_sint(dce_srcdestt pdsid,
		  pds_transidt transid,
		  int transsn,
		  pds_fhandlet fhandle,
		  pious_offt offset,
		  int nint,
		  long *buf)
#else
int PDS_read_sint(pdsid, transid, transsn, fhandle, offset, nint, buf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     int nint;
     long *buf;
#endif
{
  int rcode;

  /* validate 'buf' argument */
  if (buf == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS read_sint request */
  else if ((rcode = PDS_read_sint_send(pdsid, transid, transsn, fhandle,
				       offset, nint)) == PIOUS_OK)
    /* receive PDS read_sint result */
    rcode = PDS_read_sint_recv(pdsid, transid, transsn, buf);

  return rcode;
}


#ifdef __STDC__
int PDS_read_sint_send(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       pds_fhandlet fhandle,
		       pious_offt offset,
		       int nint)
#else
int PDS_read_sint_send(pdsid, transid, transsn, fhandle, offset, nint)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     int nint;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.ReadsintHead.transid = transid;
  reqmsg.ReadsintHead.transsn = transsn;

  reqmsg.ReadsintBody.fhandle = fhandle;
  reqmsg.ReadsintBody.offset  = offset;
  reqmsg.ReadsintBody.nint    = nint;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_READ_SINT_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_read_sint_recv(dce_srcdestt pdsid,
		       pds_transidt transid,
		       int transsn,
		       long *buf)
#else
int PDS_read_sint_recv(pdsid, transid, transsn, buf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     long *buf;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;
  struct PDS_vbuf_dscrp vbuf;

  /* validate 'buf' argument */
  if (buf == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      /* define read buffer information and receive read reply */
      vbuf.firstblk_ptr = (char *)buf;

      vbuf.transid      = transid;
      vbuf.transsn      = transsn;

      mcode = PDSMSG_reply_recv(pdsid,
				PDS_READ_SINT_OP, &replymsg, &vbuf);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; (transid, transsn) match */
	  rcode = replymsg.ReadsintHead.rcode;
	  break;

	case PIOUS_EPERM:
	  /* read data not received; (transid, transsn) mismatch */
	  rcode = PIOUS_EUNXP;
	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_write_sint() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_write_sint(dce_srcdestt pdsid,
		   pds_transidt transid,
		   int transsn,
		   pds_fhandlet fhandle,
		   pious_offt offset,
		   int nint,
		   long *buf)
#else
int PDS_write_sint(pdsid, transid, transsn, fhandle, offset, nint, buf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     int nint;
     long *buf;
#endif
{
  int rcode;

  /* send PDS write_sint request */
  if ((rcode = PDS_write_sint_send(pdsid, transid, transsn, fhandle,
				   offset, nint, buf)) == PIOUS_OK)
    /* receive PDS write_sint result */
    rcode = PDS_write_sint_recv(pdsid, transid, transsn);

  return rcode;
}


#ifdef __STDC__
int PDS_write_sint_send(dce_srcdestt pdsid,
			pds_transidt transid,
			int transsn,
			pds_fhandlet fhandle,
			pious_offt offset,
			int nint,
			long *buf)
#else
int PDS_write_sint_send(pdsid, transid, transsn, fhandle, offset, nint, buf)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     int nint;
     long *buf;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;
  struct PDS_vbuf_dscrp vbuf;

  /* validate 'nint' and 'buf' arguments */
  if (nint < 0 || buf == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields */
      reqmsg.WritesintHead.transid = transid;
      reqmsg.WritesintHead.transsn = transsn;

      reqmsg.WritesintBody.fhandle = fhandle;
      reqmsg.WritesintBody.offset  = offset;
      reqmsg.WritesintBody.nint    = nint;
      reqmsg.WritesintBody.buf     = NULL;

      /* set write buffer info and send to PDS */
      vbuf.firstblk_ptr = (char *)buf;

      mcode = PDSMSG_req_send(pdsid, PDS_WRITE_SINT_OP, &reqmsg, &vbuf);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_write_sint_recv(dce_srcdestt pdsid,
			pds_transidt transid,
			int transsn)
#else
int PDS_write_sint_recv(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_WRITE_SINT_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (transid, transsn) */
      if (!transid_eq(transid, replymsg.WritesintHead.transid) ||
	  transsn != replymsg.WritesintHead.transsn)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.WritesintHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_fa_sint() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_fa_sint(dce_srcdestt pdsid,
		pds_transidt transid,
		int transsn,
		pds_fhandlet fhandle,
		pious_offt offset,
		long increment,
		long *rvalue)
#else
int PDS_fa_sint(pdsid, transid, transsn, fhandle, offset, increment, rvalue)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     long increment;
     long *rvalue;
#endif
{
  int rcode;

  /* validate 'rvalue' argument */
  if (rvalue == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS fa_sint request */
  else if ((rcode = PDS_fa_sint_send(pdsid, transid, transsn, fhandle,
				     offset, increment)) == PIOUS_OK)
    /* receive PDS fa_sint result */
    rcode = PDS_fa_sint_recv(pdsid, transid, transsn, rvalue);

  return rcode;
}


#ifdef __STDC__
int PDS_fa_sint_send(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn,
		     pds_fhandlet fhandle,
		     pious_offt offset,
		     long increment)
#else
int PDS_fa_sint_send(pdsid, transid, transsn, fhandle, offset, increment)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     pds_fhandlet fhandle;
     pious_offt offset;
     long increment;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.FasintHead.transid   = transid;
  reqmsg.FasintHead.transsn   = transsn;
  reqmsg.FasintBody.fhandle   = fhandle;
  reqmsg.FasintBody.offset    = offset;
  reqmsg.FasintBody.increment = increment;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_FA_SINT_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_fa_sint_recv(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn,
		     long *rvalue)
#else
int PDS_fa_sint_recv(pdsid, transid, transsn, rvalue)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
     long *rvalue;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* validate 'rvalue' argument */
  if (rvalue == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      mcode = PDSMSG_reply_recv(pdsid,
				PDS_FA_SINT_OP,
				&replymsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; check (transid, transsn) */
	  if (!transid_eq(transid, replymsg.FasintHead.transid) ||
	      transsn != replymsg.FasintHead.transsn)
	    rcode = PIOUS_EUNXP;

	  /* extract rvalue and PDS result code */
	  else
	    if ((rcode = replymsg.FasintHead.rcode) == PIOUS_OK)
	      *rvalue = replymsg.FasintBody.rvalue;
	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_prepare() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_prepare(dce_srcdestt pdsid,
		pds_transidt transid,
		int transsn)
#else
int PDS_prepare(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int rcode;

  /* send PDS prepare request */
  if ((rcode = PDS_prepare_send(pdsid, transid, transsn)) == PIOUS_OK)
    /* receive PDS prepare result */
    rcode = PDS_prepare_recv(pdsid, transid, transsn);

  return rcode;
}


#ifdef __STDC__
int PDS_prepare_send(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn)
#else
int PDS_prepare_send(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.PrepareHead.transid   = transid;
  reqmsg.PrepareHead.transsn   = transsn;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_PREPARE_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_prepare_recv(dce_srcdestt pdsid,
		     pds_transidt transid,
		     int transsn)
#else
int PDS_prepare_recv(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_PREPARE_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (transid, transsn) */
      if (!transid_eq(transid, replymsg.PrepareHead.transid) ||
	  transsn != replymsg.PrepareHead.transsn)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.PrepareHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_commit() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_commit(dce_srcdestt pdsid,
	       pds_transidt transid,
	       int transsn)
#else
int PDS_commit(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int rcode;

  /* send PDS commit request */
  if ((rcode = PDS_commit_send(pdsid, transid, transsn)) == PIOUS_OK)
    /* receive PDS commit result */
    rcode = PDS_commit_recv(pdsid, transid, transsn);

  return rcode;
}


#ifdef __STDC__
int PDS_commit_send(dce_srcdestt pdsid,
		    pds_transidt transid,
		    int transsn)
#else
int PDS_commit_send(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.CommitHead.transid   = transid;
  reqmsg.CommitHead.transsn   = transsn;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_COMMIT_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_commit_recv(dce_srcdestt pdsid,
		    pds_transidt transid,
		    int transsn)
#else
int PDS_commit_recv(pdsid, transid, transsn)
     dce_srcdestt pdsid;
     pds_transidt transid;
     int transsn;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_COMMIT_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (transid, transsn) */
      if (!transid_eq(transid, replymsg.CommitHead.transid) ||
	  transsn != replymsg.CommitHead.transsn)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.CommitHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_abort() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_abort(dce_srcdestt pdsid,
	      pds_transidt transid)
#else
int PDS_abort(pdsid, transid)
     dce_srcdestt pdsid;
     pds_transidt transid;
#endif
{
  int rcode;

  /* send PDS abort request */
  if ((rcode = PDS_abort_send(pdsid, transid)) == PIOUS_OK)
    /* receive PDS abort result */
    rcode = PDS_abort_recv(pdsid, transid);

  return rcode;
}


#ifdef __STDC__
int PDS_abort_send(dce_srcdestt pdsid,
		   pds_transidt transid)
#else
int PDS_abort_send(pdsid, transid)
     dce_srcdestt pdsid;
     pds_transidt transid;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set message fields and send to PDS */
  reqmsg.AbortHead.transid   = transid;
  reqmsg.AbortHead.transsn   = -1;

  mcode = PDSMSG_req_send(pdsid,
			  PDS_ABORT_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL (or other error) indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_abort_recv(dce_srcdestt pdsid,
		   pds_transidt transid)
#else
int PDS_abort_recv(pdsid, transid)
     dce_srcdestt pdsid;
     pds_transidt transid;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_ABORT_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (transid) */
      if (!transid_eq(transid, replymsg.AbortHead.transid))
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.AbortHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_lookup() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_lookup(dce_srcdestt pdsid,
	       int cmsgid,
	       char *path,
	       int cflag,
	       pious_modet mode,
	       pds_fhandlet *fhandle,
	       int *amode)
#else
int PDS_lookup(pdsid, cmsgid, path, cflag, mode, fhandle, amode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     int cflag;
     pious_modet mode;
     pds_fhandlet *fhandle;
     int *amode;
#endif
{
  int rcode;

  /* validate 'path', 'fhandle', and 'amode' arguments */
  if (path == NULL || fhandle == NULL || amode == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS lookup request */
  else if ((rcode = PDS_lookup_send(pdsid, cmsgid, path, cflag,
				    mode)) == PIOUS_OK)
    /* receive PDS lookup result */
    rcode = PDS_lookup_recv(pdsid, cmsgid, fhandle, amode);

  return rcode;
}


#ifdef __STDC__
int PDS_lookup_send(dce_srcdestt pdsid,
		    int cmsgid,
		    char *path,
		    int cflag,
		    pious_modet mode)
#else
int PDS_lookup_send(pdsid, cmsgid, path, cflag, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     int cflag;
     pious_modet mode;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.LookupHead.cmsgid  = cmsgid;

      reqmsg.LookupBody.cflag   = cflag;
      reqmsg.LookupBody.mode    = mode;
      reqmsg.LookupBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_LOOKUP_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_lookup_recv(dce_srcdestt pdsid,
		    int cmsgid,
		    pds_fhandlet *fhandle,
		    int *amode)
#else
int PDS_lookup_recv(pdsid, cmsgid, fhandle, amode)
     dce_srcdestt pdsid;
     int cmsgid;
     pds_fhandlet *fhandle;
     int *amode;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* validate 'fhandle' and 'amode' arguments */
  if (fhandle == NULL || amode == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      mcode = PDSMSG_reply_recv(pdsid,
				PDS_LOOKUP_OP,
				&replymsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; check (cmsgid) */
	  if (cmsgid != replymsg.LookupHead.cmsgid)
	    rcode = PIOUS_EUNXP;

	  /* extract fhandle, amode, and PDS result code */
	  else
	    if ((rcode = replymsg.LookupHead.rcode) == PIOUS_OK)
	      { /* results valid */
		*fhandle = replymsg.LookupBody.fhandle;
		*amode   = replymsg.LookupBody.amode;
	      }
	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_cacheflush() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_cacheflush(dce_srcdestt pdsid,
		   int cmsgid)
#else
int PDS_cacheflush(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode;

  /* send PDS cacheflush request */
  if ((rcode = PDS_cacheflush_send(pdsid, cmsgid)) == PIOUS_OK)

    /* receive PDS cacheflush result */
    rcode = PDS_cacheflush_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_cacheflush_send(dce_srcdestt pdsid,
			int cmsgid)
#else
int PDS_cacheflush_send(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set request message fields */
  reqmsg.CacheflushHead.cmsgid = cmsgid;

  /* send request message to PDS */
  mcode = PDSMSG_req_send(pdsid,
			  PDS_CACHEFLUSH_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code appropriately */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_cacheflush_recv(dce_srcdestt pdsid,
			int cmsgid)
#else
int PDS_cacheflush_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;

  /* receive reply message from PDS */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_CACHEFLUSH_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.CacheflushHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.CacheflushHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_mkdir() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_mkdir(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path,
	      pious_modet mode)
#else
int PDS_mkdir(pdsid, cmsgid, path, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     pious_modet mode;
#endif
{
  int rcode;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS mkdir request */
  else if ((rcode = PDS_mkdir_send(pdsid, cmsgid, path, mode)) == PIOUS_OK)
    /* receive PDS mkdir result */
    rcode = PDS_mkdir_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_mkdir_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path,
		   pious_modet mode)
#else
int PDS_mkdir_send(pdsid, cmsgid, path, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     pious_modet mode;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.MkdirHead.cmsgid  = cmsgid;

      reqmsg.MkdirBody.mode    = mode;
      reqmsg.MkdirBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_MKDIR_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_mkdir_recv(dce_srcdestt pdsid,
		   int cmsgid)
#else
int PDS_mkdir_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_MKDIR_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.MkdirHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.MkdirHead.rcode;

      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_rmdir() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_rmdir(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path)
#else
int PDS_rmdir(pdsid, cmsgid, path)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
#endif
{
  int rcode;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS rmdir request */
  else if ((rcode = PDS_rmdir_send(pdsid, cmsgid, path)) == PIOUS_OK)
    /* receive PDS rmdir result */
    rcode = PDS_rmdir_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_rmdir_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path)
#else
int PDS_rmdir_send(pdsid, cmsgid, path)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.RmdirHead.cmsgid  = cmsgid;

      reqmsg.RmdirBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_RMDIR_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_rmdir_recv(dce_srcdestt pdsid,
		   int cmsgid)
#else
int PDS_rmdir_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_RMDIR_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.RmdirHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.RmdirHead.rcode;

      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_unlink() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_unlink(dce_srcdestt pdsid,
	       int cmsgid,
	       char *path)
#else
int PDS_unlink(pdsid, cmsgid, path)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
#endif
{
  int rcode;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS unlink request */
  else if ((rcode = PDS_unlink_send(pdsid, cmsgid, path)) == PIOUS_OK)
    /* receive PDS unlink result */
    rcode = PDS_unlink_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_unlink_send(dce_srcdestt pdsid,
		    int cmsgid,
		    char *path)
#else
int PDS_unlink_send(pdsid, cmsgid, path)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.UnlinkHead.cmsgid  = cmsgid;

      reqmsg.UnlinkBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_UNLINK_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_unlink_recv(dce_srcdestt pdsid,
		    int cmsgid)
#else
int PDS_unlink_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* receive PDS reply */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_UNLINK_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.UnlinkHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.UnlinkHead.rcode;

      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_chmod() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_chmod(dce_srcdestt pdsid,
	      int cmsgid,
	      char *path,
	      pious_modet mode,
	      int *amode)
#else
int PDS_chmod(pdsid, cmsgid, path, mode, amode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     pious_modet mode;
     int *amode;
#endif
{
  int rcode;

  /* validate 'path' and 'amode' argument */
  if (path == NULL || amode == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS chmod request */
  else if ((rcode = PDS_chmod_send(pdsid, cmsgid, path, mode)) == PIOUS_OK)
    /* receive PDS chmod result */
    rcode = PDS_chmod_recv(pdsid, cmsgid, amode);

  return rcode;
}


#ifdef __STDC__
int PDS_chmod_send(dce_srcdestt pdsid,
		   int cmsgid,
		   char *path,
		   pious_modet mode)
#else
int PDS_chmod_send(pdsid, cmsgid, path, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     pious_modet mode;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.ChmodHead.cmsgid  = cmsgid;

      reqmsg.ChmodBody.mode    = mode;
      reqmsg.ChmodBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_CHMOD_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_chmod_recv(dce_srcdestt pdsid,
		   int cmsgid,
		   int *amode)
#else
int PDS_chmod_recv(pdsid, cmsgid, amode)
     dce_srcdestt pdsid;
     int cmsgid;
     int *amode;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* validate 'amode' argument */
  if (amode == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      mcode = PDSMSG_reply_recv(pdsid,
				PDS_CHMOD_OP,
				&replymsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; check (cmsgid) */
	  if (cmsgid != replymsg.ChmodHead.cmsgid)
	    rcode = PIOUS_EUNXP;

	  /* extract amode and PDS result code */
	  else if ((rcode = replymsg.ChmodHead.rcode) == PIOUS_OK)
	    *amode = replymsg.ChmodBody.amode;

	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_stat() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_stat(dce_srcdestt pdsid,
	     int cmsgid,
	     char *path,
	     pious_modet *mode)
#else
int PDS_stat(pdsid, cmsgid, path, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
     pious_modet *mode;
#endif
{
  int rcode;

  /* validate 'path' and 'mode' argument */
  if (path == NULL || mode == NULL)
    rcode = PIOUS_EINVAL;

  /* send PDS stat request */
  else if ((rcode = PDS_stat_send(pdsid, cmsgid, path)) == PIOUS_OK)
    /* receive PDS stat result */
    rcode = PDS_stat_recv(pdsid, cmsgid, mode);

  return rcode;
}


#ifdef __STDC__
int PDS_stat_send(dce_srcdestt pdsid,
		  int cmsgid,
		  char *path)
#else
int PDS_stat_send(pdsid, cmsgid, path)
     dce_srcdestt pdsid;
     int cmsgid;
     char *path;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* validate 'path' argument */
  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PDS */
      reqmsg.StatHead.cmsgid  = cmsgid;

      reqmsg.StatBody.path    = path;

      mcode = PDSMSG_req_send(pdsid,
			      PDS_STAT_OP,
			      &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = mcode;
	  break;
	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}


#ifdef __STDC__
int PDS_stat_recv(dce_srcdestt pdsid,
		  int cmsgid,
		  pious_modet *mode)
#else
int PDS_stat_recv(pdsid, cmsgid, mode)
     dce_srcdestt pdsid;
     int cmsgid;
     pious_modet *mode;
#endif
{
  int rcode, mcode;
  pdsmsg_replyt replymsg;

  /* validate 'mode' argument */
  if (mode == NULL)
    rcode = PIOUS_EINVAL;

  /* receive PDS reply */
  else
    {
      mcode = PDSMSG_reply_recv(pdsid,
				PDS_STAT_OP,
				&replymsg, (struct PDS_vbuf_dscrp *)NULL);

      /* set result code */
      switch(mcode)
	{
	case PIOUS_OK:
	  /* reply msg received without error; check (cmsgid) */
	  if (cmsgid != replymsg.StatHead.cmsgid)
	    rcode = PIOUS_EUNXP;

	  /* extract mode and PDS result code */
	  else if ((rcode = replymsg.StatHead.rcode) == PIOUS_OK)
	    *mode = replymsg.StatBody.mode;

	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  /* error receiving PDS reply */
	  rcode = mcode;
	  break;

	default:
	  /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDS_ping() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_ping(dce_srcdestt pdsid,
	     int cmsgid)
#else
int PDS_ping(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode;

  /* send PDS ping request */
  if ((rcode = PDS_ping_send(pdsid, cmsgid)) == PIOUS_OK)

    /* receive PDS ping result */
    rcode = PDS_ping_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_ping_send(dce_srcdestt pdsid,
		  int cmsgid)
#else
int PDS_ping_send(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set request message fields */
  reqmsg.PingHead.cmsgid = cmsgid;

  /* send request message to PDS */
  mcode = PDSMSG_req_send(pdsid,
			  PDS_PING_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code appropriately */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_ping_recv(dce_srcdestt pdsid,
		  int cmsgid)
#else
int PDS_ping_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;

  /* receive reply message from PDS */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_PING_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.PingHead.cmsgid)
	rcode = PIOUS_EUNXP;

      else
	rcode = PIOUS_OK;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_reset() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_reset(dce_srcdestt pdsid,
	      int cmsgid)
#else
int PDS_reset(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode;

  /* send PDS reset request */
  if ((rcode = PDS_reset_send(pdsid, cmsgid)) == PIOUS_OK)

    /* receive PDS reset result */
    rcode = PDS_reset_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_reset_send(dce_srcdestt pdsid,
		   int cmsgid)
#else
int PDS_reset_send(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set request message fields */
  reqmsg.ResetHead.cmsgid = cmsgid;

  /* send request message to PDS */
  mcode = PDSMSG_req_send(pdsid,
			  PDS_RESET_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code appropriately */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_reset_recv(dce_srcdestt pdsid,
		   int cmsgid)
#else
int PDS_reset_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;

  /* receive reply message from PDS */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_RESET_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.ResetHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.ResetHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDS_shutdown() - See pds.h for description.
 */

#ifdef __STDC__
int PDS_shutdown(dce_srcdestt pdsid,
		 int cmsgid)
#else
int PDS_shutdown(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int rcode;

  /* send PDS shutdown request */
  if ((rcode = PDS_shutdown_send(pdsid, cmsgid)) == PIOUS_OK)

    /* receive PDS shutdown result */
    rcode = PDS_shutdown_recv(pdsid, cmsgid);

  return rcode;
}


#ifdef __STDC__
int PDS_shutdown_send(dce_srcdestt pdsid,
		      int cmsgid)
#else
int PDS_shutdown_send(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_reqt reqmsg;

  /* set request message fields */
  reqmsg.ShutdownHead.cmsgid = cmsgid;

  /* send request message to PDS */
  mcode = PDSMSG_req_send(pdsid,
			  PDS_SHUTDOWN_OP,
			  &reqmsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code appropriately */
  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}


#ifdef __STDC__
int PDS_shutdown_recv(dce_srcdestt pdsid,
		      int cmsgid)
#else
int PDS_shutdown_recv(pdsid, cmsgid)
     dce_srcdestt pdsid;
     int cmsgid;
#endif
{
  int mcode, rcode;
  pdsmsg_replyt replymsg;

  /* receive reply message from PDS */
  mcode = PDSMSG_reply_recv(pdsid,
			    PDS_SHUTDOWN_OP,
			    &replymsg, (struct PDS_vbuf_dscrp *)NULL);

  /* set result code */
  switch(mcode)
    {
    case PIOUS_OK:
      /* reply msg received without error; check (cmsgid) */
      if (cmsgid != replymsg.ShutdownHead.cmsgid)
	rcode = PIOUS_EUNXP;

      /* extract PDS result code */
      else
	rcode = replymsg.ShutdownHead.rcode;
      break;

    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      /* error receiving PDS reply */
      rcode = mcode;
      break;

    default:
      /* PIOUS_EINVAL or other error indicates a bug in the PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}
