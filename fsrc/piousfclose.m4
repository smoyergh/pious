/*
 * pious_close() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfclose.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfclose) ARGS(`fd, rc')
  int *fd, *rc;
{
  *rc = pious_close(*fd);
}
