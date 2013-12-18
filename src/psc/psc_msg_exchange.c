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
 * @(#)psc_msg_exchange.c	2.2  28 Apr 1995  Moyer
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
#include "pious_sysconfig.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"

#include "pdce_srcdestt.h"
#include "pdce_msgtagt.h"
#include "pdce.h"

#include "psc_msg_exchange.h"

/*
 * Private Declarations - Types and Constants
 */


/*
 * Private Variable Definitions
 */

static int msgexchange_initialized = FALSE;

static dce_srcdestt psc_msgid;




/* Local Function Declarations */

#ifdef __STDC__
static int pack_serverinfo(pscmsg_reqt *reqmsg);

static int unpack_serverinfo(pscmsg_reqt *reqmsg);

static void dealloc_serverinfo(pscmsg_reqt *reqmsg);
#else
static int pack_serverinfo();

static int unpack_serverinfo();

static void dealloc_serverinfo();
#endif




/* Function Definitions - MSG passing operations */


/*
 * PSCMSG_req_send() - See psc_msg_exchange.h for description
 */

#ifdef __STDC__
int PSCMSG_req_send(int reqop,
		    pscmsg_reqt *reqmsg)
#else
int PSCMSG_req_send(reqop, reqmsg)
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, tcode;
  int pathlen, grouplen;

  /* validate 'reqop' argument */

  rcode = PIOUS_OK;

  if (!PscOp(reqop))
    rcode = PIOUS_EINVAL;

  /* initialize PSC message passing id */

  else if (!msgexchange_initialized)
    {
      if ((tcode = DCE_locate(PSC_DAEMON_EXEC, &psc_msgid)) == PIOUS_OK)
	msgexchange_initialized = TRUE;

      else if (tcode == PIOUS_EINVAL)
	rcode = PIOUS_ESRCDEST;

      else if (tcode == PIOUS_ETPORT)
	rcode = PIOUS_ETPORT;

      else
	rcode = PIOUS_EUNXP;
    }

  /* send request to server */

  if (rcode == PIOUS_OK)
    { /* initialize send buffer and pack message header */

      if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	  (tcode = (PscServerOp(reqop) ?
		    pack_serverinfo(reqmsg) : PIOUS_OK)) == PIOUS_OK)

	switch(reqop)
	  { /* pack message body */

	  case PSC_OPEN_OP:
	  case PSC_SOPEN_OP:
	    pathlen  = strlen(reqmsg->body.open.path) + 1;
	    grouplen = strlen(reqmsg->body.open.group) + 1;

	    if ((tcode = DCE_pkint(&reqmsg->body.open.view, 1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&reqmsg->body.open.oflag, 1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&reqmsg->body.open.faultmode,
				   1)) == PIOUS_OK &&

		(tcode = DCE_pkmodet(&reqmsg->body.open.mode,
				     1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&reqmsg->body.open.seg, 1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&grouplen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.open.group,
				    grouplen)) == PIOUS_OK &&

		(tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.open.path, pathlen)));
	    break;

	  case PSC_CLOSE_OP:
	  case PSC_SCLOSE_OP:
	    pathlen  = strlen(reqmsg->body.close.path) + 1;
	    grouplen = strlen(reqmsg->body.close.group) + 1;

	    if ((tcode = DCE_pkint(&grouplen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.close.group,
				    grouplen)) == PIOUS_OK &&

		(tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.close.path, pathlen)));
	    break;

	  case PSC_MKDIR_OP:
	  case PSC_SMKDIR_OP:
	    pathlen = strlen(reqmsg->body.mkdir.path) + 1;

	    if ((tcode = DCE_pkmodet(&reqmsg->body.mkdir.mode,
				     1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.mkdir.path, pathlen)));
	    break;

	  case PSC_RMDIR_OP:
	  case PSC_SRMDIR_OP:
	    pathlen = strlen(reqmsg->body.rmdir.path) + 1;

	    if ((tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.rmdir.path, pathlen)));
	    break;

	  case PSC_CHMODDIR_OP:
	  case PSC_SCHMODDIR_OP:
	    pathlen = strlen(reqmsg->body.chmoddir.path) + 1;

	    if ((tcode = DCE_pkmodet(&reqmsg->body.chmoddir.mode,
				     1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.chmoddir.path, pathlen)));
	    break;

	  case PSC_UNLINK_OP:
	  case PSC_SUNLINK_OP:
	    pathlen = strlen(reqmsg->body.unlink.path) + 1;

	    if ((tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.unlink.path, pathlen)));
	    break;

	  case PSC_CHMOD_OP:
	  case PSC_SCHMOD_OP:
	    pathlen = strlen(reqmsg->body.chmod.path) + 1;

	    if ((tcode = DCE_pkmodet(&reqmsg->body.chmod.mode,
				     1)) == PIOUS_OK &&

		(tcode = DCE_pkint(&pathlen, 1)) == PIOUS_OK &&

		(tcode = DCE_pkchar(reqmsg->body.chmod.path, pathlen)));
	    break;
	  }

      /* send message to server */

      if (tcode == PIOUS_OK)
	tcode = DCE_send(psc_msgid, msgtag_encode(reqop));

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
 * PSCMSG_req_recv() - See psc_msg_exchange.h for description
 */

#ifdef __STDC__
int PSCMSG_req_recv(dce_srcdestt *clientid,
		    int *reqop,
		    pscmsg_reqt *reqmsg,
		    int timeout)
#else
int PSCMSG_req_recv(clientid, reqop, reqmsg, timeout)
     dce_srcdestt *clientid;
     int *reqop;
     pscmsg_reqt *reqmsg;
     int timeout;
#endif
{
  int rcode, tcode;
  dce_srcdestt msgsrc;
  dce_msgtagt msgtag;
  int pathlen, grouplen;

  /* wait for next client request with a valid message tag */

  while ((tcode = DCE_recv(DCE_ANY_SRC, DCE_ANY_TAG,
			   &msgsrc, &msgtag, timeout)) == PIOUS_OK &&
	 (!msgtag_valid(msgtag) || !PscOp(msgtag_decode(msgtag))) &&
	 (tcode = DCE_freerecvbuf()) == PIOUS_OK);

  /* unpack message */

  if (tcode == PIOUS_OK)
    { /* set 'clientid' and 'reqop' */

      *clientid = msgsrc;
      *reqop    = msgtag_decode(msgtag);

      /* unpack message header */

      if ((tcode = (PscServerOp(*reqop) ?
		    unpack_serverinfo(reqmsg) : PIOUS_OK)) == PIOUS_OK)

	switch(*reqop)
	  { /* unpack message body */

	  case PSC_OPEN_OP:
	  case PSC_SOPEN_OP:
	    reqmsg->body.open.group = reqmsg->body.open.path = NULL;

	    if ((tcode = DCE_upkint(&reqmsg->body.open.view, 1)) != PIOUS_OK ||

		(tcode = DCE_upkint(&reqmsg->body.open.oflag,
				    1)) != PIOUS_OK ||

		(tcode = DCE_upkint(&reqmsg->body.open.faultmode,
				    1)) != PIOUS_OK ||

		(tcode = DCE_upkmodet(&reqmsg->body.open.mode,
				      1)) != PIOUS_OK ||

		(tcode = DCE_upkint(&reqmsg->body.open.seg, 1)) != PIOUS_OK ||

		/* alloc/unpack group name */

		(tcode = DCE_upkint(&grouplen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.open.group =
			   malloc((unsigned)grouplen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.open.group,
				     grouplen)) != PIOUS_OK ||

		/* alloc/unpack path name */

		(tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.open.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.open.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.open.group != NULL)
		  free(reqmsg->body.open.group);

		if (reqmsg->body.open.path != NULL)
		  free(reqmsg->body.open.path);

		if (*reqop == PSC_SOPEN_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_CLOSE_OP:
	  case PSC_SCLOSE_OP:
	    reqmsg->body.close.group = reqmsg->body.close.path = NULL;

	    /* alloc/unpack group */
	    if ((tcode = DCE_upkint(&grouplen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.close.group =
			   malloc((unsigned)grouplen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.close.group,
				     grouplen)) != PIOUS_OK ||

		/* alloc/unpack path */

		(tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.close.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.close.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.close.group != NULL)
		  free(reqmsg->body.close.group);

		if (reqmsg->body.close.path != NULL)
		  free(reqmsg->body.close.path);

		if (*reqop == PSC_SCLOSE_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_MKDIR_OP:
	  case PSC_SMKDIR_OP:
	    reqmsg->body.mkdir.path = NULL;

	    if ((tcode = DCE_upkmodet(&reqmsg->body.mkdir.mode,
				      1)) != PIOUS_OK ||

		/* alloc/unpack path */

		(tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.mkdir.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.mkdir.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.mkdir.path != NULL)
		  free(reqmsg->body.mkdir.path);

		if (*reqop == PSC_SMKDIR_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_RMDIR_OP:
	  case PSC_SRMDIR_OP:
	    reqmsg->body.rmdir.path = NULL;

	    /* alloc/unpack path */
	    if ((tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.rmdir.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.rmdir.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.rmdir.path != NULL)
		  free(reqmsg->body.rmdir.path);

		if (*reqop == PSC_SRMDIR_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_CHMODDIR_OP:
	  case PSC_SCHMODDIR_OP:
	    reqmsg->body.chmoddir.path = NULL;

	    if ((tcode = DCE_upkmodet(&reqmsg->body.chmoddir.mode,
				      1)) != PIOUS_OK ||

		/* alloc/unpack path */

		(tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.chmoddir.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.chmoddir.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.chmoddir.path != NULL)
		  free(reqmsg->body.chmoddir.path);

		if (*reqop == PSC_SCHMODDIR_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_UNLINK_OP:
	  case PSC_SUNLINK_OP:
	    reqmsg->body.unlink.path = NULL;

	    /* alloc/unpack path */
	    if ((tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.unlink.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.unlink.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.unlink.path != NULL)
		  free(reqmsg->body.unlink.path);

		if (*reqop == PSC_SUNLINK_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;

	  case PSC_CHMOD_OP:
	  case PSC_SCHMOD_OP:
	    reqmsg->body.chmod.path = NULL;

	    if ((tcode = DCE_upkmodet(&reqmsg->body.chmod.mode,
				      1)) != PIOUS_OK ||

		/* alloc/unpack path */

		(tcode = DCE_upkint(&pathlen, 1)) != PIOUS_OK ||

		(tcode = ((reqmsg->body.chmod.path =
			   malloc((unsigned)pathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

		(tcode = DCE_upkchar(reqmsg->body.chmod.path,
				     pathlen)) != PIOUS_OK)
	      { /* error; deallocate storage */
		if (reqmsg->body.chmod.path != NULL)
		  free(reqmsg->body.chmod.path);

		if (*reqop == PSC_SCHMOD_OP)
		  dealloc_serverinfo(reqmsg);
	      }

	    break;
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
 * PSCMSG_reply_send() - See psc_msg_exchange.h for description
 */

#ifdef __STDC__
int PSCMSG_reply_send(dce_srcdestt clientid,
		      int replyop,
		      pscmsg_replyt *replymsg)
#else
int PSCMSG_reply_send(clientid, replyop, replymsg)
     dce_srcdestt clientid;
     int replyop;
     pscmsg_replyt *replymsg;
#endif
{
  int rcode, tcode;

  /* validate 'replyop' argument */

  if (!PscOp(replyop))
    rcode = PIOUS_EINVAL;

  /* send reply to client */

  else
    { /* initialize send buffer and pack message header */

      if ((tcode = DCE_mksendbuf()) == PIOUS_OK &&

	  (tcode = DCE_pklong(&replymsg->rcode, 1)) == PIOUS_OK)

	switch(replyop)
	  { /* pack message body */

	  case PSC_OPEN_OP:
	  case PSC_SOPEN_OP:
	    /* determine if data is returned */
	    if (replymsg->rcode == PIOUS_OK)
	      {
		if ((tcode = DCE_pkint(&replymsg->body.open.pds_cnt,
				       1)) == PIOUS_OK &&

		    (tcode = DCE_pkint(&replymsg->body.open.seg_cnt,
				       1)) == PIOUS_OK &&

		    (tcode = DCE_pkfhandlet(&replymsg->body.open.sptr_fhandle,
					    1)) == PIOUS_OK &&

		    (tcode = DCE_pkofft(&replymsg->body.open.sptr_idx,
					1)) == PIOUS_OK &&

		    (tcode = DCE_pkfhandlet(replymsg->body.open.seg_fhandle,
					    replymsg->body.open.seg_cnt))
		    == PIOUS_OK &&

		    (tcode = DCE_pksrcdestt(replymsg->body.open.pds_id,
					    replymsg->body.open.pds_cnt)));
	      }
	    break;

	  case PSC_CONFIG_OP:
	    /* determine if data is returned */
	    if (replymsg->rcode == PIOUS_OK)
	      tcode = DCE_pkint(&replymsg->body.config.def_pds_cnt, 1);

	    break;
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
 * PSCMSG_reply_recv() - See psc_msg_exchange.h for description
 */

#ifdef __STDC__
int PSCMSG_reply_recv(int replyop,
		      pscmsg_replyt *replymsg)
#else
int PSCMSG_reply_recv(replyop, replymsg)
     int replyop;
     pscmsg_replyt *replymsg;
#endif
{
  int rcode, tcode;
  dce_srcdestt msgsrc;
  dce_msgtagt msgtag;

  /* validate 'replyop' argument */

  rcode = PIOUS_OK;

  if (!PscOp(replyop))
    rcode = PIOUS_EINVAL;

  /* initialize PSC message passing id */

  else if (!msgexchange_initialized)
    {
      if ((tcode = DCE_locate(PSC_DAEMON_EXEC, &psc_msgid)) == PIOUS_OK)
	msgexchange_initialized = TRUE;

      else if (tcode == PIOUS_EINVAL)
	rcode = PIOUS_ESRCDEST;

      else if (tcode == PIOUS_ETPORT)
	rcode = PIOUS_ETPORT;

      else
	rcode = PIOUS_EUNXP;
    }

  /* receive reply from server */

  if (rcode == PIOUS_OK)
    {
      if ((tcode = DCE_recv(psc_msgid, msgtag_encode(replyop),
			    &msgsrc, &msgtag, DCE_BLOCK)) == PIOUS_OK)
	{ /* unpack message header */

	  if ((tcode = DCE_upklong(&replymsg->rcode, 1)) == PIOUS_OK)

	    switch(replyop)
	      { /* unpack message body */

	      case PSC_OPEN_OP:
	      case PSC_SOPEN_OP:
		replymsg->body.open.seg_fhandle = NULL;
		replymsg->body.open.pds_id      = NULL;

		/* determine if data is returned */
		if (replymsg->rcode == PIOUS_OK)
		  {
		    if ((tcode = DCE_upkint(&replymsg->body.open.pds_cnt,
					    1)) != PIOUS_OK ||

			(tcode = DCE_upkint(&replymsg->body.open.seg_cnt,
					    1)) != PIOUS_OK ||

			(tcode =
			 DCE_upkfhandlet(&replymsg->body.open.sptr_fhandle,
					 1)) != PIOUS_OK ||

			(tcode = DCE_upkofft(&replymsg->body.open.sptr_idx,
					     1)) != PIOUS_OK ||

			/* allocate storage for the segment fhandle array */
			(tcode =
			 ((replymsg->body.open.seg_fhandle = (pds_fhandlet *)
			   malloc((unsigned)(replymsg->body.open.seg_cnt *
					     sizeof(pds_fhandlet)))) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

			(tcode =
			 DCE_upkfhandlet(replymsg->body.open.seg_fhandle,
					 replymsg->body.open.seg_cnt))
			!= PIOUS_OK ||

			/* allocate storage for the message id array */
			(tcode =
			 ((replymsg->body.open.pds_id = (dce_srcdestt *)
			   malloc((unsigned)(replymsg->body.open.pds_cnt *
					     sizeof(dce_srcdestt)))) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) != PIOUS_OK ||

			(tcode =
			 DCE_upksrcdestt(replymsg->body.open.pds_id,
					 replymsg->body.open.pds_cnt))
			!= PIOUS_OK)
		      { /* error; deallocate storage */
			if (replymsg->body.open.seg_fhandle != NULL)
			  free((char *)replymsg->body.open.seg_fhandle);

			if (replymsg->body.open.pds_id != NULL)
			  free((char *)replymsg->body.open.pds_id);
		      }
		  }
		break;

	      case PSC_CONFIG_OP:
		/* determine if data is returned */
		if (replymsg->rcode == PIOUS_OK)
		  tcode = DCE_upkint(&replymsg->body.config.def_pds_cnt, 1);
		break;
	      }
	}

      /* free receive buffer */

      if (DCE_freerecvbuf() != PIOUS_OK)
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
 * Local Function definitions
 */


/*
 * pack_serverinfo()
 *
 * Parameters:
 *
 *   reqmsg - request message
 *
 * Pack PDS server information from request message 'reqmsg' for "server"
 * version of PSC function.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - pack_serverinfo() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no send buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
static int pack_serverinfo(pscmsg_reqt *reqmsg)
#else
static int pack_serverinfo(reqmsg)
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, tcode;
  int i, hnamelen, spathlen, lpathlen;

  if ((tcode = DCE_pkint(&reqmsg->pds_cnt, 1)) == PIOUS_OK)

    for (i = 0; i < reqmsg->pds_cnt && tcode == PIOUS_OK; i++)
      {
	hnamelen = strlen(reqmsg->pds_hname[i]) + 1;
	spathlen = strlen(reqmsg->pds_spath[i]) + 1;
	lpathlen = strlen(reqmsg->pds_lpath[i]) + 1;

	if ((tcode = DCE_pkint(&hnamelen, 1)) == PIOUS_OK &&

	    (tcode = DCE_pkchar(reqmsg->pds_hname[i], hnamelen)) == PIOUS_OK &&

	    (tcode = DCE_pkint(&spathlen, 1)) == PIOUS_OK &&

	    (tcode = DCE_pkchar(reqmsg->pds_spath[i], spathlen)) == PIOUS_OK &&

	    (tcode = DCE_pkint(&lpathlen, 1)) == PIOUS_OK &&

	    (tcode = DCE_pkchar(reqmsg->pds_lpath[i], lpathlen)));
      }

  /* set result code */

  switch(tcode)
    { /* tcode is PIOUS_OK or result of a failed DCE_pk*() function */
    case PIOUS_OK:
    case PIOUS_EINSUF:
    case PIOUS_EPERM:
      rcode = tcode;
      break;
    default:
      rcode = PIOUS_ETPORT;
      break;
    }

  return rcode;
}




/*
 * unpack_serverinfo()
 *
 * Parameters:
 *
 *   reqmsg - request message
 *
 * Unpack PDS server information into request message 'reqmsg' for "server"
 * version of PSC function.
 *
 * Note: if error occurs, storage is deallocated and server information
 *       pointers in 'reqmsg' are set to NULL.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - unpack_serverinfo() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no send buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
static int unpack_serverinfo(pscmsg_reqt *reqmsg)
#else
static int unpack_serverinfo(reqmsg)
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, tcode;
  int i, hnamelen, spathlen, lpathlen;

  reqmsg->pds_hname = reqmsg->pds_spath = reqmsg->pds_lpath = NULL;

  if ((tcode = DCE_upkint(&reqmsg->pds_cnt, 1)) == PIOUS_OK)
    { /* allocate hname, spath, lpath string pointer storage */
      if ((reqmsg->pds_hname = (char **)
	   malloc((unsigned)(reqmsg->pds_cnt * sizeof(char *)))) == NULL ||

	  (reqmsg->pds_spath = (char **)
	   malloc((unsigned)(reqmsg->pds_cnt * sizeof(char *)))) == NULL ||

	  (reqmsg->pds_lpath = (char **)
	   malloc((unsigned)(reqmsg->pds_cnt * sizeof(char *)))) == NULL)
	tcode = PIOUS_EINSUF;

      else
	{ /* set hname, spath, lpath char ptrs NULL so can dealloc if error */
	  for (i = 0; i < reqmsg->pds_cnt; i++)
	    reqmsg->pds_hname[i] =
	      reqmsg->pds_spath[i] = reqmsg->pds_lpath[i] = NULL;

	  /* unpack hname, spath, lpath strings */

	  for (i = 0; i < reqmsg->pds_cnt && tcode == PIOUS_OK; i++)
	    if (/* alloc/unpack hname */

		(tcode = DCE_upkint(&hnamelen, 1)) == PIOUS_OK &&

		(tcode = ((reqmsg->pds_hname[i] =
			   malloc((unsigned)hnamelen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) == PIOUS_OK &&

		(tcode = DCE_upkchar(reqmsg->pds_hname[i],
				     hnamelen)) == PIOUS_OK &&

		/* alloc/unpack spath */

		(tcode = DCE_upkint(&spathlen, 1)) == PIOUS_OK &&

		(tcode = ((reqmsg->pds_spath[i] =
			   malloc((unsigned)spathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) == PIOUS_OK &&

		(tcode = DCE_upkchar(reqmsg->pds_spath[i],
				     spathlen)) == PIOUS_OK &&

		/* alloc/unpack lpath */

		(tcode = DCE_upkint(&lpathlen, 1)) == PIOUS_OK &&

		(tcode = ((reqmsg->pds_lpath[i] =
			   malloc((unsigned)lpathlen)) == NULL ?
			  PIOUS_EINSUF : PIOUS_OK)) == PIOUS_OK &&

		(tcode = DCE_upkchar(reqmsg->pds_lpath[i], lpathlen)));

	  /* deallocate string storage if error */
	  if (tcode != PIOUS_OK)
	    for (i = 0; i < reqmsg->pds_cnt; i++)
	      {
		if (reqmsg->pds_hname[i] != NULL)
		  free(reqmsg->pds_hname[i]);

		if (reqmsg->pds_spath[i] != NULL)
		  free(reqmsg->pds_spath[i]);

		if (reqmsg->pds_lpath[i] != NULL)
		  free(reqmsg->pds_lpath[i]);
	      }
	}
    }

  /* deallocate string pointer storage if error occured */

  if (tcode != PIOUS_OK)
    {
      if (reqmsg->pds_hname != NULL)
	free((char *)reqmsg->pds_hname);

      if (reqmsg->pds_spath != NULL)
	free((char *)reqmsg->pds_spath);

      if (reqmsg->pds_lpath != NULL)
	free((char *)reqmsg->pds_lpath);

      reqmsg->pds_hname = reqmsg->pds_spath = reqmsg->pds_lpath = NULL;
    }

  /* set result code */

  switch(tcode)
    { /* tcode is PIOUS_OK or result of a failed DCE_pk*() function */
    case PIOUS_OK:
    case PIOUS_EINSUF:
    case PIOUS_EPERM:
      rcode = tcode;
      break;
    default:
      rcode = PIOUS_ETPORT;
      break;
    }

  return rcode;
}




/*
 * dealloc_serverinfo()
 *
 * Parameters:
 *
 *   reqmsg - request message
 *
 * Deallocate PDS server information in request message 'reqmsg'.
 *
 * Note: presumes server information has been fully allocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void dealloc_serverinfo(pscmsg_reqt *reqmsg)
#else
static void dealloc_serverinfo(reqmsg)
     pscmsg_reqt *reqmsg;
#endif
{
  int i;

  /* deallocate string storage */

  for (i = 0; i < reqmsg->pds_cnt; i++)
    {
      free(reqmsg->pds_hname[i]);
      free(reqmsg->pds_spath[i]);
      free(reqmsg->pds_lpath[i]);
    }

  /* deallocate string pointer storage */

  free((char *)reqmsg->pds_hname);
  free((char *)reqmsg->pds_spath);
  free((char *)reqmsg->pds_lpath);

  reqmsg->pds_hname = reqmsg->pds_spath = reqmsg->pds_lpath = NULL;
}
