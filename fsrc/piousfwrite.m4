/*
 * pious_write() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfwrite.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfwrite) ARGS(`fd, buf, nbyte, rc')
  int *fd, *nbyte, *rc;
  char *buf;
{
  *rc = pious_write(*fd, buf, (pious_sizet)(*nbyte));
}
