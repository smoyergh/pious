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




/* PIOUS Distributed Computing Environment (PDCE)
 *
 * @(#)pdce.c	2.2  28 Apr 1995  Moyer
 *
 * The PIOUS distributed computing environment (PDCE) interface provides
 * the following services for PIOUS components:
 *
 *   - reliable point to point message transport
 *   - service registration and location
 *   - task spawning
 *
 * PIOUS is easily ported to any DCE that provides the above services; only
 * pdce.c (and pdce_msgtagt, pdce_srcdestt) need be updated.
 *
 * Function Summary:
 *
 *   DCE_mksendbuf();
 *   DCE_pk*();
 *   DCE_pkbyte_blk();
 *   DCE_send();
 *   DCE_freesendbuf();
 *
 *   DCE_recv();
 *   DCE_upk*();
 *   DCE_upkbyte_blk();
 *   DCE_freerecvbuf();
 *
 *   DCE_register();
 *   DCE_locate();
 *   DCE_unregister();
 *
 *   DCE_spawn();
 *
 *   DCE_exit();
 *
 * ----------------------------------------------------------------------------
 * PVM Implementation Notes:
 *
 *   1) This implementation of the PDCE interface is built on top of
 *      the PVM 3 environment.
 *
 *   2) Performance of the PDCE interface can be improved by creating PVM
 *      send message buffers using the PvmDataInPlace encoding.  However,
 *      at this time, PvmDataInPlace is not yet fully implemented.
 *
 *   3) It might seem strange that there is no DCE_{u}pkstr() ({un}pack string)
 *      function when this would simplify passing strings in messages.
 *      The reason these functions were not implemented is that most of the
 *      time storage must be allocated when receiving a string, and it just
 *      doesn't seem appropriate to have the message passing layer perform
 *      this function, though it easily could.
 *
 *
 * ----------------------------------------------------------------------------
 * MPI Porting Notes:
 *
 *   Given the increasing popularity of MPI, it is reasonable to assume that
 *   PIOUS, specifically the PDCE interface, will eventually be ported to that
 *   environment.
 *
 *   Porting the PDCE interface to MPI should be trivial.  Here is how I
 *   believe it can easily be done:
 *
 *   Sending Messages:
 *
 *     DCE_mksendbuf() - informs system that a new derived data type is about
 *                       to be created for a new message to be sent.
 *
 *     DCE_pk*() - each pack operation augments the new derived data type
 *                 with a set of (type, displacement) pairs.  Note that
 *                 absolute addresses are supplied, so that when the message
 *                 is sent the buffer argument will be MPI_BOTTOM.
 *
 *     DCE_pkbyte_blk() - same as basic DCE_pk*()
 *
 *     DCE_send() - commit derived data type and send message.
 *
 *     DCE_freesendbuf() - free the (potentially uncommited) derived data type.
 *
 *
 *  Receiving Messages:
 *
 *     DCE_recv() - receive message as uninterpreted bytes into a buffer
 *                  allocated by the PDCE layer.
 *
 *     DCE_upk*() - unpack required data from receive buffer.
 *
 *     DCE_upkbyte_blk() - same as basic DCE_upk*().
 *
 *     DCE_freerecvbuf() - deallocate receive buffer.
 *
 *
 *  Other:
 *
 *     Functions relating to service registration/location and task spawning
 *     fall outside the domain of MPI and will have to be implemented in
 *     a system/environment dependent manner.
 *
 *
 * ----------------------------------------------------------------------------
 * General Porting Notes:
 *
 *   The PIOUS system makes no assumptions regarding the message passing
 *   layer beyond requiring reliable best-effort delivery.  There is one
 *   exception regarding the commit optimization (VTCOMMITNOACK): if the
 *   message passing layer does not preserve message ordering between a pair
 *   of communicating processes then it is possible, though unlikely, to lose
 *   file updates if an application does not close all of the files it writes.
 *
 *   Fortunately, most message passing systems maintain message ordering
 *   between communicating pairs so that this is not an issue.
 */




/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include <pvm3.h>

#include "gpmacro.h"
#include "gputil.h"

#include "pious_types.h"
#include "pious_errno.h"

#include "pds_fhandlet.h"
#include "pds_transidt.h"

#include "pdce_msgtagt.h"
#include "pdce_srcdestt.h"
#include "pdce.h"


/*
 * Private Variable Definitions
 */


/* DCE and buffer state */
static int dce_initialized = FALSE;

static int sendbuf_alloced = FALSE;
static int sendbufid, old_sendbufid;

static int recvbuf_alloced = FALSE;
static int recvbufid, old_recvbufid;


/* Local Function Declarations */

#ifdef __STDC__
static int dce_init(void);
#else
static int dce_init();
#endif


/* Function Definitions - PDCE Operations */


/*
 * DCE_mksendbuf() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_mksendbuf(void)
#else
int DCE_mksendbuf()
#endif
{
  int rcode;
  int encoding;

  /* verify that DCE is initialized and that buffer is not already alloced */

  if (!dce_initialized)
    rcode = dce_init();

  else if (sendbuf_alloced)
    rcode = PIOUS_EPERM;

  else
    rcode = PIOUS_OK;

  if (rcode == PIOUS_OK)
    { /* allocate a new send buffer */

#ifdef USEPVMRAW
      encoding = PvmDataRaw;
#else
      encoding = PvmDataDefault;
#endif

      if ((sendbufid = pvm_mkbuf(encoding)) < 0)
	{ /* buffer not allocated */
	  if (sendbufid == PvmNoMem)
	    rcode = PIOUS_EINSUF;
	  else
	    rcode = PIOUS_ETPORT;
	}

      /* switch active send buffer to newly allocated buffer */

      else if ((old_sendbufid = pvm_setsbuf(sendbufid)) < 0)
	{ /* unable to switch buffers */
	  pvm_freebuf(sendbufid);
	  rcode = PIOUS_ETPORT;
	}

      /* mark send buffer as valid */

      else
	sendbuf_alloced = TRUE;
    }

  return rcode;
}




/*
 * DCE_pk*() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_pkbyte(char *addr,
	       int nitem)
#else
int DCE_pkbyte(addr, nitem)
     char *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pkbyte(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pkchar(char *addr,
	       int nitem)
#else
int DCE_pkchar(addr, nitem)
     char *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pkbyte(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pkint(int *addr,
	      int nitem)
#else
int DCE_pkint(addr, nitem)
     int *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pkint(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pkuint(unsigned int *addr,
	       int nitem)
#else
int DCE_pkuint(addr, nitem)
     unsigned int *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pkuint(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pklong(long *addr,
	       int nitem)
#else
int DCE_pklong(addr, nitem)
     long *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pklong(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pkulong(unsigned long *addr,
		int nitem)
#else
int DCE_pkulong(addr, nitem)
     unsigned long *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else if ((pvmcode = pvm_pkulong(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_pkfhandlet(pds_fhandlet *addr,
		   int nitem)
#else
int DCE_pkfhandlet(addr, nitem)
     pds_fhandlet *addr;
     int nitem;
#endif
{
  int rcode, pvmcode, i;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;
  else
    { /* pack 'nitem' complete fhandles. note that this routine must violate
       * the pds_fhandlet abstraction; as discussed in pds/pds_fhandlet.h,
       * pdce.c is a "friend" of the pds_fhandlet ADT in the C++ sense.
       */

      rcode = PIOUS_OK;

      for (i = 0; i < nitem && rcode == PIOUS_OK; i++, addr++)
	if ((pvmcode = pvm_pkulong(&(addr->dev), 1, 1)) != PvmOk ||
	    (pvmcode = pvm_pkulong(&(addr->ino), 1, 1)) != PvmOk)
	  {
	    if (pvmcode == PvmNoMem)
	      rcode = PIOUS_EINSUF;
	    else
	      rcode = PIOUS_ETPORT;
	  }
    }

  return rcode;
}


#ifdef __STDC__
int DCE_pktransidt(pds_transidt *addr,
		   int nitem)
#else
int DCE_pktransidt(addr, nitem)
     pds_transidt *addr;
     int nitem;
#endif
{
  int rcode, pvmcode, i;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;
  else
    { /* pack 'nitem' complete transaction ids. note that this routine must
       * violate the pds_transidt abstraction; as discussed in
       * pds/pds_transidt.h, pdce.c is a "friend" of the pds_transidt ADT
       * in the C++ sense.
       */
      rcode = PIOUS_OK;

      for (i = 0; i < nitem && rcode == PIOUS_OK; i++, addr++)
	if ((pvmcode = pvm_pkulong(&(addr->hostid), 1, 1)) != PvmOk ||
	    (pvmcode = pvm_pklong(&(addr->procid), 1, 1))  != PvmOk ||
	    (pvmcode = pvm_pklong(&(addr->sec), 1, 1))     != PvmOk ||
	    (pvmcode = pvm_pklong(&(addr->usec), 1, 1))    != PvmOk)
	  {
	    if (pvmcode == PvmNoMem)
	      rcode = PIOUS_EINSUF;
	    else
	      rcode = PIOUS_ETPORT;
	  }
    }

  return rcode;
}




/*
 * DCE_pkbyte_blk() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_pkbyte_blk(char *addr,
		   int blksz,
		   int blkstride,
		   int nitem)
#else
int DCE_pkbyte_blk(addr, blksz, blkstride, nitem)
     char *addr;
     int blksz;
     int blkstride;
     int nitem;
#endif
{
  int rcode, pvmcode, addrinc, i;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* pack data */
  else
    { /* case: data can be packed with a single PVM call */

      if (blksz == 1)
	pvmcode = pvm_pkbyte(addr, nitem, blkstride);

      else if (blkstride == 1)
	pvmcode = pvm_pkbyte(addr, nitem * blksz, 1);

      else if (nitem == 1)
	pvmcode = pvm_pkbyte(addr, blksz, 1);

      /* case: each block must be packed separately */

      else
	{
	  pvmcode = PvmOk;
	  addrinc = blksz * blkstride;

	  for (i = 0; i < nitem && pvmcode == PvmOk; i++, addr += addrinc)
	    pvmcode = pvm_pkbyte(addr, blksz, 1);
	}

      if (pvmcode == PvmOk)
	rcode = PIOUS_OK;
      else if (pvmcode == PvmNoMem)
	rcode = PIOUS_EINSUF;
      else
	rcode = PIOUS_ETPORT;
    }

  return rcode;
}




/*
 * DCE_send() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_send(dce_srcdestt msgdest,
	     dce_msgtagt msgtag)
#else
int DCE_send(msgdest, msgtag)
     dce_srcdestt msgdest;
     dce_msgtagt msgtag;
#endif
{
  int rcode, pvmcode;

  /* check that send buf alloced */
  if (!sendbuf_alloced)
    rcode = PIOUS_EPERM;

  /* validate 'msgtag' parameter */
  else if ((int)msgtag < 0)
    rcode = PIOUS_EINVAL;

  /* transport send buffer to destination */
  else if ((pvmcode = pvm_send((int)msgdest, (int)msgtag)) == PvmOk)
    rcode = PIOUS_OK;

  /* set result code if error */
  else if (pvmcode == PvmBadParam)
    rcode = PIOUS_ESRCDEST;

  else
    rcode = PIOUS_ETPORT;

  return rcode;
}




/*
 * DCE_freesendbuf() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_freesendbuf(void)
#else
int DCE_freesendbuf()
#endif
{
  int rcode;

  rcode = PIOUS_OK;

  /* if send buffer is allocated, deallocate and reset active send buffer */
  if (sendbuf_alloced)
    {
      if (pvm_setsbuf(old_sendbufid) < 0 ||
	  pvm_freebuf(sendbufid) < 0)
	rcode = PIOUS_ETPORT;

      sendbuf_alloced = FALSE;
    }

  return rcode;
}




/*
 * DCE_recv() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_recv(dce_srcdestt msgsrc,
	     dce_msgtagt msgtag,
	     dce_srcdestt *recv_src,
	     dce_msgtagt *recv_tag,
	     int timeout)
#else
int DCE_recv(msgsrc, msgtag, recv_src, recv_tag, timeout)
     dce_srcdestt msgsrc;
     dce_msgtagt msgtag;
     dce_srcdestt *recv_src;
     dce_msgtagt *recv_tag;
     int timeout;
#endif
{
  int rcode;
  int mbytes, mtag, msrc;
  util_clockt timeclock;

  /* verify that DCE is initialized and that buffer is not already alloced */

  if (!dce_initialized)
    rcode = dce_init();

  else if (recvbuf_alloced)
    rcode = PIOUS_EPERM;

  else
    rcode = PIOUS_OK;


  /* receive message */

  if (rcode == PIOUS_OK)
    { /* validate 'msgtag' argument */

      if ((int)msgtag < 0 && (int)msgtag != DCE_ANY_TAG)
	rcode = PIOUS_EINVAL;

      /* save previous active receive buffer */

      else if ((old_recvbufid = pvm_setrbuf(0)) < 0)
	/* unable to switch buffers */
	rcode = PIOUS_ETPORT;

      /* wait to receive message, or until time-out  */

      else
	{
	  if (timeout < 0)
	    { /* do blocking receive */
	      recvbufid = pvm_recv((int)msgsrc, (int)msgtag);
	    }

	  else
	    { /* do timeout receive */

#ifdef USEPVM33FNS
	      struct timeval tmout;

	      tmout.tv_sec  = timeout / 1000;
	      tmout.tv_usec = (timeout - (tmout.tv_sec * 1000)) * 1000;

	      recvbufid = pvm_trecv((int)msgsrc, (int)msgtag, &tmout);
#else
	      /* mark time clock */
	      UTIL_clock_mark(&timeclock);

	      /* do non-blocking receive until time-out */
	      do
		recvbufid = pvm_nrecv((int)msgsrc, (int)msgtag);
	      while (recvbufid == 0 &&
		     UTIL_clock_delta(&timeclock, UTIL_MSEC) < timeout);
#endif
	    }

	  /* case: message NOT properly received */

	  if (recvbufid <= 0)
	    { /* reset active receive buffer */
	      if (pvm_setrbuf(old_recvbufid) < 0)
		rcode = PIOUS_ETPORT;

	      /* set result code */
	      else
		switch (recvbufid)
		  {
		  case 0:
		    /* time-out before message arrival */
		    rcode = PIOUS_ETIMEOUT;
		    break;
		  case PvmBadParam:
		    rcode = PIOUS_ESRCDEST;
		    break;
		  case PvmNoMem:
		    rcode = PIOUS_EINSUF;
		    break;
		  default:
		    rcode = PIOUS_ETPORT;
		    break;
		  }
	    }

	  /* case: message properly received */

	  else if (pvm_bufinfo(recvbufid, &mbytes, &mtag, &msrc) < 0)
	    { /* query error; reset recv buffer and free alloced buffer */
	      pvm_setrbuf(old_recvbufid);
	      pvm_freebuf(recvbufid);

	      rcode = PIOUS_ETPORT;
	    }

	  else
	    { /* set return arguments */
	      *recv_src = (dce_srcdestt)msrc;
	      *recv_tag = (dce_msgtagt)mtag;

	      /* mark receive buffer as valid */
	      recvbuf_alloced = TRUE;
	    }
	}
    }

  return rcode;
}




/*
 * DCE_upk*() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_upkbyte(char *addr,
		int nitem)
#else
int DCE_upkbyte(addr, nitem)
     char *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upkbyte(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upkchar(char *addr,
		int nitem)
#else
int DCE_upkchar(addr, nitem)
     char *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upkbyte(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upkint(int *addr,
	       int nitem)
#else
int DCE_upkint(addr, nitem)
     int *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upkint(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upkuint(unsigned int *addr,
		int nitem)
#else
int DCE_upkuint(addr, nitem)
     unsigned int *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upkuint(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upklong(long *addr,
		int nitem)
#else
int DCE_upklong(addr, nitem)
     long *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upklong(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upkulong(unsigned long *addr,
		 int nitem)
#else
int DCE_upkulong(addr, nitem)
     unsigned long *addr;
     int nitem;
#endif
{
  int rcode, pvmcode;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else if ((pvmcode = pvm_upkulong(addr, nitem, 1)) == PvmOk)
    rcode = PIOUS_OK;
  else if (pvmcode == PvmNoMem)
    rcode = PIOUS_EINSUF;
  else
    rcode = PIOUS_ETPORT;

  return rcode;
}


#ifdef __STDC__
int DCE_upkfhandlet(pds_fhandlet *addr,
		    int nitem)
#else
int DCE_upkfhandlet(addr, nitem)
     pds_fhandlet *addr;
     int nitem;
#endif
{
  int rcode, pvmcode, i;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;
  else
    { /* unpack 'nitem' complete fhandles. note that this routine must violate
       * the pds_fhandlet abstraction; as discussed in pds/pds_fhandlet.h,
       * pdce.c is a "friend" of the pds_fhandlet ADT in the C++ sense.
       */

      rcode = PIOUS_OK;

      for (i = 0; i < nitem && rcode == PIOUS_OK; i++, addr++)
	if ((pvmcode = pvm_upkulong(&(addr->dev), 1, 1)) != PvmOk ||
	    (pvmcode = pvm_upkulong(&(addr->ino), 1, 1)) != PvmOk)
	  {
	    if (pvmcode == PvmNoMem)
	      rcode = PIOUS_EINSUF;
	    else
	      rcode = PIOUS_ETPORT;
	  }
    }

  return rcode;
}


#ifdef __STDC__
int DCE_upktransidt(pds_transidt *addr,
		    int nitem)
#else
int DCE_upktransidt(addr, nitem)
     pds_transidt *addr;
     int nitem;
#endif
{
  int rcode, pvmcode, i;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;
  else
    { /* unpack 'nitem' complete transaction ids. note that this routine must
       * violate the pds_transidt abstraction; as discussed in
       * pds/pds_transidt.h, pdce.c is a "friend" of the pds_transidt ADT
       * in the C++ sense.
       */

      rcode = PIOUS_OK;

      for (i = 0; i < nitem && rcode == PIOUS_OK; i++, addr++)
	if ((pvmcode = pvm_upkulong(&(addr->hostid), 1, 1)) != PvmOk ||
	    (pvmcode = pvm_upklong(&(addr->procid), 1, 1))  != PvmOk ||
	    (pvmcode = pvm_upklong(&(addr->sec), 1, 1))     != PvmOk ||
	    (pvmcode = pvm_upklong(&(addr->usec), 1, 1))    != PvmOk)
	  {
	    if (pvmcode == PvmNoMem)
	      rcode = PIOUS_EINSUF;
	    else
	      rcode = PIOUS_ETPORT;
	  }
    }

  return rcode;
}




/*
 * DCE_upkbyte_blk() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_upkbyte_blk(char *addr,
		    int blksz,
		    int blkstride,
		    int nitem)
#else
int DCE_upkbyte_blk(addr, blksz, blkstride, nitem)
     char *addr;
     int blksz;
     int blkstride;
     int nitem;
#endif
{
  int rcode, pvmcode, addrinc, i;

  /* check that recv buf alloced */
  if (!recvbuf_alloced)
    rcode = PIOUS_EPERM;

  /* unpack data */
  else
    { /* case: data can be unpacked with a single PVM call */

      if (blksz == 1)
	pvmcode = pvm_upkbyte(addr, nitem, blkstride);

      else if (blkstride == 1)
	pvmcode = pvm_upkbyte(addr, nitem * blksz, 1);

      else if (nitem == 1)
	pvmcode = pvm_upkbyte(addr, blksz, 1);

      /* case: each block must be unpacked separately */

      else
	{
	  pvmcode = PvmOk;
	  addrinc = blksz * blkstride;

	  for (i = 0; i < nitem && pvmcode == PvmOk; i++, addr += addrinc)
	    pvmcode = pvm_upkbyte(addr, blksz, 1);
	}

      if (pvmcode == PvmOk)
	rcode = PIOUS_OK;
      else if (pvmcode == PvmNoMem)
	rcode = PIOUS_EINSUF;
      else
	rcode = PIOUS_ETPORT;
    }

  return rcode;
}




/*
 * DCE_freerecvbuf() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_freerecvbuf(void)
#else
int DCE_freerecvbuf()
#endif
{
  int rcode;

  rcode = PIOUS_OK;

  /* if recv buffer is allocated, deallocate and reset active recv buffer */
  if (recvbuf_alloced)
    {
      if (pvm_setrbuf(old_recvbufid) < 0 ||
	  pvm_freebuf(recvbufid) < 0)
	rcode = PIOUS_ETPORT;

      recvbuf_alloced = FALSE;
    }

  return rcode;
}




/*
 * DCE_register() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_register(char *name)
#else
int DCE_register(name)
     char *name;
#endif
{
  int rcode, pvmcode;

  /* verify that DCE is initialized */

  if (!dce_initialized)
    rcode = dce_init();
  else
    rcode = PIOUS_OK;

  /* register service name */

  if (rcode == PIOUS_OK)
    { /* attempt to join a group named 'name' */

      if ((pvmcode = pvm_joingroup(name)) == 0)
	/* assume that if instance number is zero then calling process is
         * first and only group member; assumption is not guaranteed true,
	 * but will serve purpose.
	 */
	rcode = PIOUS_OK;

      else if (pvmcode > 0)
	{ /* not first group member; name is already registered */
	  if (pvm_lvgroup(name) < 0)
	    /* unable to leave group just joined */
	    rcode = PIOUS_ETPORT;
	  else
	    rcode = PIOUS_EINVAL;
	}

      else if (pvmcode == PvmBadParam)
	/* name string is not valid */
	rcode = PIOUS_EINVAL;

      else
	/* transport error */
	rcode = PIOUS_ETPORT;
    }

  return rcode;
}




/*
 * DCE_locate() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_locate(char *name,
	       dce_srcdestt *id)
#else
int DCE_locate(name, id)
     char *name;
     dce_srcdestt *id;
#endif
{
  int rcode, pvmcode;

  /* verify that DCE is initialized */

  if (!dce_initialized)
    rcode = dce_init();
  else
    rcode = PIOUS_OK;

  /* locate service name */

  if (rcode == PIOUS_OK)
    { /* get id of process in group 'name' with instance number 0 */

      if ((pvmcode = pvm_gettid(name, 0)) > 0)
	{ /* assume that a process in a group that has an instance number of
	   * zero is registered via the DCE_register() function; there really
	   * is no way to detected when this assumption is violated using
	   * the PVM group mechanism.
	   */

	  rcode = PIOUS_OK;
	  *id   = (dce_srcdestt)pvmcode;
	}

      else if (pvmcode == PvmNoInst || pvmcode == PvmNoGroup ||
	       pvmcode == PvmBadParam)
	/* service is not registered or name string is not valid */
	rcode = PIOUS_EINVAL;

      else
	/* transport error */
	rcode = PIOUS_ETPORT;
    }

  return rcode;
}




/*
 * DCE_unregister() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_unregister(char *name)
#else
int DCE_unregister(name)
     char *name;
#endif
{
  int rcode, pvmcode;

  /* verify that DCE is initialized */

  if (!dce_initialized)
    /* if not initialized then could not have registered service name */
    rcode = PIOUS_EINVAL;

  /* unregister service name */

  else if ((pvmcode = pvm_lvgroup(name)) == PvmOk)
    rcode = PIOUS_OK;

  else if (pvmcode == PvmBadParam || pvmcode == PvmNoGroup ||
	   pvmcode == PvmNotInGroup)
    /* name string is not valid or not registered by caller */
    rcode = PIOUS_EINVAL;

  else
    /* transport error */
    rcode = PIOUS_ETPORT;

  return rcode;
}




/*
 * DCE_spawn()  - See pdce.h for description
 */

#ifdef __STDC__
int DCE_spawn(char *task,
	      char **argv,
	      char *where,
	      dce_srcdestt *id)
#else
int DCE_spawn(task, argv, where, id)
     char *task;
     char **argv;
     char *where;
     dce_srcdestt *id;
#endif
{
  int rcode, pvmcode, tid;

  if ((pvmcode = pvm_spawn(task, argv, PvmTaskHost, where, 1, &tid)) == 1)
    { /* task spawned without error */
      *id   = (dce_srcdestt)tid;
      rcode = PIOUS_OK;
    }

  else
    { /* if partial failure, set pvmcode to returned error code */
      if (pvmcode == 0)
	pvmcode = tid;

      /* set result code */
      switch(pvmcode)
	{
	case PvmBadParam:
	case PvmNoHost:
	case PvmNoFile:
	  rcode = PIOUS_EINVAL;
	  break;
	case PvmNoMem:
	case PvmOutOfRes:
	  rcode = PIOUS_EINSUF;
	  break;
	default:
	  rcode = PIOUS_ETPORT;
	  break;
	}
    }

  return rcode;
}




/*
 * DCE_exit() - See pdce.h for description
 */

#ifdef __STDC__
int DCE_exit(void)
#else
int DCE_exit()
#endif
{
  int rcode;

  if (!dce_initialized)
    rcode = PIOUS_OK;

  else
    { 
#ifndef USEPVM33FNS
      /* warning: PVM versions prior to 3.3 can lose last message when a
       *          process exits soon after; sleeping is a kludge that helps
       *          but is no guarantee.
       */
      sleep(3);
#endif

      if (pvm_exit() < 0)
	rcode = PIOUS_ETPORT;
      else
	rcode = PIOUS_OK;
    }

  return rcode;
}




/* Function Definitions - Local Functions */


/*
 * dce_init()
 *
 * Parameters:
 *
 * Perform any initialization that is required prior to utilizing
 * the distributed computing environment (DCE) services.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - dce_init() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
static int dce_init(void)
#else
static int dce_init()
#endif
{
  int rcode;

  /* enroll in PVM DCE */
  if (pvm_mytid() < 0)
    {
      rcode = PIOUS_ETPORT;
    }

  /* set message routing preference */
  else
    {
#ifdef USEPVMDIRECT
      pvm_setopt(PvmRoute, PvmRouteDirect);
#endif
      rcode           = PIOUS_OK;
      dce_initialized = TRUE;
    }

  return rcode;
}
