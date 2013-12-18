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
 * @(#)pdce.h	2.2  28 Apr 1995  Moyer
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
 * --------------------------------------------------------------------------
 *
 * Send/Receive Usage:
 *
 *   Send message -
 *
 *     DCE_mksendbuf();
 *     DCE_pk*();
 *         ...
 *     DCE_pk*();
 *     DCE_send();
 *     DCE_freesendbuf();
 *
 *   Receive message -
 *
 *     DCE_recv();
 *     DCE_upk*();
 *         ...
 *     DCE_upk*();
 *     DCE_freerecvbuf();
 */




/*
 * DCE_mksendbuf()
 *
 * Parameters:
 *
 * Allocate an empty send buffer.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_mksendbuf() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM    - buffer already allocated; operation not permitted
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error in underlying transport system
 */

#ifdef __STDC__
int DCE_mksendbuf(void);
#else
int DCE_mksendbuf();
#endif




/*
 * DCE_pk*()
 *
 * Parameters:
 *
 *   addr  - item array address
 *   nitem - item array count
 *
 * Pack an array of 'nitem' items into allocated send buffer from memory
 * buffer 'addr'.
 *
 * This is a family of functions, one per data type that can be packed.
 * To support heterogeneity, all data types EXCEPT 'byte' are converted
 * to the representation of the receiving host.
 *
 * NOTE: for performance, presumes arguments to be correct.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_pk*() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no send buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_pkbyte     (char          *addr, int nitem);
int DCE_pkchar     (char          *addr, int nitem);
int DCE_pkint      (int           *addr, int nitem);
int DCE_pkuint     (unsigned int  *addr, int nitem);
int DCE_pklong     (long          *addr, int nitem);
int DCE_pkulong    (unsigned long *addr, int nitem);

int DCE_pkfhandlet (pds_fhandlet  *addr, int nitem);
int DCE_pktransidt (pds_transidt  *addr, int nitem);

#else
int DCE_pkbyte();
int DCE_pkchar();
int DCE_pkint();
int DCE_pkuint();
int DCE_pklong();
int DCE_pkulong();

int DCE_pkfhandlet();
int DCE_pktransidt();
#endif


#define DCE_pkofft(addr, nitem) \
DCE_pklong((long *)(addr), (int)(nitem))

#define DCE_pksizet(addr, nitem) \
DCE_pkulong((unsigned long *)(addr), (int)(nitem))

#define DCE_pkssizet(addr, nitem) \
DCE_pklong((long *)(addr), (int)(nitem))

#define DCE_pkmodet(addr, nitem) \
DCE_pkulong((unsigned long *)(addr), (int)(nitem))

#define DCE_pksrcdestt(addr, nitem) \
DCE_pkint((int *)(addr), (int)(nitem))




/*
 * DCE_pkbyte_blk()
 *
 * Parameters:
 *
 *   addr      - byte array address
 *   blksz     - block size
 *   blkstride - block stride
 *   nitem     - block count
 *
 * Pack an array of 'nitem' byte blocks of size 'blksz' at a stride of
 * 'blkstride' into allocated send buffer from memory buffer 'addr'.
 * Byte blocks are packed "raw"; i.e. not interpretted.
 *
 * NOTE: for performance, presumes arguments to be correct; in particular,
 *       it must be true that (nitem * blksz) <= PIOUS_INT_MAX
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_pkbyte_blk() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no send buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_pkbyte_blk(char *addr,
		   int blksz,
		   int blkstride,
		   int nitem);
#else
int DCE_pkbyte_blk();
#endif
		    



/*
 * DCE_send()
 *
 * Parameters:
 *
 *   msgdest  - message destination
 *   msgtag   - message tag
 *
 * Send a message consisting of the packed data in the allocated send buffer
 * to object 'msgdest' with identifier tag 'msgtag'.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_send() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'msgdest' argument
 *       PIOUS_EINVAL   - invalid 'msgtag' argument
 *       PIOUS_EPERM    - no send buffer allocated; operation not permitted
 *       PIOUS_ETPORT   - error in underlying transport system
 */

#ifdef __STDC__
int DCE_send(dce_srcdestt msgdest,
	     dce_msgtagt msgtag);
#else
int DCE_send();
#endif




/*
 * DCE_freesendbuf();
 *
 * Parameters:
 *
 * Deallocate send buffer.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_freesendbuf() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_freesendbuf(void);
#else
int DCE_freesendbuf();
#endif




/*
 * DCE_recv()
 *
 * Parameters:
 *
 *   msgsrc   - message source or DCE_ANY_SRC
 *   msgtag   - message tag or DCE_ANY_TAG
 *   recv_src - source from which message received
 *   recv_tag - tag of message received
 *   timeout  - time-out period (in milliseconds)
 *
 * Receive a message, thereby allocating a receive buffer, from the
 * object 'msgsrc' with identifier tag 'msgtag'.  The source and tag
 * of the message received are placed in 'recv_src' and 'recv_tag'.
 *
 * DCE_recv() is a blocking call with a time-out period of 'timeout'
 * milliseconds.  A 'timeout' value of DCE_BLOCK, or any other negative value,
 * will cause the function to block waiting without time-out.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_recv() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'msgsrc' argument
 *       PIOUS_EINVAL   - invalid 'msgtag' argument
 *       PIOUS_ETIMEOUT - function timed-out prior to completion
 *       PIOUS_EPERM    - buffer already allocated; operation not permitted
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error in underlying transport system
 */

#define DCE_BLOCK -1

#ifdef __STDC__
int DCE_recv(dce_srcdestt msgsrc,
	     dce_msgtagt msgtag,
	     dce_srcdestt *recv_src,
	     dce_msgtagt *recv_tag,
	     int timeout);
#else
int DCE_recv();
#endif




/*
 * DCE_upk*()
 *
 * Parameters:
 *
 *   addr  - item array address
 *   nitem - item array count
 *
 * Unpack an array of 'nitem' items from allocated receive buffer into
 * memory buffer 'addr'.
 *
 * This is a family of functions, one per data type that can be unpacked.
 * To support heterogeneity, all data types EXCEPT 'byte' are converted
 * to the host representation.
 *
 * NOTE: for performance, presumes arguments to be correct.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_upk*() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no receive buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient resources; no more data in receive buffer
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_upkbyte     (char          *addr, int nitem);
int DCE_upkchar     (char          *addr, int nitem);
int DCE_upkint      (int           *addr, int nitem);
int DCE_upkuint     (unsigned int  *addr, int nitem);
int DCE_upklong     (long          *addr, int nitem);
int DCE_upkulong    (unsigned long *addr, int nitem);

int DCE_upkfhandlet (pds_fhandlet  *addr, int nitem);
int DCE_upktransidt (pds_transidt  *addr, int nitem);
#else
int DCE_upkbyte();
int DCE_upkchar();
int DCE_upkint();
int DCE_upkuint();
int DCE_upklong();
int DCE_upkulong();

int DCE_upkfhandlet();
int DCE_upktransidt();
#endif


#define DCE_upkofft(addr, nitem) \
DCE_upklong((long *)(addr), (int)(nitem))

#define DCE_upksizet(addr, nitem) \
DCE_upkulong((unsigned long *)(addr), (int)(nitem))

#define DCE_upkssizet(addr, nitem) \
DCE_upklong((long *)(addr), (int)(nitem))

#define DCE_upkmodet(addr, nitem) \
DCE_upkulong((unsigned long *)(addr), (int)(nitem))

#define DCE_upksrcdestt(addr, nitem) \
DCE_upkint((int *)(addr), (int)(nitem))




/*
 * DCE_upkbyte_blk()
 *
 * Parameters:
 *
 *   addr       - byte array address
 *   blksz      - block size
 *   blkstride  - block stride
 *   nitem      - block count
 *
 * Unpack 'nitem' byte blocks of size 'blksz' from allocated receive buffer
 * into memory buffer 'addr' at a stride of 'blkstride'.  Byte blocks are
 * unpacked "raw"; i.e. not interpretted.
 *
 * NOTE: for performance, presumes arguments to be correct; in particular,
 *       it must be true that (nitem * blksz) <= PIOUS_INT_MAX
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_upkbyte_blk() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EPERM  - no receive buffer allocated; operation not permitted
 *       PIOUS_EINSUF - insufficient resources; no more data in receive buffer
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_upkbyte_blk(char *addr,
		    int blksz,
		    int blkstride,
		    int nitem);
#else
int DCE_upkbyte_blk();
#endif




/*
 * DCE_freerecvbuf();
 *
 * Parameters:
 *
 * Deallocate receive buffer.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_freerecvbuf() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_freerecvbuf(void);
#else
int DCE_freerecvbuf();
#endif




/*
 * DCE_register()
 *
 * Parameters:
 *
 *   name   - service name
 *
 * Register a service with the unique identifier string 'name'.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_register() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - service id 'name' invalid or already registered
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_register(char *name);
#else
int DCE_register();
#endif




/*
 * DCE_locate()
 *
 * Parameters:
 *
 *   name - service name
 *   id   - service message passing id
 *
 * Locate a service registered with the identifier string 'name' and
 * return in 'id' the server id for sending/receiving messages.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_locate() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - service id 'name' invalid or not registered
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_locate(char *name,
	       dce_srcdestt *id);
#else
int DCE_locate();
#endif




/*
 * DCE_unregister()
 *
 * Parameters:
 *
 *   name   - service name
 *
 * Unregister a previously registered service identified by string 'name'.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_unregister() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - service id 'name' invalid or not registered by caller
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_unregister(char *name);
#else
int DCE_unregister();
#endif




/*
 * DCE_spawn()
 *
 * Parameters:
 *
 *   task  - task executable name
 *   argv  - task arguments
 *   where - task host
 *   id    - task message passing id
 *
 * Spawn the executable 'task' with arguments 'argv' on host 'where',
 * returning in 'id' the task id for sending/receiving messages.
 * The task argument array 'argv' must be terminated  by a null pointer.
 *
 * NOTE: The DCE is presumed to have been configured in such a way that
 *       it is capable of locating the executable 'task'.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_spawn() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - invalid 'task' or 'where' argument
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_spawn(char *task,
	      char **argv,
	      char *where,
	      dce_srcdestt *id);
#else
int DCE_spawn();
#endif




/*
 * DCE_exit()
 *
 * Parameters:
 *
 * Exit the distributed computing environment.  Informing the DCE that
 * the process no longer requires DCE services allows the DCE to complete
 * any pending operations, e.g. message sends, posted by the exiting process.
 *
 * NOTE: Must NOT be used by PIOUS library routines, or may remove user
 *       process from the DCE.  Intended for use by PIOUS service daemons.
 *
 * Returns:
 *
 *   PIOUS_OK(0) - DCE_exit() completed without error
 *   <  0        - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int DCE_exit(void);
#else
int DCE_exit();
#endif
