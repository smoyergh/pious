/*
 * pious_umask() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfumask.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfumask) ARGS(`cmask, prevcmask, rc')
  int *cmask, *prevcmask, *rc;
{
  pious_modet tmpcmask;

  if ((*rc = pious_umask((pious_modet)(*cmask), &tmpcmask)) == PIOUS_OK)
    *prevcmask = tmpcmask;
}
