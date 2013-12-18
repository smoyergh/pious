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




/* PIOUS Service Coordinator (PSC): Data Server Manager
 *
 * @(#)psc_dataserver_manager.h	2.2 28 Apr 1995 Moyer
 *
 * The psc_dataserver_manager exports functions for managing the set
 * of PIOUS data servers (PDS) that provide the mechanism for accessing
 * user data.
 *
 *
 * Function Summary:
 *
 *   SM_add_pds();
 *   SM_reset_pds();
 *   SM_shutdown_pds();
 */




/*
 * SM_add_pds()
 *
 * Parameters:
 *
 *   pds_hname    - PDS host name
 *   pds_lpath    - PDS host log directory path
 *   pds_id       - PDS message passing id
 *
 * Spawn a PIOUS data server on host 'pds_hname', where the log file
 * directory path is 'pds_lpath'.  The PDS message passing id is returned
 * in 'pds_id'.
 *
 * NOTE: if a data server was previously spawned on host 'pds_hname',
 *       then the message passing id of that data server is returned;
 *       multiple servers are not spawned on a single host.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - PIOUS data server spawned without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - invalid host, or unable to locate PDS executable
 *       PIOUS_EINSUF - insufficient system resources to complete
 *       PIOUS_ETPORT - error in underlying transport system
 */

#ifdef __STDC__
int SM_add_pds(char *pds_hname,
	       char *pds_lpath,
	       dce_srcdestt *pds_id);
#else
int SM_add_pds();
#endif




/*
 * SM_reset_pds()
 *
 * Parameters:
 *
 * Reset all PIOUS data servers, checkpointing access operations.  All PDS
 * remain active and can continue servicing requests.
 *
 * NOTE: a PDS will only reset if it has no pending operations to perform.
 *
 * Returns:
 */

#ifdef __STDC__
void SM_reset_pds(void);
#else
void SM_reset_pds();
#endif




/*
 * SM_shutdown_pds()
 *
 * Parameters:
 *
 * Shutdown all PIOUS data servers.
 *
 * Returns:
 */

#ifdef __STDC__
void SM_shutdown_pds(void);
#else
void SM_shutdown_pds();
#endif
