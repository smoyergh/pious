/*
 * pious_rmdir() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfrmdir.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfrmdir) ARGS(`STRING_ARG(path), rc')
  STRING_ARG_DECL(path);
  int *rc;
{
  char pbuf[PIOUS_PATH_MAX + 1];

  if (f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(path), STRING_LEN(path)))
    *rc = pious_rmdir(pbuf);
  else
    *rc = PIOUS_ENAMETOOLONG;
}
