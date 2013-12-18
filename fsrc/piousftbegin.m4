/*
 * pious_tbegin() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousftbegin.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousftbegin) ARGS(`faultmode, rc')
  int *faultmode, *rc;
{
  *rc = pious_tbegin(*faultmode);
}
