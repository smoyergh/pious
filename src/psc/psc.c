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




/* PIOUS Service Coordinator (PSC): Remote Procedure Call Interface
 *
 * @(#)psc.c	2.2  28 Apr 1995  Moyer
 *
 *   The psc module provides a remote procedure call interface to operations
 *   provided by the PSC daemon.
 *
 *
 * Function Summary:
 *
 * PSC_{s}open();
 * PSC_{s}close();
 * PSC_{s}chmod();
 * PSC_{s}unlink();
 * PSC_{s}mkdir();
 * PSC_{s}rmdir();
 * PSC_{s}chmoddir();
 * PSC_config();
 * PSC_shutdown();
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) Only parameters that effect the ability to send/receive messages
 *      or return results are validated; actual function parameters checked
 *      by the PSC daemon.
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

#include "psc_msg_exchange.h"

#include "psc.h"


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



/* Function Definitions - PSC operations */


/*
 * PSC_{s}open() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_sopen(struct PSC_dsinfo *pds,
	      char *group,
	      char *path,
	      int view,
	      int faultmode,
	      int oflag,
	      pious_modet mode,
	      int seg,

	      struct PSC_pfinfo *pf_info)
#else
int PSC_sopen(pds, group, path, view, faultmode, oflag, mode, seg, pf_info)
     struct PSC_dsinfo *pds;
     char *group;
     char *path;
     int view;
     int faultmode;
     int oflag;
     pious_modet mode;
     int seg;

     struct PSC_pfinfo *pf_info;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (group == NULL || path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_OPEN_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SOPEN_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.open.view      = view;
      reqmsg.body.open.oflag     = oflag;
      reqmsg.body.open.faultmode = faultmode;
      reqmsg.body.open.mode      = mode;
      reqmsg.body.open.seg       = seg;
      reqmsg.body.open.group     = group;
      reqmsg.body.open.path      = path;


      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  if ((rcode = replymsg.rcode) == PIOUS_OK)
	    { /* function results valid */
	      pf_info->pds_cnt      = replymsg.body.open.pds_cnt;
	      pf_info->seg_cnt      = replymsg.body.open.seg_cnt;
	      pf_info->sptr_fhandle = replymsg.body.open.sptr_fhandle;
	      pf_info->sptr_idx     = replymsg.body.open.sptr_idx;
	      pf_info->seg_fhandle  = replymsg.body.open.seg_fhandle;
	      pf_info->pds_id       = replymsg.body.open.pds_id;
	    }
	}
    }

  return rcode;
}




/*
 * PSC_{s}close() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_sclose(struct PSC_dsinfo *pds,
	       char *group,
	       char *path)
#else
int PSC_sclose(pds, group, path)
     struct PSC_dsinfo *pds;
     char *group;
     char *path;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (group == NULL || path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_CLOSE_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SCLOSE_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.close.group = group;
      reqmsg.body.close.path  = path;


      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_{s}chmod() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_schmod(struct PSC_dsinfo *pds,
	       char *path,
	       pious_modet mode)
#else
int PSC_schmod(pds, path, mode)
     struct PSC_dsinfo *pds;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_CHMOD_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SCHMOD_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.chmod.path  = path;
      reqmsg.body.chmod.mode  = mode;


      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_{s}unlink() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_sunlink(struct PSC_dsinfo *pds,
		char *path)
#else
int PSC_sunlink(pds, path)
     struct PSC_dsinfo *pds;
     char *path;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_UNLINK_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SUNLINK_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.unlink.path = path;

      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_{s}mkdir() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_smkdir(struct PSC_dsinfo *pds,
	       char *path,
	       pious_modet mode)
#else
int PSC_smkdir(pds, path, mode)
     struct PSC_dsinfo *pds;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_MKDIR_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SMKDIR_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.mkdir.path  = path;
      reqmsg.body.mkdir.mode  = mode;


      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_{s}rmdir() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_srmdir(struct PSC_dsinfo *pds,
	       char *path)
#else
int PSC_srmdir(pds, path)
     struct PSC_dsinfo *pds;
     char *path;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_RMDIR_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SRMDIR_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.rmdir.path  = path;

      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_{s}chmoddir() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_schmoddir(struct PSC_dsinfo *pds,
		  char *path,
		  pious_modet mode)
#else
int PSC_schmoddir(pds, path, mode)
     struct PSC_dsinfo *pds;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, mcode, pscop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (path == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if (pds == NULL)
	{ /* standard version of function */
	  pscop = PSC_CHMODDIR_OP;
	}

      else
	{ /* server version of function */
	  pscop = PSC_SCHMODDIR_OP;

	  reqmsg.pds_cnt   = pds->cnt;
	  reqmsg.pds_hname = pds->hname;
	  reqmsg.pds_spath = pds->spath;
	  reqmsg.pds_lpath = pds->lpath;
	}

      reqmsg.body.chmoddir.path  = path;
      reqmsg.body.chmoddir.mode  = mode;


      if ((mcode = PSCMSG_req_send(pscop, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(pscop, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  rcode = replymsg.rcode;
	}
    }

  return rcode;
}




/*
 * PSC_config() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_config(struct PSC_configinfo *buf)
#else
int PSC_config(buf)
     struct PSC_configinfo *buf;
#endif
{
  int rcode, mcode;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;

  /* validate ptr arguments */

  if (buf == NULL)
    rcode = PIOUS_EINVAL;

  else
    { /* set message fields and send to PSC */

      if ((mcode = PSCMSG_req_send(PSC_CONFIG_OP, &reqmsg)) != PIOUS_OK ||
	  (mcode = PSCMSG_reply_recv(PSC_CONFIG_OP, &replymsg)) != PIOUS_OK)
	{ /* send/receive failed; set result code */
	  switch(mcode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	      rcode = mcode;
	      break;
	    default:
	      /* PIOUS_ESRCDEST indicates that PSC can not be located.
               * PIOUS_EINVAL or other error indicates a bug in PIOUS code
	       */
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* received reply from PSC; set operation results */
	  if ((rcode = replymsg.rcode) == PIOUS_OK)
	    { /* function results valid */
	      buf->def_pds_cnt = replymsg.body.config.def_pds_cnt;
	    }
	}
    }

  return rcode;
}




/*
 * PSC_shutdown() - See psc.h for description.
 */

#ifdef __STDC__
int PSC_shutdown(void)
#else
int PSC_shutdown()
#endif
{
  int rcode, mcode;
  pscmsg_reqt reqmsg;

  /* set message fields and send to PSC; no reply expected */

  mcode = PSCMSG_req_send(PSC_SHUTDOWN_OP, &reqmsg);

  switch(mcode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
      rcode = PIOUS_OK;
      break;
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = mcode;
      break;
    default:
      /* PIOUS_EINVAL or other error indicates a bug in PIOUS code */
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}
