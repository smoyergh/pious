/*
 * pious_getcwd() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfgetcwd.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfgetcwd) ARGS(`STRING_ARG(buf), rc')
  STRING_ARG_DECL(buf);
  int *rc;
{
  char pbuf[PIOUS_PATH_MAX + 1];

  if ((*rc =
       pious_getcwd(pbuf, (pious_sizet)(PIOUS_PATH_MAX + 1))) == PIOUS_OK)
    {
      if (!c2fstr(STRING_PTR(buf), STRING_LEN(buf), pbuf))
        *rc = PIOUS_EINSUF;
    }
}
