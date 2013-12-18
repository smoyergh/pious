/* C - Fortran String Conversion Functions
 *
 * @(#)fcstrcnvt.c	2.2  28 Apr 1995  Moyer
 *
 * Functions for converting C strings to Fortran format, and vice versa.
 *
 * Function Summary:
 *
 *   c2fstr();
 *   f2cstr();
 */


/* Include Files */

#ifndef __STDC__
#include <memory.h>
#endif

#include <string.h>
#include "fcstrcnvt.h"




/*
 * c2fstr() - See fcstrcnvt.h for description
 */

#ifdef __STDC__
int c2fstr(char *dstr,
	   int dlen,
	   char *sstr)
#else
int c2fstr(dstr, dlen, sstr)
     char *dstr;
     int dlen;
     char *sstr;
#endif
{
  int slen, rc;

  if ((slen = strlen(sstr)) > dlen)
    { /* insufficient storage */
      rc = 0;
    }

  else
    { /* copy C string into Fortran string storage */
      memcpy(dstr, sstr, slen);

      /* fill remaining Fortran string storage with blank characters */
      memset(dstr + slen, ' ', dlen - slen);

      rc = 1;
    }

  return rc;
}




/*
 * f2cstr() - See fcstrcnvt.h for description
 */

#ifdef __STDC__
int f2cstr(char *dstr,
	   int dlen,
	   char *sstr,
	   int slen)
#else
int f2cstr(dstr, dlen, sstr, slen)
     char *dstr;
     int dlen;
     char *sstr;
     int slen;
#endif
{
  int rc;
  char *last;

  /* adjust Fortran string length to compensate for blank character fill */
  for (last = sstr + (slen - 1); last >= sstr && *last == ' '; last--);

  slen = (last - sstr) + 1;

  if (slen > dlen)
    { /* insufficient storage */
      rc = 0;
    }

  else
    { /* copy Fortran string into C string storage and append a null */
      memcpy(dstr, sstr, slen);

      *(dstr + slen) = '\0';

      rc = 1;
    }

  return rc;
}
