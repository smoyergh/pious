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




/* PIOUS: System Configuration File
 *
 * @(#)pious_sysconfig.h	2.2  28 Apr 1995  Moyer
 *
 * Defines symbolic constants for important system configuration parameters.
 *
 * Five general catagories of parameters are defined:
 *
 *   - General system-wide parameters
 *   - PIOUS Library Function (PLIB) parameters
 *   - PIOUS Data Server (PDS) parameters
 *   - PIOUS Service Coordinator (PSC) parameters
 *   - Physical/logical limits of the underlying architecture/OS.
 *
 * Grouping configuration parameters into a single file makes it easy to
 * customize PIOUS for a particular execution environment.
 */


/*----------------------------------------------------------------------*
 *                   General System-wide Parameters                     *
 *----------------------------------------------------------------------*/

/* PDS and PSC executable file names. */

#define PDS_DAEMON_EXEC "pious1DS"
#define PSC_DAEMON_EXEC "pious1SC"




/*----------------------------------------------------------------------*
 *                  PIOUS Library Function Parameters                   *
 *----------------------------------------------------------------------*/

/* process file descriptor table size (must be > 0) */

#define PLIB_OPEN_MAX   64

/* upper bound on the number of attempts to execute an independent (non
 * user-transaction) file access operation that aborts due to the inability
 * to obtain the required locks (must be > 0).
 */

#define PLIB_RETRY_MAX  10




/*----------------------------------------------------------------------*
 *                     PIOUS Data Server Parameters                     *
 *----------------------------------------------------------------------*/

/* PDS cache management parameters (pds/pds_cache_manager.c):
 *
 * PDS_CM_DBLK_SZ  - data block size in bytes; a data block is the unit
 *                   of caching. PDS_CM_DBLK_SZ *must* be greater than
 *                   zero (> 0) for compilation.
 *
 * PDS_CM_CACHE_SZ - cache size in number of data blocks. PDS_CM_CACHE_SZ must
 *                   be greater than or equal to zero (>= 0); a value of zero
 *                   specifies no caching.
 *
 *                   NOTE: because the PDS employs a segmented LRU cache,
 *                         comprised of a protected and a probationary segment,
 *                         if PDS_CM_CACHE_SZ is set to one (1) then a cache
 *                         size of two (2) will be used.
 */

#define PDS_CM_DBLK_SZ     16384
#define PDS_CM_CACHE_SZ       64


/* PDS daemon timeout parameter (pds/pds_daemon.c):
 *
 * PDS_TDEADLOCK - time-out period for deadlock avoidance (in milliseconds).
 *                 once an operation has remained blocked for a time
 *                 period greater than PDS_TDEADLOCK, it is a candidate
 *                 for abortion.
 */

#define PDS_TDEADLOCK    250   /* milliseconds */




/*----------------------------------------------------------------------*
 *                 PIOUS Service Coordinator Parameters                 *
 *----------------------------------------------------------------------*/

/* PSC daemon timeout parameter (psc/psc_daemon.c):
 *
 * PSC_TOPRSLTVALID - time-out period for most recent PSC operation result
 *
 *                    parallel applications may generate sets of identical PSC
 *                    requests, either out of necessity, such as in the case
 *                    of PSC_{s}open(), or because the application is
 *                    implemented in the common SPMD model.  for efficiency,
 *                    the PSC retains the result of the most recent operation
 *                    to allow response to identical operations with minimal
 *                    delay, where appropriate.
 *
 *                    in recognition of the fact that the results of two
 *                    successive identical operations can potentially be
 *                    different due to external factors, the most recent
 *                    operation result becomes invalid after a time period
 *                    of PSC_TOPRSLTVALID milliseconds.
 *
 *                    NOTE: the time-out period is measured from the first
 *                    of a set of successive identical operations; later
 *                    operations in the set do NOT reset the timer.
 */

#define PSC_TOPRSLTVALID  20000   /* milliseconds */


/* PSC configuration file parser (psc/psc_cfparse.c): maximum token size */

#define PSC_INPUTBUF_MAX 511


/* PSC daemon (psc/psc_daemon.c): file table size (must be > 0) */

#define PSC_OPEN_MAX     64




/*----------------------------------------------------------------------*
 *              PIOUS Physical/Logical Limit Parameters                 *
 *----------------------------------------------------------------------*/
