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




/* PIOUS Service Coordinator (PSC): Server Daemon
 *
 * @(#)psc_daemon.c	2.2  28 Apr 1995  Moyer
 *
 * The psc_daemon is the main server loop that processes client requests.
 *
 * NOTE:
 *
 * The file psc/psc.h provides a full discussion of PSC services and operation.
 * This is a must read prior to examining this code.
 *
 *
 * Function Summary:
 *
 * PSC_{s}open();
 * PSC_{s}close();
 * PSC_{s}chmod();
 * PSC_{s}unlink();
 * PSC_{s}mkdir();
 * PSC_{s}rmdir();
 * PSC_{s}chmoddir();
 * PSC_config();
 * PSC_shutdown();
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) A parafile metadata update should be performed as a stable operation.
 *      However, since the PDS does not currently implement recovery, there
 *      is no point doing this.
 *
 *   2) When first opening an existing file, the file handles for each data
 *      segment are obtained individually from the appropriate data server.
 *      This serves the dual purpose of obtaining the data segment file
 *      handles required for file access, and of checking that the data
 *      segment files do indeed exist.  Storing file handles with the parafile
 *      metadata is not an alternative since file handles go stale between
 *      data server instantiations.  Even if the PDS is modified to
 *      change this behavior, a file handle lookup for each data segment will
 *      still be required in the case where parafile truncation is specified.
 *
 *
 *
 * ----------------------------------------------------------------------------
 * Procedure for Adding PSC Functions:
 *
 *   The following is a step-by-step procedure for properly adding new PSC
 *   functions with consistent documentation.  Each step lists the file to
 *   be modified, and what those modifications must be.  Most modifications
 *   are very obvious extensions of existing code; basically adding more of
 *   the same.
 *
 *
 *   1) psc/psc_msg_exchange.h -
 *        a) add op code symbolic constant for new function and update
 *           operation type test macros.
 *        b) add message structures defining request/reply parameters
 *           for new function
 *
 *   2) psc/psc_msg_exchange.c - add new function to switch statements
 *        that control the packing/unpacking of message structures.
 *
 *   3) psc/psc_daemon.c -
 *        a) add new function to summary at top of file (documentation)
 *        b) add new function to private function declarations (for both
 *           ANSI/non-ANSI C)
 *        c) add new function call to switch in main server loop
 *        d) add new function definition with other PSC functions defs
 *        e) update request_eq()
 *        f) update req_dealloc(), if necessary
 *        g) update reply_dealloc(), if necessary
 *
 *   4) psc/psc.h - RPC stub declarations; add new function to summary near
 *        top (documentation) and add complete function documentation and
 *        declaration(s) where appropriate.
 *
 *   5) psc/psc.c - RPC stub definitions; add new function to summary at
 *        top (documentation) and add function definition(s) where
 *        appropriate.
 *
 */


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#else
#include "nonansi.h"
#include <memory.h>
#endif

#include <string.h>
#include <stdio.h>

#include "gpmacro.h"
#include "gputil.h"

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

#include "psc_cmsgidt.h"
#include "psc_cfparse.h"
#include "psc_dataserver_manager.h"
#include "psc_msg_exchange.h"




/*
 * Private Declaration - Types, Constants, and Macros
 */


/* macro to unregister service name, exit DCE, and exit process */

#define BailOut(n)    DCE_unregister(PSC_DAEMON_EXEC); DCE_exit(); exit((n))


/* macro to append a different segment number to an existing parafile
 * data segment path; works in conjunction with function mk_segpath().
 */

#define RenumberSegPath(pfsegpath, segnmbr) \
sprintf((strrchr((pfsegpath), '.') + 1), "%d", (segnmbr))


/* macros to validate and test validity of most recent operation record */

#define MROperationValidate() \
mroperation.valid = TRUE;  UTIL_clock_mark(&mroperation.tstamp)

#define MROperationValid() \
(mroperation.valid && \
 UTIL_clock_delta(&mroperation.tstamp, UTIL_MSEC) < PSC_TOPRSLTVALID)


/* macro to determine if access in oflag is permitted by amode */

#define OflagPermByAmode(oflag, amode) \
((((oflag) & PIOUS_RDONLY) || ((amode) & PIOUS_W_OK)) && \
 (((oflag) & PIOUS_WRONLY) || ((amode) & PIOUS_R_OK)))


/* macro to determine if access in oflag is permitted by user bits in mode */

#define OflagPermByUmode(oflag, mode) \
((((oflag) & PIOUS_RDONLY) || ((mode) & PIOUS_IWUSR)) && \
 (((oflag) & PIOUS_WRONLY) || ((mode) & PIOUS_IRUSR)))


/* data server information record */

typedef struct {
  int cnt;                    /* PDS count */
  char **hname;               /* PDS host names */
  char **spath;               /* PDS search root paths */
  char **lpath;               /* PDS log directory paths */
  dce_srcdestt *id;           /* PDS message passing ids */
} server_infot;


/* parafile group information record */

typedef struct gp_info {
  char *group;                /* group name */
  int view;                   /* group file view */
  pious_offt sptr_idx;        /* group index into shared pointer file */
  int open_cnt;               /* number of procs in group with file open */
  struct gp_info *next;       /* next group information entry */
  struct gp_info *prev;       /* prev group information entry */
} gp_infot;


/* parafile component information record */

typedef struct {
  int pds_cnt;                /* parafile PDS count (degree of declustering) */
  int seg_cnt;                /* parafile data segment count */
  pds_fhandlet *seg_fhandle;  /* parafile data segment file handles */
  pds_fhandlet mdata_fhandle; /* parafile metadata file handle */
  pds_fhandlet sptr_fhandle;  /* parafile shared pointer file handle */
} cmp_infot;


/* parafile file table entry */

typedef struct {
  int valid;                  /* table entry valid flag */
  int amode;                  /* accessability */
  int faultmode;              /* file access fault-tolerance mode */
  pious_offt sptr_idx_last;   /* shared pointer file index last assigned */
  int gpcnt;                  /* group count; number of groups w/file open */
  char *path;                 /* path name */
  server_infot *dsinfo;       /* data server information record */
  gp_infot *gpinfo;           /* group information records list */
  cmp_infot *cmpinfo;         /* component information record */
  char **hostdirpath;         /* host dir paths for pftable lookup */
} pftable_entryt;


/* parafile metadata file structure definition
 *
 *   the metadata file is an array of integers that specify the following
 *   information:
 *
 *     0: parafile extant (TRUE or FALSE)
 *     1: PDS count; number of data servers on which the file is declustered
 *     2: Segment count; number of data segments that comprise the parafile
 *
 *   note: PDS count <= Segment count
 */

#define METADATA_ENTRIES    3

#define METADATA_PF_EXTANT  0
#define METADATA_PDS_CNT    1
#define METADATA_SEG_CNT    2


/* parafile component file name strings */

#define PF_SEGMENT_FNAME     "segment"
#define PF_METADATA_FNAME    ".metadata"
#define PF_SHARPTR_FNAME     ".sharptr"


/* parafile component permissions
 *
 *   the PSC code assumes these permissions in implementing parafile
 *   operations and in interpreting parafile access result codes.
 *   the PDS code must be updated if default permissions are changed.
 *   note that data segment permissions are set by the user.
 */

/*   parafile directory: all users may examine; only owner may write */
#define PF_DIR_PERM \
((pious_modet)(PIOUS_IRWXU | PIOUS_IRGRP | PIOUS_IXGRP | \
               PIOUS_IROTH | PIOUS_IXOTH))

/*   parafile metadata file: all users may read; only owner may write */
#define PF_METADATA_PERM \
((pious_modet)(PIOUS_IRUSR | PIOUS_IWUSR | PIOUS_IRGRP | PIOUS_IROTH))


/*   parafile shared pointer file: all users may read and write */
#define PF_SHARPTR_PERM \
((pious_modet)(PIOUS_IRUSR | PIOUS_IWUSR | PIOUS_IRGRP | PIOUS_IWGRP | \
	       PIOUS_IROTH | PIOUS_IWOTH))


/* metadata_access() action macros */

#define READ_DATA     0
#define WRITE_DATA    1


/* PDS control operation request state */

struct cntrlop_state {
  int code;    /* result code of request send/recv */
  int id;      /* control message id */
};




/*
 * Private Variable Definitions
 */


/* SCCS version information */
static char VersionID[] = "@(#)psc_daemon.c	2.2  28 Apr 1995  Moyer";


/* Default Data Server Information
 *
 *   the following structure defines the host names, search root paths,
 *   log directory paths, and message passing ids of the default set of
 *   PIOUS data servers (PDS).
 *
 *   this information, excepting the PDS message passing ids, is supplied
 *   via the PSC configuration file.
 */

static server_infot def_pds;


/* Parafile File Table
 *
 *   Table of open file descriptions for PIOUS parafiles.  The variable
 *   'parafile_table_max' serves as a high-water mark for searching the
 *   file table.
 */

static pftable_entryt parafile_table[PSC_OPEN_MAX];
static int parafile_table_max;


/* Most Recent Operation Record
 *
 *  Parallel applications may generate sets of identical PSC requests, either
 *  out of necessity, such as in the case of PSC_{s}open(), or because the
 *  application is implemented in the common SPMD model.  For efficiency,
 *  the PSC retains the result of the most recent operation to allow responses
 *  to identical operations with minimal delay, and without additional
 *  communication, where appropriate.
 *
 *  In recognition of the fact that the results of two successive identical
 *  operations can potentially be different due to external factors, the most
 *  recent operation result becomes invalid after a time period of
 *  PSC_TOPRSLTVALID milliseconds.
 *
 *  NOTE: the time-out period is measured from the first of a set of successive
 *  identical operations; later operations in the set do NOT reset the timer.
 */

static struct {
  int valid;
  util_clockt tstamp;
  int reqop;
  pscmsg_reqt reqmsg;
  pscmsg_replyt replymsg;
} mroperation;


/* Control message id; distinguishes responses to PDS control operation reqs */

int psc_global_cmsgid = 0;




/*
 * Private Function Declarations
 */

#ifdef __STDC__
static void PSC_open_(dce_srcdestt clientid,
		      int reqop,
		      pscmsg_reqt *reqmsg);

static void PSC_close_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg);

static void PSC_chmod_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg);

static void psc_unlink_(dce_srcdestt clientid,
			int reqop,
			pscmsg_reqt *reqmsg);

static void PSC_mkdir_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg);

static void PSC_rmdir_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg);

static void psc_chmoddir_(dce_srcdestt clientid,
			  int reqop,
			  pscmsg_reqt *reqmsg);

static void PSC_config_(dce_srcdestt clientid,
			int reqop,
			pscmsg_reqt *reqmsg);

static void PSC_shutdown_(dce_srcdestt clientid,
			  int reqop,
			  pscmsg_reqt *reqmsg);

static int pftable_locate(int pds_cnt,
			  char **pds_hname,
			  char **pds_spath,
			  char *path,
			  pftable_entryt **ftable);

static int pf_struct_on_pds(server_infot *pds,
			    char *path,
			    pds_fhandlet *mdata_fhandle,
			    long *mdata_buf);

static int pf_creat(server_infot *pds,
		    char *path,
		    pious_modet mode,
		    int segcnt,
		    cmp_infot *cmpinfo,
		    int *amode);

static int pf_lookup(server_infot *pds,
		     char *path,
		     int cflag,
		     pious_modet mode,
		     pds_fhandlet mdata_fhandle,
		     long *mdata_buf,
		     cmp_infot *cmpinfo,
		     int *amode);

static int pf_unlink(server_infot *pds,
		     char *path);

static int pf_chmod(server_infot *pds,
		    char *path,
		    pious_modet mode,
		    int *amode);

static int metadata_access(int action,
			   server_infot *pds,
			   char *path,
			   pds_fhandlet *mdata_fhandle,
			   long *mdata_buf);

static int mkdir_all(server_infot *pds,
		     char *path,
		     pious_modet mode);

static int rmdir_all(server_infot *pds,
		     char *path);

static int chmoddir_all(server_infot *pds,
			char *path,
			pious_modet mode);

static int spawn_servers(server_infot *pds);

static int sptr_zero(dce_srcdestt pdsid,
		     pds_fhandlet fhandle,
		     pious_offt offset);

static int request_eq(int reqop1,
		      register pscmsg_reqt *reqmsg1,
		      int reqop2,
		      register pscmsg_reqt *reqmsg2);

static server_infot *dsinfo_alloc(int cnt);

static cmp_infot *cmpinfo_alloc(int cnt);

static gp_infot *gpinfo_alloc(int len);

static server_infot *dsinfo_cpy(int cnt,
				char **hname,
				char **spath,
				char **lpath);

static void req_dealloc(int reqop,
			pscmsg_reqt *reqmsg);

static void reply_dealloc(int replyop,
			  pscmsg_replyt *replymsg);

static void pftable_dealloc(register pftable_entryt *ftable);

static char *mk_dirpath(char *prefix,
			char *pathname);

static char *mk_comppath(char *prefix,
			 char *pathname,
			 char *compname);

static char *mk_segpath(char *prefix,
			char *pathname,
			int segnmbr);

static char *mk_hostdirpath(char *hostname,
			    char *prefix,
			    char *pathname);
#else
static void PSC_open_();

static void PSC_close_();

static void PSC_chmod_();

static void psc_unlink_();

static void PSC_mkdir_();

static void PSC_rmdir_();

static void psc_chmoddir_();

static void PSC_config_();

static void PSC_shutdown_();

static int pftable_locate();

static int pf_struct_on_pds();

static int pf_creat();

static int pf_lookup();

static int pf_unlink();

static int pf_chmod();

static int metadata_access();

static int mkdir_all();

static int rmdir_all();

static int chmoddir_all();

static int spawn_servers();

static int sptr_zero();

static int request_eq();

static server_infot *dsinfo_alloc();

static cmp_infot *cmpinfo_alloc();

static gp_infot *gpinfo_alloc();

static server_infot *dsinfo_cpy();

static void req_dealloc();

static void reply_dealloc();

static void pftable_dealloc();

static char *mk_dirpath();

static char *mk_comppath();

static char *mk_segpath();

static char *mk_hostdirpath();
#endif




/* ---------------------------- PSC Daemon Loop --------------------------- */


/*
 * main()
 *
 * Parameters:
 *
 *   hostfile - PSC configuration file
 *
 * Returns:
 */

#ifdef __STDC__
int main(int argc, char **argv)
#else
int main(argc, argv)
     int argc;
     char **argv;
#endif
{
  int rcode, i;
  dce_srcdestt clientid;
  int reqop;
  pscmsg_reqt reqmsg;

  /* Parse system configuration file and set data server host defaults */

  if (argc != 1 && argc != 2)
    { /* incorrect number of arguments */
      fprintf (stderr, "\nUsage: pious [configfile]\n");

      BailOut(1);
    }

  if (argc == 1)
    /* no configuration file specified; no default data servers */
    def_pds.cnt = 0;

  else if ((rcode = CF_parse(argv[1],
			     &def_pds.cnt,
			     &def_pds.hname,
			     &def_pds.spath, &def_pds.lpath)) != PIOUS_OK)
    { /* unable to parse configuration file; syntax/access errors reported */
      if (rcode == PIOUS_EINSUF)
	fprintf(stderr, "\npious: insufficient memory\n");

      BailOut(1);
    }


  /* Register service coordinator with DCE so clients can locate */

  if ((rcode = DCE_register(PSC_DAEMON_EXEC)) != PIOUS_OK)
    { /* unable to register PSC with naming service */
      if (rcode == PIOUS_EINVAL)
	fprintf(stderr, "\npious: already running\n");
      else
	fprintf(stderr, "\npious: can not register with naming service\n");

      BailOut(1);
    }


  /* Spawn default set of data servers */

  if (def_pds.cnt != 0)
    {
      if ((def_pds.id = (dce_srcdestt *)
	   malloc((unsigned)(def_pds.cnt * sizeof(dce_srcdestt)))) == NULL)
	{ /* unable to allocate storage for default message passing ids */
	  fprintf(stderr, "\npious: insufficient memory\n");

	  BailOut(1);
	}

      for (i = 0; i < def_pds.cnt; i++)
	if (SM_add_pds(def_pds.hname[i],
		       def_pds.lpath[i], &def_pds.id[i]) != PIOUS_OK)
	  { /* unable to spawn data server */
	    fprintf(stderr, "\npious: unable to spawn data server on %s\n",
		    def_pds.hname[i]);

	    /* shutdown any active data servers */
	    SM_shutdown_pds();

	    BailOut(1);
	  }
    }


  /* Mark file table entries as invalid and set high-water mark and count */

  for (i = 0; i < PSC_OPEN_MAX; i++)
    parafile_table[i].valid = FALSE;

  parafile_table_max = 0;


  /* Mark most recent operation as invalid */

  mroperation.valid = FALSE;


  /* Initialization sequence is complete */

  fprintf(stdout, "\nPIOUS is ready.\n");
  fflush(stdout);



  /* -------------------- Service Client Requests -------------------- */

  while (TRUE)
    { /* receive client request */
      while (PSCMSG_req_recv(&clientid,
			     &reqop,
			     &reqmsg,
			     DCE_BLOCK) != PIOUS_OK);

      /* perform requested operation */
      switch(reqop)
	{
	case PSC_OPEN_OP:
	case PSC_SOPEN_OP:
	  PSC_open_(clientid, reqop, &reqmsg);
	  break;

	case PSC_CLOSE_OP:
	case PSC_SCLOSE_OP:
	  PSC_close_(clientid, reqop, &reqmsg);
	  break;

	case PSC_CHMOD_OP:
	case PSC_SCHMOD_OP:
	  PSC_chmod_(clientid, reqop, &reqmsg);
	  break;

	case PSC_UNLINK_OP:
	case PSC_SUNLINK_OP:
	  psc_unlink_(clientid, reqop, &reqmsg);
	  break;

	case PSC_MKDIR_OP:
	case PSC_SMKDIR_OP:
	  PSC_mkdir_(clientid, reqop, &reqmsg);
	  break;

	case PSC_RMDIR_OP:
	case PSC_SRMDIR_OP:
	  PSC_rmdir_(clientid, reqop, &reqmsg);
	  break;

	case PSC_CHMODDIR_OP:
	case PSC_SCHMODDIR_OP:
	  psc_chmoddir_(clientid, reqop, &reqmsg);
	  break;

	case PSC_CONFIG_OP:
	  PSC_config_(clientid, reqop, &reqmsg);
	  break;

	case PSC_SHUTDOWN_OP:
	  PSC_shutdown_(clientid, reqop, &reqmsg);
	  break;
	}
    }
}




/*
 * Private Function Definitions - PSC functions
 */


/*
 * PSC_open() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_open_(dce_srcdestt clientid,
		      int reqop,
		      pscmsg_reqt *reqmsg)
#else
static void PSC_open_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int i, rcode, lcode, oflagtmp, cflag;
  register pftable_entryt *ftable;
  pftable_entryt *ftentry, *ftable_final;
  register gp_infot *gp;
  gp_infot *gp_final;
  pds_fhandlet mdata_fhandle;
  long mdata_buf[METADATA_ENTRIES];


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* if previous open successful, update group open count in file table */

      if (mroperation.replymsg.rcode == PIOUS_OK)
	{ /* locate file table entry for either default or specified servers */

	  if (reqop == PSC_OPEN_OP)
	    lcode = pftable_locate(def_pds.cnt,
				   def_pds.hname, def_pds.spath,
				   reqmsg->body.open.path,
				   &ftentry);

	  else /* reqop == PSC_SOPEN_OP */
	    lcode = pftable_locate(reqmsg->pds_cnt,
				   reqmsg->pds_hname, reqmsg->pds_spath,
				   reqmsg->body.open.path,
				   &ftentry);

	  ftable = ftentry;  /* can not take the adress of a register var */

	  /* if pftable locate successful, update open count */

	  if (lcode == PIOUS_OK)
	    { /* locate appropriate group information */
	      gp = ftable->gpinfo;

	      while (gp != NULL && strcmp(gp->group, reqmsg->body.open.group))
		gp = gp->next;

	      if (gp != NULL)
		{ /* update open count for this group */
		  if (gp->open_cnt < PIOUS_INT_MAX)
		    gp->open_cnt++;
		  else
		    lcode = PIOUS_EINSUF;
		}

	      else
		{ /* group not located; error in PIOUS code */
		  lcode = PIOUS_EUNXP;
		}
	    }

	  /* if error occured, reset operation result */

	  if (lcode != PIOUS_OK)
	    { /* set new result code */

	      if (lcode == PIOUS_EINSUF)
		/* insufficient resources to complete operation */
		mroperation.replymsg.rcode = PIOUS_EINSUF;
	      else
		/* pftable entry or group not located; error in PIOUS code */
		mroperation.replymsg.rcode = PIOUS_EUNXP;

	      /* re-validate operation as this constitutes a new result */
	      MROperationValidate();

	      /* note: no storage to dealloc in reply message; alloced space
               *       is shared with file table entry.
	       */

	      mroperation.replymsg.body.open.seg_fhandle = NULL;
	      mroperation.replymsg.body.open.pds_id      = NULL;
	    }
	}

      /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'pds_cnt', 'oflag', 'faultmode', 'seg', and 'path' args */

      rcode = PIOUS_OK;

      /* check that only one access mode is specified in oflag */
      oflagtmp = ((reqmsg->body.open.oflag & PIOUS_RDONLY) |
		  (reqmsg->body.open.oflag & PIOUS_WRONLY) |
		  (reqmsg->body.open.oflag & PIOUS_RDWR));

      if (oflagtmp != PIOUS_RDONLY && oflagtmp != PIOUS_WRONLY &&
	  oflagtmp != PIOUS_RDWR)
	rcode = PIOUS_EINVAL;

      /* check if PIOUS_RDONLY specified with PIOUS_TRUNC */
      else if ((reqmsg->body.open.oflag & PIOUS_RDONLY) &&
	       (reqmsg->body.open.oflag & PIOUS_TRUNC))
	rcode = PIOUS_EINVAL;

      /* check if PIOUS_CREAT specified and 'seg' invalid */
      else if ((reqmsg->body.open.oflag & PIOUS_CREAT) &&
	       (reqmsg->body.open.seg <= 0))
	rcode = PIOUS_EINVAL;

      /* check specified fault-tolerance mode */
      else if (reqmsg->body.open.faultmode != PIOUS_STABLE &&
	       reqmsg->body.open.faultmode != PIOUS_VOLATILE)
	rcode = PIOUS_EINVAL;

      /* check if 'pds_cnt', specified or implied, is invalid */
      else if ((reqop == PSC_SOPEN_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_OPEN_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      /* check 'path' for empty string */
      else if (*reqmsg->body.open.path == '\0')
	rcode = PIOUS_ENOENT;


      /* determine if file currently open for default or specified servers */

      if (rcode == PIOUS_OK)
	{
	  if (reqop == PSC_OPEN_OP)
	    lcode = pftable_locate(def_pds.cnt,
				   def_pds.hname, def_pds.spath,
				   reqmsg->body.open.path,
				   &ftentry);

	  else /* reqop == PSC_SOPEN_OP */
	    lcode = pftable_locate(reqmsg->pds_cnt,
				   reqmsg->pds_hname, reqmsg->pds_spath,
				   reqmsg->body.open.path,
				   &ftentry);

	  ftable = ftentry;  /* can not take the adress of a register var */

	  if (lcode == PIOUS_EINSUF)
	    /* insufficient memory to perform pftable lookup */
	    rcode = PIOUS_EINSUF;


	  /* subcase: file open by *some* group */


	  else if (lcode == PIOUS_OK)
	    { /* check file accessability against requested mode in oflag */

	      if (!OflagPermByAmode(reqmsg->body.open.oflag, ftable->amode))
		rcode = PIOUS_EACCES;

	      /* check file fault mode against requested fault mode */

	      else if (reqmsg->body.open.faultmode != ftable->faultmode)
		rcode = PIOUS_EACCES;

	      /* determine if file is open by group of requesting process */

	      else
		{
		  gp = ftable->gpinfo;

		  while (gp != NULL &&
			 strcmp(gp->group, reqmsg->body.open.group))
		    gp = gp->next;


		  if (gp != NULL)
		    { /* file open by process' group */

		      if (reqmsg->body.open.view == gp->view)
			{ /* process view consistent; update open count */
			  if (gp->open_cnt < PIOUS_INT_MAX)
			    {
			      gp->open_cnt++;

			      /* set final ftable pointers */
			      ftable_final = ftable;
			      gp_final     = gp;
			    }

			  else
			    { /* insufficient resources to complete */
			      rcode = PIOUS_EINSUF;
			    }
			}

		      else
			{ /* process view inconsistent */
			  rcode = PIOUS_EACCES;
			}
		    }

		  else
		    { /* file NOT open by process' group */

		      /* determine if resources for another group */

		      if (ftable->gpcnt         == PIOUS_INT_MAX ||
			  ftable->sptr_idx_last == PIOUS_OFFT_MAX)
			rcode = PIOUS_EINSUF;

		      /* zero next shared pointer for group */

		      else if ((lcode =
				sptr_zero(ftable->dsinfo->id[0],
					  ftable->cmpinfo->sptr_fhandle,
					  ftable->sptr_idx_last + 1))
			       != PIOUS_OK)
			{ /* can not write to shared pointer file */
			  switch(lcode)
			    {
			    case PIOUS_EINSUF:
			    case PIOUS_ETPORT:
			    case PIOUS_EFATAL:
			      rcode = lcode;
			      break;
			    case PIOUS_EINVAL:
			      /* file offset too large; allow no more groups */
			      rcode = PIOUS_EINSUF;
			      break;
			    default:
			      rcode = PIOUS_EUNXP;
			      break;
			    }
			}

		      /* allocate a parafile group information record */

		      else if ((gp =
				gpinfo_alloc((int)
					     strlen(reqmsg->body.open.group)))
			       == NULL)
			rcode = PIOUS_EINSUF;

		      else
			{ /* initialize gpinfo record for process' group */
			  strcpy(gp->group, reqmsg->body.open.group);

			  gp->view     = reqmsg->body.open.view;

			  gp->sptr_idx = ++ftable->sptr_idx_last;
			  gp->open_cnt = 1;

			  /* insert gpinfo at head of group information list */
			  gp->next     = ftable->gpinfo;
			  gp->prev     = NULL;

			  ftable->gpinfo = gp->next->prev = gp;

			  /* increment parafile group count */
			  ftable->gpcnt++;

			  /* set final ftable pointers */
			  ftable_final = ftable;
			  gp_final     = gp;
			}
		    }
		}
	    }


	  /* subcase: file NOT open by *any* group */


	  else /* lcode == PIOUS_ENOENT */
	    { /* locate empty slot in file table for a new entry */

	      for (ftable  = parafile_table;
		   ftable  < (parafile_table + PSC_OPEN_MAX) && ftable->valid;
		   ftable++);

	      if (ftable == parafile_table + PSC_OPEN_MAX)
		/* no more empty slots in table */
		rcode = PIOUS_EINSUF;

	      /* initialize file table entry */

	      else
		{ /* step 0: set ptrs to null for pftable_dealloc() if fail */


		  ftable->path        = NULL;
		  ftable->dsinfo      = NULL;
		  ftable->gpinfo      = NULL;
		  ftable->cmpinfo     = NULL;
		  ftable->hostdirpath = NULL;


		  /* step 1: define parafile path and data server information
		   *         record; spawn servers if required.
		   */


		  if ((ftable->path = (char *)
		       malloc((unsigned)
			      (strlen(reqmsg->body.open.path) + 1))) == NULL)
		    { /* unable to allocate storage for path */
		      rcode = PIOUS_EINSUF;
		    }

		  else
		    { /* assign path to pftable entry */
		      strcpy(ftable->path, reqmsg->body.open.path);

		      /* define data server information record */

		      if (reqop == PSC_OPEN_OP)
			{ /* use default data servers */
			  ftable->dsinfo = &def_pds;
			}

		      else /* reqop == PSC_SOPEN_OP */
			{ /* use specified data servers; spawn if required */

			  if ((ftable->dsinfo =
			       dsinfo_cpy(reqmsg->pds_cnt,
					  reqmsg->pds_hname,
					  reqmsg->pds_spath,
					  reqmsg->pds_lpath)) == NULL)
			    /* unable to allocate dsinfo storage */
			    rcode = PIOUS_EINSUF;

			  else
			    /* spawn servers, if required, and set msg ids */
			    rcode = spawn_servers(ftable->dsinfo);
			}
		    }


		  /* step 2: determine if file extant and do lookup/creat;
                   *         defines parafile component information record
                   *         and parafile accessability (amode).
		   */


		  if (rcode == PIOUS_OK)
		    { /* check for parafile structure on specified PDS */

		      rcode = pf_struct_on_pds(ftable->dsinfo,
					       ftable->path,
					       &mdata_fhandle,
					       mdata_buf);

		      if (rcode == PIOUS_OK && !mdata_buf[METADATA_PF_EXTANT])
			/* pf struct exists but metadata flagged not extant */
			rcode = PIOUS_ENOENT;

		      if (rcode == PIOUS_OK && mdata_buf[METADATA_PF_EXTANT])
			{ /* parafile struct exists, attempt to open */

			  if ((ftable->cmpinfo =
			       cmpinfo_alloc((int)mdata_buf[METADATA_SEG_CNT]))
			      == NULL)
			    { /* unable to allocate cmpinfo storage */
			      rcode = PIOUS_EINSUF;
			    }

			  else
			    { /* indicate no create, but trunc if specified */
			      cflag = (PIOUS_NOCREAT |
				       (reqmsg->body.open.oflag &
					PIOUS_TRUNC));

			      rcode = pf_lookup(ftable->dsinfo,
						ftable->path,
						cflag,
						(pious_modet)0,
						mdata_fhandle,
						mdata_buf,
						ftable->cmpinfo,
						&ftable->amode);

			      /* check file accessability against requested
                               * mode in oflag
			       */

			      if (rcode == PIOUS_OK)
				if (!OflagPermByAmode(reqmsg->body.open.oflag,
						      ftable->amode))
				  rcode = PIOUS_EACCES;
			    }
			}

		      else if (rcode == PIOUS_ENOENT &&
			       (reqmsg->body.open.oflag & PIOUS_CREAT))
			{ /* parafile does NOT exist but create specified */

			  if (!OflagPermByUmode(reqmsg->body.open.oflag,
						reqmsg->body.open.mode))
			    /* oflag access not permitted by mode for user */
			    rcode = PIOUS_EINVAL;

			  else if ((ftable->cmpinfo =
				    cmpinfo_alloc(reqmsg->body.open.seg))
				   == NULL)
			    /* unable to allocate cmpinfo storage */
			    rcode = PIOUS_EINSUF;

			  else

			    rcode = pf_creat(ftable->dsinfo,
					     ftable->path,
					     reqmsg->body.open.mode,
					     reqmsg->body.open.seg,
					     ftable->cmpinfo,
					     &ftable->amode);
			}
		    }


		  /* step 3: define the parafile group information record */


		  if (rcode == PIOUS_OK)
		    { /* allocate a parafile group information record */

		      if ((ftable->gpinfo =
			   gpinfo_alloc((int)strlen(reqmsg->body.open.group)))
			  == NULL)
			{ /* unable to allocate gpinfo storage */
			  rcode = PIOUS_EINSUF;
			}

		      else
			{ /* initialize gpinfo record for process' group */
			  strcpy(ftable->gpinfo->group,
				 reqmsg->body.open.group);

			  ftable->gpinfo->view     = reqmsg->body.open.view;

			  ftable->gpinfo->sptr_idx = 0;
			  ftable->gpinfo->open_cnt = 1;

			  ftable->gpinfo->next     = NULL;
			  ftable->gpinfo->prev     = NULL;
			}
		    }


		  /* step 4: set parafile host dir paths for pftable lookup */


		  if (rcode == PIOUS_OK)
		    { /* allocate host directory path pointer storage */

		      if ((ftable->hostdirpath = (char **)
			   malloc((unsigned)(ftable->cmpinfo->pds_cnt *
					     sizeof(char *)))) == NULL)
			{ /* unable to allocate hostdirpath storage */
			  rcode = PIOUS_EINSUF;
			}

		      else
			{ /* mark hostdirpath for deallocation if failure */

			  for (i = 0; i < ftable->cmpinfo->pds_cnt; i++)
			    ftable->hostdirpath[i] = NULL;

			  /* form all host directory paths */

			  for (i = 0;
			       i < ftable->cmpinfo->pds_cnt &&
			       rcode == PIOUS_OK; i++)
			    if ((ftable->hostdirpath[i] =
				 mk_hostdirpath(ftable->dsinfo->hname[i],
						ftable->dsinfo->spath[i],
						ftable->path)) == NULL)
			      rcode = PIOUS_EINSUF;
			}
		    }


		  /* step 5: define remaining file table entry components */


		  if (rcode == PIOUS_OK)
		    { /* define flags and counts */
		      ftable->gpcnt         = 1;
		      ftable->sptr_idx_last = 0;
		      ftable->faultmode     = reqmsg->body.open.faultmode;
		      ftable->valid         = TRUE;
		    }


		  /* step 6: zero the shared pointer for this group */


		  if (rcode == PIOUS_OK)
		    {
		      if ((lcode = sptr_zero(ftable->dsinfo->id[0],
					     ftable->cmpinfo->sptr_fhandle,
					     (pious_offt)0)) != PIOUS_OK)
			{ /* can not write to shared pointer file */
			  switch(lcode)
			    {
			    case PIOUS_EINSUF:
			    case PIOUS_ETPORT:
			    case PIOUS_EFATAL:
			      rcode = lcode;
			      break;
			    case PIOUS_EINVAL:
			      /* file offset too large -- no more groups */
			      rcode = PIOUS_EINSUF;
			      break;
			    default:
			      rcode = PIOUS_EUNXP;
			      break;
			    }
			}
		    }


		  /* step 7: cleanup */


		  if (rcode == PIOUS_OK)
		    { /* increment file table high-water mark if required */
		      if (ftable == parafile_table + parafile_table_max)
			parafile_table_max++;

		      /* set final ftable pointers */
		      ftable_final = ftable;
		      gp_final     = ftable->gpinfo;
		    }

		  else
		    { /* deallocate partial file table entry */
		      pftable_dealloc(ftable);
		    }
		}
	    }
	}


      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      if (rcode == PIOUS_OK)
	{ /* open succeeded; return required information */
	  mroperation.replymsg.body.open.pds_cnt =
	    ftable_final->cmpinfo->pds_cnt;

	  mroperation.replymsg.body.open.seg_cnt =
	    ftable_final->cmpinfo->seg_cnt;

	  mroperation.replymsg.body.open.sptr_fhandle =
	    ftable_final->cmpinfo->sptr_fhandle;

	  mroperation.replymsg.body.open.seg_fhandle =
	    ftable_final->cmpinfo->seg_fhandle;

	  mroperation.replymsg.body.open.sptr_idx =
	    gp_final->sptr_idx;

	  mroperation.replymsg.body.open.pds_id =
	    ftable_final->dsinfo->id;
	}

      else
	{
	  mroperation.replymsg.body.open.seg_fhandle = NULL;
	  mroperation.replymsg.body.open.pds_id      = NULL;
	}

      MROperationValidate();


      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_close() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_close_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg)
#else
static void PSC_close_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, lcode;
  register pftable_entryt *ftable;
  pftable_entryt *ftentry;
  register gp_infot *gp;
  pscmsg_replyt replymsg;

  /* deallocate/invalidate most recent (previous) request/reply storage
   *
   *   the close function does not generate any PSC-PDS communication and
   *   hence does not benefit from request caching.
   */

  if (mroperation.valid)
    {
      req_dealloc(mroperation.reqop, &mroperation.reqmsg);
      reply_dealloc(mroperation.reqop, &mroperation.replymsg);

      mroperation.valid = FALSE;
    }

  /* validate 'path' and 'pds_cnt' argument, specified or implied */

  if (*reqmsg->body.close.path == '\0')
    rcode = PIOUS_ENOENT;

  else if ((reqop == PSC_SCLOSE_OP && reqmsg->pds_cnt <= 0) ||
	   (reqop == PSC_CLOSE_OP  && def_pds.cnt <= 0))
    rcode = PIOUS_EINVAL;

  else
    { /* locate file table entry for either default or specified servers */
      if (reqop == PSC_CLOSE_OP)
	lcode = pftable_locate(def_pds.cnt,
			       def_pds.hname, def_pds.spath,
			       reqmsg->body.close.path,
			       &ftentry);

      else /* reqop == PSC_SCLOSE_OP */
	lcode = pftable_locate(reqmsg->pds_cnt,
			       reqmsg->pds_hname, reqmsg->pds_spath,
			       reqmsg->body.close.path,
			       &ftentry);

      ftable = ftentry;  /* can not take the adress of a register var */


      if (lcode == PIOUS_EINSUF)
	/* insufficient memory to perform pftable lookup */
	rcode = PIOUS_EINSUF;

      else if (lcode == PIOUS_ENOENT)
	/* specified file not open by any group */
	rcode = PIOUS_ENOENT;

      else /* lcode == PIOUS_OK */
	{ /* file in pftable; determine if file open by process' group */
	  gp = ftable->gpinfo;

	  while (gp != NULL && strcmp(gp->group, reqmsg->body.close.group))
	    gp = gp->next;

	  if (gp == NULL)
	    /* process' group does not have file open */
	    rcode = PIOUS_ENOENT;

	  else
	    { /* process' group has file open */
	      rcode = PIOUS_OK;

	      /* update open count for this group */
	      gp->open_cnt--;

	      /* deallocate group record if last process has closed file */
	      if (gp->open_cnt == 0)
		{ /* remove record from list */
		  if (gp->next != NULL)
		    gp->next->prev = gp->prev;

		  if (gp->prev != NULL)
		    gp->prev->next = gp->next;
		  else
		    ftable->gpinfo = gp->next;

		  /* deallocate group information record storage */
		  free(gp->group);
		  free((char *)gp);

		  /* decrement group count in ftable entry */
		  ftable->gpcnt--;


		  /* remove parafile from table if no group has open */

		  if (ftable->gpcnt == 0)
		    { /* invalidate/deallocate pftable entry */
		      pftable_dealloc(ftable);

		      /* decrement file table high-water mark if required */
		      while (parafile_table_max > 0 &&
			     !parafile_table[parafile_table_max - 1].valid)
			parafile_table_max--;


		      /* reset data servers if no parafiles open */

		      if (parafile_table_max == 0)
			SM_reset_pds();
		    }
		}
	    }
	}
    }

  /* initialize reply message */
  replymsg.rcode = rcode;

  /* reply to client; inability to send is equivalent to a lost message */
  PSCMSG_reply_send(clientid, reqop, &replymsg);

  /* deallocate request message storage */
  req_dealloc(reqop, reqmsg);
}




/*
 * PSC_chmod() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_chmod_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg)
#else
static void PSC_chmod_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, lcode, amode;
  pftable_entryt *ftentry;
  server_infot *pds;


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'path' and 'pds_cnt' argument, specified or implied */

      if (*reqmsg->body.chmod.path == '\0')
	rcode = PIOUS_ENOENT;

      else if ((reqop == PSC_SCHMOD_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_CHMOD_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      else
	{ /* locate file table entry for either default or specified servers */
	  if (reqop == PSC_CHMOD_OP)
	    lcode = pftable_locate(def_pds.cnt,
				   def_pds.hname, def_pds.spath,
				   reqmsg->body.chmod.path,
				   &ftentry);

	  else /* reqop == PSC_SCHMOD_OP */
	    lcode = pftable_locate(reqmsg->pds_cnt,
				   reqmsg->pds_hname, reqmsg->pds_spath,
				   reqmsg->body.chmod.path,
				   &ftentry);

	  if (lcode == PIOUS_EINSUF)
	    /* insufficient memory to perform pftable lookup */
	    rcode = PIOUS_EINSUF;

	  else if (lcode == PIOUS_OK)
	    /* file open by some group; unlink not permitted */
	    rcode = PIOUS_EBUSY;

	  else /* lcode == PIOUS_ENOENT */
	    { /* specified file not open by any group */

	      rcode = PIOUS_OK;

	      /* define temp data server info record; spawn servers */

	      if (reqop == PSC_CHMOD_OP)
		{ /* utilize default data servers */
		  pds = &def_pds;
		}

	      else /* reqop == PSC_SCHMOD_OP */
		{ /* allocate data server info record storage */
		  if ((pds = dsinfo_alloc(reqmsg->pds_cnt)) == NULL)
		    { /* unable to allocate storage */
		      rcode = PIOUS_EINSUF;
		    }

		  else
		    { /* initialize data server info record */
		      pds->cnt   = reqmsg->pds_cnt;
		      pds->hname = reqmsg->pds_hname;
		      pds->spath = reqmsg->pds_spath;
		      pds->lpath = reqmsg->pds_lpath;

		      /* spawn servers, if required, and obtain PDS msg ids */
		      rcode = spawn_servers(pds);
		    }
		}


	      /* chmod of file on data servers */

	      if (rcode == PIOUS_OK)
		rcode = pf_chmod(pds,
				 reqmsg->body.chmod.path,
				 reqmsg->body.chmod.mode,
				 &amode);

	      /* deallocate temp data server info record */

	      if (pds != NULL && pds != &def_pds)
		{
		  free((char *)pds->id);
		  free((char *)pds);
		}
	    }
	}

      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      MROperationValidate();

      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_unlink() - See psc.h for description.
 */

#ifdef __STDC__
static void psc_unlink_(dce_srcdestt clientid,
			int reqop,
			pscmsg_reqt *reqmsg)
#else
static void psc_unlink_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode, lcode;
  pftable_entryt *ftentry;
  server_infot *pds;


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'path' and 'pds_cnt' argument, specified or implied */

      if (*reqmsg->body.unlink.path == '\0')
	rcode = PIOUS_ENOENT;

      else if ((reqop == PSC_SUNLINK_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_UNLINK_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      else
	{ /* locate file table entry for either default or specified servers */
	  if (reqop == PSC_UNLINK_OP)
	    lcode = pftable_locate(def_pds.cnt,
				   def_pds.hname, def_pds.spath,
				   reqmsg->body.unlink.path,
				   &ftentry);

	  else /* reqop == PSC_SUNLINK_OP */
	    lcode = pftable_locate(reqmsg->pds_cnt,
				   reqmsg->pds_hname, reqmsg->pds_spath,
				   reqmsg->body.unlink.path,
				   &ftentry);

	  if (lcode == PIOUS_EINSUF)
	    /* insufficient memory to perform pftable lookup */
	    rcode = PIOUS_EINSUF;

	  else if (lcode == PIOUS_OK)
	    /* file open by some group; unlink not permitted */
	    rcode = PIOUS_EBUSY;

	  else /* lcode == PIOUS_ENOENT */
	    { /* specified file not open by any group */

	      rcode = PIOUS_OK;

	      /* define temp data server info record; spawn servers */

	      if (reqop == PSC_UNLINK_OP)
		{ /* utilize default data servers */
		  pds = &def_pds;
		}

	      else /* reqop == PSC_SUNLINK_OP */
		{ /* allocate data server info record storage */
		  if ((pds = dsinfo_alloc(reqmsg->pds_cnt)) == NULL)
		    { /* unable to allocate storage */
		      rcode = PIOUS_EINSUF;
		    }

		  else
		    { /* initialize data server info record */
		      pds->cnt   = reqmsg->pds_cnt;
		      pds->hname = reqmsg->pds_hname;
		      pds->spath = reqmsg->pds_spath;
		      pds->lpath = reqmsg->pds_lpath;

		      /* spawn servers, if required, and obtain PDS msg ids */
		      rcode = spawn_servers(pds);
		    }
		}


	      /* unlink file on data servers */

	      if (rcode == PIOUS_OK)
		rcode = pf_unlink(pds, reqmsg->body.unlink.path);

	      /* deallocate temp data server info record */

	      if (pds != NULL && pds != &def_pds)
		{
		  free((char *)pds->id);
		  free((char *)pds);
		}
	    }
	}

      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      MROperationValidate();

      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_mkdir() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_mkdir_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg)
#else
static void PSC_mkdir_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode;
  server_infot *pds;


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'path' and 'pds_cnt' argument, specified or implied */

      if (*reqmsg->body.mkdir.path == '\0')
	rcode = PIOUS_ENOENT;

      else if ((reqop == PSC_SMKDIR_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_MKDIR_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      else
	{ /* define temp data server info record; spawn servers */

	  rcode = PIOUS_OK;

	  if (reqop == PSC_MKDIR_OP)
	    { /* utilize default data servers */
	      pds = &def_pds;
	    }

	  else /* reqop == PSC_SMKDIR_OP */
	    { /* allocate data server info record storage */
	      if ((pds = dsinfo_alloc(reqmsg->pds_cnt)) == NULL)
		{ /* unable to allocate storage */
		  rcode = PIOUS_EINSUF;
		}

	      else
		{ /* initialize data server info record */
		  pds->cnt   = reqmsg->pds_cnt;
		  pds->hname = reqmsg->pds_hname;
		  pds->spath = reqmsg->pds_spath;
		  pds->lpath = reqmsg->pds_lpath;

		  /* spawn servers, if required, and obtain PDS msg ids */
		  rcode = spawn_servers(pds);
		}
	    }


	  /* mkdir on all data servers */

	  if (rcode == PIOUS_OK)
	    rcode = mkdir_all(pds,
			      reqmsg->body.mkdir.path,
			      reqmsg->body.mkdir.mode);

	  /* deallocate temp data server info record */

	  if (pds != NULL && pds != &def_pds)
	    {
	      free((char *)pds->id);
	      free((char *)pds);
	    }
	}

      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      MROperationValidate();

      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_rmdir() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_rmdir_(dce_srcdestt clientid,
		       int reqop,
		       pscmsg_reqt *reqmsg)
#else
static void PSC_rmdir_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode;
  server_infot *pds;


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'path' and 'pds_cnt' argument, specified or implied */

      if (*reqmsg->body.rmdir.path == '\0')
	rcode = PIOUS_ENOENT;

      else if ((reqop == PSC_SRMDIR_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_RMDIR_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      else
	{ /* define temp data server info record; spawn servers */

	  rcode = PIOUS_OK;

	  if (reqop == PSC_RMDIR_OP)
	    { /* utilize default data servers */
	      pds = &def_pds;
	    }

	  else /* reqop == PSC_SRMDIR_OP */
	    { /* allocate data server info record storage */
	      if ((pds = dsinfo_alloc(reqmsg->pds_cnt)) == NULL)
		{ /* unable to allocate storage */
		  rcode = PIOUS_EINSUF;
		}

	      else
		{ /* initialize data server info record */
		  pds->cnt   = reqmsg->pds_cnt;
		  pds->hname = reqmsg->pds_hname;
		  pds->spath = reqmsg->pds_spath;
		  pds->lpath = reqmsg->pds_lpath;

		  /* spawn servers, if required, and obtain PDS msg ids */
		  rcode = spawn_servers(pds);
		}
	    }


	  /* rmdir on all data servers */

	  if (rcode == PIOUS_OK)
	    rcode = rmdir_all(pds, reqmsg->body.rmdir.path);

	  /* deallocate temp data server info record */

	  if (pds != NULL && pds != &def_pds)
	    {
	      free((char *)pds->id);
	      free((char *)pds);
	    }
	}

      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      MROperationValidate();

      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_chmoddir() - See psc.h for description.
 */

#ifdef __STDC__
static void psc_chmoddir_(dce_srcdestt clientid,
			  int reqop,
			  pscmsg_reqt *reqmsg)
#else
static void psc_chmoddir_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int rcode;
  server_infot *pds;


  /* case: current request identical to previous */

  if (MROperationValid() &&
      request_eq(reqop, reqmsg, mroperation.reqop, &mroperation.reqmsg))
    { /* reply to client; inability to send is equivalent to a lost message */
      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);

      /* deallocate request message storage */
      req_dealloc(reqop, reqmsg);
    }


  /* case: current request NOT identical to previous */

  else
    { /* deallocate/invalidate most recent (previous) request/reply storage */
      if (mroperation.valid)
	{
	  req_dealloc(mroperation.reqop, &mroperation.reqmsg);
	  reply_dealloc(mroperation.reqop, &mroperation.replymsg);

	  mroperation.valid = FALSE;
	}

      /* validate 'path' and 'pds_cnt' argument, specified or implied */

      if (*reqmsg->body.chmoddir.path == '\0')
	rcode = PIOUS_ENOENT;

      else if ((reqop == PSC_SCHMODDIR_OP && reqmsg->pds_cnt <= 0) ||
	       (reqop == PSC_CHMODDIR_OP  && def_pds.cnt <= 0))
	rcode = PIOUS_EINVAL;

      else
	{ /* define temp data server info record; spawn servers */

	  rcode = PIOUS_OK;

	  if (reqop == PSC_CHMODDIR_OP)
	    { /* utilize default data servers */
	      pds = &def_pds;
	    }

	  else /* reqop == PSC_SCHMODDIR_OP */
	    { /* allocate data server info record storage */
	      if ((pds = dsinfo_alloc(reqmsg->pds_cnt)) == NULL)
		{ /* unable to allocate storage */
		  rcode = PIOUS_EINSUF;
		}

	      else
		{ /* initialize data server info record */
		  pds->cnt   = reqmsg->pds_cnt;
		  pds->hname = reqmsg->pds_hname;
		  pds->spath = reqmsg->pds_spath;
		  pds->lpath = reqmsg->pds_lpath;

		  /* spawn servers, if required, and obtain PDS msg ids */
		  rcode = spawn_servers(pds);
		}
	    }


	  /* chmoddir on all data servers */

	  if (rcode == PIOUS_OK)
	    rcode = chmoddir_all(pds,
				 reqmsg->body.chmoddir.path,
				 reqmsg->body.chmoddir.mode);

	  /* deallocate temp data server info record */

	  if (pds != NULL && pds != &def_pds)
	    {
	      free((char *)pds->id);
	      free((char *)pds);
	    }
	}

      /* initialize and validate most recent request/reply storage */

      mroperation.reqop  = reqop;
      mroperation.reqmsg = *reqmsg;

      mroperation.replymsg.rcode = rcode;

      MROperationValidate();

      /* reply to client; inability to send is equivalent to a lost message */

      PSCMSG_reply_send(clientid, reqop, &mroperation.replymsg);
    }
}




/*
 * PSC_config() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_config_(dce_srcdestt clientid,
			int reqop,
			pscmsg_reqt *reqmsg)
#else
static void PSC_config_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  pscmsg_replyt replymsg;

  /* deallocate/invalidate most recent (previous) request/reply storage
   *
   *   the config function does not generate any PSC-PDS communication and
   *   hence does not benefit from request caching.
   */

  if (mroperation.valid)
    {
      req_dealloc(mroperation.reqop, &mroperation.reqmsg);
      reply_dealloc(mroperation.reqop, &mroperation.replymsg);

      mroperation.valid = FALSE;
    }

  /* initialize reply message */
  replymsg.rcode                   = PIOUS_OK;
  replymsg.body.config.def_pds_cnt = def_pds.cnt;

  /* reply to client; inability to send is equivalent to a lost message */
  PSCMSG_reply_send(clientid, reqop, &replymsg);
}




/*
 * PSC_shutdown() - See psc.h for description.
 */

#ifdef __STDC__
static void PSC_shutdown_(dce_srcdestt clientid,
			  int reqop,
			  pscmsg_reqt *reqmsg)
#else
static void PSC_shutdown_(clientid, reqop, reqmsg)
     dce_srcdestt clientid;
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  /* deallocate/invalidate most recent (previous) request/reply storage
   *
   *   the shutdown function will only execute once and hence does not
   *   benefit from request caching.
   */

  if (mroperation.valid)
    {
      req_dealloc(mroperation.reqop, &mroperation.reqmsg);
      reply_dealloc(mroperation.reqop, &mroperation.replymsg);

      mroperation.valid = FALSE;
    }

  /* shutdown all data servers and service coordinator if no parafile open */

  if (parafile_table_max == 0)
    {
      SM_shutdown_pds();
      BailOut(0);
    }

  /* note that shutdown function does not reply to client */
}




/*
 * Private Function Definitions - Local Functions
 */


/*
 * pftable_locate()
 *
 * Parameters:
 *
 *   pds_cnt   - PDS host entry count
 *   pds_hname - PDS host names
 *   pds_spath - PDS host search root paths
 *   path      - parafile path name
 *   ftable    - parafile file table entry
 *
 * Determine if a parafile 'path' on data servers 'pds_hname', using search
 * roots paths 'pds_spath', is currently open; if so, return a pointer to
 * the parafile file table entry in 'ftable'.
 *
 * Note: assumes pds arguments to be valid
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile located in file table
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ENOENT - parafile not located in file table
 *       PIOUS_EINSUF - insufficient system resources to complete; retry
 */

#ifdef __STDC__
static int pftable_locate(int pds_cnt,
			  char **pds_hname,
			  char **pds_spath,
			  char *path,
			  pftable_entryt **ftable)
#else
static int pftable_locate(pds_cnt, pds_hname, pds_spath, path, ftable)
     int pds_cnt;
     char **pds_hname;
     char **pds_spath;
     char *path;
     pftable_entryt **ftable;
#endif
{
  int rcode, i, match;
  register char **hostdirpath;
  register pftable_entryt *ftentry;

  /* because of "server" versions of PSC functions, i.e. versions that allow
   * the user to specify data server information directly in the function
   * call, file table lookup can not be performed by simply comparing
   * 'path' arguments.
   */

  /* check parafile file table high water mark to see if table is empty */
  if (parafile_table_max == 0)
    rcode = PIOUS_ENOENT;

  /* allocate host directory path pointer storage */
  else if ((hostdirpath = (char **)
	    malloc((unsigned)(pds_cnt * sizeof(char *)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* mark hostdirpath for deallocation */
      for (i = 0; i < pds_cnt; i++)
	hostdirpath[i] = NULL;

      /* form all host directory paths */
      rcode = PIOUS_OK;

      for (i = 0; i < pds_cnt && rcode == PIOUS_OK; i++)
	if ((hostdirpath[i] = mk_hostdirpath(pds_hname[i],
					     pds_spath[i], path)) == NULL)
	  rcode = PIOUS_EINSUF;

      /* scan file table for parafile entry */
      if (rcode == PIOUS_OK)
	{
	  match = FALSE;

	  for (ftentry = parafile_table;
	       ftentry < (parafile_table + parafile_table_max) && !match;
	       ftentry++)
	    if (ftentry->valid && ftentry->cmpinfo->pds_cnt <= pds_cnt)
	      {
		match = TRUE;

		for (i = 0; i < ftentry->cmpinfo->pds_cnt && match; i++)
		  match = !strcmp(hostdirpath[i], ftentry->hostdirpath[i]);
	      }

	  if (match)
	    *ftable = ftentry - 1;
	  else
	    rcode = PIOUS_ENOENT;
	}

      /* deallocate host directory paths */
      for (i = 0; i < pds_cnt; i++)
	if (hostdirpath[i] != NULL)
	  free(hostdirpath[i]);

      free((char *)hostdirpath);
    }

  return rcode;
}




/*
 * pf_struct_on_pds()
 *
 * Parameters:
 *
 *   pds           - data server information record
 *   path          - parafile path name
 *   mdata_fhandle - parafile metadata file handle
 *   mdata_buf     - parafile metadata contents
 *
 * Determine if a parafile structure for file 'path' exists on data servers
 * specified by 'pds'.  If file structure exists, return metadata file handle
 * and data in 'mdata_fhandle' and 'mdata_buf', respectively.
 *
 * A parafile structure consists of a set of parafile directories, with a
 * metadata file in the low order directory.
 *
 * Note: assumes 'pds' argument is valid
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile structure exists on specified set of PDS
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - parafile structure 'path' does not exist on 'pds'
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int pf_struct_on_pds(server_infot *pds,
			    char *path,
			    pds_fhandlet *mdata_fhandle,
			    long *mdata_buf)
#else
static int pf_struct_on_pds(pds, path, mdata_fhandle, mdata_buf)
     server_infot *pds;
     char *path;
     pds_fhandlet *mdata_fhandle;
     long *mdata_buf;
#endif
{
  int rcode, acode, i;
  struct cntrlop_state *cntrlop;
  char *pfdirpath;
  pious_modet mode, *modev;


  /* parafile structure extant on specified pds if:
   *
   *   1) can read metadata file in low order parafile directory, thus
   *      defining pds_cnt, the number of data servers on which the
   *      parafile is declustered, and
   *   2) at least pds_cnt data servers specified in 'pds', and
   *   3) the first pds_cnt data servers in 'pds' possess a parafile directory
   */

  /* determine if parafile directory exists on low order PDS */

  cntrlop   = NULL;
  modev     = NULL;
  pfdirpath = NULL;

  if ((pfdirpath = mk_dirpath(pds->spath[0], path)) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((acode = PDS_stat(pds->id[0], CntrlIdNext,
			     pfdirpath, &mode)) != PIOUS_OK)
    { /* unable to determine status of pfdirpath */
      switch (acode)
	{
        case PIOUS_EACCES:
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
    }

  else if (!(mode & PIOUS_ISDIR))
    /* path not a directory on low order PDS */
    rcode = PIOUS_ENOENT;


  /* read metadata from parafile directory on low order PDS */

  else if ((acode = metadata_access(READ_DATA,
				    pds, path,
				    mdata_fhandle, mdata_buf)) != PIOUS_OK)
    { /* unable to read parafile metadata */
      switch (acode)
	{
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;
	case PIOUS_EACCES:
	case PIOUS_EPERM:
	case PIOUS_ENOTREG:
	  /* if metadata file can not be read, is incomplete, or is not regular
           * then defined parafile structure is not on specified pds
	   */
	  rcode = PIOUS_ENOENT;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }


  /* check metadata to see if a sufficient number of PDS are specified in
   * the pds information record.
   */

  else if (mdata_buf[METADATA_PDS_CNT] > pds->cnt)
    rcode = PIOUS_ENOENT;

  /* check for existence of parafile directory on remaining PDS of interest */

  else if ((cntrlop = (struct cntrlop_state *)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((modev = (pious_modet *)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(pious_modet)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* deallocate extraneous storage */

      free(pfdirpath);
      pfdirpath = NULL;

      /* send stat requests */

      for (i = 1; i < mdata_buf[METADATA_PDS_CNT]; i++)
	{ /* form parafile directory pathname */

	  if ((pfdirpath = mk_dirpath(pds->spath[i], path)) == NULL)
	    cntrlop[i].code = PIOUS_EINSUF;

	  /* check status of parafile directory */

	  else
	    {
	      cntrlop[i].id   = CntrlIdNext;
	      cntrlop[i].code = PDS_stat_send(pds->id[i], CntrlIdLast,
					      pfdirpath);

	      /* deallocate pfdirpath storage */
	      free(pfdirpath);
	      pfdirpath = NULL;
	    }
	}


      /* receive stat replys */

      for (i = 1; i < mdata_buf[METADATA_PDS_CNT]; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  cntrlop[i].code = PDS_stat_recv(pds->id[i],
					  cntrlop[i].id, &modev[i]);

      /* set result code */

      acode = PIOUS_OK;

      for (i = 1; i < mdata_buf[METADATA_PDS_CNT] && acode == PIOUS_OK; i++)
	if (cntrlop[i].code != PIOUS_OK)
	  acode = cntrlop[i].code;
	else if (!(modev[i] & PIOUS_ISDIR))
	  acode = PIOUS_ENOENT;

      switch (acode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
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
    }

  /* deallocate extraneous storage */

  if (pfdirpath != NULL)
    free(pfdirpath);

  if (modev != NULL)
    free((char *)modev);

  if (cntrlop != NULL)
    free((char *)cntrlop);

  /* return result */
  return rcode;
}




/*
 * pf_creat()
 *
 * Parameters:
 *
 *   pds          - data server information record
 *   path         - parafile path name
 *   mode         - parafile access control (permission) bits
 *   segcnt       - parafile data segment count
 *   cmpinfo      - parafile component information record
 *   amode        - parafile accessability
 *
 * Create parafile 'path' with 'segcnt' data segments on data servers 'pds';
 * file permission bits are set according to 'mode'.
 * A complete parafile component information record is returned in 'cmpinfo',
 * and parafile accessability is returned in 'amode'.
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *    PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 * Note: Will not create a parafile on data servers where 'path' already
 *       exists as either a regular or directory file; i.e. will not use
 *       existing directories.
 *
 *       Assumes 'pds' argument to be valid.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile successfully created
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid 'segcnt' argument
 *       PIOUS_EACCES       - path search permission denied or write permission
 *                            to create denied
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EPERM        - operation not permitted; 'path' name conflict
 *       PIOUS_ENOSPC       - no space to extend directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int pf_creat(server_infot *pds,
		    char *path,
		    pious_modet mode,
		    int segcnt,
		    cmp_infot *cmpinfo,
		    int *amode)
#else
static int pf_creat(pds, path, mode, segcnt, cmpinfo, amode)
     server_infot *pds;
     char *path;
     pious_modet mode;
     int segcnt;
     cmp_infot *cmpinfo;
     int *amode;
#endif
{
  int rcode, acode;
  int i, tmp_pds_cnt;
  long mdata_buf[METADATA_ENTRIES];
  pds_fhandlet mdata_fhandle;
  pds_transidt transid;
  struct cntrlop_state *cntrlop;
  char *pfdirpath;
  pious_modet stat_mode;


  /* parafile creation performed as follows:
   *
   *   1) check for name conflicts on 'pds'
   *   2) make a directory 'path' on required number of data servers
   *   3) create metadata file with parafile marked as NOT extant
   *   4) create data segment and shared pointer files
   *   5) mark metadata file indicating that parafile is extant
   *
   * performing operations in this order insures that a partial parafile
   * resulting from a failed create can always be detected.  furthermore,
   * sufficient information is supplied so that a partial parafile can
   * be unlinked.
   */


  /* initialize metadata array and other values */

  mdata_buf[METADATA_PF_EXTANT] = FALSE;
  mdata_buf[METADATA_PDS_CNT]   = Min(pds->cnt, segcnt);
  mdata_buf[METADATA_SEG_CNT]   = segcnt;

  cntrlop   = NULL;
  pfdirpath = NULL;

  /* validate 'segcnt' argument */

  if (segcnt <= 0)
    rcode = PIOUS_EINVAL;

  /* check for pathname conflict */

  else if ((cntrlop = (struct cntrlop_state *)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* send stat requests */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	{ /* form full pathname of parafile dir on PDS i */

	  if ((pfdirpath = mk_dirpath(pds->spath[i], path)) == NULL)
	    cntrlop[i].code = PIOUS_EINSUF;

	  /* check for existence of parafile dir on PDS i */

	  else
	    {
	      cntrlop[i].id   = CntrlIdNext;
	      cntrlop[i].code = PDS_stat_send(pds->id[i], CntrlIdLast,
					      pfdirpath);

	      free(pfdirpath);
	    }
	}


      /* receive stat replys */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  cntrlop[i].code = PDS_stat_recv(pds->id[i], cntrlop[i].id,
					  &stat_mode);

      /* determine stat result; continue only if all return PIOUS_ENOENT */

      acode = PIOUS_ENOENT;

      for (i = 0;
	   i < mdata_buf[METADATA_PDS_CNT] && acode == PIOUS_ENOENT; i++)
	acode = cntrlop[i].code;

      if (acode != PIOUS_ENOENT)
	{ /* can not create parafile 'path' on data servers 'pds' */
	  switch (acode)
	    {
	    case PIOUS_EACCES:
	    case PIOUS_ENAMETOOLONG:
	    case PIOUS_ENOTDIR:
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	    case PIOUS_EFATAL:
	      rcode = acode;
	      break;
	    case PIOUS_OK:
	      rcode = PIOUS_EPERM;
	      break;
	    default:
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      /* create parafile directory on minimal set of data servers needed */

      else
	{
	  tmp_pds_cnt = pds->cnt;
	  pds->cnt    = mdata_buf[METADATA_PDS_CNT];

	  /* NOTE: if PDS share a file system then mkdir_all() will return
	   *       PIOUS_EEXIST; this does not connote name conflict since
	   *       previously determined that no conflict exists.
	   */

	  if ((acode = mkdir_all(pds, path, PF_DIR_PERM)) == PIOUS_EEXIST)
	    acode = PIOUS_OK;

	  pds->cnt = tmp_pds_cnt;

	  if (acode != PIOUS_OK)
	    { /* unable to create required set of parafile directories */
	      rmdir_all(pds, path);

	      switch(acode)
		{
		case PIOUS_EACCES:
		case PIOUS_ENAMETOOLONG:
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
	    }

	  /* create/write metadata file in low order parafile directory */

	  else if ((acode = metadata_access(WRITE_DATA,
					    pds, path,
					    &mdata_fhandle,
					    mdata_buf)) != PIOUS_OK)
	    { /* unable to create/write metadata file */
	      switch (acode)
		{
		case PIOUS_ENAMETOOLONG:
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
	    }


	  /* create parafile data segment files and shared pointer file */

	  else if ((acode = pf_lookup(pds,
				      path, PIOUS_CREAT, mode,
				      mdata_fhandle, mdata_buf,
				      cmpinfo, amode)) != PIOUS_OK)
	    { /* unable to create parafile components */
	      switch(acode)
		{
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
	    }

	  /* write to metadata file, marking parafile as extant */

	  else
	    { /* note: not using metadata_access() to avoid lookup overhead */
	      mdata_buf[METADATA_PF_EXTANT] = TRUE;

	      if (transid_assign(&transid) != PIOUS_OK)
		/* unable to get a transaction id for writing metadata file */
		rcode = PIOUS_EUNXP;

	      else if ((acode = PDS_write_sint(pds->id[0], transid, 0,
					       mdata_fhandle,
					       (pious_offt)0,
					       METADATA_ENTRIES,
					       mdata_buf)) != METADATA_ENTRIES)
		{ /* unable to write all parafile metadata */
		  if (acode != PIOUS_EABORT)
		    PDS_abort(pds->id[0], transid);

		  switch (acode)
		    {
		    case PIOUS_EINSUF:
		    case PIOUS_ETPORT:
		    case PIOUS_EFATAL:
		      rcode = acode;
		      break;
		    default:
		      rcode = PIOUS_EUNXP;
		      break;
		    }
		}

	      else
		{ /* commit metadata write */

#ifdef VTCOMMITNOACK
		  acode = PDS_commit_send(pds->id[0], transid, 1);
#else
		  acode = PDS_commit(pds->id[0], transid, 1);
#endif

		  switch (acode)
		    {
		    case PIOUS_OK:
		    case PIOUS_EINSUF:
		    case PIOUS_ETPORT:
		    case PIOUS_EFATAL:
		      rcode = acode;
		      break;
		    default:
		      rcode = PIOUS_EUNXP;
		      break;
		    }
		}
	    }
	}
    }

  /* deallocate extraneous storage */

  if (cntrlop != NULL)
    free((char *)cntrlop);

  /* return result */
  return rcode;
}




/*
 * pf_lookup()
 *
 * Parameters:
 *
 *   pds           - data server information record
 *   path          - parafile path name
 *   cflag         - file creation flag
 *   mode          - file creation access control (permission) bits
 *   mdata_fhandle - parafile metadata file handle
 *   mdata_buf     - parafile metadata file contents
 *   cmpinfo       - parafile component information record
 *   amode         - parafile accessability
 *
 * Obtain file handles for shared pointer file and all data segment files of
 * parafile 'path' on data servers 'pds', where the parafile metadata file
 * handle and data are specified by 'mdata_fhandle' and 'mdata_buf',
 * respectively.  If these files do not exist and 'cflag' indicates file
 * creation, then the files are created with data segment file permission bits
 * set according to 'mode'.  If these files do exist and 'cflag' indicates
 * file truncation, then the data segment file lengths are set to zero;
 * the shared pointer file is truncated regardless of the value of 'cflag'.
 * A complete parafile component information record is returned in 'cmpinfo',
 * and parafile accessability is returned in 'amode'.
 *
 * 'cflag' is the inclusive OR of:
 *    exactly one of:     PIOUS_NOCREAT, PIOUS_CREAT and
 *    any combination of: PIOUS_TRUNC
 *
 * 'mode' is the bitwise inclusive OR of any of:
 *    PIOUS_IRUSR, PIOUS_IWUSR, PIOUS_IXUSR (PIOUS_IRWXU)
 *    PIOUS_IRGRP, PIOUS_IWGRP, PIOUS_IXGRP (PIOUS_IRWXG)
 *    PIOUS_IROTH, PIOUS_IWOTH, PIOUS_IXOTH (PIOUS_IRWXO)
 *
 * 'amode' is the bitwise inclusive OR of any of:
 *    PIOUS_R_OK, PIOUS_W_OK, PIOUS_X_OK
 *
 * Note: Presumes that:
 *
 *       1) specifying PIOUS_NOCREAT implies that parafile 'path' is known to
 *          exist and be accessable on data servers 'pds', and
 *
 *       2) specifying PIOUS_CREAT implies that parafile 'path' is being
 *          created on data servers 'pds' and that a parafile structure,
 *          consisting of a set of parafile directories and a metadata file,
 *          has previously been created and is accessable.
 *
 * Thus all access errors are considered to be unexpected (PIOUS_EUNXP),
 * with the single exception of a write permission error when truncation
 * is specified.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile file handles successfully obtained
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid 'cflag' argument
 *       PIOUS_EACCES       - PIOUS_TRUNC specified and write permission denied
 *       PIOUS_ENOSPC       - PIOUS_CREAT specified and no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int pf_lookup(server_infot *pds,
		     char *path,
		     int cflag,
		     pious_modet mode,
		     pds_fhandlet mdata_fhandle,
		     long *mdata_buf,
		     cmp_infot *cmpinfo,
		     int *amode)
#else
static int pf_lookup(pds, path, cflag, mode, mdata_fhandle, mdata_buf,
		     cmpinfo, amode)
     server_infot *pds;
     char *path;
     int cflag;
     pious_modet mode;
     pds_fhandlet mdata_fhandle;
     long *mdata_buf;
     cmp_infot *cmpinfo;
     int *amode;
#endif
{
  int rcode, lkcode;
  int i, ds_send, ds_recv, sendcnt, recvcnt;
  struct cntrlop_state *cntrlop;
  char *pfcomppath, **pfsegpathv;
  int lkamode;


  /* init work variables */

  cntrlop    = NULL;
  pfcomppath = NULL;
  pfsegpathv = NULL;

  /* assign PDS count and metadata fhandle to parafile component info record */

  cmpinfo->pds_cnt       = mdata_buf[METADATA_PDS_CNT];
  cmpinfo->mdata_fhandle = mdata_fhandle;


  /* validate 'cflag' argument */

  if ((cflag & (~(PIOUS_CREAT | PIOUS_TRUNC))) != 0)
    rcode = PIOUS_EINVAL;

  /* lookup shared pointer file fhandle */

  else if ((pfcomppath = mk_comppath(pds->spath[0],
				     path, PF_SHARPTR_FNAME)) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((lkcode = PDS_lookup(pds->id[0], CntrlIdNext,
				pfcomppath,
				cflag | PIOUS_TRUNC,
				PF_SHARPTR_PERM,
				&(cmpinfo->sptr_fhandle),
				&lkamode)) != PIOUS_OK)
    { /* unable to obtain file handle for shared pointer file */
      switch (lkcode)
	{
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = lkcode;
	  break;
	case PIOUS_EACCES:
	  if (cflag & PIOUS_TRUNC)
	    rcode = PIOUS_EACCES;
	  else
	    rcode = PIOUS_EUNXP;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }


  /* lookup file handles for all parafile data segments */

  else if ((cntrlop = (struct cntrlop_state *)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((pfsegpathv = (char **)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(char *)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* pipeline lookup requests */

      cmpinfo->seg_cnt = mdata_buf[METADATA_SEG_CNT];

      /* ---- fill pipe ---- */

      lkcode  = PIOUS_OK;
      *amode  = PIOUS_R_OK | PIOUS_W_OK;

      ds_send = ds_recv = 0;
      sendcnt = recvcnt = 0;

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	pfsegpathv[i] = NULL;


      for (i = 0; i < mdata_buf[METADATA_PDS_CNT] && lkcode == PIOUS_OK; i++)
	{ /* form parafile data segment path for segment i */

	  if ((pfsegpathv[i] = mk_segpath(pds->spath[i], path, i)) == NULL)
	    lkcode = PIOUS_EINSUF;

	  /* perform lookup on segment i */

	  else
	    {
	      cntrlop[i].id = CntrlIdNext;

	      lkcode = PDS_lookup_send(pds->id[i], CntrlIdLast,
				       pfsegpathv[i], cflag, mode);

	      /* increment counters */
	      if (lkcode == PIOUS_OK)
		sendcnt++;
	    }
	}


      /* ---- pipe steady state ---- */

      for (i = mdata_buf[METADATA_PDS_CNT];
	   i < mdata_buf[METADATA_SEG_CNT] && lkcode == PIOUS_OK; i++)
	{ /* receive lookup reply */

	  lkcode = PDS_lookup_recv(pds->id[ds_recv], cntrlop[ds_recv].id,
				   &(cmpinfo->seg_fhandle[recvcnt]), &lkamode);

	  /* factor segment accessability into 'amode' and incr. counters */

	  *amode &= lkamode;

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];

	  /* send next lookup request */

	  if (lkcode == PIOUS_OK)
	    { /* form full pathname of segment i */

	      RenumberSegPath(pfsegpathv[ds_send], i);

	      /* perform lookup on segment i */

	      cntrlop[ds_send].id = CntrlIdNext;

	      lkcode = PDS_lookup_send(pds->id[ds_send], CntrlIdLast,
				       pfsegpathv[ds_send], cflag, mode);

	      /* increment counters */
	      if (lkcode == PIOUS_OK)
		{
		  sendcnt++;
		  ds_send = (ds_send + 1) % mdata_buf[METADATA_PDS_CNT];
		}
	    }
	}


      /* ---- drain pipe ---- */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT] && lkcode == PIOUS_OK; i++)
	{ /* receive lookup reply */

	  lkcode = PDS_lookup_recv(pds->id[ds_recv], cntrlop[ds_recv].id,
				   &(cmpinfo->seg_fhandle[recvcnt]), &lkamode);

	  /* factor segment accessability into 'amode' and incr. counters */

	  *amode &= lkamode;

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	}


      /* consume outstanding requests that may result if an error occured */

      while (recvcnt < sendcnt)
	{ /* receive lookup reply */

	  PDS_lookup_recv(pds->id[ds_recv], cntrlop[ds_recv].id,
			  &(cmpinfo->seg_fhandle[recvcnt]), &lkamode);

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	}

      /* set final result code */

      switch (lkcode)
	{
	case PIOUS_OK:
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = lkcode;
	  break;
	case PIOUS_EACCES:
	  if (cflag & PIOUS_TRUNC)
	    rcode = PIOUS_EACCES;
	  else
	    rcode = PIOUS_EUNXP;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate parafile data segment pathname storage */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	if (pfsegpathv[i] != NULL)
	  free(pfsegpathv[i]);

      free((char *)pfsegpathv);
    }

  /* deallocate extraneous storage */
  if (cntrlop != NULL)
    free((char *)cntrlop);

  if (pfcomppath != NULL)
    free(pfcomppath);

  /* return result */
  return rcode;
}




/*
 * pf_unlink()
 *
 * Parameters:
 *
 *   pds          - data server information record
 *   path         - parafile path name
 *
 * Unlink parafile 'path' on data servers 'pds'.
 *
 * Note: assumes 'pds' argument to be valid.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile successfully removed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *                            or insufficient privileges to perform operation
 *       PIOUS_ENOENT       - parafile 'path' does not exist on 'pds'
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int pf_unlink(server_infot *pds,
		     char *path)
#else
static int pf_unlink(pds, path)
     server_infot *pds;
     char *path;
#endif
{
  int rcode, acode;
  int i, pfstruct_extant, tmp_pds_cnt;
  int ds_send, ds_recv, sendcnt, recvcnt;
  long mdata_buf[METADATA_ENTRIES];
  pds_fhandlet mdata_fhandle;
  pds_transidt transid;
  struct cntrlop_state *cntrlop;
  char *pfcomppath, **pfsegpathv;


  /* parafile unlink performed as follows:
   *
   *   1) determine if parafile structure extant on pds; if not, goto 4
   *   2) mark parafile metadata as not extant
   *   3) remove data segment and shared pointer files
   *   4) remove metadata file
   *   5) remove all parafile directories
   *
   * performing operations in this order insures that a partial parafile
   * resulting from a failed unlink can always be detected.  furthermore,
   * sufficient information is retained so that a partial parafile can
   * be re-unlinked.
   */


  /* initialize working variables */

  cntrlop    = NULL;
  pfcomppath = NULL;
  pfsegpathv = NULL;


  /* 1) determine if parafile structure extant on 'pds' */

  /* examine data servers for parafile structure */
  acode = pf_struct_on_pds(pds, path, &mdata_fhandle, mdata_buf);

  switch (acode)
    {
    case PIOUS_OK:
    case PIOUS_EACCES:
    case PIOUS_ENAMETOOLONG:
    case PIOUS_ENOTDIR:
    case PIOUS_EINSUF:
    case PIOUS_ETPORT:
    case PIOUS_EFATAL:
      rcode           = acode;
      pfstruct_extant = TRUE;  /* applicable when rcode == PIOUS_OK */
      break;
    case PIOUS_ENOENT:
      /* must assume a partial parafile structure resulting from failed
       * unlink/create.  a partial parafile structure can consist of
       * an incomplete metedata file and/or a (possibly incomplete)
       * set of parafile directories.
       */

      /* flag a potential incomplete parafile struct and goto step 4) */
      rcode           = PIOUS_OK;
      pfstruct_extant = FALSE;

      goto PFUnlink_Step4;

    default:
      rcode = PIOUS_EUNXP;
      break;
    }


  /* 2) mark parafile as not extant */

  if (rcode == PIOUS_OK)
    { /* may already be marked as such if this is an unlink of a failed
       * create or unlink, but ability to write to this file proves
       * permission to unlink based on default parafile component permissions.
       */

      mdata_buf[METADATA_PF_EXTANT] = FALSE;

      /* note: not using metadata_access() to avoid lookup overhead */

      if (transid_assign(&transid) != PIOUS_OK)
	/* unable to get a transaction id for writing metadata file */
	rcode = PIOUS_EUNXP;

      else if ((acode = PDS_write_sint(pds->id[0], transid, 0,
				       mdata_fhandle,
				       (pious_offt)0,
				       METADATA_ENTRIES,
				       mdata_buf)) != METADATA_ENTRIES)
	{ /* unable to write all parafile metadata */
	  if (acode != PIOUS_EABORT)
	    PDS_abort(pds->id[0], transid);

	  switch (acode)
	    {
	    case PIOUS_EACCES:
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	    case PIOUS_EFATAL:
	      rcode = acode;
	      break;
	    default:
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      else
	{ /* metadata written; commit */

#ifdef VTCOMMITNOACK
	  acode = PDS_commit_send(pds->id[0], transid, 1);
#else
	  acode = PDS_commit(pds->id[0], transid, 1);
#endif

	  if (acode != PIOUS_OK)
	    { /* unable to commit metadata file write */
	      switch (acode)
		{
		case PIOUS_EINSUF:
		case PIOUS_ETPORT:
		case PIOUS_EFATAL:
		  rcode = acode;
		  break;
		default:
		  rcode = PIOUS_EUNXP;
		  break;
		}
	    }
	}
    }


  /* 3) remove data segment and shared pointer files */

  if (rcode == PIOUS_OK)
    { /* unlink shared pointer file */

      if ((pfcomppath = mk_comppath(pds->spath[0],
				    path, PF_SHARPTR_FNAME)) == NULL)
	rcode = PIOUS_EINSUF;

      else if ((acode = PDS_unlink(pds->id[0], CntrlIdNext,
				   pfcomppath)) != PIOUS_OK &&
	       acode != PIOUS_ENOENT)
	{ /* unable to unlink shared pointer file */

	  /* note: at this point expect to have sufficient permission to
           *       unlink, since were able to update metadata file; see
           *       default parafile component permissions.
	   */

	  switch (acode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	    case PIOUS_EFATAL:
	      rcode = acode;
	      break;
	    default:
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}


      /* unlink all parafile data segments */

      else if ((cntrlop = (struct cntrlop_state *)
		malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
				  sizeof(struct cntrlop_state)))) == NULL)
	rcode = PIOUS_EINSUF;

      else if ((pfsegpathv = (char **)
		malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
				  sizeof(char *)))) == NULL)
	rcode = PIOUS_EINSUF;

      else
	{ /* pipeline unlink requests */

	  /* ---- fill pipe ---- */

	  acode   = PIOUS_OK;
	  ds_send = ds_recv = 0;
	  sendcnt = recvcnt = 0;

	  for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	    pfsegpathv[i] = NULL;


	  for (i = 0;
	       i < mdata_buf[METADATA_PDS_CNT] && acode == PIOUS_OK; i++)
	    { /* form parafile data segment path for segment i */

	      if ((pfsegpathv[i] = mk_segpath(pds->spath[i], path, i)) == NULL)
		acode = PIOUS_EINSUF;

	      /* perform unlink on segment i */

	      else
		{
		  cntrlop[i].id = CntrlIdNext;

		  acode = PDS_unlink_send(pds->id[i], CntrlIdLast,
					  pfsegpathv[i]);

		  /* increment counters */
		  if (acode == PIOUS_OK)
		    sendcnt++;
		}
	    }


	  /* ---- pipe steady state ---- */

	  for (i = mdata_buf[METADATA_PDS_CNT];
	       i < mdata_buf[METADATA_SEG_CNT] && acode == PIOUS_OK; i++)
	    { /* receive unlink reply */

	      acode = PDS_unlink_recv(pds->id[ds_recv], cntrlop[ds_recv].id);

	      recvcnt++;
	      ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];

	      /* send next unlink request */

	      if (acode == PIOUS_OK || acode == PIOUS_ENOENT)
		{ /* form full pathname of segment i */

		  RenumberSegPath(pfsegpathv[ds_send], i);

		  /* perform unlink on segment i */

		  cntrlop[ds_send].id = CntrlIdNext;

		  acode = PDS_unlink_send(pds->id[ds_send], CntrlIdLast,
					  pfsegpathv[ds_send]);

		  /* increment counters */
		  if (acode == PIOUS_OK)
		    {
		      sendcnt++;
		      ds_send = (ds_send + 1) % mdata_buf[METADATA_PDS_CNT];
		    }
		}
	    }


	  /* ---- drain pipe ---- */

	  for (i = 0;
	       i < mdata_buf[METADATA_PDS_CNT] && (acode == PIOUS_OK ||
						   acode == PIOUS_ENOENT);
	       i++)
	    { /* receive unlink reply */

	      acode = PDS_unlink_recv(pds->id[ds_recv], cntrlop[ds_recv].id);

	      recvcnt++;
	      ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	    }


	  /* consume outstanding requests resulting from an error */

	  while (recvcnt < sendcnt)
	    { /* receive unlink reply */

	      PDS_unlink_recv(pds->id[ds_recv], cntrlop[ds_recv].id);

	      recvcnt++;
	      ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	    }

	  /* set result code if error unlinking parafile data segment files */

	  if (acode != PIOUS_OK && acode != PIOUS_ENOENT)
	    {
	      /* note: at this point expect to have sufficient permission to
	       *       unlink, since were able to update metadata file; see
	       *       default parafile component permissions.
	       */

	      switch (acode)
		{
		case PIOUS_EINSUF:
		case PIOUS_ETPORT:
		case PIOUS_EFATAL:
		  rcode = acode;
		  break;
		default:
		  rcode = PIOUS_EUNXP;
		  break;
		}
	    }

	  /* deallocate parafile data segment pathname storage */

	  for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	    if (pfsegpathv[i] != NULL)
	      free(pfsegpathv[i]);

	  free((char *)pfsegpathv);
	}

      /* deallocate extraneous storage */

      if (cntrlop != NULL)
	{
	  free((char *)cntrlop);
	  cntrlop = NULL;
	}

      if (pfcomppath != NULL)
	{
	  free(pfcomppath);
	  pfcomppath = NULL;
	}
    }


  /* 4) remove metadata file */

 PFUnlink_Step4:

  if (rcode == PIOUS_OK)
    { /* unlink metadata file */

      if ((pfcomppath = mk_comppath(pds->spath[0],
				    path, PF_METADATA_FNAME)) == NULL)
	rcode = PIOUS_EINSUF;

      else if ((acode = PDS_unlink(pds->id[0], CntrlIdNext,
				   pfcomppath)) != PIOUS_OK &&
	       acode != PIOUS_ENOENT && pfstruct_extant)
	{ /* began unlink with a complete parafile structure and are not able
	   * to unlink metadata file
	   */

	  /* note: at this point expect to have sufficient permission to
	   *       unlink, since were able to update metadata file; see
	   *       default parafile component permissions.
	   */

	  switch (acode)
	    {
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	    case PIOUS_EFATAL:
	      rcode = acode;
	      break;
	    default:
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}

      /* deallocate parafile component path storage */

      if (pfcomppath != NULL)
	{
	  free(pfcomppath);
	  pfcomppath = NULL;
	}
    }


  /* 5) remove all parafile directories */

  if (rcode == PIOUS_OK)
    {
      if (pfstruct_extant)
	{ /* remove only parafile directories on which file is declustered */
	  tmp_pds_cnt = pds->cnt;
	  pds->cnt    = mdata_buf[METADATA_PDS_CNT];

	  acode = rmdir_all(pds, path);

	  pds->cnt = tmp_pds_cnt;
	}

      else
	{ /* began with *incomplete* parafile struct; try rmdir on all pds */
	  acode = rmdir_all(pds, path);
	}

      /* set final result code; note: ability to remove parafile
       * directories is not guaranteed; determined by parent directories
       */

      switch(acode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;
	case PIOUS_ENOENT:
	  /* this can occur when re-unlinking due to a failure OR when PDS
           * share a file system.
	   */
	  rcode = PIOUS_OK;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  /* return result */
  return rcode;
}




/*
 * pf_chmod()
 *
 * Parameters:
 *
 *   pds          - data server information record
 *   path         - parafile path name
 *   mode         - parafile permission bits
 *   amode        - parafile accessability
 *
 * Set the file permission bits of parafile 'path' on data servers 'pds'
 * to the value specified by 'mode'.  Parafile accessability, after
 * performing pf_chmod(), is returned in 'amode'.
 *
 * Note: - only the modes of parafile data segments are updated
 *       - assumes the 'pds' argument is valid
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile mode successfully updated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - parafile 'path' does not exist on 'pds'
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int pf_chmod(server_infot *pds,
		    char *path,
		    pious_modet mode,
		    int *amode)
#else
static int pf_chmod(pds, path, mode, amode)
     server_infot *pds;
     char *path;
     pious_modet mode;
     int *amode;
#endif
{
  int rcode, acode;
  int i, chamode;
  int ds_send, ds_recv, sendcnt, recvcnt;
  long mdata_buf[METADATA_ENTRIES];
  pds_fhandlet mdata_fhandle;
  struct cntrlop_state *cntrlop;
  char **pfsegpathv;


  /* initialize working variables */

  cntrlop    = NULL;
  pfsegpathv = NULL;


  /* determine if parafile extant on 'pds'; essentially determine if parafile
   * structure 'path' exists on the specified data servers and is marked
   * as extant; this is an efficient heuristic but can fail if user
   * corrupts parafile structure or passes a "trojan" pds structure.
   */

  if ((acode = pf_struct_on_pds(pds, path,
				&mdata_fhandle, mdata_buf)) != PIOUS_OK)
    { /* parafile structure can not be accessed or does not exist */
      switch (acode)
	{
	case PIOUS_EACCES:
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
    }

  else if (!mdata_buf[METADATA_PF_EXTANT])
    rcode = PIOUS_ENOENT;


  /* perform chmod operation on all parafile data segments */

  else if ((cntrlop = (struct cntrlop_state *)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((pfsegpathv = (char **)
	    malloc((unsigned)(mdata_buf[METADATA_PDS_CNT] *
			      sizeof(char *)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* pipeline chmod requests */

      /* ---- fill pipe ---- */

      acode   = PIOUS_OK;
      *amode  = PIOUS_R_OK | PIOUS_W_OK;

      ds_send = ds_recv = 0;
      sendcnt = recvcnt = 0;

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	pfsegpathv[i] = NULL;


      for (i = 0; i < mdata_buf[METADATA_PDS_CNT] && acode == PIOUS_OK; i++)
	{ /* form parafile data segment path for segment i */

	  if ((pfsegpathv[i] = mk_segpath(pds->spath[i], path, i)) == NULL)
	    acode = PIOUS_EINSUF;

	  /* perform chmod on segment i */

	  else
	    {
	      cntrlop[i].id = CntrlIdNext;

	      acode = PDS_chmod_send(pds->id[i], CntrlIdLast,
				     pfsegpathv[i], mode);
	      
	      /* increment counters */
	      if (acode == PIOUS_OK)
		sendcnt++;
	    }
	}


      /* ---- pipe steady state ---- */

      for (i = mdata_buf[METADATA_PDS_CNT];
	   i < mdata_buf[METADATA_SEG_CNT] && acode == PIOUS_OK; i++)
	{ /* receive chmod reply */

	  acode = PDS_chmod_recv(pds->id[ds_recv], cntrlop[ds_recv].id,
				 &chamode);

	  /* factor segment accessability into 'amode' and incr. counters */

	  *amode &= chamode;

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];

	  /* send next chmod request */

	  if (acode == PIOUS_OK)
	    { /* form full pathname of segment i */

	      RenumberSegPath(pfsegpathv[ds_send], i);

	      /* perform chmod on segment i */

	      cntrlop[ds_send].id = CntrlIdNext;

	      acode = PDS_chmod_send(pds->id[ds_send], CntrlIdLast,
				     pfsegpathv[ds_send], mode);

	      /* increment counters */

	      if (acode == PIOUS_OK)
		{
		  sendcnt++;
		  ds_send = (ds_send + 1) % mdata_buf[METADATA_PDS_CNT];
		}
	    }
	}


      /* ---- drain pipe ---- */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT] && acode == PIOUS_OK; i++)
	{ /* receive chmod reply */

	  acode = PDS_chmod_recv(pds->id[ds_recv], cntrlop[ds_recv].id,
				 &chamode);

	  /* factor segment accessability into 'amode' and incr. counters */

	  *amode &= chamode;

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	}


      /* consume outstanding requests that may result if an error occured */

      while (recvcnt < sendcnt)
	{ /* receive chmod reply */

	  PDS_chmod_recv(pds->id[ds_recv], cntrlop[ds_recv].id, &chamode);

	  recvcnt++;
	  ds_recv = (ds_recv + 1) % mdata_buf[METADATA_PDS_CNT];
	}

      /* set final result code.  note that assumption is that parafile
       * exists on data servers and that parafile component permissions
       * are set to defaults
       */

      switch (acode)
	{
	case PIOUS_OK:
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


      /* deallocate parafile data segment pathname storage */

      for (i = 0; i < mdata_buf[METADATA_PDS_CNT]; i++)
	if (pfsegpathv[i] != NULL)
	  free(pfsegpathv[i]);

      free((char *)pfsegpathv);
    }

  /* deallocate extraneous storage */
  if (cntrlop != NULL)
    free((char *)cntrlop);

  /* return result */
  return rcode;
}




/*
 * metadata_access()
 *
 * Parameters:
 *
 *   action        - READ_DATA or WRITE_DATA
 *   pds           - data server information record
 *   path          - parafile path name
 *   mdata_fhandle - parafile metadata file handle
 *   mdata_buf     - parafile metadata contents
 *
 * Access metadata file of parafile 'path' on data servers 'pds'.
 * If 'action' specifies read, the contents of the metadata file are placed
 * in buffer 'mdata_buf'; otherwise, if the action is write, the contents
 * of 'mdata_buf' are written to the metadata file.  In either case the
 * metadata file handle is looked-up and returned in 'mdata_fhandle'.
 * If 'action' specifies write, then the metadata file is created if it
 * does not exist.
 *
 * Note: assumes 'pds' argument is valid
 *
 * Returns:
 *
 *   PIOUS_OK (0) - parafile metadata file successfully accessed
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid 'action' argument
 *       PIOUS_EACCES       - path search permission denied or file read/write
 *                            permission denied or write permission to create
 *                            file denied.
 *       PIOUS_ENOENT       - file does not exist and READ_DATA, or WRITE_DATA
 *                            and path prefix component does not exist
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a dir
 *       PIOUS_ENOTREG      - file is not a regular file
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EPERM        - read/write operation incomplete
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int metadata_access(int action,
			   server_infot *pds,
			   char *path,
			   pds_fhandlet *mdata_fhandle,
			   long *mdata_buf)
#else
static int metadata_access(action, pds, path, mdata_fhandle, mdata_buf)
     int action;
     server_infot *pds;
     char *path;
     pds_fhandlet *mdata_fhandle;
     long *mdata_buf;
#endif
{
  int rcode, acode, cflag;
  char *pfcomppath;
  int amode;
  pds_transidt transid;


  /* initialize working variables and set creation flag */

  pfcomppath = NULL;

  if (action == WRITE_DATA)
    cflag = PIOUS_CREAT;
  else
    cflag = PIOUS_NOCREAT;

  /* validate 'action' argument */

  if (action != READ_DATA && action != WRITE_DATA)
    rcode = PIOUS_EINVAL;

  /* form complete path for metadata file on low order PDS */

  else if ((pfcomppath = mk_comppath(pds->spath[0],
				     path, PF_METADATA_FNAME)) == NULL)
    rcode = PIOUS_EINSUF;

  /* obtain file handle for metadata file */

  else if ((acode = PDS_lookup(pds->id[0], CntrlIdNext,
			       pfcomppath, cflag, PF_METADATA_PERM,
			       mdata_fhandle, &amode)) != PIOUS_OK)
    { /* unable to obtain file handle for metadata file */
      switch (acode)
	{
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_ENOTREG:
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
    }

  /* access (read/write) metadata file */

  else if (transid_assign(&transid) != PIOUS_OK)
    /* unable to obtain a transaction id for accessing metadata file */
    rcode = PIOUS_EUNXP;

  else
    {
      if (action == READ_DATA)
	acode = PDS_read_sint(pds->id[0], transid, 0,
			      *mdata_fhandle,
			      (pious_offt)0,
			      METADATA_ENTRIES,
			      mdata_buf);
      else
	acode = PDS_write_sint(pds->id[0], transid, 0,
			       *mdata_fhandle,
			       (pious_offt)0,
			       METADATA_ENTRIES,
			       mdata_buf);

      if (acode != METADATA_ENTRIES)
	{ /* unable to read/write all parafile metadata */

	  if (acode != PIOUS_EABORT)
	    PDS_abort(pds->id[0], transid);

	  if (acode >= 0)
	    /* incomplete metadata file */
	    rcode = PIOUS_EPERM;
	  else
	    /* error reading metadata file */
	    switch (acode)
	      {
	      case PIOUS_EACCES:
	      case PIOUS_EINSUF:
	      case PIOUS_ETPORT:
	      case PIOUS_EFATAL:
		rcode = acode;
		break;
	      default:
		rcode = PIOUS_EUNXP;
		break;
	      }
	}

      else
	{ /* metadata read/written; commit */

#ifdef VTCOMMITNOACK
	  acode = PDS_commit_send(pds->id[0], transid, 1);
#else
	  acode = PDS_commit(pds->id[0], transid, 1);
#endif

	  switch (acode)
	    { /* transaction committed */
	    case PIOUS_OK:

	      /* unable to commit read/write transaction on metadata */
	    case PIOUS_EINSUF:
	    case PIOUS_ETPORT:
	    case PIOUS_EFATAL:
	      rcode = acode;
	      break;
	    default:
	      rcode = PIOUS_EUNXP;
	      break;
	    }
	}
    }

  /* deallocate parafile component pathname storage */
  if (pfcomppath != NULL)
    free(pfcomppath);

  /* return result */
  return rcode;
}




/*
 * mkdir_all()
 *
 * Parameters:
 *
 *   pds  - data server information record
 *   path - directory file path name
 *   mode - file permission bits
 *
 * For all data servers specified by 'pds', create directory file 'path'
 * with access control bits set according to the value of 'mode'.
 *
 * Note: assumes 'pds' argument is valid.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully created on all PDS
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EEXIST       - named file exists
 *       PIOUS_ENOENT       - path prefix component does not exist
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int mkdir_all(server_infot *pds,
		     char *path,
		     pious_modet mode)
#else
static int mkdir_all(pds, path, mode)
     server_infot *pds;
     char *path;
     pious_modet mode;
#endif
{
  int mkcode, rcode, i;
  struct cntrlop_state *cntrlop;
  char *dirpath;

  /* allocate storage for PDS control op state */

  if ((cntrlop = (struct cntrlop_state *)
       malloc((unsigned)(pds->cnt * sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* send mkdir requests */

      for (i = 0; i < pds->cnt; i++)
	if ((dirpath = mk_dirpath(pds->spath[i], path)) == NULL)
	  cntrlop[i].code = PIOUS_EINSUF;
	else
	  {
	    cntrlop[i].id   = CntrlIdNext;
	    cntrlop[i].code = PDS_mkdir_send(pds->id[i], CntrlIdLast,
					     dirpath, mode);

	    free(dirpath);
	  }

      /* receive mkdir replys */

      for (i = 0; i < pds->cnt; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  cntrlop[i].code = PDS_mkdir_recv(pds->id[i], cntrlop[i].id);

      /* set result code */

      mkcode = PIOUS_OK;

      for (i = 0;
	   i < pds->cnt && (mkcode == PIOUS_OK || mkcode == PIOUS_EEXIST); i++)
	/* note: PIOUS_EEXIST is the only "acceptable" error */
	if (cntrlop[i].code != PIOUS_OK)
	  mkcode = cntrlop[i].code;

      switch(mkcode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
	case PIOUS_EEXIST:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_ENOSPC:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = mkcode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */

      free((char *)cntrlop);
    }

  return rcode;
}




/*
 * rmdir_all()
 *
 * Parameters:
 *
 *   pds  - data server information record
 *   path - directory file path name
 *
 * For all data servers specified by 'pds', remove directory file 'path'
 * if empty.
 *
 * Note: assumes 'pds' argument is valid.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file successfully removed from all PDS
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission or write permission denied
 *       PIOUS_EBUSY        - directory in use
 *       PIOUS_ENOTEMPTY    - directory not empty
 *       PIOUS_ENOENT       - directory file does not exist
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path is not a directory
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int rmdir_all(server_infot *pds,
		     char *path)
#else
static int rmdir_all(pds, path)
     server_infot *pds;
     char *path;
#endif
{
  int rmcode, rcode, i;
  struct cntrlop_state *cntrlop;
  char *dirpath;

  /* allocate storage for PDS control op state */

  if ((cntrlop = (struct cntrlop_state *)
       malloc((unsigned)(pds->cnt * sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* send rmdir requests */

      for (i = 0; i < pds->cnt; i++)
	if ((dirpath = mk_dirpath(pds->spath[i], path)) == NULL)
	  cntrlop[i].code = PIOUS_EINSUF;
	else
	  {
	    cntrlop[i].id   = CntrlIdNext;
	    cntrlop[i].code = PDS_rmdir_send(pds->id[i], CntrlIdLast, dirpath);

	    free(dirpath);
	  }

      /* receive rmdir replys */

      for (i = 0; i < pds->cnt; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  cntrlop[i].code = PDS_rmdir_recv(pds->id[i], cntrlop[i].id);

      /* set result code */

      rmcode = PIOUS_OK;

      for (i = 0;
	   i < pds->cnt && (rmcode == PIOUS_OK || rmcode == PIOUS_ENOENT); i++)
	/* note: PIOUS_ENOENT is the only "acceptable" error */
	if (cntrlop[i].code != PIOUS_OK)
	  rmcode = cntrlop[i].code;

      switch(rmcode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
	case PIOUS_EBUSY:
	case PIOUS_ENOTEMPTY:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = rmcode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}

      /* deallocate extraneous storage */

      free((char *)cntrlop);
    }

  return rcode;
}




/*
 * chmoddir_all()
 *
 * Parameters:
 *
 *   pds  - data server information record
 *   path - directory file path name
 *   mode - directory file permission bits
 *
 * For all data servers specified by 'pds', set the file permission bits of
 * directory file 'path' to the value specified by 'mode'.
 *
 * Note: assumes 'pds' argument is valid.
 *
 *
 * Returns:
 *
 *   PIOUS_OK (0) - directory file mode successfully updated on all PDS
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path not a directory
 *       PIOUS_EPERM        - insufficient privileges to complete operation
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int chmoddir_all(server_infot *pds,
			char *path,
			pious_modet mode)
#else
static int chmoddir_all(pds, path, mode)
     server_infot *pds;
     char *path;
     pious_modet mode;
#endif
{
  int chcode, rcode, i;
  int amode;
  pious_modet *modev;
  struct cntrlop_state *cntrlop;
  char *dirpath;

  /* alloc storage for PDS cntrl op state / file mode */

  modev   = NULL;
  cntrlop = NULL;

  if ((cntrlop = (struct cntrlop_state *)
       malloc((unsigned)(pds->cnt * sizeof(struct cntrlop_state)))) == NULL)
    rcode = PIOUS_EINSUF;

  else if ((modev = (pious_modet *)
	    malloc((unsigned)(pds->cnt * sizeof(pious_modet)))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    { /* send stat requests to determine if named file is a directory */

      for (i = 0; i < pds->cnt; i++)
	{ /* form directory pathname */

	  if ((dirpath = mk_dirpath(pds->spath[i], path)) == NULL)
	    cntrlop[i].code = PIOUS_EINSUF;

	  /* check file status */

	  else
	    {
	      cntrlop[i].id   = CntrlIdNext;
	      cntrlop[i].code = PDS_stat_send(pds->id[i], CntrlIdLast,
					      dirpath);

	      /* deallocate dirpath storage */
	      free(dirpath);
	      dirpath = NULL;
	    }
	}


      /* receive stat replys / send chmod requests */

      for (i = 0; i < pds->cnt; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  if ((cntrlop[i].code = PDS_stat_recv(pds->id[i],
					       cntrlop[i].id,
					       &modev[i])) == PIOUS_OK)
	    { /* successfully received stat reply, send chmod if file is dir */

	      if (!(modev[i] & PIOUS_ISDIR))
		{ /* named file is not a directory */
		  cntrlop[i].code = PIOUS_ENOTDIR;
		}

	      else
		{ /* form directory pathname */

		  if ((dirpath = mk_dirpath(pds->spath[i], path)) == NULL)
		    cntrlop[i].code = PIOUS_EINSUF;

		  /* send chmod request for directory file */

		  else
		    {
		      cntrlop[i].id   = CntrlIdNext;
		      cntrlop[i].code = PDS_chmod_send(pds->id[i], CntrlIdLast,
						       dirpath,
						       mode);

		      /* deallocate dirpath storage */
		      free(dirpath);
		      dirpath = NULL;
		    }
		}
	    }


      /* receive chmod replys */

      for (i = 0; i < pds->cnt; i++)
	if (cntrlop[i].code == PIOUS_OK)
	  cntrlop[i].code = PDS_chmod_recv(pds->id[i], cntrlop[i].id, &amode);

      /* set result code -- derived from union of stat and chmod results */

      chcode = PIOUS_OK;

      for (i = 0; i < pds->cnt && chcode == PIOUS_OK; i++)
	if (cntrlop[i].code != PIOUS_OK)
	  chcode = cntrlop[i].code;

      switch (chcode)
	{
	case PIOUS_OK:
	case PIOUS_EACCES:
	case PIOUS_ENOENT:
	case PIOUS_ENAMETOOLONG:
	case PIOUS_ENOTDIR:
	case PIOUS_EPERM:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = chcode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  /* deallocate extraneous storage */

  if (modev != NULL)
    free((char *)modev);

  if (cntrlop != NULL)
    free((char *)cntrlop);

  /* return result */
  return rcode;
}




/*
 * spawn_servers()
 *
 * Parameters:
 *
 *   pds - data server information record
 *
 * Spawn a PIOUS data server on any of the specified hosts in 'pds'
 * for which a data server has not previously been spawned.  The PDS message
 * passing id for each data server is returned in the 'id' field of 'pds'.
 *
 * Note: assumes 'pds' argument is valid.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - servers spawned on all hosts
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid host, or unable to locate PDS executable
 *       PIOUS_EINSUF       - insufficient system resources to complete
 *       PIOUS_ETPORT       - error in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 */

#ifdef __STDC__
static int spawn_servers(server_infot *pds)
#else
static int spawn_servers(pds)
     server_infot *pds;
#endif
{
  int rcode, scode, i;

  /* spawn data servers if not already extant */
  rcode = PIOUS_OK;

  for (i = 0; i < pds->cnt && rcode == PIOUS_OK; i++)
    if ((scode = SM_add_pds(pds->hname[i],
			    pds->lpath[i],
			    &pds->id[i])) != PIOUS_OK)
      { /* unable to spawn data server */
	switch(scode)
	  {
	  case PIOUS_EINVAL:
	  case PIOUS_EINSUF:
	  case PIOUS_ETPORT:
	    rcode = scode;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
      }

  return rcode;
}




/*
 * sptr_zero()
 *
 * Parameters:
 *
 *   pdsid   - PDS id
 *   fhandle - shared pointer file fhandle
 *   offset  - integer file index
 *
 * Write zero (0) to the shared pointer file 'fhandle' on data server 'pdsid'
 * at index position 'offset'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - shared pointer file successfully written
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ESRCDEST     - invalid 'pdsid' argument
 *       PIOUS_EBADF        - invalid 'fhandle' argument
 *       PIOUS_EINVAL       - invalid 'offset' argument
 *       PIOUS_EACCES       - file write permission denied
 *       PIOUS_EINSUF       - insufficient system resources to complete; retry
 *       PIOUS_ETPORT       - error condition in underlying transport system
 *       PIOUS_EUNXP        - unexpected error condition encountered
 *       PIOUS_EFATAL       - fatal error; check PDS error log
 */

#ifdef __STDC__
static int sptr_zero(dce_srcdestt pdsid,
		     pds_fhandlet fhandle,
		     pious_offt offset)
#else
static int sptr_zero(pdsid, fhandle, offset)
     dce_srcdestt pdsid;
     pds_fhandlet fhandle;
     pious_offt offset;
#endif
{
  int rcode, acode;
  long buf;
  pds_transidt transid;

  /* write zero to shared pointer file at specified offset */

  buf = 0;

  if (transid_assign(&transid) != PIOUS_OK)
    /* unable to obtain a transaction id for accessing file */
    rcode = PIOUS_EUNXP;

  else if ((acode = PDS_write_sint(pdsid, transid, 0,
				   fhandle,
				   offset,
				   1,
				   &buf)) != 1)
    { /* unable to write shared pointer file */

      if (acode != PIOUS_EABORT)
	PDS_abort(pdsid, transid);

      switch (acode)
	{
	case PIOUS_EBADF:
	case PIOUS_EACCES:
	case PIOUS_ESRCDEST:
	case PIOUS_EINVAL:
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  else
    { /* shared pointer file written; commit */

#ifdef VTCOMMITNOACK
      acode = PDS_commit_send(pdsid, transid, 1);
#else
      acode = PDS_commit(pdsid, transid, 1);
#endif

      switch (acode)
	{ /* transaction committed */
	case PIOUS_OK:

	  /* unable to commit write transaction */
	case PIOUS_EINSUF:
	case PIOUS_ETPORT:
	case PIOUS_EFATAL:
	  rcode = acode;
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  /* return result */
  return rcode;
}




/*
 * request_eq()
 *
 * Parameters:
 *
 *   reqop1  - request operation type 1
 *   reqmsg1 - request message 1
 *   reqop2  - request operation type 2
 *   reqmsg2 - request message 2
 *
 * Determine if two service requests are identical.
 *
 * Returns:
 *
 *   TRUE  - requests are identical
 *   FALSE - requests are NOT identical
 */

#ifdef __STDC__
static int request_eq(int reqop1,
		      register pscmsg_reqt *reqmsg1,
		      int reqop2,
		      register pscmsg_reqt *reqmsg2)
#else
static int request_eq(reqop1, reqmsg1, reqop2, reqmsg2)
     int reqop1;
     register pscmsg_reqt *reqmsg1;
     int reqop2;
     register pscmsg_reqt *reqmsg2;
#endif
{
  int rcode, i;

  /* check operation type */

  if (reqop1 != reqop2)
    rcode = FALSE;

  /* check message contents */

  else
    {
      rcode = TRUE;

      if (PscServerOp(reqop1))
	{ /* "server" version of operation, check PDS information */

	  if (reqmsg1->pds_cnt != reqmsg2->pds_cnt)
	    rcode = FALSE;

	  else
	    for (i = 0; i < reqmsg1->pds_cnt && rcode == TRUE; i++)
	      if (strcmp(reqmsg1->pds_hname[i], reqmsg2->pds_hname[i]) ||
		  strcmp(reqmsg1->pds_spath[i], reqmsg2->pds_spath[i]) ||
		  strcmp(reqmsg1->pds_lpath[i], reqmsg2->pds_lpath[i]))
		rcode = FALSE;
	}


      if (rcode == TRUE)
	{ /* check message contents based on request type */

	  switch(reqop1)
	    {
	    case PSC_OPEN_OP:
	    case PSC_SOPEN_OP:
	      if (reqmsg1->body.open.view      != reqmsg2->body.open.view    ||
		  reqmsg1->body.open.oflag     != reqmsg2->body.open.oflag   ||

		  reqmsg1->body.open.faultmode !=
		  reqmsg2->body.open.faultmode                               ||

		  strcmp(reqmsg1->body.open.group, reqmsg2->body.open.group) ||
		  strcmp(reqmsg1->body.open.path,  reqmsg2->body.open.path))
		rcode = FALSE;

	      else if (reqmsg1->body.open.oflag & PIOUS_CREAT)
		/* mode and seg only defined if creat specified */
		if (reqmsg1->body.open.mode != reqmsg2->body.open.mode ||
		    reqmsg1->body.open.seg  != reqmsg2->body.open.seg)
		  rcode = FALSE;
	      break;

	    case PSC_CLOSE_OP:
	    case PSC_SCLOSE_OP:
	      if (strcmp(reqmsg1->body.close.group,
			 reqmsg2->body.close.group) ||

		  strcmp(reqmsg1->body.close.path,
			 reqmsg2->body.close.path))
		rcode = FALSE;
	      break;

	    case PSC_CHMOD_OP:
	    case PSC_SCHMOD_OP:
	      if (reqmsg1->body.chmod.mode != reqmsg2->body.chmod.mode ||
		  strcmp(reqmsg1->body.chmod.path, reqmsg2->body.chmod.path))
		rcode = FALSE;
	      break;

	    case PSC_UNLINK_OP:
	    case PSC_SUNLINK_OP:
	      if (strcmp(reqmsg1->body.unlink.path, reqmsg2->body.unlink.path))
		rcode = FALSE;
	      break;

	    case PSC_MKDIR_OP:
	    case PSC_SMKDIR_OP:
	      if (reqmsg1->body.mkdir.mode != reqmsg2->body.mkdir.mode ||
		  strcmp(reqmsg1->body.mkdir.path, reqmsg2->body.mkdir.path))
		rcode = FALSE;
	      break;

	    case PSC_RMDIR_OP:
	    case PSC_SRMDIR_OP:
	      if (strcmp(reqmsg1->body.rmdir.path, reqmsg2->body.rmdir.path))
		rcode = FALSE;
	      break;

	    case PSC_CHMODDIR_OP:
	    case PSC_SCHMODDIR_OP:
	      if (reqmsg1->body.chmoddir.mode != reqmsg2->body.chmoddir.mode ||
		  strcmp(reqmsg1->body.chmoddir.path,
			 reqmsg2->body.chmoddir.path))
		rcode = FALSE;
	      break;

	    case PSC_CONFIG_OP:
	      break;

	    case PSC_SHUTDOWN_OP:
	      break;
	    }
	}
    }

  return rcode;
}




/*
 * dsinfo_alloc()
 *
 * Parameters:
 *
 *   cnt - PDS host count
 *
 * Allocate a data server information record with a message passing id
 * array of 'cnt' elements.
 *
 * NOTE: storage is NOT allocated for 'hname', 'spath', or 'lpath'.
 *
 * Returns:
 *
 *   server_infot * - pointer to (partial) data server information record
 *   NULL           - can not allocate required storage
 */

#ifdef __STDC__
static server_infot *dsinfo_alloc(int cnt)
#else
static server_infot *dsinfo_alloc(cnt)
     int cnt;
#endif
{
  server_infot *pds;

  /* allocate (partial) data server information record */

  if ((pds = (server_infot *)malloc(sizeof(server_infot))) != NULL)
    if ((pds->id = (dce_srcdestt *)
	 malloc((unsigned)(cnt * sizeof(dce_srcdestt)))) == NULL)
      { /* unable to allocate all storage */
	free((char *)pds);
	pds = NULL;
      }

  return pds;
}




/*
 * cmpinfo_alloc()
 *
 * Parameters:
 *
 *   cnt - parafile data segment count
 *
 * Allocate a parafile component information record with a data segment
 * file handle array of 'cnt' elements.
 *
 * Returns:
 *
 *   cmp_infot * - pointer to parafile component information record
 *   NULL        - can not allocate required storage
 */

#ifdef __STDC__
static cmp_infot *cmpinfo_alloc(int cnt)
#else
static cmp_infot *cmpinfo_alloc(cnt)
     int cnt;
#endif
{
  cmp_infot *cmp;

  /* allocate parafile component information record */

  if ((cmp = (cmp_infot *)malloc(sizeof(cmp_infot))) != NULL)
    if ((cmp->seg_fhandle = (pds_fhandlet *)
	 malloc((unsigned)(cnt * sizeof(pds_fhandlet)))) == NULL)
      { /* unable to allocate all storage */
	free((char *)cmp);
	cmp = NULL;
      }

  return cmp;
}




/*
 * gpinfo_alloc()
 *
 * Parameters:
 *
 *   len - group name string length
 *
 * Allocate a parafile group information record with a group name component
 * of ('len' + 1) elements.
 *
 * Returns:
 *
 *   gp_infot * - pointer to parafile group information record
 *   NULL       - can not allocate required storage
 */

#ifdef __STDC__
static gp_infot *gpinfo_alloc(int len)
#else
static gp_infot *gpinfo_alloc(len)
     int len;
#endif
{
  gp_infot *gp;

  /* allocate parafile group information record */

  if ((gp = (gp_infot *)malloc(sizeof(gp_infot))) != NULL)
    if ((gp->group = (char *)malloc((unsigned)(len + 1))) == NULL)
      { /* unable to allocate all storage */
	free((char *)gp);
	gp = NULL;
      }

  return gp;
}




/*
 * dsinfo_cpy()
 *
 * Parameters:
 *
 *   cnt   - PDS count
 *   hname - PDS host names
 *   spath - PDS search root paths
 *   lpath - PDS log directory paths
 *
 * Allocate a complete data server information record and initialize with
 * the values 'cnt', 'hname', 'spath', 'lpath'; the message passing id
 * array 'id' is allocated but NOT initialized.
 *
 * Returns:
 *
 *   server_infot * - pointer to initialized data server information record
 *   NULL           - can not allocate required storage
 */

#ifdef __STDC__
static server_infot *dsinfo_cpy(int cnt,
				char **hname,
				char **spath,
				char **lpath)
#else
static server_infot *dsinfo_cpy(cnt, hname, spath, lpath)
     int cnt;
     char **hname;
     char **spath;
     char **lpath;
#endif
{
  int i, failed;
  register server_infot *pds;

  /* allocate a data server information record */

  if ((pds = (server_infot *)malloc(sizeof(server_infot))) != NULL)
    { /* allocate hname, spath, lpath, id pointer storage */
      pds->hname = pds->spath = pds->lpath = NULL;
      pds->id    = NULL;

      failed     = FALSE;

      if ((pds->hname = (char **)
	   malloc((unsigned)(cnt * sizeof(char *)))) == NULL ||

	  (pds->spath = (char **)
	   malloc((unsigned)(cnt * sizeof(char *)))) == NULL ||

	  (pds->lpath = (char **)
	   malloc((unsigned)(cnt * sizeof(char *)))) == NULL ||

	  (pds->id = (dce_srcdestt *)
	   malloc((unsigned)(cnt * sizeof(dce_srcdestt)))) == NULL)

	/* unable to allocate all storage */
	failed = TRUE;

      else
	{ /* set hname, spath, lpath char ptrs NULL so can dealloc if fail */

	  for (i = 0; i < cnt; i++)
	    pds->hname[i] = pds->spath[i] = pds->lpath[i] = NULL;


	  /* allocate and initialize data server information */

	  pds->cnt = cnt;

	  for (i = 0; i < cnt && !failed; i++)
	    if ((pds->hname[i] =
		 malloc((unsigned)(strlen(hname[i]) + 1))) == NULL ||

		(pds->spath[i] =
		 malloc((unsigned)(strlen(spath[i]) + 1))) == NULL ||

		(pds->lpath[i] =
		 malloc((unsigned)(strlen(lpath[i]) + 1))) == NULL)

	      { /* unable to allocate all storage */
		failed = TRUE;
	      }

	    else
	      { /* allocated storage, copy in data */
		strcpy(pds->hname[i], hname[i]);
		strcpy(pds->spath[i], spath[i]);
		strcpy(pds->lpath[i], lpath[i]);
	      }

	  /* deallocate string storage if failure */

	  if (failed)
	    for (i = 0; i < cnt; i++)
	      {
		if (pds->hname[i] != NULL)
		  free(pds->hname[i]);

		if (pds->spath[i] != NULL)
		  free(pds->spath[i]);

		if (pds->lpath[i] != NULL)
		  free(pds->lpath[i]);
	      }
	}


      /* if failed free hname, spath, lpath, id pointer storage */

      if (failed)
	{
	  if (pds->hname != NULL)
	    free((char *)pds->hname);

	  if (pds->spath != NULL)
	    free((char *)pds->spath);

	  if (pds->lpath != NULL)
	    free((char *)pds->lpath);

	  if (pds->id != NULL)
	    free((char *)pds->id);

	  free((char *)pds);
	  pds = NULL;
	}
    }

  return pds;
}




/*
 * req_dealloc()
 *
 * Parameters:
 *
 *   reqop  - request operation type
 *   reqmsg - request message
 *
 * Deallocate extraneous storage in the service request message 'reqmsg'.
 * Note that 'reqmsg' itself is not deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void req_dealloc(int reqop,
			pscmsg_reqt *reqmsg)
#else
static void req_dealloc(reqop, reqmsg)
     int reqop;
     pscmsg_reqt *reqmsg;
#endif
{
  int i;

  /* deallocate PDS server info if request is a "server" version */

  if (PscServerOp(reqop))
    { /* dealloc string storage */
      for (i = 0; i < reqmsg->pds_cnt; i++)
	{
	  free(reqmsg->pds_hname[i]);
	  free(reqmsg->pds_spath[i]);
	  free(reqmsg->pds_lpath[i]);
	}

      /* dealloc string pointer storage */
      if (reqmsg->pds_cnt > 0)
	{
	  free((char *)reqmsg->pds_hname);
	  free((char *)reqmsg->pds_spath);
	  free((char *)reqmsg->pds_lpath);
	}

      reqmsg->pds_cnt   = 0;
      reqmsg->pds_hname = reqmsg->pds_spath = reqmsg->pds_lpath = NULL;
    }

  /* deallocate remaining storage based on request type */

  switch(reqop)
    {
    case PSC_OPEN_OP:
    case PSC_SOPEN_OP:
      free(reqmsg->body.open.group);
      free(reqmsg->body.open.path);
      reqmsg->body.open.group = reqmsg->body.open.path = NULL;
      break;

    case PSC_CLOSE_OP:
    case PSC_SCLOSE_OP:
      free(reqmsg->body.close.group);
      free(reqmsg->body.close.path);
      reqmsg->body.close.group = reqmsg->body.close.path = NULL;
      break;

    case PSC_CHMOD_OP:
    case PSC_SCHMOD_OP:
      free(reqmsg->body.chmod.path);
      reqmsg->body.chmod.path = NULL;
      break;

    case PSC_UNLINK_OP:
    case PSC_SUNLINK_OP:
      free(reqmsg->body.unlink.path);
      reqmsg->body.unlink.path = NULL;
      break;

    case PSC_MKDIR_OP:
    case PSC_SMKDIR_OP:
      free(reqmsg->body.mkdir.path);
      reqmsg->body.mkdir.path = NULL;
      break;

    case PSC_RMDIR_OP:
    case PSC_SRMDIR_OP:
      free(reqmsg->body.rmdir.path);
      reqmsg->body.rmdir.path = NULL;
      break;

    case PSC_CHMODDIR_OP:
    case PSC_SCHMODDIR_OP:
      free(reqmsg->body.chmoddir.path);
      reqmsg->body.chmoddir.path = NULL;
      break;
    }
}




/*
 * reply_dealloc()
 *
 * Parameters:
 *
 *   replyop  - replied operation type
 *   replymsg - reply message
 *
 * Deallocate extraneous storage in the reply message 'replymsg'. Note that
 * 'replymsg' itself is not deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void reply_dealloc(int replyop,
			  pscmsg_replyt *replymsg)
#else
static void reply_dealloc(replyop, replymsg)
     int replyop;
     pscmsg_replyt *replymsg;
#endif
{ /* note: having a function with a single item switch is not efficient, but
   *       it goes a long way towards making the PSC control structure
   *       easy to maintain and expand.
   */

  /* deallocate storage based on reply type */

  switch(replyop)
    {
    case PSC_OPEN_OP:
    case PSC_SOPEN_OP:
      /* no dealloc; storage shared with file table entry (see PSC_open()) */
      replymsg->body.open.seg_fhandle = NULL;
      replymsg->body.open.pds_id      = NULL;
      break;
    }
}




/*
 * pftable_dealloc()
 *
 * Parameters:
 *
 *   ftable - parafile file table entry
 *
 * Deallocate extraneous storage in the parafile file table entry 'ftable'
 * and mark entry as invalid. Note that 'ftable' itself is not deallocated.
 *
 * Returns:
 */

#ifdef __STDC__
static void pftable_dealloc(register pftable_entryt *ftable)
#else
static void pftable_dealloc(ftable)
     register pftable_entryt *ftable;
#endif
{
  int i;
  register gp_infot *gp;
  gp_infot *gp_next;

  /* presumes a potentially partial ftable entry due to a failed PSC_open() */


  /* deallocate parafile host directory paths */

  if (ftable->hostdirpath != NULL)
    { /* note: due to alloc order in PSC_open(), know that cmpinfo extant */
      for (i = 0; i < ftable->cmpinfo->pds_cnt; i++)
	if (ftable->hostdirpath[i] != NULL)
	  free(ftable->hostdirpath[i]);

      free((char *)ftable->hostdirpath);

      ftable->hostdirpath = NULL;
    }


  /* deallocate parafile group information */

  if (ftable->gpinfo != NULL)
    {
      gp = ftable->gpinfo;

      while (gp != NULL)
	{
	  gp_next = gp->next;

	  free(gp->group);
	  free((char *)gp);

	  gp = gp_next;
	}

      ftable->gpinfo = NULL;
    }


  /* deallocate parafile component information */

  if (ftable->cmpinfo != NULL)
    {
      free((char *)ftable->cmpinfo->seg_fhandle);
      free((char *)ftable->cmpinfo);

      ftable->cmpinfo = NULL;
    }


  /* deallocate parafile data server information */

  if (ftable->dsinfo != NULL)
    { /* do NOT deallocate default data server information record */

      if (ftable->dsinfo != &def_pds)
	{ /* deallocate string storage */

	  for (i = 0; i < ftable->dsinfo->cnt; i++)
	    {
	      free(ftable->dsinfo->hname[i]);
	      free(ftable->dsinfo->spath[i]);
	      free(ftable->dsinfo->lpath[i]);
	    }

	  /* deallocate pointer storage */

	  free((char *)ftable->dsinfo->hname);
	  free((char *)ftable->dsinfo->spath);
	  free((char *)ftable->dsinfo->lpath);
	  free((char *)ftable->dsinfo->id);

	  /* deallocate data server info record */
	  free((char *)ftable->dsinfo);
	}

      ftable->dsinfo = NULL;
    }


  /* deallocate parafile path storage */

  if (ftable->path != NULL)
    {
      free(ftable->path);

      ftable->path = NULL;
    }


  /* invalidate pftable entry */

  ftable->valid = FALSE;
}




/*
 * mk_dirpath()
 *
 * Parameters:
 *
 *   prefix   - prefix string
 *   pathname - parafile directory pathname string
 *
 * Allocate storage and concatenate 'prefix' and 'pathname' strings, in
 * that order, separated by a '/' character.
 *
 * Returns:
 *
 *   char * - pointer to resultant string
 *   NULL   - can not allocate required storage
 */

#ifdef __STDC__
static char *mk_dirpath(char *prefix,
			char *pathname)
#else
static char *mk_dirpath(prefix, pathname)
     char *prefix;
     char *pathname;
#endif
{
  char *path;

  if ((path = malloc((unsigned)
		     (strlen(prefix) + strlen(pathname) + 2))) != NULL)
    sprintf(path, "%s/%s", prefix, pathname);

  return path;
}




/*
 * mk_comppath()
 *
 * Parameters:
 *
 *   prefix   - prefix string
 *   pathname - parafile directory pathname string
 *   compname - parafile component name string
 *
 * Allocate storage and concatenate 'prefix', 'pathname', and 'compname'
 * strings, in that order, separated by a '/' character.
 *
 * Returns:
 *
 *   char * - pointer to resultant string
 *   NULL   - can not allocate required storage
 */

#ifdef __STDC__
static char *mk_comppath(char *prefix,
			 char *pathname,
			 char *compname)
#else
static char *mk_comppath(prefix, pathname, compname)
     char *prefix;
     char *pathname;
     char *compname;
#endif
{
  char *path;

  if ((path = malloc((unsigned)
		     (strlen(prefix) + strlen(pathname) +
		      strlen(compname) + 3))) != NULL)
    sprintf(path, "%s/%s/%s", prefix, pathname, compname);

  return path;
}




/*
 * mk_segpath()
 *
 * Parameters:
 *
 *   prefix   - prefix string
 *   pathname - parafile directory pathname string
 *   segnmbr  - segment number
 *
 * Allocate storage and form the pathname for parafile data segment number
 * 'segnmbr' in the parafile directory 'pathname' using the prefix
 * string 'prefix'.
 *
 * Returns:
 *
 *   char * - pointer to resultant string
 *   NULL   - can not allocate required storage
 */

#ifdef __STDC__
static char *mk_segpath(char *prefix,
			char *pathname,
			int segnmbr)
#else
static char *mk_segpath(prefix, pathname, segnmbr)
     char *prefix;
     char *pathname;
     int segnmbr;
#endif
{
  char *path;

  if ((path = malloc((unsigned)
		     (strlen(prefix) + strlen(pathname) +
		      strlen(PF_SEGMENT_FNAME) +
		      PIOUS_SCALAR_DIGITS_MAX + 4))) != NULL)
    sprintf(path, "%s/%s/%s.%d", prefix, pathname, PF_SEGMENT_FNAME, segnmbr);

  return path;
}




/*
 * mk_hostdirpath()
 *
 * Parameters:
 *
 *   hostname - host name
 *   prefix   - prefix string
 *   pathname - parafile directory pathname string
 *
 * Allocate storage and form the parafile directory path for file 'pathname'
 * on host 'hostname' using the prefix string 'prefix'.
 *
 * Note: this function forms paths for file table lookup, not actual file
 *       access; hence the path includes the host name. furthermore, multiple
 *       successive '/' characters in prefix and pathname, which normally do
 *       not effect pathname resolution, are replaced by a single '/' character
 *       for proper string comparison.
 *
 * Returns:
 *
 *   char * - pointer to resultant string
 *   NULL   - can not allocate required storage
 */

#ifdef __STDC__
static char *mk_hostdirpath(char *hostname,
			    char *prefix,
			    char *pathname)
#else
static char *mk_hostdirpath(hostname, prefix, pathname)
     char *hostname;
     char *prefix;
     char *pathname;
#endif
{
  char *path;
  int hnamelen;
  register char *full, *strip;

  if ((path = malloc((unsigned)((hnamelen = strlen(hostname)) +
				strlen(prefix) +
				strlen(pathname) + 3))) != NULL)
    { /* form complete string */
      sprintf(path, "%s:%s/%s", hostname, prefix, pathname);

      /* strip multiple successive '/' chars from 'prefix' and 'pathname' */
      
      full = strip = (path + hnamelen + 1);

      while (*strip++ = *full)
	if (*full++ == '/')
	  while (*full == '/')
	    full++;
    }

  return path;
}
