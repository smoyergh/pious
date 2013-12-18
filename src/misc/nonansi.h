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




/* PIOUS: Non ANSI-C Declarations
 *
 * @(#)nonansi.h	2.2  28 Apr 1995  Moyer
 *
 * Set of declarations to include when using a non ANSI-C compiler.
 * These declarations are typically not part of a standard header file
 * in non ANSI-C.
 */


/* typedef int void; /* only required for very old C compilers */


#ifndef NULL
#define NULL  0
#endif

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

extern char *malloc();              /* memory allocate */
extern void free();                 /* memory deallocate */

extern char *getenv();              /* get environment variable value */

/* time() is declared (often indirectly) in <sys/time.h> on most systems.
 * if this is NOT the case then uncomment the following declaration and
 * check to be sure that the return type of time() is correct.
 */
/* extern long time();                 /* determine current calendar time */

extern void exit();                 /* cause program to terminate */
