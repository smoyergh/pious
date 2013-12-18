/*
 * pious_popen() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfpopen.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfpopen) ARGS(`STRING_ARG(group), STRING_ARG(path),
                            view, map, faultmode, oflag, mode, seg, rc')
  STRING_ARG_DECL(group);
  STRING_ARG_DECL(path);
  int *view, *map, *faultmode, *oflag, *mode, *seg, *rc;
{
  char gbuf[PIOUS_NAME_MAX + 1];
  char pbuf[PIOUS_PATH_MAX + 1];

  if (f2cstr(gbuf, PIOUS_NAME_MAX, STRING_PTR(group), STRING_LEN(group)) &&
      f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(path), STRING_LEN(path)))
    *rc = pious_popen(gbuf, pbuf,
                      *view,
                      (pious_sizet)(*map),
                      *faultmode,
                      *oflag,
                      (pious_modet)(*mode),
                      *seg);
  else
    *rc = PIOUS_ENAMETOOLONG;
}
