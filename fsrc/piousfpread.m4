/*
 * pious_pread() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfpread.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfpread) ARGS(`fd, buf, nbyte, offset, rc')
  int *fd, *nbyte, *offset, *rc;
  char *buf;
{
  *rc = pious_pread(*fd, buf, (pious_sizet)(*nbyte), (pious_offt)(*offset));
}
