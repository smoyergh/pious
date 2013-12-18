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




/* PIOUS: Standard Symbolic Constant and Structure Definitions
 *
 * @(#)pious_std.h	2.2  28 Apr 1995  Moyer
 *
 * This file defines standard symbolic constants and structures used in
 * implementing PIOUS.  To the extent possible, PIOUS symbolic constants
 * and structures correspond to similar constants and structures defined
 * in POSIX.1, but with the PIOUS_* prefix.
 * 
 * Where appropriate, symbolic constants represent single-bit masks that are
 * unique within a logical grouping; values are formed via the bitwise
 * inclusive OR of constants within the relevant group.
 */


#ifndef PIOUS_STD_H
#define PIOUS_STD_H


/* Symbolic constants for testing file accessibility */

#define PIOUS_R_OK   0x01
#define PIOUS_W_OK   0x02
#define PIOUS_X_OK   0x04
#define PIOUS_F_OK   0x08


/* Symbolic constants for specifying relative file offset */

#define PIOUS_SEEK_SET 0
#define PIOUS_SEEK_CUR 1
#define PIOUS_SEEK_END 2


/* Symbolic constants for defining values of type pious_modet */

#define PIOUS_IRUSR  00400  /* file permission bits */
#define PIOUS_IWUSR  00200
#define PIOUS_IXUSR  00100
#define PIOUS_IRWXU  00700

#define PIOUS_IRGRP  00040
#define PIOUS_IWGRP  00020
#define PIOUS_IXGRP  00010
#define PIOUS_IRWXG  00070

#define PIOUS_IROTH  00004
#define PIOUS_IWOTH  00002
#define PIOUS_IXOTH  00001
#define PIOUS_IRWXO  00007

#define PIOUS_ISREG  02000  /* file type bits */
#define PIOUS_ISDIR  01000


/* Symbolic constants for defining file access/creation modes */

#define PIOUS_NOCREAT 0x00
#define PIOUS_RDONLY  0x01
#define PIOUS_WRONLY  0x02
#define PIOUS_RDWR    0x04
#define PIOUS_CREAT   0x08
#define PIOUS_TRUNC   0x10


/* Symbolic constants for defining file/transaction fault tolerance mode */

#define PIOUS_STABLE   0
#define PIOUS_VOLATILE 1


/* Symbolic constants for defining file views */

#define PIOUS_GLOBAL       0
#define PIOUS_INDEPENDENT  1
#define PIOUS_SEGMENTED    2


/* Symbolic constants for querying system configuration parameters */

#define PIOUS_DS_DFLT    0
#define PIOUS_OPEN_MAX   1
#define PIOUS_TAG_LOW    2
#define PIOUS_TAG_HIGH   3
#define PIOUS_BADSTATE   4


/* Data server information vector structure */

struct pious_dsvec {
  char *hname;         /* host name */
  char *spath;         /* search root path */
  char *lpath;         /* log directory path */
};


/* File status structure */

struct pious_stat {
  int dscnt;           /* data server count, ie degree of file declustering */
  int segcnt;          /* number of file segments */
};


#endif /* PIOUS_STD_H */




