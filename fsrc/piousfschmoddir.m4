/*
 * pious_schmoddir() Fortran interface - See src/plib/plib.h for description.
 *
 * @(#)piousfschmoddir.m4	2.2  28 Apr 1995  Moyer
 */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "plib.h"

#include "fdefs.h"
#include "fcstrcnvt.h"


void
FUNCTION(piousfschmoddir) ARGS(`STRING_ARG(hname),
                                STRING_ARG(spath),
                                STRING_ARG(lpath),
                                dsvcnt,
                                STRING_ARG(path),
                                mode,
                                rc')
  STRING_ARG_DECL(hname);
  STRING_ARG_DECL(spath);
  STRING_ARG_DECL(lpath);
  STRING_ARG_DECL(path);
  int *dsvcnt, *mode, *rc;
{
  struct pious_dsvec *dsv;
  char *hnstore, *spstore, *lpstore;
  char pbuf[PIOUS_PATH_MAX + 1];
  int i;

  dsv     = NULL;
  hnstore = spstore = lpstore = NULL;

  /* validate parameters */

  if (*dsvcnt <= 0 ||
      STRING_LEN(hname) < (PIOUS_NAME_MAX * (*dsvcnt)) ||
      STRING_LEN(spath) < (PIOUS_PATH_MAX * (*dsvcnt)) ||
      STRING_LEN(lpath) < (PIOUS_PATH_MAX * (*dsvcnt)))
    {
      *rc = PIOUS_EINVAL;
    }


  else if (!f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(path), STRING_LEN(path)))
    *rc = PIOUS_ENAMETOOLONG;


  /* allocate data server information storage */

  else if ((dsv = (struct pious_dsvec *)
                  malloc((*dsvcnt) * sizeof(struct pious_dsvec))) == NULL ||

           (hnstore = malloc((*dsvcnt) * (PIOUS_NAME_MAX + 1))) == NULL ||

           (spstore = malloc((*dsvcnt) * (PIOUS_PATH_MAX + 1))) == NULL ||

           (lpstore = malloc((*dsvcnt) * (PIOUS_PATH_MAX + 1))) == NULL)
    {
      *rc = PIOUS_EINSUF;
    }


  /* fill dsv structure and make call. */

  else
    {
      for (i = 0; i < *dsvcnt; i++)
        {
          dsv[i].hname = hnstore + ((PIOUS_NAME_MAX + 1) * i);
          dsv[i].spath = spstore + ((PIOUS_PATH_MAX + 1) * i);
          dsv[i].lpath = lpstore + ((PIOUS_PATH_MAX + 1) * i);

          /* f2cstr() must succeed since sufficient storage alloced */

          f2cstr(dsv[i].hname, PIOUS_NAME_MAX,
                 STRING_PTR(hname) + (PIOUS_NAME_MAX * i), PIOUS_NAME_MAX);

          f2cstr(dsv[i].spath, PIOUS_PATH_MAX,
                 STRING_PTR(spath) + (PIOUS_PATH_MAX * i), PIOUS_PATH_MAX);

          f2cstr(dsv[i].lpath, PIOUS_PATH_MAX,
                 STRING_PTR(lpath) + (PIOUS_PATH_MAX * i), PIOUS_PATH_MAX);
        }

      *rc = pious_schmoddir(dsv, *dsvcnt, pbuf, (pious_modet)(*mode));
    }

  if (dsv != NULL)
    free((char *)dsv);

  if (hnstore != NULL)
    free(hnstore);

  if (spstore != NULL)
    free(spstore);

  if (lpstore != NULL)
    free(lpstore);
}
