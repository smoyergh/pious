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
 * @(#)pds_msg_exchange.c	2.2  28 Apr 1995  Moyer
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
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) The code for packing/unpacking strings can be cleaned up by writing
 *      functions for this purpose.  However, doing so will increase message
 *      passing overhead by increasing the number of function calls required
 *      to exchange a message.  Thus this code has been inlined.
 */




/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include <string.h>

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"

#include "pdce_srcdestt.h"
#include "pdce_msgtagt.h"
#include "pdce.h"

#include "pds.h"
#include "pds_msg_exchange.h"


/*
 * Private Declarations - Types and Constants
 */


/*
 * Private Variable Definitions
 */


/* Local Function Declarations */


/* Function Definitions - MSG passing operations */


/*
 * PDSMSG_req_send() - See pds_msg_exchange.h for description
 */

#ifdef __STDC__
int PDSMSG_req_send(dce_srcdestt pdsid,
		    int reqop,
		    pdsmsg_reqt *reqmsg,
		    struct PDS_vbuf_dscrp *vbuf)
#else
int PDSMSG_req_send(pdsid, reqop, reqmsg, vbuf)
     dce_srcdestt pdsid;
     int reqop;
     pdsmsg_reqt *reqmsg;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  int rcode, tcode;
  int pathlen;

  int firstblk_sz, lastblk_sz, middleblk_cnt;
  char *bufptr;

  /* validate 'reqop' argument */

  if (!PdsOp(reqop))
    rcode = PIOUS_EINVAL;

  /* send request to server */

  else
    { /* case : transaction operation */

      if (PdsTransop(reqop))
	{ /* initialize send buffer and pack message header */

	  if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	      (tcode = DCE_pktransidt(&reqmsg->TransopHead.transid,
				      1)) == PIOUS_OK &&

	      (tcode = DCE_pkint(&reqmsg->TransopHead.transsn, 1)) == PIOUS_OK)

	    switch(reqop)
	      { /* pack message body */

	      case PDS_READ_OP:
		/* do not allow req that exceeds transport capability */

		if ((tcode = ((reqmsg->ReadBody.nbyte <= PIOUS_INT_MAX) ?
			      PIOUS_OK : PIOUS_EINSUF)) == PIOUS_OK &&

		    (tcode = DCE_pkfhandlet(&reqmsg->ReadBody.fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&reqmsg->ReadBody.offset,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pksizet(&reqmsg->ReadBody.nbyte,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&reqmsg->ReadBody.lock, 1)));
		break;

	      case PDS_WRITE_OP:
		/* do not allow req that exceeds transport capability */

		if ((tcode = ((reqmsg->WriteBody.nbyte <= PIOUS_INT_MAX) ?
			      PIOUS_OK : PIOUS_EINSUF)) == PIOUS_OK &&

		    (tcode = DCE_pkfhandlet(&reqmsg->WriteBody.fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&reqmsg->WriteBody.offset,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pksizet(&reqmsg->WriteBody.nbyte,
					 1)) == PIOUS_OK)
		  {
		    if (reqmsg->WriteBody.nbyte > 0)
		      { /* extract/pack data from vector buffer */

			if (vbuf->firstblk_netsz >= reqmsg->WriteBody.nbyte ||
			    vbuf->stride == 1)
			  { /* contiguous data region to extract */
			    tcode = DCE_pkbyte(vbuf->firstblk_ptr,
					       (int)reqmsg->WriteBody.nbyte);
			  }

			else if (vbuf->blksz == 1)
			  { /* regular non-contiguous data region to extract */
			    tcode =
			      DCE_pkbyte_blk(vbuf->firstblk_ptr,
					     1,
					     vbuf->stride,
					     (int)reqmsg->WriteBody.nbyte);
			  }

			else
			  { /* (potentially) irregular non-contiguous data */

			    firstblk_sz   = vbuf->firstblk_netsz;
			    middleblk_cnt = ((reqmsg->WriteBody.nbyte -
					      firstblk_sz) / vbuf->blksz);
			    lastblk_sz    = (reqmsg->WriteBody.nbyte -
					     (firstblk_sz +
					      (middleblk_cnt * vbuf->blksz)));

			    bufptr        = vbuf->firstblk_ptr;

			    /* pack data from first vector buffer block */

			    tcode   = DCE_pkbyte(bufptr, firstblk_sz);

			    bufptr += (firstblk_sz +
				       ((vbuf->stride - 1) * vbuf->blksz));

			    /* pack data from "middle" vector buffer blocks */

			    if (tcode == PIOUS_OK && middleblk_cnt > 0)
			      {
				tcode = DCE_pkbyte_blk(bufptr,
						       vbuf->blksz,
						       vbuf->stride,
						       middleblk_cnt);

				bufptr += (vbuf->stride * vbuf->blksz *
					   middleblk_cnt);
			      }

			    /* pack data from last vector buffer block */

			    if (tcode == PIOUS_OK && lastblk_sz > 0)
			      {
				tcode = DCE_pkbyte(bufptr, lastblk_sz);
			      }
			  }
		      }
		  }

		break;

	      case PDS_READ_SINT_OP:
		if ((tcode = DCE_pkfhandlet(&reqmsg->ReadsintBody.fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&reqmsg->ReadsintBody.offset,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&reqmsg->ReadsintBody.nint, 1)));
		break;

	      case PDS_WRITE_SINT_OP:
		if ((tcode = DCE_pkfhandlet(&reqmsg->WritesintBody.fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&reqmsg->WritesintBody.offset,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&reqmsg->WritesintBody.nint,
				       1)) == PIOUS_OK)
		  { /* pack data, if any */
		    if (reqmsg->WritesintBody.nint > 0)
		      tcode = DCE_pklong((long *)vbuf->firstblk_ptr,
					 reqmsg->WritesintBody.nint);
		  }
		break;

	      case PDS_FA_SINT_OP:
		if ((tcode = DCE_pkfhandlet(&reqmsg->FasintBody.fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&reqmsg->FasintBody.offset,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pklong(&reqmsg->FasintBody.increment, 1)));
		break;
	      }
	}

      /* case: control operation */

      else
	{ /* initialize send buffer and pack message header */

	  if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	      (tcode = DCE_pkint(&reqmsg->CntrlopHead.cmsgid, 1)) == PIOUS_OK)

	    switch(reqop)
	      { /* pack message body */

	      case PDS_LOOKUP_OP:
		pathlen = strlen(reqmsg->LookupBody.path) + 1;

		if ((tcode = DCE_pkint(&reqmsg->LookupBody.cflag,
				       1)) == PIOUS_OK &&

		    (tcode = DCE_pkmodet(&reqmsg->LookupBody.mode,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->LookupBody.path, pathlen)));
		break;

	      case PDS_MKDIR_OP:
		pathlen = strlen(reqmsg->MkdirBody.path) + 1;

		if ((tcode = DCE_pkmodet(&reqmsg->MkdirBody.mode,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->MkdirBody.path, pathlen)));
		break;

	      case PDS_RMDIR_OP:
		pathlen = strlen(reqmsg->RmdirBody.path) + 1;

		if ((tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->RmdirBody.path, pathlen)));
		break;

	      case PDS_UNLINK_OP:
		pathlen = strlen(reqmsg->UnlinkBody.path) + 1;

		if ((tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->UnlinkBody.path, pathlen)));
		break;

	      case PDS_CHMOD_OP:
		pathlen = strlen(reqmsg->ChmodBody.path) + 1;

		if ((tcode = DCE_pkmodet(&reqmsg->ChmodBody.mode,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->ChmodBody.path, pathlen)));
		break;

	      case PDS_STAT_OP:
		pathlen = strlen(reqmsg->StatBody.path) + 1;

		if ((tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		    (tcode = DCE_pkchar(reqmsg->StatBody.path, pathlen)));
		break;
	      }
	}


      /* send message to specified server */

      if (tcode == PIOUS_OK)
	tcode = DCE_send(pdsid, msgtag_encode(reqop));

      /* free send buffer */

      if (DCE_freesendbuf() != PIOUS_OK)
	tcode = PIOUS_ETPORT;

      /* set result code */
      switch(tcode)
	{ /* tcode is PIOUS_OK or result of a failed DCE function */
	case PIOUS_OK:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_ESRCDEST:
	  rcode = tcode;
	  break;
	default:
	  /* any other error code indicates a bug in PIOUS */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDSMSG_req_recv() - See pds_msg_exchange.h for description
 */

#ifdef __STDC__
int PDSMSG_req_recv(dce_srcdestt *clientid,
		    int *reqop,
		    pdsmsg_reqt *reqmsg,
		    int timeout)
#else
int PDSMSG_req_recv(clientid, reqop, reqmsg, timeout)
     dce_srcdestt *clientid;
     int *reqop;
     pdsmsg_reqt *reqmsg;
     int timeout;
#endif
{
  int rcode, tcode;
  dce_srcdestt msgsrc;
  dce_msgtagt msgtag;
  int pathlen;

  /* wait for next client request with a valid message tag */

  while ((tcode = DCE_recv(DCE_ANY_SRC, DCE_ANY_TAG,
			   &msgsrc, &msgtag, timeout)) == PIOUS_OK &&
	 (!msgtag_valid(msgtag) || !PdsOp(msgtag_decode(msgtag))) &&
	 (tcode = DCE_freerecvbuf()) == PIOUS_OK);

  /* unpack message */

  if (tcode == PIOUS_OK)
    { /* set 'clientid' and 'reqop' */
      *clientid = msgsrc;
      *reqop    = msgtag_decode(msgtag);

      /* case : transaction operation */

      if (PdsTransop(*reqop))
	{ /* unpack message header */

	  if ((tcode = DCE_upktransidt(&reqmsg->TransopHead.transid,
				       1)) == PIOUS_OK &&

	      (tcode = DCE_upkint(&reqmsg->TransopHead.transsn,
				  1)) == PIOUS_OK)

	    switch(*reqop)
	      { /* unpack message body */

	      case PDS_READ_OP:
		if ((tcode = DCE_upkfhandlet(&reqmsg->ReadBody.fhandle,
					     1)) == PIOUS_OK &&

		    (tcode = DCE_upkofft(&reqmsg->ReadBody.offset,
					 1)) == PIOUS_OK &&


		    (tcode = DCE_upksizet(&reqmsg->ReadBody.nbyte,
					  1)) == PIOUS_OK &&

		    (tcode = DCE_upkint(&reqmsg->ReadBody.lock, 1)));
		break;

	      case PDS_WRITE_OP:
		if ((tcode = DCE_upkfhandlet(&reqmsg->WriteBody.fhandle,
					     1)) == PIOUS_OK &&

		    (tcode = DCE_upkofft(&reqmsg->WriteBody.offset,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_upksizet(&reqmsg->WriteBody.nbyte,
					  1)) == PIOUS_OK)

		  { /* if data sent, allocate space for write buf */
		    if (reqmsg->WriteBody.nbyte == 0)
		      reqmsg->WriteBody.buf = NULL;

		    else if ((reqmsg->WriteBody.buf =
			      malloc((unsigned)reqmsg->WriteBody.nbyte))
			     == NULL)

		      tcode = PIOUS_EINSUF;

		    else if ((tcode =
			      DCE_upkbyte(reqmsg->WriteBody.buf,
					  (int)reqmsg->WriteBody.nbyte))
			     != PIOUS_OK)
		      /* error; deallocate storage */
		      free(reqmsg->WriteBody.buf);
		  }
		break;

	      case PDS_READ_SINT_OP:
		if ((tcode = DCE_upkfhandlet(&reqmsg->ReadsintBody.fhandle,
					     1)) == PIOUS_OK &&

		    (tcode = DCE_upkofft(&reqmsg->ReadsintBody.offset,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_upkint(&reqmsg->ReadsintBody.nint, 1)));
		break;

	      case PDS_WRITE_SINT_OP:
		if ((tcode = DCE_upkfhandlet(&reqmsg->WritesintBody.fhandle,
					     1)) == PIOUS_OK &&

		    (tcode = DCE_upkofft(&reqmsg->WritesintBody.offset,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_upkint(&reqmsg->WritesintBody.nint,
					1)) == PIOUS_OK)

		  { /* if data sent, allocate space for write buf */
		    if (reqmsg->WritesintBody.nint <= 0)
		      reqmsg->WritesintBody.buf = NULL;

		    else if ((reqmsg->WritesintBody.buf = (long *)
			      malloc((unsigned)(reqmsg->WritesintBody.nint *
						sizeof(long)))) == NULL)

		      tcode = PIOUS_EINSUF;

		    else if ((tcode =
			      DCE_upklong(reqmsg->WritesintBody.buf,
					  reqmsg->WritesintBody.nint))
			     != PIOUS_OK)
		      /* error; deallocate storage */
		      free((char *)reqmsg->WritesintBody.buf);
		  }
		break;

	      case PDS_FA_SINT_OP:
		if ((tcode = DCE_upkfhandlet(&reqmsg->FasintBody.fhandle,
					     1)) == PIOUS_OK &&

		    (tcode = DCE_upkofft(&reqmsg->FasintBody.offset,
					 1)) == PIOUS_OK &&

		    (tcode = DCE_upklong(&reqmsg->FasintBody.increment, 1)));
		break;
	      }
	}

      /* case: control operation */

      else
	{ /* unpack message header */

	  if ((tcode = DCE_upkint(&reqmsg->CntrlopHead.cmsgid, 1)) == PIOUS_OK)

	    switch(*reqop)
	      { /* unpack message body */

	      case PDS_LOOKUP_OP:
		reqmsg->LookupBody.path = NULL;

		if ((tcode = DCE_upkint(&reqmsg->LookupBody.cflag,
					1)) != PIOUS_OK ||

		    (tcode = DCE_upkmodet(&reqmsg->LookupBody.mode,
					  1)) != PIOUS_OK ||

		    /* alloc/unpack path */
		    (tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->LookupBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->LookupBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->LookupBody.path != NULL)
		      free(reqmsg->LookupBody.path);
		  }
		break;

	      case PDS_MKDIR_OP:
		reqmsg->MkdirBody.path = NULL;

		if ((tcode = DCE_upkmodet(&reqmsg->MkdirBody.mode,
					  1)) != PIOUS_OK ||

		    /* alloc/unpack path */
		    (tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->MkdirBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->MkdirBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->MkdirBody.path != NULL)
		      free(reqmsg->MkdirBody.path);
		  }
		break;

	      case PDS_RMDIR_OP:
		reqmsg->RmdirBody.path = NULL;

		/* alloc/unpack path */
		if ((tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->RmdirBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->RmdirBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->RmdirBody.path != NULL)
		      free(reqmsg->RmdirBody.path);
		  }
		break;

	      case PDS_UNLINK_OP:
		reqmsg->UnlinkBody.path = NULL;

		/* alloc/unpack path */
		if ((tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->UnlinkBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->UnlinkBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->UnlinkBody.path != NULL)
		      free(reqmsg->UnlinkBody.path);
		  }
		break;

	      case PDS_CHMOD_OP:
		reqmsg->ChmodBody.path = NULL;

		if ((tcode = DCE_upkmodet(&reqmsg->ChmodBody.mode,
					  1)) != PIOUS_OK ||

		    /* alloc/unpack path */
		    (tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->ChmodBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->ChmodBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->ChmodBody.path != NULL)
		      free(reqmsg->ChmodBody.path);
		  }
		break;

	      case PDS_STAT_OP:
		reqmsg->StatBody.path = NULL;

		/* alloc/unpack path */
		if ((tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		    (tcode = ((reqmsg->StatBody.path =
			       malloc((unsigned)pathlen)) == NULL ?
			      PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		    (tcode = DCE_upkchar(reqmsg->StatBody.path,
					 pathlen)) != PIOUS_OK)
		  { /* error; deallocate storage */
		    if (reqmsg->StatBody.path != NULL)
		      free(reqmsg->StatBody.path);
		  }
		break;
	      }
	}
    }

  /* free receive buffer */

  if (DCE_freerecvbuf() != PIOUS_OK)
    tcode = PIOUS_ETPORT;

  /* set result code */
  switch(tcode)
    { /* tcode is PIOUS_OK or result of a failed DCE function */
    case PIOUS_OK:
    case PIOUS_ETIMEOUT:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = tcode;
      break;
    default:
      /* any other error code indicates a bug in PIOUS */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * PDSMSG_reply_send() - See pds_msg_exchange.h for description
 */

#ifdef __STDC__
int PDSMSG_reply_send(dce_srcdestt clientid,
		      int replyop,
		      pdsmsg_replyt *replymsg)
#else
int PDSMSG_reply_send(clientid, replyop, replymsg)
     dce_srcdestt clientid;
     int replyop;
     pdsmsg_replyt *replymsg;
#endif
{
  int rcode, tcode;

  /* validate 'replyop' argument */

  if (!PdsOp(replyop))
    rcode = PIOUS_EINVAL;

  /* send reply to client */

  else
    { /* case : transaction operation */

      if (PdsTransop(replyop))
	{ /* initialize send buffer and pack message header */

	  if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	      (tcode = DCE_pktransidt(&replymsg->TransopHead.transid,
				      1)) == PIOUS_OK &&

	      (tcode = DCE_pkint(&replymsg->TransopHead.transsn,
				 1)) == PIOUS_OK &&

	      (tcode = DCE_pklong(&replymsg->TransopHead.rcode,
				  1)) == PIOUS_OK)

	    switch(replyop)
	      { /* pack message body */

	      case PDS_READ_OP:
		/* determine if data is returned */
		if (replymsg->TransopHead.rcode > 0)
		  tcode = DCE_pkbyte(replymsg->ReadBody.buf,
				     (int)replymsg->TransopHead.rcode);
		break;

	      case PDS_READ_SINT_OP:
		/* determine if data is returned */
		if (replymsg->TransopHead.rcode > 0)
		  tcode = DCE_pklong(replymsg->ReadsintBody.buf,
				     (int)replymsg->TransopHead.rcode);
		break;

	      case PDS_FA_SINT_OP:
		/* determine if data is returned */
		if (replymsg->TransopHead.rcode == PIOUS_OK)
		  tcode = DCE_pklong(&replymsg->FasintBody.rvalue, 1);
		break;
	      }
	}

      /* case: control operation */

      else
	{ /* initialize send buffer and pack message header */

	  if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	      (tcode = DCE_pkint(&replymsg->CntrlopHead.cmsgid,
				 1)) == PIOUS_OK &&

	      (tcode = DCE_pklong(&replymsg->CntrlopHead.rcode,
				  1)) == PIOUS_OK)

	    switch(replyop)
	      { /* pack message body */

	      case PDS_LOOKUP_OP:
		/* determine if data is returned */
		if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		  {
		    if ((tcode = DCE_pkfhandlet(&replymsg->LookupBody.fhandle,
						1)) == PIOUS_OK &&

			(tcode = DCE_pkint(&replymsg->LookupBody.amode, 1)));
		  }
		break;

	      case PDS_CHMOD_OP:
		/* determine if data is returned */
		if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		  tcode = DCE_pkint(&replymsg->ChmodBody.amode, 1);
		break;

	      case PDS_STAT_OP:
		/* determine if data is returned */
		if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		  tcode = DCE_pkmodet(&replymsg->StatBody.mode, 1);
	      }
	}


      /* send message to specified client */

      if (tcode == PIOUS_OK)
	tcode = DCE_send(clientid, msgtag_encode(replyop));

      /* free send buffer */

      if (DCE_freesendbuf() != PIOUS_OK)
	tcode = PIOUS_ETPORT;

      /* set result code */

      switch(tcode)
	{ /* tcode is PIOUS_OK or result of a failed DCE function */
	case PIOUS_OK:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_ESRCDEST:
	  rcode = tcode;
	  break;
	default:
	  /* any other error code indicates a bug in PIOUS */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * PDSMSG_reply_recv() - See pds_msg_exchange.h for description
 */

#ifdef __STDC__
int PDSMSG_reply_recv(dce_srcdestt pdsid,
		      int replyop,
		      pdsmsg_replyt *replymsg,
		      struct PDS_vbuf_dscrp *vbuf)
#else
int PDSMSG_reply_recv(pdsid, replyop, replymsg, vbuf)
     dce_srcdestt pdsid;
     int replyop;
     pdsmsg_replyt *replymsg;
     struct PDS_vbuf_dscrp *vbuf;
#endif
{
  int rcode, tcode;
  dce_srcdestt msgsrc;
  dce_msgtagt msgtag;

  int firstblk_sz, lastblk_sz, middleblk_cnt;
  char *bufptr;

  /* validate 'replyop' argument */

  if (!PdsOp(replyop))
    rcode = PIOUS_EINVAL;

  /* receive reply from server */

  else
    {
      if ((tcode = DCE_recv(pdsid, msgtag_encode(replyop),
			    &msgsrc, &msgtag, DCE_BLOCK)) == PIOUS_OK)
	{
	  /* case : transaction operation */

	  if (PdsTransop(replyop))
	    { /* unpack message header */

	      if ((tcode = DCE_upktransidt(&replymsg->TransopHead.transid,
					   1)) == PIOUS_OK &&

		  (tcode = DCE_upkint(&replymsg->TransopHead.transsn,
				      1)) == PIOUS_OK &&

		  (tcode = DCE_upklong(&replymsg->TransopHead.rcode,
				       1)) == PIOUS_OK)

		switch(replyop)
		  { /* unpack message body */

		  case PDS_READ_OP:
		    /* determine if (transid, transsn) match - if so, place any
		     * data returned into specified buffer
		     */

		    replymsg->ReadBody.buf = NULL;

		    if (!transid_eq(vbuf->transid,
				    replymsg->TransopHead.transid) ||
			vbuf->transsn != replymsg->TransopHead.transsn)
		      { /* (transid, transsn) mismatch; do not receive data */
			tcode = PIOUS_EPERM;
		      }

		    else if (replymsg->TransopHead.rcode > 0)
		      { /* unpack/fill data into vector buffer */

			if (vbuf->firstblk_netsz >=
			    replymsg->TransopHead.rcode ||
			    vbuf->stride == 1)
			  { /* contiguous data region to fill */
			    tcode =
			      DCE_upkbyte(vbuf->firstblk_ptr,
					  (int)replymsg->TransopHead.rcode);
			  }

			else if (vbuf->blksz == 1)
			  { /* regular non-contiguous data region to fill */
			    tcode =
			      DCE_upkbyte_blk(vbuf->firstblk_ptr,
					      1,
					      vbuf->stride,
					      (int)
					      replymsg->TransopHead.rcode);
			  }

			else
			  { /* (potentially) irregular non-contiguous data */

			    firstblk_sz   = vbuf->firstblk_netsz;
			    middleblk_cnt = ((replymsg->TransopHead.rcode -
					      firstblk_sz) / vbuf->blksz);
			    lastblk_sz    = (replymsg->TransopHead.rcode -
					     (firstblk_sz +
					      (middleblk_cnt * vbuf->blksz)));

			    bufptr        = vbuf->firstblk_ptr;

			    /* unpack data into first vector buffer block */

			    tcode   = DCE_upkbyte(bufptr, firstblk_sz);

			    bufptr += (firstblk_sz +
				       ((vbuf->stride - 1) * vbuf->blksz));

			    /* unpack data into "middle" buffer blocks */

			    if (tcode == PIOUS_OK && middleblk_cnt > 0)
			      {
				tcode = DCE_upkbyte_blk(bufptr,
							vbuf->blksz,
							vbuf->stride,
							middleblk_cnt);

				bufptr += (vbuf->stride * vbuf->blksz *
					   middleblk_cnt);
			      }

			    /* unpack data into last vector buffer block */

			    if (tcode == PIOUS_OK && lastblk_sz > 0)
			      {
				tcode = DCE_upkbyte(bufptr, lastblk_sz);
			      }
			  }
		      }

		    break;

		  case PDS_READ_SINT_OP:
		    /* determine if (transid, transsn) match - if so, place any
		     * data returned into specified buffer
		     */

		    replymsg->ReadsintBody.buf = NULL;

		    if (!transid_eq(vbuf->transid,
				    replymsg->TransopHead.transid) ||
			vbuf->transsn != replymsg->TransopHead.transsn)
		      { /* (transid, transsn) mismatch; do not receive data */
			tcode = PIOUS_EPERM;
		      }

		    else if (replymsg->TransopHead.rcode > 0)
		      { /* unpack/fill data into contiguous buffer */
			tcode = DCE_upklong((long *)vbuf->firstblk_ptr,
					    (int)replymsg->TransopHead.rcode);
		      }

		    break;

		  case PDS_FA_SINT_OP:
		    /* determine if data is returned */
		    if (replymsg->TransopHead.rcode == PIOUS_OK)
		      tcode = DCE_upklong(&replymsg->FasintBody.rvalue, 1);
		    break;
		  }
	    }

	  /* case: control operation */

	  else
	    { /* unpack message header */

	      if ((tcode = DCE_upkint(&replymsg->CntrlopHead.cmsgid,
				      1)) == PIOUS_OK &&

		  (tcode = DCE_upklong(&replymsg->CntrlopHead.rcode,
				       1)) == PIOUS_OK)

		switch(replyop)
		  { /* unpack message body */

		  case PDS_LOOKUP_OP:
		    /* determine if data is returned */
		    if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		      {
			if ((tcode =
			     DCE_upkfhandlet(&replymsg->LookupBody.fhandle,
					     1)) == PIOUS_OK &&

			    (tcode =
			     DCE_upkint(&replymsg->LookupBody.amode, 1)));
		      }
		    break;

		  case PDS_CHMOD_OP:
		    /* determine if data is returned */
		    if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		      tcode = DCE_upkint(&replymsg->ChmodBody.amode, 1);
		    break;

		  case PDS_STAT_OP:
		    /* determine if data is returned */
		    if (replymsg->CntrlopHead.rcode == PIOUS_OK)
		      tcode = DCE_upkmodet(&replymsg->StatBody.mode, 1);
		    break;
		  }
	    }
	}

      /* free receive buffer */

      if (DCE_freerecvbuf() != PIOUS_OK)
	tcode = PIOUS_ETPORT;

      /* set result code */
      switch(tcode)
	{ /* tcode is PIOUS_OK, or the result of a failed DCE function,
	   * or PIOUS_EPERM if there is a (transid, transsn) mismatch when
	   * receiving a read reply.
	   */
	case PIOUS_OK:
	case PIOUS_ESRCDEST:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EPERM:
	  rcode = tcode;
	  break;
	default:
	  /* any other error code indicates a bug in PIOUS */
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}
