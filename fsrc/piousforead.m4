/*
 * pious_oread() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousforead.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousforead) ARGS(`fd, buf, nbyte, offset, rc')
  int *fd, *nbyte, *offset, *rc;
  char *buf;
{
  pious_offt tmpoff;

  if ((*rc = pious_oread(*fd, buf, (pious_sizet)(*nbyte), &tmpoff)) >= 0)
    *offset = tmpoff;
}
