/*
 * pious_sysinfo() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfsysinfo.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

void
FUNCTION(piousfsysinfo) ARGS(`name, rc')
  int *name, *rc;
{
  *rc = pious_sysinfo(*name);
}
