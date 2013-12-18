/*
 * pious_mkdir() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfmkdir.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfmkdir) ARGS(`STRING_ARG(path), mode, rc')
  STRING_ARG_DECL(path);
  int *mode, *rc;
{
  char pbuf[PIOUS_PATH_MAX + 1];

  if (f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(path), STRING_LEN(path)))
    *rc = pious_mkdir(pbuf, (pious_modet)(*mode));
  else
    *rc = PIOUS_ENAMETOOLONG;
}
