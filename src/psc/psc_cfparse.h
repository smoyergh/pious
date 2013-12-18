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




/* PIOUS Service Coordinator (PSC): Configuration File Parser
 *
 * @(#)psc_cfparse.h	2.2 28 Apr 1995 Moyer
 *
 * This module exports functions for parsing the PSC configuration file
 * containing PDS host information entries.
 *
 *
 * Function Summary:
 *
 *   CF_parse();
 */




/*
 * CF_parse()
 *
 * Parameters:
 *
 *   psc_hostfile - PSC configuration file name
 *   pds_cnt      - PDS host entry count
 *   pds_hname    - PDS host names
 *   pds_spath    - PDS host search root paths
 *   pds_lpath    - PDS host log directory paths
 *
 * Parse the PSC configuration file containing PDS host information entries.
 * PDS host information is returned in a set of corresponding arrays,
 * with entries in lexicographic order based on host name.
 *
 * Parser guarantees that all host names are unique and non-null, and that
 * all host search root and log directory paths are non-empty with '/' as
 * the first character.
 *
 *
 * NOTE: if the configuration file can not be accessed or contains a syntax
 *       or semantic error, this is reported on stream stderr.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - PSC configuration file parsed without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - can not access configuration file or file is invalid
 *       PIOUS_EINSUF - insufficient system resources to complete
 */

#ifdef __STDC__
int CF_parse(char *psc_hostfile,
	     int *pds_cnt,
	     char ***pds_hname,
	     char ***pds_spath,
	     char ***pds_lpath);
#else
int CF_parse();
#endif
