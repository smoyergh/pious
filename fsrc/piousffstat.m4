/*
 * pious_fstat() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousffstat.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousffstat) ARGS(`fd, dscnt, segcnt, rc')
  int *fd, *dscnt, *segcnt, *rc;
{
  struct pious_stat buf;

  if ((*rc = pious_fstat(*fd, &buf)) == PIOUS_OK)
    {
      *dscnt  = buf.dscnt;
      *segcnt = buf.segcnt;
    }
}
