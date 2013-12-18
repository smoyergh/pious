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




/* PIOUS: Error Code Symbolic Constants
 *
 * @(#)pious_errno.h	2.2  28 Apr 1995  Moyer
 *
 * This file defines symbolic constants for error codes used in implementing
 * PIOUS. All error codes have a negative integer value.  To the extent
 * possible, PIOUS error codes correspond to POSIX.1 error codes with
 * the PIOUS_* prefix.
 *
 * Symbolic constant definitions provided here are general; function
 * definitions provide details of specific meaning for the given context.
 *
 * Note that in addition to error symbolic constants, the few successful
 * completion constant are defined as well; these are non-negative integers.
 */


#ifndef PIOUS_ERRNO_H
#define PIOUS_ERRNO_H


#define PIOUS_READONLY        1  /* vote to commit read-only transaction */
#define PIOUS_OK              0  /* function completed successfully */

#define PIOUS_EACCES         -2  /* path search or access mode denied */
#define PIOUS_EACCES_TXT      "path search permission or access mode denied"

#define PIOUS_EBADF          -3  /* bad file descriptor */
#define PIOUS_EBADF_TXT       "bad file handle/descriptor"

#define PIOUS_EBUSY          -4  /* resource currently unavailabe for use */
#define PIOUS_EBUSY_TXT       "resource currently unavailable for use"

#define PIOUS_EEXIST         -5  /* file exists */
#define PIOUS_EEXIST_TXT      "file exists"

#define PIOUS_EFBIG          -6  /* file size exceeds system constraints */
#define PIOUS_EFBIG_TXT       "file size exceeds system constraints"

#define PIOUS_EINVAL         -7  /* invalid argument */
#define PIOUS_EINVAL_TXT      "invalid argument"

#define PIOUS_ENOTREG        -8  /* file is not a regular file */
#define PIOUS_ENOTREG_TXT     "file is not a regular file"

#define PIOUS_EINSUF         -9  /* insufficient resources to complete */
#define PIOUS_EINSUF_TXT      "insufficient system resources for operation"

#define PIOUS_ENAMETOOLONG  -10  /* path or path component name is too long */
#define PIOUS_ENAMETOOLONG_TXT "path or path component name is too long"

#define PIOUS_ENOENT        -11  /* no such file or directory */
#define PIOUS_ENOENT_TXT      "no such file or directory"

#define PIOUS_ENOSPC        -12  /* no space left on device */
#define PIOUS_ENOSPC_TXT      "no space left on device"

#define PIOUS_ENOTDIR       -13  /* a component of the path prefix not a dir */
#define PIOUS_ENOTDIR_TXT     "a component of the path prefix is not a dir"

#define PIOUS_ENOTEMPTY     -14  /* directory not empty */
#define PIOUS_ENOTEMPTY_TXT   "directory not empty"

#define PIOUS_EISDIR        -15  /* path specifies a directory */
#define PIOUS_EISDIR_TXT      "path specifies a directory entry"

#define PIOUS_EPERM         -16  /* operation not permitted */
#define PIOUS_EPERM_TXT       "operation not permitted"

#define PIOUS_EXDEV         -17  /* improper link to external file system */
#define PIOUS_EXDEV_TXT       "attempted improper link to external file system"

#define PIOUS_ETIMEOUT      -90  /* function timed-out prior to completion */
#define PIOUS_ETIMEOUT_TXT    "function timed-out prior to completion"

#define PIOUS_EPROTO        -91  /* transaction op or 2PC protocol error */
#define PIOUS_EPROTO_TXT      "transaction operation or 2PC protocol error"

#define PIOUS_ENOTLOG       -92  /* information not written to log file */
#define PIOUS_ENOTLOG_TXT     "information not written to log file"

#define PIOUS_ESRCDEST      -93  /* invalid transport source/dest address */
#define PIOUS_ESRCDEST_TXT    "invalid transport source/destination address"

#define PIOUS_ETPORT        -94  /* error in underlying transport system */
#define PIOUS_ETPORT_TXT      "error condition in underlying transport system"

#define PIOUS_EABORT        -95  /* transaction is aborted */
#define PIOUS_EABORT_TXT      "transaction operation aborted"

#define PIOUS_EUNXP         -96  /* unexpected error condition encountered */
#define PIOUS_EUNXP_TXT       "unexpected error condition encountered"

#define PIOUS_ECHCKPT       -97  /* check-point required */
#define PIOUS_ECHCKPT_TXT     "check-point required"

#define PIOUS_ERECOV        -98  /* failure recovery required */
#define PIOUS_ERECOV_TXT      "failure recovery required"

#define PIOUS_EFATAL        -99  /* fatal error occured; check error log */
#define PIOUS_EFATAL_TXT      "fatal error occured; check error log"




/* Implementation Notes:
 *
 *   1) PIOUS source code assumes all PIOUS error codes are defined to
 *      have a negative integer value; this assumption must not be broken.
 *
 *   2) Function misc/gputil.c:UTIL_errno2errtxt() converts error numbers
 *      to the appropriate error text and thus must be updated as error
 *      codes are added to or removed from include/pious_errno.h.
 */


#endif /* PIOUS_ERRNO_H */




