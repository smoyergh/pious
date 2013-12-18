/* C - Fortran String Conversion Functions
 *
 * @(#)fcstrcnvt.h	2.2  28 Apr 1995  Moyer
 *
 * Functions for converting C strings to Fortran format, and vice versa.
 *
 * Function Summary:
 *
 *   c2fstr();
 *   f2cstr();
 */




/*
 * c2fstr()
 *
 * Parameters:
 *
 *   dstr - Fortran destination string pointer
 *   dlen - Fortran destination string length
 *   sstr - C source string pointer
 *
 * Convert C string to Fortran string format.
 *
 * Returns:
 *
 *   1 - successful
 *   0 - failed due to insufficient destination storage
 */

#ifdef __STDC__
int c2fstr(char *dstr,
	   int dlen,
	   char *sstr);
#else
int c2fstr();
#endif




/*
 * f2cstr()
 *
 * Parameters:
 *
 *   dstr - C destination string pointer
 *   dlen - C destination string length
 *   sstr - Fortran source string pointer
 *   slen - Fortran source string length
 *
 * Convert Fortran string to C string format.
 *
 * Note: C string length 'dlen' must NOT include storage for terminating null
 *
 *
 * Returns:
 *
 *   1 - successful
 *   0 - failed due to insufficient destination storage
 */

#ifdef __STDC__
int f2cstr(char *dstr,
	   int dlen,
	   char *sstr,
	   int slen);
#else
int f2cstr();
#endif
