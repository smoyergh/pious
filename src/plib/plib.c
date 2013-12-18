/* PIOUS1 Parallel Input/OUtput System
 * Copyright (C) 1994,1995 by Steven A. Moyer and V. S. Sunderam
 *
 * PIOUS1 is a software system distributed under the terms of the
 * GNU Library General Public License Version 2.  All PIOUS1 software,
 * including PIOUS1 code that is not intended to be directly linked with
 * non PIOUS1 code, is considered to be part of a single logical software
 * library for the purposes of licensing and distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */




/* PIOUS Library Functions (PLIB)
 *
 * @(#)plib.c	2.2  28 Apr 1995  Moyer
 *
 * The PIOUS library functions define the file model and interface (API)
 * exported to the user.  Essentially, library functions translate user
 * operations into the appropriate PIOUS Data Server (PDS) and PIOUS
 * Service Coordinator (PSC) service requests.
 *
 * ----------------------------------------------------------------------------
 *
 * Though internally PIOUS operates on parafile objects, as implemented by
 * the PIOUS Service Coordinator (see psc/psc.h), the library functions may
 * define any logical file model that can reasonably be implemented on top
 * of parafiles.
 *
 * File abstraction layering in PIOUS is as follows:
 *
 *     0) At the lowest layer, independent PDS provide transaction-based
 *        access to local files that represent a linear sequence of bytes.
 *        PDS have no notion of a higher level file structure.
 *
 *     1) At the middle layer, the PSC implements parafiles.  Parafiles are
 *        two-dimensional files composed of one or more physically disjoint
 *        data segment, where each data segment is a linear sequence of zero
 *        or more bytes.
 *
 *        Note that the logical parafile structure directly reflects the
 *        physical structure that naturally results from declustering file
 *        data.
 *
 *     2) At the highest layer, PIOUS library functions implement the file
 *        model and interface exported to the user and determine access
 *        semantics and policies.  User-level file objects represent logical
 *        "views" of the underlying parafile objects.
 *
 * Layering PIOUS in this manner achieves a clean separation of mechanism
 * and policy, allowing a new user-level file model/interface to be implemented
 * without modification of the PDS or PSC.
 *
 * ----------------------------------------------------------------------------
 *
 *
 * Function Summary:
 *
 * pious_{s}open()
 * pious_{s}popen()
 * pious_close()
 * pious_fstat()
 * pious_sysinfo()
 *
 * pious_read()
 * pious_oread()
 * pious_pread()
 * pious_write()
 * pious_owrite()
 * pious_pwrite()
 * pious_lseek()
 *
 * pious_tbegin()
 * pious_tabort()
 * pious_tend()
 *
 * pious_setcwd()
 * pious_getcwd()
 * pious_umask()
 *
 * pious_{s}chmod()
 * pious_{s}unlink()
 * pious_{s}mkdir()
 * pious_{s}rmdir()
 * pious_{s}chmoddir()
 *
 * pious_shutdown()
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) Stable mode access is not implemented except for benchmarking purposes.
 *      Currently the PDS do not perform recovery and the library routines do
 *      not log the outcome of a transaction; both must be implemented to
 *      guarantee recoverability.
 *
 *      To enable stable mode access for benchmarking compile with the
 *      STABLEACCESS flag set.
 *
 *   2) All functions return a result of PIOUS_EUNXP after the execution of
 *      a library function that results in an inconsistent system state.
 *      The single exception is pious_sysinfo(PIOUS_BADSTATE), which will
 *      return a value of TRUE.
 *
 *      Any library function that causes a transition into an inconsistent
 *      state attempts to return a meaningful error code so that the
 *      reason for the transition can be determined.
 */


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include <stdio.h>
#include <string.h>

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "pious_sysconfig.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"

#include "pdce_srcdestt.h"
#include "pdce_msgtagt.h"
#include "pdce.h"

#include "pds.h"
#include "psc.h"

#include "plib.h"




/*
 * Private Declaration - Types, Constants, and Macros
 */


/* macros specifying access_generic() action */

#define READ      0
#define WRITE     1
#define FILEPTR   ((pious_offt) -1)


/* macros for operating on global control message id */

#define CntrlIdNext  ((cmsgid == PIOUS_INT_MAX) ? (cmsgid = 0) : (++cmsgid))
#define CntrlIdLast  (cmsgid)


/* PDS transaction operation request state */

typedef struct trans_state{
  dce_srcdestt pdsid;         /* PDS message passing id */
  int transsn;                /* next trans sequence number expected by PDS */
  long rcode;                 /* trans operation send/recv result code */
  int linkcnt;                /* number of file table entries sharing state */
  struct trans_state *next;   /* next entry in list */
  struct trans_state *prev;   /* prev entry in list */
} trans_statet;


/* file table entry */

typedef struct {
  int valid;                  /* table entry valid flag */
  struct PSC_dsinfo *pds;     /* data server information; NULL if default */
  char *group;                /* group accessing file */
  char *path;                 /* file path name */
  int view;                   /* file view for group */
  pious_sizet map;            /* view dependent access mapping */
  int faultmode;              /* file access fault-tolerance mode */
  int oflag;                  /* file access mode flags */
  pious_offt offset;          /* file offset - local file pointer */
  int utrans_access;          /* file accessed by user-level transaction */
  pious_offt utrans_offset;   /* 'offset' at start of user-level transaction */
  trans_statet **trans_state; /* transaction state at each PDS hosting file */
  struct PSC_pfinfo *pfinfo;  /* information record for underlying parafile */
} ftable_entryt;




/*
 * Private Variable Definitions
 */


/* File Table
 *
 *   Table of open file descriptions; file_table_max serves as a high-water
 *   mark for searching the file table.
 */

static ftable_entryt file_table[PLIB_OPEN_MAX];
static int file_table_max = 0;


/* PDS Transaction Operation Protocol State List
 *
 *   Transaction state of each PDS that contains a portion of one or more of
 *   all files currently open.  In addition, the variable pds_trans_prepared
 *   indicates if the most recent transaction was prepared.
 *
 *   This list should not be modified often and is likely to contain very few
 *   entries, so search performance should not be an issue; hence it is a
 *   simple non-hashed linked list.
 */

static trans_statet *pds_trans_state = NULL;
static int pds_trans_state_cnt       = 0;
static int pds_trans_prepared        = FALSE;


/* Transaction ID to reuse
 *
 * Each transaction is normally assigned a unique (time-stamped) id.
 * However, if a PDS aborts a transaction due to the inability to obtain a
 * lock, the next transaction (presumably a re-try of the previous operation)
 * should be submitted under the same id; doing so results in a higher
 * priority for that transaction and guarantees that a transaction with that
 * id must eventually complete.
 *
 * + transid2reuse consumed when transid assigned in access_generic(),
 *   lseek_proxy(), and pious_tbegin()
 *
 * + transid2reuse set when lock acquisition attempt fails in access_generic()
 *   and lseek_proxy()
 */

static pds_transidt transid2reuse;
static int transid2reuse_valid = FALSE;


/* File Mode Creation Mask */

static pious_modet creatmask = 0;


/* Current Working Directory String */

static char *cwd = NULL;


/* User-Level Transaction Flags and Transaction ID */

static int utrans_in_progress = FALSE;
static pds_transidt utrans_id;
static int utrans_faultmode;


/* Service Coordinator Configuration Information */

static int psc_config_valid = FALSE;
static struct PSC_configinfo psc_config;


/* Global Control Message Id */

static int cmsgid = 0;


/* Inconsistent System State Flag */

static int badstate = FALSE;




/*
 * Private Function Declarations
 */

#ifdef __STDC__
static int open_generic(struct PSC_dsinfo *pds,
			char *group,
			char *path,
			int view,
			pious_sizet map,
			int faultmode,
			int oflag,
			pious_modet mode,
			int seg);

static pious_ssizet access_generic(int action,
				   int fd,
				   char *buf,
				   pious_sizet nbyte,
				   pious_offt offset,
				   pious_offt *eoff);

static pious_offt lseek_proxy(int fd,
			      pious_offt offset,
			      int whence);

static int prepare_all(pds_transidt transid,
		       trans_statet **tsv,
		       int cnt);

static int commit_all(pds_transidt transid,
		      trans_statet **tsv,
		      int cnt);

static int abort_all(pds_transidt transid,
		     trans_statet **tsv,
		     int cnt);

static int ping_all(trans_statet **tsv,
		    int cnt);

static int dsv2dsinfo(struct pious_dsvec *dsv,
		      int dsvcnt,
		      struct PSC_dsinfo **dsinfo);

static int tstate_insert(dce_srcdestt *idv,
			 trans_statet **tsv,
			 int cnt);

static void tstate_rm(register trans_statet **tsv,
		      int cnt);

static void ftable_dealloc(ftable_entryt *ftable);

static void dsinfo_dealloc(struct PSC_dsinfo *pds);

static char *mk_fullpath(char *pathname);
#else
static int open_generic();

static pious_ssizet access_generic();

static pious_offt lseek_proxy();

static int prepare_all();

static int commit_all();

static int abort_all();

static int ping_all();

static int dsv2dsinfo();

static int tstate_insert();

static void tstate_rm();

static void ftable_dealloc();

static void dsinfo_dealloc();

static char *mk_fullpath();
#endif




/*
 * Function Definitions - PLIB functions
 */


/*
 * pious_open() - See plib.h for description.
 */

#ifdef __STDC__
int pious_open(char *path,
	       int oflag,
	       pious_modet mode)
#else
int pious_open(path, oflag, mode)
     char *path;
     int oflag;
     pious_modet mode;
#endif
{
  int rcode, seg;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* set the number of file segments to the number of default data servers */

  else if ((seg = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (seg < 0)
	rcode = seg;
      else
	rcode = PIOUS_EINVAL;
    }

  else
    { /* attempt open of file on default data servers */
      rcode = open_generic((struct PSC_dsinfo *)NULL,
			   "",
			   path,
			   PIOUS_INDEPENDENT,
			   (pious_sizet)1,
			   PIOUS_VOLATILE,
			   oflag,
			   mode,
			   seg);
    }

  return rcode;
}




/*
 * pious_sopen() - See plib.h for description.
 */

#ifdef __STDC__
int pious_sopen(struct pious_dsvec *dsv,
		int dsvcnt,
		char *path,
		int oflag,
		pious_modet mode)
#else
int pious_sopen(dsv, dsvcnt, path, oflag, mode)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
     int oflag;
     pious_modet mode;
#endif
{
  int rcode, seg;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* set the number of file segments to the number of default data servers */

  else if ((seg = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (seg < 0)
	rcode = seg;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) == PIOUS_OK)
    {
      /* attempt to open file on specified data servers */

      rcode = open_generic(pds,
			   "",
			   path,
			   PIOUS_INDEPENDENT,
			   (pious_sizet)1,
			   PIOUS_VOLATILE,
			   oflag,
			   mode,
			   seg);

      if (rcode < 0)
	/* open failed; deallocate data server information record */
	dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_popen() - See plib.h for description.
 */

#ifdef __STDC__
int pious_popen(char *group,
		char *path,
		int view,
		pious_sizet map,
		int faultmode,
		int oflag,
		pious_modet mode,
		int seg)
#else
int pious_popen(group, path, view, map, faultmode, oflag, mode, seg)
     char *group;
     char *path;
     int view;
     pious_sizet map;
     int faultmode;
     int oflag;
     pious_modet mode;
     int seg;
#endif
{
  int rcode, pdsdflt;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'group' arg; null group name reserved for pious_{s}open() */

  else if (group == NULL || *group == '\0')
    rcode = PIOUS_EINVAL;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* attempt open of file on default data servers */

  else
    rcode = open_generic((struct PSC_dsinfo *)NULL,
			 group, path, view, map, faultmode, oflag, mode, seg);

  return rcode;
}




/*
 * pious_spopen() - See plib.h for description.
 */

#ifdef __STDC__
int pious_spopen(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *group,
		 char *path,
		 int view,
		 pious_sizet map,
		 int faultmode,
		 int oflag,
		 pious_modet mode,
		 int seg)
#else
int pious_spopen(dsv, dsvcnt,
		 group, path, view, map, faultmode, oflag, mode, seg)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *group;
     char *path;
     int view;
     pious_sizet map;
     int faultmode;
     int oflag;
     pious_modet mode;
     int seg;
#endif
{
  int rcode;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'group' arg; null group name reserved for pious_{s}open() */

  else if (group == NULL || *group == '\0')
    rcode = PIOUS_EINVAL;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) == PIOUS_OK)
    {
      /* attempt to open file on specified data servers */

      rcode =
	open_generic(pds, group, path, view, map, faultmode, oflag, mode, seg);

      if (rcode < 0)
	/* open failed; deallocate data server information record */
	dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_close() - See plib.h for description.
 */

#ifdef __STDC__
int pious_close(int fd)
#else
int pious_close(fd)
     int fd;
#endif
{
  int rcode, acode;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'fd' argument */

  else if (fd < 0 || fd >= PLIB_OPEN_MAX || !file_table[fd].valid)
    rcode = PIOUS_EBADF;

  /* forbid closing file if accessed by a currently active user-level trans */

  else if (utrans_in_progress && file_table[fd].utrans_access)
    rcode = PIOUS_EPERM;

#ifdef VTCOMMITNOACK
  /* if file fault tolerance mode is volatile, then ping all data servers on
   * which file resides to insure all commit messages relating to this file
   * have been received/processed.  presumes ordered message delivery between
   * communicating pairs.
   */

  else if ((file_table[fd].faultmode == PIOUS_VOLATILE) &&
	   (acode = ping_all(file_table[fd].trans_state,
			     file_table[fd].pfinfo->pds_cnt)) != PIOUS_OK)
    { /* unable to ping all data servers */

      switch(acode)
	{
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = acode;
	  break;
	case PIOUS_ESRCDEST:
	  /* should never occur; inconsistent system state (bug in PIOUS) */
	  rcode    = PIOUS_EUNXP;
	  badstate = TRUE;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }
#endif

  /* request PSC to close file */

  else if ((acode = PSC_sclose(file_table[fd].pds,
			       file_table[fd].group,
			       file_table[fd].path)) != PIOUS_OK)
    { /* error closing */

      switch (acode)
	{
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = acode;
	  break;
	case PIOUS_ENOENT:
	  /* should never occur; inconsistent system state (bug in PIOUS) */
	  rcode    = PIOUS_EUNXP;
	  badstate = TRUE;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  /* deallocate/invalidate file table entry */

  else
    { /* remove PDS transaction state recs from global list, if necessary */
      tstate_rm(file_table[fd].trans_state, file_table[fd].pfinfo->pds_cnt);

      /* deallocate/invalidate entry */
      ftable_dealloc(&file_table[fd]);

      /* adjust file table high-water mark if required */
      while (file_table_max > 0 && !file_table[file_table_max - 1].valid)
	file_table_max--;

      rcode = PIOUS_OK;
    }

  return rcode;
}




/*
 * pious_fstat() - See plib.h for description.
 */

#ifdef __STDC__
int pious_fstat(int fd,
		struct pious_stat *buf)
#else
int pious_fstat(fd, buf)
     int fd;
     struct pious_stat *buf;
#endif
{
  int rcode;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'buf' argument */

  else if (buf == NULL)
    rcode = PIOUS_EINVAL;

  /* validate 'fd' argument */

  else if (fd < 0 || fd >= PLIB_OPEN_MAX || !file_table[fd].valid)
    rcode = PIOUS_EBADF;

  /* set file status information */

  else
    {
      buf->dscnt  = file_table[fd].pfinfo->pds_cnt;
      buf->segcnt = file_table[fd].pfinfo->seg_cnt;

      rcode = PIOUS_OK;
    }

  return rcode;
}




/*
 * pious_sysinfo() - See plib.h for description.
 */

#ifdef __STDC__
long pious_sysinfo(int name)
#else
long pious_sysinfo(name)
     int name;
#endif
{
  long rcode, psccode;

  /* check for inconsistent system state */

  if (badstate)
    { /* special case: return TRUE if enquiring about PIOUS state */
      if (name == PIOUS_BADSTATE)
	rcode = TRUE;
      else
	rcode = PIOUS_EUNXP;
    }

  else
    { /* get PSC configuration information */
      rcode = PIOUS_OK;

      if (!psc_config_valid)
	{
	  if ((psccode = PSC_config(&psc_config)) == PIOUS_OK)
	    { /* successfully obtained configuration info */
	      psc_config_valid = TRUE;
	    }

	  else
	    { /* error querying configuration info */
	      switch(psccode)
		{
		case PIOUS_EINSUF:
		case PIOUS_ETPORT:
		  rcode = psccode;
		  break;
		default:
		  rcode = PIOUS_EUNXP;
		  break;
		}
	    }
	}

      /* return appropriate configuration value */

      if (rcode == PIOUS_OK)
	{ /* config parameter determined by name */
	  switch(name)
	    {
	    case PIOUS_DS_DFLT:
	      rcode = psc_config.def_pds_cnt;
	      break;
	    case PIOUS_OPEN_MAX:
	      rcode = PLIB_OPEN_MAX;
	      break;
	    case PIOUS_TAG_LOW:
	      rcode = DCE_MSGTAGT_BASE;
	      break;
	    case PIOUS_TAG_HIGH:
	      rcode = DCE_MSGTAGT_BASE + DCE_MSGTAGT_MAX;
	      break;
	    case PIOUS_BADSTATE:
	      rcode = badstate;
	      break;
	    default:
	      /* invalid 'name' argument */
	      rcode = PIOUS_EINVAL;
	      break;
	    }
	}
    }

  return rcode;
}




/*
 * pious_read() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_read(int fd,
			char *buf,
			pious_sizet nbyte)
#else
pious_ssizet pious_read(fd, buf, nbyte)
     int fd;
     char *buf;
     pious_sizet nbyte;
#endif
{
  pious_ssizet rcode;
  pious_offt eoff;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(READ,
			   fd,
			   buf,
			   nbyte,
			   FILEPTR,
			   &eoff)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_oread() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_oread(int fd,
			 char *buf,
			 pious_sizet nbyte,
			 pious_offt *offset)
#else
pious_ssizet pious_oread(fd, buf, nbyte, offset)
     int fd;
     char *buf;
     pious_sizet nbyte;
     pious_offt *offset;
#endif
{
  pious_ssizet rcode;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(READ,
			   fd,
			   buf,
			   nbyte,
			   FILEPTR,
			   offset)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_pread() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_pread(int fd,
			 char *buf,
			 pious_sizet nbyte,
			 pious_offt offset)
#else
pious_ssizet pious_pread(fd, buf, nbyte, offset)
     int fd;
     char *buf;
     pious_sizet nbyte;
     pious_offt offset;
#endif
{
  pious_ssizet rcode;
  pious_offt eoff;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'offset' parameter to insure not equal to FILEPTR */

  else if (offset == FILEPTR)
    { /* must abort a user-level transaction to insure proper semantics */
      if (utrans_in_progress)
	pious_tabort();

      rcode = PIOUS_EINVAL;
    }

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(READ,
			   fd,
			   buf,
			   nbyte,
			   offset,
			   &eoff)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_write() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_write(int fd,
			 char *buf,
			 pious_sizet nbyte)
#else
pious_ssizet pious_write(fd, buf, nbyte)
     int fd;
     char *buf;
     pious_sizet nbyte;
#endif
{
  pious_ssizet rcode;
  pious_offt eoff;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(WRITE,
			   fd,
			   buf,
			   nbyte,
			   FILEPTR,
			   &eoff)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_owrite() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_owrite(int fd,
			  char *buf,
			  pious_sizet nbyte,
			  pious_offt *offset)
#else
pious_ssizet pious_owrite(fd, buf, nbyte, offset)
     int fd;
     char *buf;
     pious_sizet nbyte;
     pious_offt *offset;
#endif
{
  pious_ssizet rcode;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(WRITE,
			   fd,
			   buf,
			   nbyte,
			   FILEPTR,
			   offset)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_pwrite() - See plib.h for description.
 */

#ifdef __STDC__
pious_ssizet pious_pwrite(int fd,
			  char *buf,
			  pious_sizet nbyte,
			  pious_offt offset)
#else
pious_ssizet pious_pwrite(fd, buf, nbyte, offset)
     int fd;
     char *buf;
     pious_sizet nbyte;
     pious_offt offset;
#endif
{
  pious_ssizet rcode;
  pious_offt eoff;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'offset' parameter to insure not equal to FILEPTR */

  else if (offset == FILEPTR)
    { /* must abort a user-level transaction to insure proper semantics */
      if (utrans_in_progress)
	pious_tabort();

      rcode = PIOUS_EINVAL;
    }

  /* perform access operation */

  else
    while ((rcode =
	    access_generic(WRITE,
			   fd,
			   buf,
			   nbyte,
			   offset,
			   &eoff)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_lseek() - See plib.h for description.
 */

#ifdef __STDC__
pious_offt pious_lseek(int fd,
		       pious_offt offset,
		       int whence)
#else
pious_offt pious_lseek(fd, offset, whence)
     int fd;
     pious_offt offset;
     int whence;
#endif
{
  pious_offt rcode;
  int retry;

  /* set retry count */

  if (utrans_in_progress)
    retry = 1;
  else
    retry = PLIB_RETRY_MAX;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* perform lseek operation */

  else
    while ((rcode =
	    lseek_proxy(fd,
			offset,
			whence)) == PIOUS_EABORT && --retry && !badstate);

  return rcode;
}




/*
 * pious_tbegin() - See plib.h for description.
 */

#ifdef __STDC__
int pious_tbegin(int faultmode)
#else
int pious_tbegin(faultmode)
     int faultmode;
#endif
{
  int i, rcode;
  register trans_statet *tstate;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'faultmode' argument */

#ifdef STABLEACCESS
  else if (faultmode != PIOUS_VOLATILE && faultmode != PIOUS_STABLE)
    rcode = PIOUS_EINVAL;
#else
  else if (faultmode != PIOUS_VOLATILE)
    rcode = PIOUS_EINVAL;
#endif

  /* determine if a user-level transaction is already in progress */

  else if (utrans_in_progress)
    rcode = PIOUS_EPERM;

  /* initiate user-level transaction */

  else
    { /* assign a user-level transaction id */

      rcode = PIOUS_OK;

      if (transid2reuse_valid)
	{
	  utrans_id           = transid2reuse;
	  transid2reuse_valid = FALSE;
	}
      else if (transid_assign(&utrans_id) != PIOUS_OK)
	rcode = PIOUS_EUNXP;

      /* set user-level transaction state variables */

      if (rcode == PIOUS_OK)
	{ /* scan file table and set utrans_access flag & utrans_offset val */

	  for (i = 0; i < file_table_max; i++)
	    { /* don't bother checking if valid; as much work as setting */
	      file_table[i].utrans_access = FALSE;
	      file_table[i].utrans_offset = file_table[i].offset;
	    }

	  /* scan PDS trans op protocol state list & set vals appropriately */

	  for (tstate = pds_trans_state; tstate != NULL; tstate = tstate->next)
	    tstate->transsn = 0;

	  pds_trans_prepared = FALSE;

	  /* set user-level transaction faultmode and mark as in progress */

	  utrans_faultmode   = faultmode;
	  utrans_in_progress = TRUE;
	}
    }

  return rcode;
}




/*
 * pious_tend() - See plib.h for description.
 */

#ifdef __STDC__
int pious_tend(void)
#else
int pious_tend()
#endif
{
  int i, rcode, acode;
  register trans_statet *tstate, **tsv;

  /* pious_tend(): any error implies trans aborted or state inconsistent */

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* determine if no user-level transaction in progress or no open files */

  else if (!utrans_in_progress || file_table_max == 0)
    {
      utrans_in_progress = FALSE;
      rcode              = PIOUS_OK;
    }

  /* prepare/commit/abort user-level transaction as required */

  else
    { /* allocate storage to perform operation. persist until succeeds since
       * pious_tend() semantics state that any error implies a transaction
       * is aborted; can not abort until have storage to work with.
       */

      while ((tsv = (trans_statet **)
	      malloc((unsigned)(pds_trans_state_cnt *
				sizeof(trans_statet *)))) == NULL);


      /* set tsv to point to all transaction operation state records */

      for (i = 0, tstate = pds_trans_state; i < pds_trans_state_cnt; i++)
	{
	  tsv[i] = tstate;
	  tstate = tstate->next;
	}

      /* prepare transaction if it is to be stable */

      acode = PIOUS_OK;

      if (utrans_faultmode == PIOUS_STABLE)
	{
	  acode = prepare_all(utrans_id, tsv, pds_trans_state_cnt);
	}

      /* commit successful transaction or abort failed transaction */

      if (acode == PIOUS_OK)
	{ /* commit transaction */
	  acode = commit_all(utrans_id, tsv, pds_trans_state_cnt);

	  if (acode == PIOUS_ENOTLOG)
	    /* do not care if commit action not logged for stable trans */
	    acode = PIOUS_OK;

	  if (acode != PIOUS_OK)
	    /* not all PDS committed; inconsistent file state */
	    badstate = TRUE;
	}

      else
	{ /* abort transaction, retaining original error code from prepare */
	  i = abort_all(utrans_id, tsv, pds_trans_state_cnt);

	  if (i == PIOUS_OK || i == PIOUS_ENOTLOG)
	    { /* transaction aborted; restore local file pointer values */
	      for (i = 0; i < file_table_max; i++)
		if (file_table[i].valid && file_table[i].utrans_access)
		  file_table[i].offset = file_table[i].utrans_offset;
	    }

	  else
	    { /* can not abort; PIOUS state potentially inconsistent */
	      badstate = TRUE;
	    }
	}

      /* set final result code */

      switch(acode)
	{
	case PIOUS_OK:
	  /* successfully committed transaction */
	case PIOUS_EABORT:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  /* a "standard" error has occured */
	  rcode = acode;
	  break;

	case PIOUS_ESRCDEST:
	case PIOUS_EPROTO:
	  /* should never occur; inconsistent system state (bug in PIOUS) */
	  rcode    = PIOUS_EUNXP;
	  badstate = TRUE;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free((char *)tsv);

      /* mark completion of user-level transaction */
      utrans_in_progress = FALSE;
    }

  return rcode;
}




/*
 * pious_tabort() - See plib.h for description.
 */

#ifdef __STDC__
int pious_tabort(void)
#else
int pious_tabort()
#endif
{
  int i, rcode, acode;
  register trans_statet *tstate, **tsv;

  /* pious_tabort(): any error implies state is inconsistent */

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* determine if no user-level transaction in progress or no open files */

  else if (!utrans_in_progress || file_table_max == 0)
    {
      utrans_in_progress = FALSE;
      rcode              = PIOUS_OK;
    }

  /* abort user-level transaction */

  else
    { /* allocate storage to perform operation. persist until succeeds since
       * pious_tabort() semantics state that any error implies an inconsistent
       * system state.
       */

      while ((tsv = (trans_statet **)
	      malloc((unsigned)(pds_trans_state_cnt *
				sizeof(trans_statet *)))) == NULL);

      /* set tsv to point to all transaction operation state records */

      for (i = 0, tstate = pds_trans_state; i < pds_trans_state_cnt; i++)
	{
	  tsv[i] = tstate;
	  tstate = tstate->next;
	}

      /* abort transaction */

      acode = abort_all(utrans_id, tsv, pds_trans_state_cnt);

      if (acode == PIOUS_ENOTLOG)
	/* do not care if abort action not logged for stable trans */
	acode = PIOUS_OK;

      if (acode == PIOUS_OK)
	{ /* transaction aborted; restore local file pointer values */

	  for (i = 0; i < file_table_max; i++)
	    if (file_table[i].valid && file_table[i].utrans_access)
	      file_table[i].offset = file_table[i].utrans_offset;
	}

      else
	{ /* not all PDS aborted; PIOUS state potentially inconsistent */
	  badstate = TRUE;
	}

      /* set final result code */

      switch(acode)
	{
	case PIOUS_OK:
	  /* successfully aborted transaction */
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  /* a "standard" error has occured */
	  rcode = acode;
	  break;

	case PIOUS_ESRCDEST: /* should never occur, implies bug in PIOUS */
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free((char *)tsv);

      /* mark completion of user-level transaction */
      utrans_in_progress = FALSE;
    }

  return rcode;
}




/*
 * pious_setcwd() - See plib.h for description.
 */

#ifdef __STDC__
int pious_setcwd(char *path)
#else
int pious_setcwd(path)
     char *path;
#endif
{
  int rcode;
  char *tmpcwd;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  /* allocate storage for new current working directory string */

  else if ((tmpcwd = malloc((unsigned)(strlen(path) + 1))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* dealloc previous cwd string, if necessary, and assign cwd 'path' */
      if (cwd != NULL)
	free(cwd);

      cwd = tmpcwd;

      strcpy(cwd, path);

      rcode = PIOUS_OK;
    }

  return rcode;
}




/*
 * pious_getcwd() - See plib.h for description.
 */

#ifdef __STDC__
int pious_getcwd(char *buf, pious_sizet size)
#else
int pious_getcwd(buf, size)
     char *buf;
     pious_sizet size;
#endif
{
  int rcode;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'buf' and 'size' arguments */

  else if (buf == NULL || size <= 0)
    rcode = PIOUS_EINVAL;

  /* determine if buffer size is sufficiently large */

  else if (cwd != NULL && size < (strlen(cwd) + 1))
    rcode = PIOUS_EINSUF;

  else
    { /* copy current working directory string to 'buf', if extant */

      if (cwd != NULL)
	strcpy(buf, cwd);
      else
	*buf = '\0';

      rcode = PIOUS_OK;
    }

  return rcode;
}




/*
 * pious_umask() - See plib.h for description.
 */

#ifdef __STDC__
int pious_umask(pious_modet cmask,
		pious_modet *prevcmask)
#else
int pious_umask(cmask, prevcmask)
     pious_modet cmask;
     pious_modet *prevcmask;
#endif
{
  int rcode;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'prevcmask' argument */

  else if (prevcmask == NULL)
    rcode = PIOUS_EINVAL;

  /* set file creation mask as specified by 'cmask' and return old value */

  else
    {
      *prevcmask = creatmask;
      creatmask  = (cmask & (PIOUS_IRWXU | PIOUS_IRWXG | PIOUS_IRWXO));
      rcode      = PIOUS_OK;
    }

  return rcode;
}




/*
 * pious_chmod() - See plib.h for description.
 */

#ifdef __STDC__
int pious_chmod(char *path,
		pious_modet mode)
#else
int pious_chmod(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode, pdsdflt;
  char *fullpath;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using current working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* chmod of underlying parafile on default data servers */

  else
    {
      acode = PSC_chmod(fullpath, mode);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_EBUSY:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
    }

  return rcode;
}




/*
 * pious_schmod() - See plib.h for description.
 */

#ifdef __STDC__
int pious_schmod(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path,
		 pious_modet mode)
#else
int pious_schmod(dsv, dsvcnt, path, mode)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode;
  char *fullpath;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using curring working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) != PIOUS_OK)
    free(fullpath);

  /* chmod of underlying parafile on specified data servers */

  else
    {
      acode = PSC_schmod(pds, fullpath, mode);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_EBUSY:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
      dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_unlink() - See plib.h for description.
 */

#ifdef __STDC__
int pious_unlink(char *path)
#else
int pious_unlink(path)
     char *path;
#endif
{
  int rcode, acode, pdsdflt;
  char *fullpath;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using current working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* unlink underlying parafile on default data servers */

  else
    {
      acode = PSC_unlink(fullpath);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_EBUSY:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
    }

  return rcode;
}




/*
 * pious_sunlink() - See plib.h for description.
 */

#ifdef __STDC__
int pious_sunlink(struct pious_dsvec *dsv,
		  int dsvcnt,
		  char *path)
#else
int pious_sunlink(dsv, dsvcnt, path)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
#endif
{
  int rcode, acode;
  char *fullpath;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using curring working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) != PIOUS_OK)
    free(fullpath);

  /* unlink underlying parafile on specified data servers */

  else
    {
      acode = PSC_sunlink(pds, fullpath);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_EBUSY:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
      dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_mkdir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_mkdir(char *path,
		pious_modet mode)
#else
int pious_mkdir(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode, pdsdflt;
  char *fullpath;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using current working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* perform mkdir on all default data servers */

  else
    {
      acode = PSC_mkdir(fullpath, (mode & (~creatmask)));

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_EEXIST:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
    }

  return rcode;
}




/*
 * pious_smkdir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_smkdir(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path,
		 pious_modet mode)
#else
int pious_smkdir(dsv, dsvcnt, path, mode)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode;
  char *fullpath;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using curring working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) != PIOUS_OK)
    free(fullpath);

  /* perform mkdir on all specified data servers */

  else
    {
      acode = PSC_smkdir(pds, fullpath, (mode & (~creatmask)));

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_EEXIST:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
      dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_rmdir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_rmdir(char *path)
#else
int pious_rmdir(path)
     char *path;
#endif
{
  int rcode, acode, pdsdflt;
  char *fullpath;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using current working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* perform rmdir on all default data servers */

  else
    {
      acode = PSC_rmdir(fullpath);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_EBUSY:
	case PIOUS_ENOTEMPTY:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
    }

  return rcode;
}




/*
 * pious_srmdir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_srmdir(struct pious_dsvec *dsv,
		 int dsvcnt,
		 char *path)
#else
int pious_srmdir(dsv, dsvcnt, path)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
#endif
{
  int rcode, acode;
  char *fullpath;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using curring working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) != PIOUS_OK)
    free(fullpath);

  /* perform rmdir on all specified data servers */

  else
    {
      acode = PSC_srmdir(pds, fullpath);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_EBUSY:
	case PIOUS_ENOTEMPTY:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
      dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_chmoddir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_chmoddir(char *path,
		   pious_modet mode)
#else
int pious_chmoddir(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode, pdsdflt;
  char *fullpath;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check that default data servers have been specified */

  else if ((pdsdflt = (int)pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    { /* no default data servers, or error in query */
      if (pdsdflt < 0)
	rcode = pdsdflt;
      else
	rcode = PIOUS_EINVAL;
    }

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using current working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* perform chmoddir on all default data servers */

  else
    {
      acode = PSC_chmoddir(fullpath, mode);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
    }

  return rcode;
}




/*
 * pious_schmoddir() - See plib.h for description.
 */

#ifdef __STDC__
int pious_schmoddir(struct pious_dsvec *dsv,
		    int dsvcnt,
		    char *path,
		    pious_modet mode)
#else
int pious_schmoddir(dsv, dsvcnt, path, mode)
     struct pious_dsvec *dsv;
     int dsvcnt;
     char *path;
     pious_modet mode;
#endif
{
  int rcode, acode;
  char *fullpath;
  struct PSC_dsinfo *pds;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* validate 'path' argument */

  else if (path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  /* form full path name using curring working directory string */

  else if ((fullpath = mk_fullpath(path)) == NULL)
    rcode = PIOUS_EINSUF;

  /* validate 'dsv' and 'dsvcnt' and form proper data server info record */

  else if ((rcode = dsv2dsinfo(dsv, dsvcnt, &pds)) != PIOUS_OK)
    free(fullpath);

  /* perform chmoddir on all specified data servers */

  else
    {
      acode = PSC_schmoddir(pds, fullpath, mode);

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINVAL:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;

	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */
      free(fullpath);
      dsinfo_dealloc(pds);
    }

  return rcode;
}




/*
 * pious_shutdown() - See plib.h for description.
 */

#ifdef __STDC__
int pious_shutdown(void)
#else
int pious_shutdown()
#endif
{
  int rcode, acode;

  /* check for inconsistent system state */

  if (badstate)
    rcode = PIOUS_EUNXP;

  /* check if user-transaction in progress or process has files open */

  else if (file_table_max != 0 || utrans_in_progress)
    rcode = PIOUS_EPERM;

  else
    { /* request PSC to shutdown PIOUS system; will only do so if no process
       * has a file open.
       */

      acode = PSC_shutdown();

      /* set result code */

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	  rcode = acode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * Private Function Definitions
 */


/*
 * open_generic()
 *
 * Parameters:
 *
 *   pds       - data server information
 *   group     - process' group
 *   path      - path name
 *   view      - view
 *   map       - view dependent access mapping
 *   faultmode - access faultmode
 *   oflag     - open flags
 *   mode      - creation permission bits
 *   seg       - creation segment count
 *
 * Generic open function underlying pious_open(), pious_popen(), and
 * pious_spopen() as described in plib.h.
 *
 * Assumes that if 'pds' is NULL then the default data servers are to be
 * utilized.  Otherwise, 'pds' must define a set of data servers in a
 * valid format as described in psc/psc.h
 *
 * Note: if 'pds' defines a set of data servers then, upon successful
 *       completion of open_generic(), 'pds' is stored in the file
 *       table; hence, the calling function must not deallocate 'pds'
 *       except in the case of failure. 
 *
 * Returns: (return codes defined in plib.h for pious_{s}{p}open())
 */

#ifdef __STDC__
static int open_generic(struct PSC_dsinfo *pds,
			char *group,
			char *path,
			int view,
			pious_sizet map,
			int faultmode,
			int oflag,
			pious_modet mode,
			int seg)
#else
static int open_generic(pds,
			group, path, view, map, faultmode, oflag, mode, seg)
     struct PSC_dsinfo *pds;
     char *group;
     char *path;
     int view;
     pious_sizet map;
     int faultmode;
     int oflag;
     pious_modet mode;
     int seg;
#endif
{
  int rcode, psccode;
  int oflagtmp, fd;
  register ftable_entryt *ftable;

  /* validate all parameters */


  oflagtmp = ((oflag & PIOUS_RDONLY) | (oflag & PIOUS_WRONLY) |
	      (oflag & PIOUS_RDWR));

  if (group == NULL || path == NULL)
    rcode = PIOUS_EINVAL;

  else if (*path == '\0')
    rcode = PIOUS_ENOENT;

  else if (view != PIOUS_GLOBAL && view != PIOUS_INDEPENDENT &&
	   view != PIOUS_SEGMENTED)
    rcode = PIOUS_EINVAL;

  else if ((map < 0) ||
	   (map == 0 && (view == PIOUS_GLOBAL || view == PIOUS_INDEPENDENT)))
    rcode = PIOUS_EINVAL;

#ifdef STABLEACCESS
  else if (faultmode != PIOUS_VOLATILE && faultmode != PIOUS_STABLE)
    rcode = PIOUS_EINVAL;
#else
  else if (faultmode != PIOUS_VOLATILE)
    rcode = PIOUS_EINVAL;
#endif

  else if (oflagtmp != PIOUS_RDONLY && oflagtmp != PIOUS_WRONLY &&
	   oflagtmp != PIOUS_RDWR)
    rcode = PIOUS_EINVAL;

  else if ((oflag & PIOUS_RDONLY) && (oflag & PIOUS_TRUNC))
    rcode = PIOUS_EINVAL;

  else if ((oflag & PIOUS_CREAT) && (seg <= 0))
    rcode = PIOUS_EINVAL;

  else
    { /* locate an empty slot in the file table */

      for (fd = 0; fd < PLIB_OPEN_MAX && file_table[fd].valid; fd++);

      if (fd == PLIB_OPEN_MAX)
	{ /* no more empty slots in table */
	  rcode = PIOUS_EINSUF;
	}


      /* open file and set file table entry appropriately */

      else
	{ /* step 1: initialize file table entry so can dealloc if failure */

	  ftable = &file_table[fd];

	  ftable->pds          = NULL;
	  ftable->group        = NULL;
	  ftable->path         = NULL;
	  ftable->trans_state  = NULL;
	  ftable->pfinfo       = NULL;


	  /* step 2: form full path and open underlying parafile */

	  if ((ftable->path = mk_fullpath(path)) == NULL ||

	      (ftable->pfinfo = (struct PSC_pfinfo *)
	       malloc(sizeof(struct PSC_pfinfo))) == NULL)
	    { /* alloc pfinfo last; non-null value indicates parafile open */
	      rcode = PIOUS_EINSUF;
	    }

	  else if ((psccode = PSC_sopen(pds,
					group,
					ftable->path,
					view,
					faultmode,
					oflag,
					(mode & (~creatmask)),
					seg,
					ftable->pfinfo)) != PIOUS_OK)
	    { /* unable to open underlying parafile */

	      switch (psccode)
		{ /* set result code */
		case PIOUS_EINVAL:
		case PIOUS_EACCES:
		case PIOUS_ENAMETOOLONG:
		case PIOUS_ENOTDIR:
		case PIOUS_ENOENT:
		case PIOUS_EPERM:
		case PIOUS_ENOSPC:
		case PIOUS_EINSUF:
		case PIOUS_ETPORT:
		case PIOUS_EFATAL:
		  rcode = psccode;
		  break;
		default:
		  rcode = PIOUS_EUNXP;
		  break;
		}

	      /* dealloc pfinfo and set null to indicate parafile not open */
	      free((char *)ftable->pfinfo);
	      ftable->pfinfo = NULL;
	    }


	  /* step 3: for segmented view, check map arg against segment count */

	  else if (view == PIOUS_SEGMENTED && map >= ftable->pfinfo->seg_cnt)
	    rcode = PIOUS_EINVAL;


	  /* step 4: allocate storage for group name and trans state ptrs */

	  else if ((ftable->group =
		    malloc((unsigned)(strlen(group) + 1))) == NULL ||

		   (ftable->trans_state = (trans_statet **)
		    malloc((unsigned)(ftable->pfinfo->pds_cnt *
				      sizeof(trans_statet *)))) == NULL)
	    rcode = PIOUS_EINSUF;


	  /* step 5: insert PDS trans state records into global list, if
           *         necessary, and set local trans state record ptr vector.
	   */

	  else if (tstate_insert(ftable->pfinfo->pds_id,
				 ftable->trans_state,
				 ftable->pfinfo->pds_cnt) != PIOUS_OK)
	    rcode = PIOUS_EINSUF;


	  /* step 6: set remaining file table entry values */

	  else
	    {
	      strcpy(ftable->group, group);

	      ftable->pds             = pds;
	      ftable->view            = view;
	      ftable->map             = map;
	      ftable->faultmode       = faultmode;
	      ftable->oflag           = oflag;
	      ftable->offset          = 0;
	      ftable->utrans_access   = FALSE;
	      ftable->utrans_offset   = 0;
	      ftable->valid           = TRUE;

	      /* increment file table high-water mark if required */
	      if (fd == file_table_max)
		file_table_max++;

	      /* set result code to file table descriptor */
	      rcode = fd;
	    }


	  /* step 6: cleanup */

	  if (rcode < 0)
	    { /* file open failed; close parafile and deallocate table entry */

	      if (ftable->pfinfo != NULL)
		if (PSC_sclose(pds, group, ftable->path) != PIOUS_OK)
		  /* unable to close file; system state inconsistent */
		  badstate = TRUE;

	      ftable_dealloc(ftable);
	    }
	}
    }

  return rcode;
}




/*
 * access_generic()
 *
 * Parameters:
 *
 *   action - READ or WRITE
 *   fd     - file descriptor
 *   buf    - buffer
 *   nbyte  - byte count
 *   offset - starting offset (>= 0) or FILEPTR
 *   eoff   - effective starting offset
 *
 * Generic access function underlying pious_read(), pious_oread(),
 * pious_pread(), pious_write(), pious_owrite(), and pious_pwrite()
 * as described in plib.h.
 *
 * Assumes that if 'offset' is FILEPTR then the file pointer associated
 * with file 'fd' determines the starting offset of the access; upon
 * successful completion, the file pointer is incremented by the
 * number of bytes actually accessed.  Otherwise, 'offset' is assumed
 * to be the file offset that determines the starting point of the
 * access; in this case the file pointer associated with the file 'fd'
 * is unaffected by the access operation.
 *
 * In all cases, the effective starting offset is returned in 'eoff'.
 *
 *
 * Note: any error implies access/user-transaction aborted or system state
 *       is inconsistent.
 *
 *
 * Returns: (return codes defined in plib.h)
 */

#ifdef __STDC__
static pious_ssizet access_generic(int action,
				   int fd,
				   char *buf,
				   pious_sizet nbyte,
				   pious_offt offset,
				   pious_offt *eoff)
#else
static pious_ssizet access_generic(action, fd, buf, nbyte, offset, eoff)
     int action;
     int fd;
     char *buf;
     pious_sizet nbyte;
     pious_offt offset;
     pious_offt *eoff;
#endif
{
  pious_ssizet rcode, acode, *seg_byte, ebyte;
  pious_sizet nbyte_orig;
  int pds_cnt, seg_cnt;
  int i, seg_access, seg_first;
  int seg_send, seg_recv, sendcnt, recvcnt, server;
  long tmp_eoff;
  register ftable_entryt *ftable;
  pds_transidt transid;

  struct PDS_vbuf_dscrp *vbuf;

  struct filearg {
    pious_offt offset;    /* data segment file offset */
    pious_sizet nbyte;    /* data segment file byte count */
  } *farg;


  vbuf     = NULL;
  farg     = NULL;
  seg_byte = NULL;


  /* validate parameters */

  if (action != READ && action != WRITE)
    rcode = PIOUS_EINVAL;

  else if (fd < 0 || fd >= PLIB_OPEN_MAX || !file_table[fd].valid)
    rcode = PIOUS_EBADF;

  else if (buf == NULL || nbyte < 0 || eoff == NULL)
    rcode = PIOUS_EINVAL;

  else if (offset != FILEPTR && (offset < 0 || offset > PIOUS_OFFT_MAX))
    rcode = PIOUS_EINVAL;

  /* for user-level trans, verify that file and trans faultmode agree */

  else if (utrans_in_progress && file_table[fd].faultmode != utrans_faultmode)
    rcode = PIOUS_EPERM;

  /* verify that file opened for access type 'action' */

  else if ((action == READ  && (file_table[fd].oflag & PIOUS_WRONLY)) ||
	   (action == WRITE && (file_table[fd].oflag & PIOUS_RDONLY)))
    rcode = PIOUS_EBADF;

  /* allocate vector buffer descriptor and file argument structure storage */

  else if ((vbuf = (struct PDS_vbuf_dscrp *)
	    malloc((unsigned)(file_table[fd].pfinfo->seg_cnt *
			      sizeof(struct PDS_vbuf_dscrp)))) == NULL ||

	   (farg = (struct filearg *)
	    malloc((unsigned)(file_table[fd].pfinfo->seg_cnt *
			      sizeof(struct filearg)))) == NULL ||

	   (seg_byte = (pious_ssizet *)
	    malloc((unsigned)(file_table[fd].pfinfo->seg_cnt *
			      sizeof(pious_ssizet)))) == NULL)
    rcode = PIOUS_EINSUF;

  /* perform access operation */

  else
    { /* STEP 1: set transid and PDS transaction op state appropriately */

      ftable  = &file_table[fd];
      acode   = PIOUS_OK;

      seg_cnt = ftable->pfinfo->seg_cnt;  /* dereference parafile seg_cnt */
      pds_cnt = ftable->pfinfo->pds_cnt;  /* dereference parafile pds_cnt */

      if (utrans_in_progress)
	{ /* user-level transaction; use global transaction id but do NOT
           * reset global PDS transaction operation state.
	   */
	  transid = utrans_id;

	  /* mark file as accessed by user-level transaction */
	  ftable->utrans_access = TRUE;
	}

      else
	{ /* independent transaction; FIRST reset global PDS transaction
           * operation state THEN assign a transaction id.
	   */

	  for (i = 0; i < pds_cnt; i++)
	    ftable->trans_state[i]->transsn = 0;

	  pds_trans_prepared = FALSE;

	  if (transid2reuse_valid)
	    {
	      transid             = transid2reuse;
	      transid2reuse_valid = FALSE;
	    }
	  else if (transid_assign(&transid) != PIOUS_OK)
	    acode = PIOUS_EUNXP;
	}


      /* STEP 2: determine the effective file offset of access and adjust
       *         nbyte so as not to overflow the file pointer.
       */

      if (acode == PIOUS_OK)
	{ /* determine effective file offset */

	  if (offset != FILEPTR)
	    { /* access at specified file offset */
	      *eoff = offset;
	    }

	  else if (ftable->view != PIOUS_GLOBAL)
	    { /* access offset determined by local file pointer */
	      *eoff = ftable->offset;
	    }

	  else
	    { /* access offset determined by shared file pointer */

	      /* read shared pointer and increment by 'nbyte' via fetch&add.
	       * if effective number of bytes read/written is fewer, then
	       * will re-write; don't worry about overflowing pointer now.
	       *
	       * doing this saves sending an extra message to update the shared
	       * pointer in the common case that nbyte bytes are read/written.
	       */

	      if ((acode = PDS_fa_sint(ftable->pfinfo->pds_id[0],
				       transid,
				       ftable->trans_state[0]->transsn++,
				       ftable->pfinfo->sptr_fhandle,
				       ftable->pfinfo->sptr_idx,
				       (long)nbyte,
				       &tmp_eoff)) == PIOUS_OK)
		{ /* shared pointer accessed/updated; set offset */
		  *eoff = tmp_eoff;
		}

	      else if (acode == PIOUS_EABORT)
		{ /* transaction aborted at PDS; reset transsn to zero */
		  ftable->trans_state[0]->transsn = 0;
		}
	    }


	  /* adjust nbyte based on effective offset */

	  if (acode == PIOUS_OK)
	    { /* save original value of nbyte to check shared pointer */
	      nbyte_orig = nbyte;

	      /* adjust nbyte to fit maximum value that function can return */
	      nbyte = Min(nbyte, PIOUS_SSIZET_MAX);

	      /* adjust nbyte so as not to overflow file pointer */
	      if (PIOUS_OFFT_MAX - *eoff - nbyte < 0)
		nbyte = PIOUS_OFFT_MAX - *eoff;
	    }
	}


      /* STEP 3: set vector buffer and file access args for each parafile data
       *         segment file to be accessed:
       *
       *   vbuf       - vector buffer arguments
       *   farg       - file access arguments
       *   seg_access - number of parafile segments to access
       *   seg_first  - first parafile segment to access
       */

      if (acode == PIOUS_OK && nbyte > 0)
	{
	  if (ftable->view == PIOUS_SEGMENTED)
	    { /* always accessing a specified parafile data segment file */
	      seg_access = 1;
	      seg_first  = ftable->map;

	      /* initialize vector buffer descriptor */
	      vbuf[seg_first].blksz          = nbyte;
	      vbuf[seg_first].stride         = 1;
	      vbuf[seg_first].firstblk_ptr   = buf;
	      vbuf[seg_first].firstblk_netsz = nbyte;

	      /* initialize file access arguments */
	      farg[seg_first].offset = *eoff;
	      farg[seg_first].nbyte  = nbyte;
	    }

	  else /* view == PIOUS_INDEPENDENT || view == PIOUS_GLOBAL */
	    { /* linear file view; striping across segments round-robin
	       * from low to high.
	       */
	      pious_sizet su_sz, sz_first, sz_last, su_access;
	      pious_offt su_off_first, foff_base;
	      int seg_of_first_su, seg_of_last_su, segnmbr;
	      int su_seg_touch;
	      char *bufptr;

	      /* set striping unit (SU) size */
	      su_sz = ftable->map;

	      /* calculate first SU offset of access and effective first/last
	       * SU size.
	       */
	      su_off_first = *eoff % su_sz;
	      sz_first     = Min(nbyte, su_sz - su_off_first);

	      if (nbyte - sz_first == 0)
		{ /* first and last SU are same */
		  sz_last = sz_first;
		}
	      else
		{ /* access spans multiple SU */
		  sz_last = (nbyte - sz_first) % su_sz;

		  if (sz_last == 0)
		    sz_last = su_sz;
		}

	      /* calculate total number of SU and file segments accessed */
	      if (nbyte - sz_first == 0)
		su_access = 1;
	      else
		su_access = 2 + ((nbyte - sz_first - 1) / su_sz);

	      seg_access = Min(seg_cnt, su_access);

	      /* determine data file segment with first and last SU accessed */
	      seg_of_first_su = (*eoff / su_sz) % seg_cnt;
	      seg_of_last_su  = ((seg_of_first_su + su_access - 1) % seg_cnt);

	      seg_first       = seg_of_first_su;

	      /* init vbuf dscrp and fargs for each segment file to access */

	      segnmbr   = seg_of_first_su;
	      foff_base = (*eoff / (su_sz * seg_cnt)) * su_sz;
	      bufptr    = buf;

	      for (i = 0; i < seg_access; i++)
		{ /* set data segment file vector buffer parameters */

		  vbuf[segnmbr].blksz        = su_sz;
		  vbuf[segnmbr].stride       = seg_cnt;
		  vbuf[segnmbr].firstblk_ptr = bufptr;

		  if (i == 0) /* segnmbr == seg_of_first_su */
		    vbuf[segnmbr].firstblk_netsz = su_sz - su_off_first;
		  else
		    vbuf[segnmbr].firstblk_netsz = su_sz;

		  bufptr += vbuf[segnmbr].firstblk_netsz;

		  /* calculate the number of SU accessed from segment file */

		  su_seg_touch = 1 + ((su_access - i - 1) / seg_cnt);

		  /* set data segment file access parameters */

		  if (i == 0) /* segnmbr == seg_of_first_su */
		    farg[segnmbr].offset = foff_base + su_off_first;
		  else
		    farg[segnmbr].offset = foff_base;

		  if (su_seg_touch == 1)
		    { /* exactly 1 SU accessed in data segment file */
		      if (segnmbr == seg_of_first_su)
			farg[segnmbr].nbyte = sz_first;

		      else if (segnmbr == seg_of_last_su)
			farg[segnmbr].nbyte = sz_last;

		      else
			farg[segnmbr].nbyte = su_sz;
		    }

		  else
		    { /* 2 or more SU accessed in data segment file */
		      if (segnmbr == seg_of_first_su)
			farg[segnmbr].nbyte = sz_first;
		      else
			farg[segnmbr].nbyte = su_sz;

		      if (segnmbr == seg_of_last_su)
			farg[segnmbr].nbyte += sz_last;
		      else
			farg[segnmbr].nbyte += su_sz;

		      if (su_seg_touch > 2)
			farg[segnmbr].nbyte += (su_seg_touch - 2) * su_sz;
		    }


		  /* move on to next data segment file */
		  segnmbr = (segnmbr + 1) % seg_cnt;

		  if (segnmbr == 0)
		    /* SU wraparound; adjust file offset base */
		    foff_base += su_sz;
		}
	    }
	}


      /* STEP 4: pipeline parafile data segment accesses across servers */

      if (acode == PIOUS_OK && nbyte > 0)
	{
	  /* if the number of parafile segments is not a multiple of the
	   * number of data servers on which the file is declustered and
	   * the set of segments accessed wraps around to include segment
           * zero, then pipelining must be performed in two phases to prevent
	   * initiating more than one outstanding transaction operation at a
	   * given data server (see pds/pds.h).
	   */

	  int phase, phase_cnt, phase_access, phase_first, stop_early;
	  pious_sizet stop_sz;

	  if (seg_cnt % pds_cnt == 0 || seg_first + seg_access <= seg_cnt)
	    phase_cnt = 1;
	  else
	    phase_cnt = 2;


	  /* set the number of bytes accessed at each segment to zero (0)
	   * in case segment access stopped early.
	   */

	  for (seg_send = seg_first, i = 0; i < seg_access; i++)
	    {
	      seg_byte[seg_send] = 0;
	      seg_send           = (seg_send + 1) % seg_cnt;
	    }

	  stop_early = FALSE;


	  /* perform access operations */

	  for (phase = 0;
	       phase < phase_cnt && acode == PIOUS_OK && !stop_early;
	       phase++)
	    {
	      /* set first segment and segment count according to phase */

	      if (phase_cnt == 1)
		{ /* all segment accesses performed in a single phase */
		  phase_first  = seg_first;
		  phase_access = seg_access;
		}

	      else
		{ /* segment accesses in two phases */

		  if (phase == 0)
		    { /* first phase parameters - from first segment to last */
		      phase_first  = seg_first;
		      phase_access = seg_cnt - seg_first;
		    }

		  else
		    { /* second phase parameters - wrap-around */
		      phase_first  = 0;
		      phase_access = seg_access - (seg_cnt - seg_first);
		    }
		}

	      /* pipeline segment access requests for the current phase */


	      /* ---- fill pipe ---- */

	      seg_send = seg_recv = phase_first;
	      sendcnt  = recvcnt  = 0;

	      for (i = 0;
		   i < Min(pds_cnt, phase_access) && acode == PIOUS_OK;
		   i++)
		{ /* send read/write access request */

		  server = seg_send % pds_cnt;

		  if (action == READ)
		    acode =
		      PDS_read_send(ftable->pfinfo->pds_id[server],
				    transid,
				    ftable->trans_state[server]->transsn++,
				    ftable->pfinfo->seg_fhandle[seg_send],
				    farg[seg_send].offset,
				    farg[seg_send].nbyte,
				    PDS_READLK);
		  else
		    acode =
		      PDS_write_send(ftable->pfinfo->pds_id[server],
				     transid,
				     ftable->trans_state[server]->transsn++,
				     ftable->pfinfo->seg_fhandle[seg_send],
				     farg[seg_send].offset,
				     farg[seg_send].nbyte,
				     &vbuf[seg_send]);

		  if (acode == PIOUS_OK)
		    { /* request sent successfully */
		      sendcnt++;
		      seg_send = (seg_send + 1) % seg_cnt;
		    }
		}


	      /* ---- pipe steady state ---- */


	      for (i = 0;
		   i < phase_access - Min(pds_cnt, phase_access) &&
		   acode == PIOUS_OK && !stop_early;
		   i++)
		{ /* alternate receiving/sending read/write access results */

		  server  = seg_recv % pds_cnt;
		  stop_sz = vbuf[seg_recv].firstblk_netsz;

		  if (action == READ)
		    acode = seg_byte[seg_recv] =
		      PDS_read_recv(ftable->pfinfo->pds_id[server],
				    transid,
				    ftable->trans_state[server]->transsn - 1,
				    &vbuf[seg_recv]);
		  else
		    acode = seg_byte[seg_recv] =
		      PDS_write_recv(ftable->pfinfo->pds_id[server],
				     transid,
				     ftable->trans_state[server]->transsn - 1);

		  recvcnt++;
		  seg_recv = (seg_recv + 1) % seg_cnt;

		  if (acode < 0)
		    { /* access resulted in an error */

		      if (acode == PIOUS_EABORT)
			{ /* PDS aborted; reset transsn to zero */
			  ftable->trans_state[server]->transsn = 0;
			}
		    }

		  else if (acode < stop_sz)
		    { /* no further data access required */
		      acode      = PIOUS_OK;
		      stop_early = TRUE;
		    }

		  else
		    { /* send next data access request */

		      server = seg_send % pds_cnt;

		      if (action == READ)
			acode =
			PDS_read_send(ftable->pfinfo->pds_id[server],
				      transid,
				      ftable->trans_state[server]->transsn++,
				      ftable->pfinfo->seg_fhandle[seg_send],
				      farg[seg_send].offset,
				      farg[seg_send].nbyte,
				      PDS_READLK);
		      else
			acode =
			PDS_write_send(ftable->pfinfo->pds_id[server],
				       transid,
				       ftable->trans_state[server]->transsn++,
				       ftable->pfinfo->seg_fhandle[seg_send],
				       farg[seg_send].offset,
				       farg[seg_send].nbyte,
				       &vbuf[seg_send]);

		      if (acode == PIOUS_OK)
			{ /* request sent successfully */
			  sendcnt++;
			  seg_send = (seg_send + 1) % seg_cnt;
			}
		    }
		}


	      /* ---- drain pipe ---- */


	      for (i = 0;
		   i < Min(pds_cnt, phase_access) &&
		   acode == PIOUS_OK && !stop_early;
		   i++)
		{ /* recieve read/write request replys */

		  server = seg_recv % pds_cnt;

		  if (action == READ)
		    acode = seg_byte[seg_recv] =
		      PDS_read_recv(ftable->pfinfo->pds_id[server],
				    transid,
				    ftable->trans_state[server]->transsn - 1,
				    &vbuf[seg_recv]);
		  else
		    acode = seg_byte[seg_recv] =
		      PDS_write_recv(ftable->pfinfo->pds_id[server],
				     transid,
				     ftable->trans_state[server]->transsn - 1);

		  recvcnt++;
		  seg_recv = (seg_recv + 1) % seg_cnt;

		  if (acode < 0)
		    { /* access resulted in an error */

		      if (acode == PIOUS_EABORT)
			{ /* PDS aborted; reset transsn to zero */
			  ftable->trans_state[server]->transsn = 0;
			}
		    }

		  else
		    { /* access performed without error */
		      acode = PIOUS_OK;
		    }
		}


	      /* consume outstanding replies that may result if:
	       *
               *   1) access stopped early, or
	       *   2) an error occured
	       *
	       * if access stopped early then set acode to first error
	       * encountered in remaining results received, if any.
	       * Otherwise, leave acode unchanged.
	       */

	      while (recvcnt < sendcnt)
		{
		  server = seg_recv % pds_cnt;

		  if (action == READ)
		    seg_byte[seg_recv] =
		      PDS_read_recv(ftable->pfinfo->pds_id[server],
				    transid,
				    ftable->trans_state[server]->transsn - 1,
				    &vbuf[seg_recv]);
		  else
		    seg_byte[seg_recv] =
		      PDS_write_recv(ftable->pfinfo->pds_id[server],
				     transid,
				     ftable->trans_state[server]->transsn - 1);

		  if (seg_byte[seg_recv] < 0)
		    { /* access resulted in an error */

		      if (seg_byte[seg_recv] == PIOUS_EABORT)
			{ /* PDS aborted; reset transsn to zero */
			  ftable->trans_state[server]->transsn = 0;
			}

		      if (acode == PIOUS_OK)
			{ /* set acode to first error encountered */
			  acode = seg_byte[seg_recv];
			}
		    }

		  recvcnt++;
		  seg_recv = (seg_recv + 1) % seg_cnt;
		}
	    }
	}


      /* STEP 5:  compute the *effective* number of bytes accessed and
       *          re-write shared file pointer if required.
       */

      if (acode == PIOUS_OK)
	{ /* compute effective number of bytes accessed */

	  if (nbyte == 0)
	    { /* no data access has taken place */

	      ebyte = 0;
	    }

	  else if (ftable->view == PIOUS_SEGMENTED)
	    { /* always accessing a single parafile data segment file */

	      ebyte = seg_byte[seg_first];
	    }

	  else /* view == PIOUS_INDEPENDENT || view == PIOUS_GLOBAL */
	    { /* linear file view; striping across segments round-robin
	       * from low to high.
	       */
	      pious_sizet su_sz, full_stripe, rembyte, part;
	      pious_offt su_off_first;
	      int segnmbr, done;

	      /* set striping unit (SU) size */
	      su_sz = ftable->map;

	      /* calculate first SU offset of access */
	      su_off_first = *eoff % su_sz;

	      /* compute number of full stripes accessed */

	      if (seg_access < seg_cnt)
		{ /* not all data segment files accessed */
		  full_stripe = 0;
		}

	      else
		{ /* calculate for special case data segment with first SU */
		  full_stripe =
		    (seg_byte[seg_first] + su_off_first) / su_sz;

		  /* take min value across all data segments */
		  for (i = 0; i < seg_cnt; i++)
		    if (i != seg_first)
		      full_stripe = Min(seg_byte[i] / su_sz, full_stripe);
		}

	      /* compute remaining bytes accessed after last full stripe */

	      segnmbr = seg_first;
	      done    = FALSE;

	      for (i = 0; i < seg_access && !done; i++)
		{
		  if (i == 0) /* segnmbr == seg_first */
		    { /* special case data segment with first SU */
		      rembyte = part =
			Min(seg_byte[segnmbr] + su_off_first -
			    (full_stripe * su_sz), su_sz);
		    }

		  else
		    { /* all data segments accessed, in order, except first */
		      rembyte += part =
			Min(seg_byte[segnmbr] - (full_stripe * su_sz), su_sz);
		    }

		  if (part < su_sz)
		    done = TRUE;
		  else
		    segnmbr = (segnmbr + 1) % seg_cnt;
		}

	      /* compute total effective bytes accessed */

	      ebyte = (full_stripe * seg_cnt * su_sz) + rembyte - su_off_first;
	    }


	  /* rewrite shared file pointer if previously incremented by
	   * incorrect value.
	   */

	  if (offset == FILEPTR && ftable->view == PIOUS_GLOBAL &&
	      ebyte  != nbyte_orig)
	    { /* shared global pointer must be rewritten */

	      tmp_eoff = *eoff + ebyte;

	      acode = PDS_write_sint(ftable->pfinfo->pds_id[0],
				     transid,
				     ftable->trans_state[0]->transsn++,
				     ftable->pfinfo->sptr_fhandle,
				     ftable->pfinfo->sptr_idx,
				     1,
				     &tmp_eoff);

	      if (acode == 1)
		{ /* shared global pointer successfully rewritten */
		  acode = PIOUS_OK;
		}

	      else if (acode >= 0)
		{ /* no error, but shared pointer could not be rewritten */
		  acode = PIOUS_EUNXP;
		}

	      else if (acode == PIOUS_EABORT)
		{ /* transaction aborted at PDS; reset transsn to zero */
		  ftable->trans_state[0]->transsn = 0;
		}
	    }
	}


      /* STEP 6: prepare/commit/abort independent transaction as required */

      if (!utrans_in_progress)
	{ /* access is not part of a larger user-level transaction */


	  /* prepare successful access if it is to be stable */

	  if (acode == PIOUS_OK && ftable->faultmode == PIOUS_STABLE)
	    {
	      acode = prepare_all(transid, ftable->trans_state, pds_cnt);
	    }


	  /* commit successful access or abort failed access */

	  if (acode == PIOUS_OK)
	    { /* commit access */
	      acode = commit_all(transid, ftable->trans_state, pds_cnt);

	      if (acode == PIOUS_ENOTLOG)
		/* do not care if commit action not logged for stable trans */
		acode = PIOUS_OK;

	      if (acode != PIOUS_OK)
		/* not all PDS committed; inconsistent file state */
		badstate = TRUE;
	    }

	  else
	    { /* abort access, retaining original error code */
	      i = abort_all(transid, ftable->trans_state, pds_cnt);

	      if (i != PIOUS_OK && i != PIOUS_ENOTLOG)
		/* can not abort; PIOUS state potentially inconsistent */
		badstate = TRUE;
	    }
	}


      /* STEP 7: set final result code for access */

      switch(acode)
	{
	case PIOUS_OK:
	  /* no errors, return effective number of bytes accessed */

	  rcode = ebyte;

	  /* update local file pointer if required */

	  if (offset == FILEPTR && ftable->view != PIOUS_GLOBAL)
	    { /* note that shared pointer updated as part of transaction */
	      ftable->offset += ebyte;
	    }
	  break;

	case PIOUS_EABORT:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  /* a "standard" error has occured */
	  rcode = acode;
	  break;

	case PIOUS_EBADF:
	case PIOUS_ESRCDEST:
	case PIOUS_EPROTO:
	  /* should never occur; inconsistent system state (bug in PIOUS) */
	  rcode    = PIOUS_EUNXP;
	  badstate = TRUE;
	  break;

	case PIOUS_EACCES:
	case PIOUS_EINVAL:
	default:
	  /* file permission changed outside PIOUS or other unexpected error */
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* mark transid as "to be reused" if PDS aborted the transaction */

      if (acode == PIOUS_EABORT)
	{
	  transid2reuse       = transid;
	  transid2reuse_valid = TRUE;
	}
    }


  /* if access resulted in error, abort user-level transaction if applicable */

  if (rcode < 0 && utrans_in_progress)
    { /* abort user-level transaction, retaining access error code */
      pious_tabort();
    }


  /* deallocate extraneous storage */

  if (vbuf != NULL)
    free((char *)vbuf);

  if (farg != NULL)
    free((char *)farg);

  if (seg_byte != NULL)
    free((char *)seg_byte);

  /* return result */

  return rcode;
}




/*
 * lseek_proxy()
 *
 * Parameters:
 *
 *   fd     - file descriptor
 *   offset - file offset
 *   whence - PIOUS_SEEK_SET or PIOUS_SEEK_CUR
 *
 * lseek_proxy() implements pious_lseek() as described in plib.h.  It exists
 * only to make it easier to implement retrying seeks that abort.
 *
 * Note: any error implies seek/user-transaction aborted or system state
 *       is inconsistent.
 *
 * Returns: (return codes defined in plib.h)
 */

#ifdef __STDC__
static pious_offt lseek_proxy(int fd,
			      pious_offt offset,
			      int whence)
#else
static pious_offt lseek_proxy(fd, offset, whence)
     int fd;
     pious_offt offset;
     int whence;
#endif
{
  pious_offt rcode;
  pds_transidt transid;
  ftable_entryt *ftable;
  long tmp_cur;
  int i, acode;


  /* validate 'fd' argument */

  if (fd < 0 || fd >= PLIB_OPEN_MAX || !file_table[fd].valid)
    rcode = PIOUS_EBADF;

  /* validate 'whence' argument */

  else if (whence != PIOUS_SEEK_SET && whence != PIOUS_SEEK_CUR)
    rcode = PIOUS_EINVAL;

  /* validate 'offset' argument */

  else if (offset < PIOUS_OFFT_MIN || offset > PIOUS_OFFT_MAX ||
	   (whence == PIOUS_SEEK_SET && offset < 0))
    rcode = PIOUS_EINVAL;


  /* case: local file pointer to be updated */

  else if (file_table[fd].view != PIOUS_GLOBAL)
    {
      if (whence == PIOUS_SEEK_SET)
	{ /* set local file pointer to specifed offset */
	  rcode = file_table[fd].offset = offset;
	}

      else
	{ /* set local file pointer to current value plus specified offset */

	  if ((offset >= 0 &&
	       PIOUS_OFFT_MAX - offset - file_table[fd].offset >= 0) ||

	      (offset <  0 &&
	       file_table[fd].offset + offset >= 0))

	    rcode = file_table[fd].offset += offset;
	  else
	    rcode = PIOUS_EINVAL;
	}
    }


  /* case: global file pointer to be updated */

  else
    { /* assign transid and set PDS transaction op state appropriately */

      ftable = &file_table[fd];
      acode  = PIOUS_OK;

      if (utrans_in_progress)
	{ /* user-level transaction; use global transaction id but do NOT
           * reset global PDS transaction operation state.
	   */
	  transid = utrans_id;

	  /* mark file as accessed by user-level transaction */
	  ftable->utrans_access = TRUE;
	}

      else
	{ /* independent transaction; FIRST reset global PDS transaction
           * operation state THEN assign a transaction id.
	   */

	  for (i = 0; i < ftable->pfinfo->pds_cnt; i++)
	    ftable->trans_state[i]->transsn = 0;

	  pds_trans_prepared = FALSE;

	  if (transid2reuse_valid)
	    {
	      transid             = transid2reuse;
	      transid2reuse_valid = FALSE;
	    }
	  else if (transid_assign(&transid) != PIOUS_OK)
	    acode = PIOUS_EUNXP;
	}


      /* access/update global file pointer */

      if (acode == PIOUS_OK)
	{
	  if (whence == PIOUS_SEEK_SET)
	    { /* write specified offset to shared file pointer */

	      tmp_cur = offset;

	      acode = PDS_write_sint(ftable->pfinfo->pds_id[0],
				     transid,
				     ftable->trans_state[0]->transsn++,
				     ftable->pfinfo->sptr_fhandle,
				     ftable->pfinfo->sptr_idx,
				     1,
				     &tmp_cur);

	      if (acode == 1)
		{ /* shared global pointer successfully written */
		  acode = PIOUS_OK;
		}

	      else if (acode >= 0)
		{ /* no error, but shared pointer could not be written */
		  acode = PIOUS_EUNXP;
		}

	      else if (acode == PIOUS_EABORT)
		{ /* transaction aborted at PDS; reset transsn to zero */
		  ftable->trans_state[0]->transsn = 0;
		}
	    }

	  else
	    { /* read shared pointer value and add 'offset' via fetch&add.
	       * will abort transaction if resultant offset is a bad value.
	       */

	      if ((acode = PDS_fa_sint(ftable->pfinfo->pds_id[0],
				       transid,
				       ftable->trans_state[0]->transsn++,
				       ftable->pfinfo->sptr_fhandle,
				       ftable->pfinfo->sptr_idx,
				       (long)offset,
				       &tmp_cur)) == PIOUS_OK)
		{ /* shared pointer accessed/updated; check resultant value */

		  if ((offset >= 0 &&
		       PIOUS_OFFT_MAX - offset - tmp_cur >= 0) ||

		      (offset <  0 && tmp_cur + offset >= 0))
		    { /* updated shared pointer is a valid value */
		      tmp_cur += offset;
		    }

		  else
		    { /* updated pointer value invalid */
		      tmp_cur  = PIOUS_EINVAL;
		    }
		}

	      else if (acode == PIOUS_EABORT)
		{ /* transaction aborted at PDS; reset transsn to zero */
		  ftable->trans_state[0]->transsn = 0;
		}
	    }
	}


      /* commit/abort independent transaction as required */

      if (!utrans_in_progress)
	{ /* access is not part of a larger user-level transaction.
	   *
	   * note: prepare not required for file in stable mode since the
           *       file pointer value is effectively incorporated into each
	   *       access.
	   */

	  /* commit successful access/update or abort failed access/update */

	  if (acode == PIOUS_OK && tmp_cur != PIOUS_EINVAL)
	    { /* commit access */
	      if ((acode = commit_all(transid,
				      ftable->trans_state,
				      ftable->pfinfo->pds_cnt)) != PIOUS_OK)
		/* not all PDS committed; inconsistent file state */
		badstate = TRUE;
	    }

	  else
	    { /* abort; retain original error code or bad offset flag */
	      if (abort_all(transid,
			    ftable->trans_state,
			    ftable->pfinfo->pds_cnt) != PIOUS_OK)
		/* can not abort; PIOUS state potentially inconsistent */
		badstate = TRUE;
	    }
	}


      /* set final result code for global file pointer update */

      switch(acode)
	{
	case PIOUS_OK:
	  /* no access errors, return resulting offset or PIOUS_EINVAL */

	  if (tmp_cur != PIOUS_EINVAL)
	    ftable->offset = tmp_cur;

	  rcode = tmp_cur;
	  break;

	case PIOUS_EABORT:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  /* a "standard" error has occured */
	  rcode = acode;
	  break;

	case PIOUS_EBADF:
	case PIOUS_ESRCDEST:
	case PIOUS_EPROTO:
	  /* should never occur; inconsistent system state (bug in PIOUS) */
	  rcode    = PIOUS_EUNXP;
	  badstate = TRUE;
	  break;

	case PIOUS_EACCES:
	case PIOUS_EINVAL:
	default:
	  /* file permission changed outside PIOUS or other unexpected error */
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* mark transid as "to be reused" if PDS aborted the transaction */

      if (acode == PIOUS_EABORT)
	{
	  transid2reuse       = transid;
	  transid2reuse_valid = TRUE;
	}
    }


  /* if access resulted in error, abort user-level transaction if applicable */

  if (rcode < 0 && utrans_in_progress)
    { /* abort user-level transaction, retaining access error code */
      pious_tabort();
    }

  return rcode;
}




/*
 * prepare_all()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   tsv     - PDS transaction operation state record pointer vector
 *   cnt     - number of vector elements
 *
 * Send a prepare request to each PDS in 'tsv' that participated in
 * transaction 'transid', i.e. for which the transaction sequence number
 * is greater than zero (0), and set the pds_trans_prepared flag to TRUE.
 *
 * If a PDS votes to abort, or indicates read-only participation, its record
 * in 'tsv' is marked as not requiring further action; i.e. the transaction
 * sequence number is reset to zero (0).
 *
 * Since a single error code is returned from multiple operations, the value
 * returned is the first error encountered.  Note that PIOUS_EABORT is NOT
 * considered an error since abort is a normal outcome of prepare.
 *
 * Returns:
 *
 *   PIOUS_OK (0)   - transaction prepared to commit at all PDS
 *   <  0           - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EABORT   - vote to abort by one or more PDS
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 */

#ifdef __STDC__
static int prepare_all(pds_transidt transid,
		       trans_statet **tsv,
		       int cnt)
#else
static int prepare_all(transid, tsv, cnt)
     pds_transidt transid;
     trans_statet **tsv;
     int cnt;
#endif
{
  int rcode, pcode, i;

  /* send prepare request to all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0)
      tsv[i]->rcode = PDS_prepare_send(tsv[i]->pdsid,
				       transid,
				       tsv[i]->transsn++);

  /* receive prepare reply from all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0 && tsv[i]->rcode == PIOUS_OK)
      tsv[i]->rcode = PDS_prepare_recv(tsv[i]->pdsid,
				       transid,
				       tsv[i]->transsn - 1);

  /* set result code to first error other than PIOUS_EABORT, if extant.
   * indicate if PDS expects no further operations on transid.
   */

  pcode = PIOUS_OK;

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0)
      { /* PIOUS_EABORT is the only "acceptable" error code */

	if ((pcode == PIOUS_OK || pcode == PIOUS_EABORT) && tsv[i]->rcode < 0)
	  pcode = tsv[i]->rcode;

	/* no further action if PDS votes abort or indicates read-only */

	if (tsv[i]->rcode == PIOUS_READONLY || tsv[i]->rcode == PIOUS_EABORT)
	  tsv[i]->transsn = 0;
      }

  switch(pcode)
    {
    case PIOUS_OK:
    case PIOUS_EABORT:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
    case PIOUS_EPROTO:
    case PIOUS_EFATAL:
      rcode = pcode;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  if (rcode == PIOUS_OK)
    /* flag that transaction prepared at all PDS */
    pds_trans_prepared = TRUE;

  return rcode;
}




/*
 * commit_all()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   tsv     - PDS transaction operation state record pointer vector
 *   cnt     - number of vector elements
 *
 * Send a commit request to each PDS in 'tsv' that participated in
 * transaction 'transid' and that still requires a commit; i.e. for which
 * the transaction sequence number is greater than zero (0).
 *
 * Since a single error code is returned from multiple operations, the value
 * returned is the first error encountered.  Note that PIOUS_ENOTLOG is NOT
 * considered an error since it is a well defined outcome of commit.
 *
 * Returns:
 *
 *   PIOUS_OK (0)   - transaction committed at all PDS
 *   <  0           - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ENOTLOG  - commit action not logged at one or more PDS
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EPROTO   - 2PC or transaction operation protocol error
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 */

#ifdef __STDC__
static int commit_all(pds_transidt transid,
		      trans_statet **tsv,
		      int cnt)
#else
static int commit_all(transid, tsv, cnt)
     pds_transidt transid;
     trans_statet **tsv;
     int cnt;
#endif
{
  int rcode, ccode, i;

  /* send commit request to all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0)
      tsv[i]->rcode = PDS_commit_send(tsv[i]->pdsid,
				      transid,
				      tsv[i]->transsn);

#ifdef VTCOMMITNOACK
  if (pds_trans_prepared)
    { /* receive commit ack for stable (ie prepared) transactions only */
      for (i = 0; i < cnt; i++)
	if (tsv[i]->transsn > 0 && tsv[i]->rcode == PIOUS_OK)
	  tsv[i]->rcode = PDS_commit_recv(tsv[i]->pdsid,
					  transid,
					  tsv[i]->transsn);
    }
#else
  /* receive commit ack for both stable and volatile transactions */
  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0 && tsv[i]->rcode == PIOUS_OK)
      tsv[i]->rcode = PDS_commit_recv(tsv[i]->pdsid,
				      transid,
				      tsv[i]->transsn);
#endif

  /* set result code to first error other than PIOUS_ENOTLOG, if extant */

  ccode = PIOUS_OK;

  for (i = 0; i < cnt && (ccode == PIOUS_OK || ccode == PIOUS_ENOTLOG); i++)
    /* PIOUS_ENOTLOG is the only "acceptable" error code */
    if (tsv[i]->transsn > 0 && tsv[i]->rcode < 0)
      ccode = tsv[i]->rcode;

  switch(ccode)
    {
    case PIOUS_OK:
    case PIOUS_ENOTLOG:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
    case PIOUS_EPROTO:
    case PIOUS_EFATAL:
      rcode = ccode;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * abort_all()
 *
 * Parameters:
 *
 *   transid - transaction id
 *   tsv     - PDS transaction operation state record pointer vector
 *   cnt     - number of vector elements
 *
 * Send an abort request to each PDS in 'tsv' that participated in
 * transaction 'transid' and that still requires an abort; i.e. for which
 * the transaction sequence number is greater than zero (0).
 *
 * Since a single error code is returned from multiple operations, the value
 * returned is the first error encountered.  Note that PIOUS_ENOTLOG is NOT
 * considered an error since it is a well defined outcome of abort.
 *
 * Returns:
 *
 *   PIOUS_OK (0)   - transaction aborted at all PDS
 *   <  0           - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ENOTLOG  - abort action not logged at one or more PDS
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 *       PIOUS_EFATAL   - fatal error; check PDS error log
 */

#ifdef __STDC__
static int abort_all(pds_transidt transid,
		     trans_statet **tsv,
		     int cnt)
#else
static int abort_all(transid, tsv, cnt)
     pds_transidt transid;
     trans_statet **tsv;
     int cnt;
#endif
{
  int rcode, acode, i;

  /* send abort request to all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0)
      tsv[i]->rcode = PDS_abort_send(tsv[i]->pdsid, transid);

  /* receive abort reply from all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->transsn > 0 && tsv[i]->rcode == PIOUS_OK)
      tsv[i]->rcode = PDS_abort_recv(tsv[i]->pdsid, transid);

  /* set result code to first error other than PIOUS_ENOTLOG, if extant */

  acode = PIOUS_OK;

  for (i = 0; i < cnt && (acode == PIOUS_OK || acode == PIOUS_ENOTLOG); i++)
    /* PIOUS_ENOTLOG is the only "acceptable" error code */
    if (tsv[i]->transsn > 0 && tsv[i]->rcode < 0)
      acode = tsv[i]->rcode;

  switch(acode)
    {
    case PIOUS_OK:
    case PIOUS_ENOTLOG:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
    case PIOUS_EFATAL:
      rcode = acode;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * ping_all()
 *
 * Parameters:
 *
 *   tsv     - PDS transaction operation state record pointer vector
 *   cnt     - number of vector elements
 *
 * Send an ping request to each PDS in 'tsv' vector.
 *
 * Since a single error code is returned from multiple operations, the value
 * returned is the first error encountered.
 *
 * Returns:
 *
 *   PIOUS_OK (0)   - all PDS responded to ping
 *   <  0           - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST - invalid 'pdsid' argument
 *       PIOUS_EINSUF   - insufficient system resources to complete
 *       PIOUS_ETPORT   - error condition in underlying transport system
 *       PIOUS_EUNXP    - unexpected error condition encountered
 */

#ifdef __STDC__
static int ping_all(trans_statet **tsv,
		    int cnt)
#else
static int ping_all(tsv, cnt)
     trans_statet **tsv;
     int cnt;
#endif
{
  int i, rcode, acode, cntrlid;

  /* send ping request to all PDS */

  cntrlid = CntrlIdNext;

  for (i = 0; i < cnt; i++)
    tsv[i]->rcode = PDS_ping_send(tsv[i]->pdsid, cntrlid);

  /* receive ping reply from all PDS */

  for (i = 0; i < cnt; i++)
    if (tsv[i]->rcode == PIOUS_OK)
      tsv[i]->rcode = PDS_ping_recv(tsv[i]->pdsid, cntrlid);

  /* set result code to first error encountered, if extant */

  acode = PIOUS_OK;

  for (i = 0; i < cnt && acode == PIOUS_OK; i++)
    if (tsv[i]->rcode < 0)
      acode = tsv[i]->rcode;

  switch(acode)
    {
    case PIOUS_OK:
    case PIOUS_ESRCDEST:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
      rcode = acode;
      break;
    default:
      rcode = PIOUS_EUNXP;
      break;
    }

  return rcode;
}




/*
 * dsv2dsinfo()
 *
 * Parameters:
 *
 *   dsv    - data server information vector
 *   dsvcnt - dsv element count
 *   dsinfo - data server information record in PSC required format
 *
 * Allocate a data server information record 'dsinfo', in PSC required
 * format, and copy data from the 'dsvcnt' element data server information
 * vector 'dsv'.
 *
 * Entries in 'dsv' are checked to insure that no duplicates are specified,
 * that host names are not null, and that all host search root and log
 * directory paths are non-empty with '/' as the first character.  Entries are
 * copied into 'dsinfo' in lexicographic order by host name, as required.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - 'dsinfo' successfully allocated and defined
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - invalid argument
 *       PIOUS_EINSUF - insufficient system resources to complete
 */

#ifdef __STDC__
static int dsv2dsinfo(struct pious_dsvec *dsv,
		      int dsvcnt,
		      struct PSC_dsinfo **dsinfo)
#else
static int dsv2dsinfo(dsv, dsvcnt, dsinfo)
	  struct pious_dsvec *dsv;
	  int dsvcnt;
	  struct PSC_dsinfo **dsinfo;
#endif
{
  register struct pious_dsvec **dsvsort, *key;
  struct PSC_dsinfo *pds;
  int i, j, keycmp, failed, rcode;

  dsvsort = NULL;
  pds     = NULL;

  /* validate 'dsv', 'dsvcnt', and 'dsinfo' arguments */

  if (dsv == NULL || dsvcnt <= 0 || dsinfo == NULL)
    rcode = PIOUS_EINVAL;

  /* validate all host names, search root paths, and log directory paths */

  else
    {
      rcode = PIOUS_OK;

      for (i = 0; i < dsvcnt && rcode == PIOUS_OK; i++)
	if (*dsv[i].hname == '\0' ||
	    *dsv[i].spath != '/'  || *dsv[i].lpath != '/')
	  rcode = PIOUS_EINVAL;
    }


  /* sort 'dsv' entries by host, checking for duplicates */

  if (rcode == PIOUS_OK)
    { /* sort 'dsv' via a permutation vector */

      if ((dsvsort = (struct pious_dsvec **)
	   malloc((unsigned)(dsvcnt * sizeof(struct pious_dsvec *)))) == NULL)
	{ /* unable to allocate permutation vector */
	  rcode = PIOUS_EINSUF;
	}

      else
	{ /* perform a simple insertion sort; expect small entry count */

	  /* note: due to the nature of insertion sort, can assign initial
	   * values to the permutation vector as sort progresses!
	   */

	  dsvsort[0] = &dsv[0];

	  for (j = 1; j < dsvcnt && rcode == PIOUS_OK; j++)
	    { /* assign next (key) value to permutation vector */
	      key = dsvsort[j] = &dsv[j];

	      /* insert key into previously sorted sequence */
	      for (i = j - 1;
		   i >= 0 &&
		   (keycmp = strcmp(dsvsort[i]->hname, key->hname)) > 0;
		   i--)
		dsvsort[i + 1] = dsvsort[i];

	      dsvsort[i + 1] = key;

	      if (i >= 0 && keycmp == 0)
		/* duplicate hname */
		rcode = PIOUS_EINVAL;
	    }
	}
    }


  /* allocate storage and copy sorted data server information */

  if (rcode == PIOUS_OK)
    {
      if ((pds = (struct PSC_dsinfo *)
	   malloc(sizeof(struct PSC_dsinfo))) == NULL)
	{
	  rcode = PIOUS_EINSUF;
	}

      else
	{ /* allocate hname, spath, lpath storage */
	  pds->hname = pds->spath = pds->lpath = NULL;

	  failed = FALSE;

	  if ((pds->hname = (char **)
	       malloc((unsigned)(dsvcnt * sizeof(char *)))) == NULL ||

	      (pds->spath = (char **)
	       malloc((unsigned)(dsvcnt * sizeof(char *)))) == NULL ||

	      (pds->lpath = (char **)
	       malloc((unsigned)(dsvcnt * sizeof(char *)))) == NULL)

	    { /* unable to allocate all storage */
	      failed = TRUE;
	    }

	  else
	    { /* set ptrs NULL so can dealloc storage if fail */

	      for (i = 0; i < dsvcnt; i++)
		pds->hname[i] = pds->spath[i] = pds->lpath[i] = NULL;

	      /* allocate and initialize data server information */

	      pds->cnt = dsvcnt;

	      for (i = 0; i < dsvcnt && !failed; i++)
		if ((pds->hname[i] =
		     malloc((unsigned)
			    (strlen(dsvsort[i]->hname) + 1))) == NULL ||

		    (pds->spath[i] =
		     malloc((unsigned)
			    (strlen(dsvsort[i]->spath) + 1))) == NULL ||

		    (pds->lpath[i] =
		     malloc((unsigned)
			    (strlen(dsvsort[i]->lpath) + 1))) == NULL)

		  { /* unable to allocate all storage */
		    failed = TRUE;
		  }

		else
		  { /* allocated storage, copy in data */
		    strcpy(pds->hname[i], dsvsort[i]->hname);
		    strcpy(pds->spath[i], dsvsort[i]->spath);
		    strcpy(pds->lpath[i], dsvsort[i]->lpath);
		  }

	      /* deallocate string storage if failure */

	      if (failed)
		for (i = 0; i < dsvcnt; i++)
		  {
		    if (pds->hname[i] != NULL)
		      free(pds->hname[i]);

		    if (pds->spath[i] != NULL)
		      free(pds->spath[i]);

		    if (pds->lpath[i] != NULL)
		      free(pds->lpath[i]);
		  }
	    }


	  /* if failed free hname, spath, lpath pointer storage */

	  if (failed)
	    {
	      if (pds->hname != NULL)
		free((char *)pds->hname);

	      if (pds->spath != NULL)
		free((char *)pds->spath);

	      if (pds->lpath != NULL)
		free((char *)pds->lpath);

	      free((char *)pds);
	      pds = NULL;

	      /* set result code to indicate failed storage allocation */
	      rcode = PIOUS_EINSUF;
	    }
	}
    }


  /* deallocate permutation vector storage */
  if (dsvsort != NULL)
    free((char *)dsvsort);

  /* return results */
  *dsinfo = pds;

  return rcode;
}




/*
 * tstate_insert()
 *
 * Parameters:
 *
 *   idv    - PDS message passing id vector
 *   tsv    - PDS transaction state record pointer vector
 *   cnt    - number of vector elements
 *
 * For each PDS message passing id in 'idv', if a record for that PDS does
 * not exist in the global pds_trans_state list, insert a new transaction
 * state record with the transaction sequence number set to zero (0).
 * Otherwise, update the existing transaction state record link count without
 * modifying the current transaction sequence number value.
 *
 * Upon successful completion, 'tsv' is a vector of PDS transaction state
 * record pointers, one for each PDS in 'idv'.
 *
 * Note: tstate_insert() is atomic!
 *
 * Returns:
 *
 *   PIOUS_OK (0) - all PDS message passing ids in pds_trans_state list
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINSUF - insufficient system resources to complete
 */

#ifdef __STDC__
static int tstate_insert(dce_srcdestt *idv,
			 trans_statet **tsv,
			 int cnt)

#else
static int tstate_insert(idv, tsv, cnt)
     dce_srcdestt *idv;
     trans_statet **tsv;
     int cnt;
#endif
{
  int rcode, i, j;
  register trans_statet *tstate;

  /* alloc 'cnt' individual transaction state records */

  rcode = PIOUS_OK;

  for (i = 0; i < cnt && rcode == PIOUS_OK; i++)
    if ((tsv[i] = (trans_statet *)malloc(sizeof(trans_statet))) == NULL)
      rcode = PIOUS_EINSUF;

  if (rcode != PIOUS_OK)
    { /* deallocate extraneous storage */
      for (j = 0; j < i - 1; j++)
	free((char *)tsv[j]);
    }

  else
    { /* operation atomicity gauranteed; simply deallocate uneeded storage */

      /* search global pds_trans_state list for each element of 'idv' */

      for (i = 0; i < cnt; i++)
	{
	  tstate = pds_trans_state;

	  while (tstate != NULL && tstate->pdsid != idv[i])
	    tstate = tstate->next;

	  if (tstate != NULL)
	    { /* located idv[i] in list; update link count */
	      tstate->linkcnt++;

	      /* deallocate newly alloced transaction state record */
	      free((char *)tsv[i]);

	      /* assign 'tsv' previously existing trans state record */
	      tsv[i] = tstate;
	    }

	  else
	    { /* idv[i] not in list; use newly alloced trans state record */
	      tstate = tsv[i];

	      tstate->pdsid    = idv[i];
	      tstate->transsn  = 0;
	      tstate->linkcnt  = 1;

	      /* put new record at head of list and increment list length */
	      tstate->next = pds_trans_state;
	      tstate->prev = NULL;

	      if (tstate->next != NULL)
		tstate->next->prev = tstate;

	      pds_trans_state = tstate;

	      pds_trans_state_cnt++;
	    }
	}
    }

  return rcode;
}




/*
 * tstate_rm()
 *
 * Parameters:
 *
 *   tsv    - PDS transaction state record pointer vector
 *   cnt    - number of vector elements
 *
 * For each PDS transaction state record in 'tsv', decrement link count
 * and remove from global pds_trans_state list if becomes zero (0).
 *
 * Returns:
 */

#ifdef __STDC__
static void tstate_rm(register trans_statet **tsv,
		      int cnt)
#else
static void tstate_rm(tsv, cnt)
     register trans_statet **tsv;
     int cnt;
#endif
{
  int i;

  for (i = 0; i < cnt; i++)
    { /* update transaction state record link count */

      tsv[i]->linkcnt--;
      
      /* remove rec from global pds_trans_state list if link count is zero */

      if (tsv[i]->linkcnt == 0)
	{ /* remove from list */

	  if (tsv[i]->next != NULL)
	    tsv[i]->next->prev = tsv[i]->prev;

	  if (tsv[i]->prev != NULL)
	    tsv[i]->prev->next = tsv[i]->next;
	  else
	    pds_trans_state = tsv[i]->next;

	  /* deallocate storage */
	  free((char *)tsv[i]);

	  /* decrement list count */
	  pds_trans_state_cnt--;
	}

      /* break link */

      tsv[i] = NULL;
    }
}




/*
 * ftable_dealloc()
 *
 * Parameters:
 *
 *   ftable - file table entry
 *
 * Deallocate a file table entry and mark as invalid
 *
 * Returns:
 */

#ifdef __STDC__
static void ftable_dealloc(ftable_entryt *ftable)
#else
static void ftable_dealloc(ftable)
     ftable_entryt *ftable;
#endif
{
  /* presumes a potentially partial file table entry due to a failed open */

  /* deallocate data server information */

  if (ftable->pds != NULL)
    {
      dsinfo_dealloc(ftable->pds);
      ftable->pds = NULL;
    }

  /* deallocate group name */

  if (ftable->group != NULL)
    {
      free(ftable->group);
      ftable->group = NULL;
    }

  /* deallocate path name */

  if (ftable->path != NULL)
    {
      free(ftable->path);
      ftable->path = NULL;
    }

  /* deallocate transaction state ptrs */

  if (ftable->trans_state != NULL)
    {
      free((char *)ftable->trans_state);
      ftable->trans_state = NULL;
    }

  /* deallocate parafile information record */

  if (ftable->pfinfo != NULL)
    {
      free((char *)ftable->pfinfo->seg_fhandle);
      free((char *)ftable->pfinfo->pds_id);
      free((char *)ftable->pfinfo);
      ftable->pfinfo = NULL;
    }

  ftable->valid = FALSE;
}




/*
 * dsinfo_dealloc()
 *
 * Parameters:
 *
 *   pds - data server information
 *
 * Deallocate a data server information record
 *
 * Returns:
 */

#ifdef __STDC__
static void dsinfo_dealloc(struct PSC_dsinfo *pds)
#else
static void dsinfo_dealloc(pds)
     struct PSC_dsinfo *pds;
#endif
{
  int i;

  /* deallocate string storage */

  for (i = 0; i < pds->cnt; i++)
    {
      free(pds->hname[i]);
      free(pds->spath[i]);
      free(pds->lpath[i]);
    }

  /* deallocate pointer storage */

  free((char *)pds->hname);
  free((char *)pds->spath);
  free((char *)pds->lpath);

  /* deallocate data server info record */

  free((char *)pds);
}




/*
 * mk_fullpath()
 *
 * Parameters:
 *
 *   pathname - pathname string
 *
 * Allocate storage and concatenate current working directory string
 * 'cwd' and 'pathname' string, in that order, separated by a '/' character.
 * If 'cwd' is the null string, or is not defined, then storage is
 * allocated and 'pathname' is copied.
 *
 * Returns:
 *
 *   char * - pointer to resultant string
 *   NULL   - can not allocate required storage
 */

#ifdef __STDC__
static char *mk_fullpath(char *pathname)
#else
static char *mk_fullpath(pathname)
     char *pathname;
#endif
{
  char *path;

  if (cwd == NULL || *cwd == '\0')
    { /* no current working directory string defined */

      if ((path = malloc((unsigned)(strlen(pathname) + 1))) != NULL)
	strcpy(path, pathname);
    }

  else
    { /* current working directory string is defined */

      if ((path = malloc((unsigned)
			 (strlen(cwd) + strlen(pathname) + 2))) != NULL)
	sprintf(path, "%s/%s", cwd, pathname);
    }

  return path;
}
