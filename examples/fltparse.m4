/*
 * Fortran interface to dsfile parse routine - see src/psc/psc_cfparse.h
 *
 * For use in flibtest.f ONLY; not meant for general consumption
 *
 * @(#)fltparse.m4	2.2  28 Apr 1995  Moyer
 */

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"

#include "fdefs.h"
#include "fcstrcnvt.h"

#include "psc_cfparse.h"


void
FUNCTION(cffparse) ARGS(`STRING_ARG(dsfile),
                         cnt,
                         STRING_ARG(hname),
                         STRING_ARG(spath),
                         STRING_ARG(lpath),
                         rc')

  STRING_ARG_DECL(dsfile);
  STRING_ARG_DECL(hname);
  STRING_ARG_DECL(spath);
  STRING_ARG_DECL(lpath);
  int *cnt, *rc;
{
  char **pds_hname, **pds_spath, **pds_lpath;
  int pds_cnt, i, allfit;
  char pbuf[PIOUS_PATH_MAX + 1];

  if (!f2cstr(pbuf, PIOUS_PATH_MAX, STRING_PTR(dsfile), STRING_LEN(dsfile)))
    *rc = PIOUS_EINSUF;

  else if ((*rc = CF_parse(pbuf,
                           &pds_cnt,
                           &pds_hname,
                           &pds_spath, &pds_lpath)) == PIOUS_OK)
    {
      *cnt = pds_cnt;

      if (pds_cnt > 0)
        {
          /* validate parameters */

          if (STRING_LEN(hname) < (PIOUS_NAME_MAX * pds_cnt) ||
              STRING_LEN(spath) < (PIOUS_PATH_MAX * pds_cnt) ||
              STRING_LEN(lpath) < (PIOUS_PATH_MAX * pds_cnt))
            {
              *rc = PIOUS_EINSUF;
            }

          /* convert C to Fortran strings */

          else
            {
              for (i = 0, allfit = 1; i < pds_cnt && allfit; i++)
                if (!c2fstr(STRING_PTR(hname) + (PIOUS_NAME_MAX * i),
                            PIOUS_NAME_MAX,
                            pds_hname[i]) ||

                    !c2fstr(STRING_PTR(spath) + (PIOUS_PATH_MAX * i),
                            PIOUS_PATH_MAX,
                            pds_spath[i]) ||

                    !c2fstr(STRING_PTR(lpath) + (PIOUS_PATH_MAX * i),
                            PIOUS_PATH_MAX,
                            pds_lpath[i]))
                  {
                    allfit = 0;
                  }

              if (!allfit)
                *rc = PIOUS_EINSUF;
            }
        }
    }
}
