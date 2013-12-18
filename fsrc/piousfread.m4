/*
 * pious_read() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfread.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfread) ARGS(`fd, buf, nbyte, rc')
  int *fd, *nbyte, *rc;
  char *buf;
{
  *rc = pious_read(*fd, buf, (pious_sizet)(*nbyte));
}
