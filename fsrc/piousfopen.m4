/*
 * pious_open() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfopen.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfopen) ARGS(`STRING_ARG(path), oflag, mode, rc')
  STRING_ARG_DECL(path);
  int *oflag, *mode, *rc;
{
  char pbuf[PIOUS_PATH_MAX + 1];

  if (f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(path), STRING_LEN(path)))
    *rc = pious_open(pbuf, *oflag, (pious_modet)(*mode));
  else
    *rc = PIOUS_ENAMETOOLONG;
}
