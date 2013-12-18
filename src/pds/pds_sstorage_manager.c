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




/* PIOUS Data Server (PDS): Stable Storage Manager
 *
 * @(#)pds_sstorage_manager.c	2.2  28 Apr 1995  Moyer
 *
 * The pds_sstorage_manager exports all functions necessary for accessing,
 * querying, updating, and maintaining stable storage.
 *
 * Function Summary:
 *
 *   SS_init();
 *
 *   SS_lookup();
 *   SS_read();
 *   SS_write();
 *   SS_faccess();
 *   SS_stat();
 *   SS_rename();    [not implemented]
 *   SS_chmod();
 *   SS_unlink();
 *
 *   SS_mkdir();
 *   SS_rmdir();
 *
 *   SS_logread();
 *   SS_logwrite();
 *   SS_logsync();
 *   SS_logtrunc();
 *
 *   SS_errlog();
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) For more flexibility in a heterogeneous environment, values in the
 *      FHDB should be recorded in XDR (or some other universal data format).
 *
 *   2) SS_rename() is not yet implemented.  Note that in renaming directories,
 *      it will be necessary to truncate the FHDB and invalidate the FIC to
 *      force lookups on what will now be stale outstanding file handles;
 *      to do so requires the TLOG to be truncated as well, i.e. this a full
 *      check-point situation.  Not performing these operations potentially
 *      results in invalid (pathname, fhandle) mappings in the FHDB after
 *      directory renaming.
 *
 *   3) SS_unlink() can result in an inconsistent FHDB if failure occurs
 *      between physical and logical file removal; see discussion there.
 *      This is one reason the FHDB is always truncated during initialization
 *      if recovery is not required; the other is to simply keep the FHDB
 *      as short as possible.
 *
 *      An unfortunate side-effect is that recovery will have to handle
 *      this situation.  It may be best to update SS_read/write() system
 *      call to indicate this situation to the recovery manager.
 *
 *   4) WARNING: SS_logtrunc() has been hacked to prevent truncation of the
 *               FHDB.  Thus file handles that have been looked up will not
 *               go stale until the PDS is shutdown and later restarted;
 *               this behavior has been documented in pds/pds.h.  The result
 *               is that the PIOUS service coordinator and library routines
 *               are easier to implement; both may require update if this
 *               behavior is changed.  Note that truncation of the FHDB is
 *               only required for long-term continuous operation.
 *
 *               fhandle_db_write() has been hacked to avoid forced writes
 *               to the FHDB.  Thus initial file handle lookup (SS_lookup())
 *               and file unlinking (SS_unlink()) will have much improved
 *               performance.  Synchronous writes to the FHDB are only
 *               required for recoverability, which is not an issue since
 *               recovery has not yet been implemented in the PDS.
 *
 *               SS_init() has been hacked to truncate the log file at open.
 *               Thus it is possible to benchmark stable mode access without
 *               the system detecting a recovery condition the next time it
 *               is started.  Maintaining the log file is not necessary since
 *               recovery has not yet been implemented in the PDS.
 */


/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#else
#include <sys/time.h>
#include "nonansi.h"
#endif

#include <stdio.h>
#include <string.h>

#include "gpmacro.h"
#include "gputil.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"

#include "pfs.h"

#include "psys.h"

#include "pds_fhandlet.h"

#include "pds_sstorage_manager.h"




/*
 * Private Declarations - Types and Constants
 */


/* file information cache (fic) entry:
 *
 *   Cached information associated with a given file handle. Cache is
 *   implemented as a doubly linked circular list, with first and last
 *   entries being MRU and LRU respectively.
 */

typedef struct fic_entry{
  int valid;                 /* fic_entry contains valid data */
  pds_fhandlet fhandle;      /* file handle */
  char *path;                /* file path name */
  int amode;                 /* file accessibility by PDS */
  int fildes;                /* associated file descriptor */
  struct fic_entry *cnext;   /* next cache entry in LRU chain (towards LRU) */
  struct fic_entry *cprev;   /* prev cache entry in LRU chain (towards MRU) */
  struct fic_entry *fhnext;  /* next entry in file handle hash chain */
  struct fic_entry *fhprev;  /* prev entry in file handle hash chain */
} fic_entryt;

/* FIC cache size in number of entries (must be > 0) */
#define SS_FIC_SZ 512

/* FIC invalid file descriptor flag */
#define FILDES_INVALID -1

/* file handle hash table size; choose prime not near a power of 2 */
#define FH_TABLE_SZ 103

/* file handle database (FHDB) name and permission mask */
#define FHDB_NAME "PIOUS.DS.FHDB"
#define FHDB_PERM (PIOUS_IRUSR | PIOUS_IWUSR)

/* transaction log (TLOG) file name and permission mask */
#define TLOG_NAME "PIOUS.DS.TLOG"
#define TLOG_PERM (PIOUS_IRUSR | PIOUS_IWUSR)

/* error log (ERRLOG) file name and permission mask */
#define ERRLOG_NAME "PIOUS.DS.ERRLOG"
#define ERRLOG_PERM (PIOUS_IRUSR | PIOUS_IWUSR | PIOUS_IRGRP | PIOUS_IROTH)




/*
 * Private Variable Definitions
 */


/* file information cache (FIC) */
static fic_entryt fic_cache[SS_FIC_SZ];
static fic_entryt *fic_lru;                  /* LRU cache entry */
static fic_entryt *fic_mru;                  /* MRU cache entry */

/* file handle hash table (for locating FIC entries) */
static fic_entryt *fh_table[FH_TABLE_SZ];

/* file handle database (FHDB) - file information entry */
static fic_entryt FHDBinfo;

/* transaction log file (TLOG) - file information entry */
static fic_entryt TLOGinfo;

/* error log (ERRLOG) - file information entry */
static fic_entryt ERRLOGinfo;


/* file descriptor (fildes) table:
 *
 *   Maps file descriptors opened by pds_sstorage_manager to corresponding
 *   entries in the file information cache.  Note that FHDBinfo.fildes
 *   and TLOGinfo.fildes are NOT included since they are not in the cache and
 *   not subject to deallocation.
 */

static int fildes_total;              /* file descriptors available total */
static fic_entryt **fildes_table;     /* file descriptor table */


/* file handle database record template:
 *
 *   The file handle database (FHDB) stores a set of tuples
 *   (pathname, file handle) that map file handles to pathnames; such a
 *   mapping can not be obtained via the hosting file system for security
 *   reasons.
 *
 *   For simplicity and speed the FHDB is a stack.  Records (tuples) are
 *   only ever appended, so that the FHDB is read backwards to obtain
 *   the most recent mapping.  When a file is deleted, a tuple containing
 *   a null path name is appended to the FHDB to indicate this "unmapping".
 *
 *   Maintaing file handle to pathname mappings is necessary for two reasons:
 *
 *     1) to avoid writing full pathnames into the transaction log while
 *        retaining the ability to recover, and
 *
 *     2) to maintain the illusion that files are always accessible,
 *        and do not need to be opened/closed.
 *
 *   The following template is used for writing/reading file handle database
 *   records. Records are constructed such that the EOR marker respresents a
 *   unique bit sequence in the file indicating the end of a complete
 *   record. An EOR marker is required for fault tolerance, as a system
 *   fault that corrupts the file handle database will leave the last
 *   record incomplete.  Precise control of the bit sequence is required,
 *   in an implementation independent manner, hence the use of an embedded
 *   array.  SS_init() initializes the template flags to the indicated values.
 *
 *   Given an FIC (file information cache) of reasonable size, the FHDB is
 *   rarely accessed, and hence has negligible impact on performance.
 *
 * NOTE: To store values of type pds_fhandlet, the FHDB record structure
 *       and the associated read/write routines must violate the
 *       pds_fhandlet type abstraction.  As discussed in
 *       pds/pds_fhandlet.h, pds_sstorage_manager.c is a "friend" of the
 *       pds_fhandlet ADT in the C++ sense.
 */


#define F_ONEBITS_0   0
#define F_PATHLEN     1
#define F_ONEBITS_1   2
#define F_FHANDLE_DEV 3
#define F_FHANDLE_INO 4
#define F_ONEBITS_2   5
#define F_EORMARKER_0 6
#define F_EORMARKER_1 7
#define F_EORMARKER_2 8

typedef unsigned long fhdb_recordt[9];

static struct fhdb_templatet{
  /* char path[pathlen];           path separate from fixed length fields */

  fhdb_recordt f;

  /* unsigned long onebits_0;      one (1) bits */
  /* unsigned long pathlen;        path name length */
  /* unsigned long onebits_1;      one (1) bits */
  /* unsigned long fhandle_dev;    file handle - dev */
  /* unsigned long fhandle_ino;    file handle - ino */
  /* unsigned long onebits_2;      one (1) bits */
  /* unsigned long EORmarker_0;    EOR marker - lower - zero (0) bits */
  /* unsigned long EORmarker_1;    EOR marker - mid   - zero (0) bits */
  /* unsigned long EORmarker_2;    EOR marker - upper - zero (0) bits */
} fhdb_template;




/* PDS global fatal error flag
 *
 *   indicates a stable storage fatal error has occured; PDS can not continue.
 *
 *   set by: pds_sstorage_manager routines, pds_daemon in main()
 *
 *   note: initialized TRUE and set FALSE by SS_init(), since it is a fatal
 *         error not to perform SS_init() to initialize stable storage
 */

int SS_fatalerror = TRUE;


/* PDS global recover flag
 *
 *   indicates that stable storage requires recovery before PDS can continue
 *
 *   set by: pds_cache_manager routines, pds_sstorage_manager in SS_init()
 */

int SS_recover = FALSE;


/* PDS global check-point flag
 *
 *   indicates that stable storage requires check-point before PDS can continue
 *
 *   set by: pds_recovery_manager routines
 */

int SS_checkpoint = FALSE;




/* local FHDB full flag
 *
 *   indicates write to FHDB exceeded logical or physical system constraints/
 *   resources and/or the last FHDB record may be incomplete; system must
 *   checkpoint/recover (thus truncating FHDB) before ANY more writes to FHDB.
 *
 *   the stable storage manager can continue normal operation with the FHDB
 *   full. however, it will not be possible to perform an SS_lookup() on
 *   a file NOT already in the FHDB or to perform an SS_unlink(); neither
 *   limitation effects the ability to recover/checkpoint.
 *
 *   note: FHDBfull can be set TRUE in SS_init() and fhandle_db_write();
 *         reset to FALSE in SS_logtrunc().
 */

static int FHDBfull = FALSE;




/* Local Function Declarations */

#ifdef __STDC__
static int path2fhandle(char *path,
			pds_fhandlet *fhandle);

static int fhandle_locate(pds_fhandlet fhandle,
			  fic_entryt **fic_entry);

static fic_entryt *fic_locate(pds_fhandlet fhandle);

static fic_entryt *fic_insert(pds_fhandlet fhandle,
			      char *path,
			      int amode);

static void fic_invalidate(fic_entryt *fic_entry);

static void fic_make_mru(fic_entryt *fic_entry);

static void fic_make_lru(fic_entryt *fic_entry);

static int fhandle_db_write(pds_fhandlet fhandle,
			    char *path);

static int fhandle_db_read(pds_fhandlet fhandle,
			   char **path);

static int fildes_alloc(fic_entryt *fic_entry,
			int cflag,
			pious_modet mode);

static void fildes_free(fic_entryt *fic_entry);
#else
static int path2fhandle();
static int fhandle_locate();
static fic_entryt *fic_locate();
static fic_entryt *fic_insert();
static void fic_invalidate();
static void fic_make_mru();
static void fic_make_lru();
static int fhandle_db_write();
static int fhandle_db_read();
static int fildes_alloc();
static void fildes_free();
#endif




/*
 * Function Definitions - Stable Storage Operations
 */




/*
 * SS_init() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_init(char *logpath)
#else
int SS_init(logpath)
     char *logpath;
#endif
{
  int idx;
  int ocode, oflag;
  struct FS_stat fstatus;
  register fic_entryt *fic_pos;
  unsigned long myhostid, myuid;
  int idwidth;

  /* note: using goto to abort SS_init(); one of the few cases where using
   *       goto can be justified.  the following variables are set for
   *       testing at the Abort_Init label.
   */

  FHDBinfo.path = TLOGinfo.path = ERRLOGinfo.path = NULL;
  fildes_table  = NULL;

  FHDBinfo.fildes = TLOGinfo.fildes = ERRLOGinfo.fildes = FILDES_INVALID;


  /* set stable storage status flags */

  SS_recover = SS_checkpoint = FHDBfull = FALSE;


  /* check that a log file directory has been provided */

  if (logpath == NULL)
    { /* no directory for system log files; fatal error */
      SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_EFATAL,
		"null system log directory pathname");

      goto Abort_Init;
    }

  /* determine user id and host id for distinguishing system log files */

  myuid = (unsigned long)SYS_getuid();

  if (SYS_gethostid(&myhostid) != PIOUS_OK)
    { /* unable to determine hostid; fatal error */
      SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_EFATAL,
		"unable to determine host id");

      goto Abort_Init;
    }


  /* set up error log file name
   *
   *   note: SS_errlog() handles case ERRLOGinfo.path == NULL by outputing
   *         to stderr, so no need to abort on failure to allocate pathname
   *         space.  Also, SS_errlog() does not require a pre-allocated file
   *         descriptor.
   */

  idwidth = sizeof(unsigned long) * CHAR_BIT / 4;

  if ((ERRLOGinfo.path = malloc((unsigned)(strlen(logpath) +
					   strlen(ERRLOG_NAME) +
					   (2 * idwidth) +
					   4))) != NULL)

    sprintf(ERRLOGinfo.path, "%s/%s.%lx.%lx",
	    logpath, ERRLOG_NAME, myuid, myhostid);



  /* initialize file information cache (FIC) as doubly linked circular list */

  if (SS_FIC_SZ == 1)
    { /* special case; cache size is one entry */
      fic_cache[0].cnext = fic_cache[0].cprev = fic_cache;
      fic_cache[0].valid = FALSE;
    }
  else
    { /* cache size is greater than one entry */
      for (fic_pos = fic_cache + 1; fic_pos < fic_cache + (SS_FIC_SZ - 1);
	   fic_pos++)
	{ /* initialize all but first and last */
	  fic_pos->cnext = fic_pos + 1;
	  fic_pos->cprev = fic_pos - 1;
	  fic_pos->valid = FALSE;
	}

      /* initialize first */
      fic_cache[0].cnext = fic_cache + 1;
      fic_cache[0].cprev = fic_cache + (SS_FIC_SZ - 1);
      fic_cache[0].valid = FALSE;

      /* initialize last */
      fic_cache[SS_FIC_SZ - 1].cnext = fic_cache;
      fic_cache[SS_FIC_SZ - 1].cprev = fic_cache + (SS_FIC_SZ - 2);
      fic_cache[SS_FIC_SZ - 1].valid = FALSE;
    }

  /* set MRU and LRU cache entry pointers */

  fic_mru = fic_cache;
  fic_lru = fic_cache + (SS_FIC_SZ - 1);

  /* initialize FHDB record template flag fields */

  fhdb_template.f[F_EORMARKER_0] ^= fhdb_template.f[F_EORMARKER_0];
  fhdb_template.f[F_EORMARKER_1] ^= fhdb_template.f[F_EORMARKER_1];
  fhdb_template.f[F_EORMARKER_2] ^= fhdb_template.f[F_EORMARKER_2];

  fhdb_template.f[F_ONEBITS_0] = ~(fhdb_template.f[F_ONEBITS_0] ^
				   fhdb_template.f[F_ONEBITS_0]);
  fhdb_template.f[F_ONEBITS_1] = ~(fhdb_template.f[F_ONEBITS_1] ^
				   fhdb_template.f[F_ONEBITS_1]);
  fhdb_template.f[F_ONEBITS_2] = ~(fhdb_template.f[F_ONEBITS_2] ^
				   fhdb_template.f[F_ONEBITS_2]);



  /* initialize file descriptor mapping table for number of available descrp */

  fildes_total = FS_open_max();

  fildes_table = (fic_entryt **)malloc((unsigned)
				       (fildes_total * sizeof(fic_entryt *)));

  if (fildes_table != NULL)
    { /* table allocated; initialize */
      for (idx = 0; idx < fildes_total; idx++)
	fildes_table[idx] = NULL;
    }

  else
    { /* unable to allocate storage for mapping table; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_EINSUF,
		"can not allocate descriptor mapping table");

      goto Abort_Init;
    }


  /* set up transaction log file name and open TLOG file for read/write */

  if ((TLOGinfo.path = malloc((unsigned)(strlen(logpath) +
					 strlen(TLOG_NAME) +
					 (2 * idwidth) +
					 4))) == NULL)

    { /* can not alloc name space; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_EINSUF,
		"can not allocate transaction log (TLOG) pathname storage");

      goto Abort_Init;
    }

  sprintf(TLOGinfo.path, "%s/%s.%lx.%lx",
	  logpath, TLOG_NAME, myuid, myhostid);

  /* WARNING: hacked log file open to truncate; see implementation notes */

  ocode = FS_open(TLOGinfo.path,
		  PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC, TLOG_PERM);

  if (ocode >=0 )
    /* successfully opened TLOG file */
    TLOGinfo.fildes = ocode;

  else
    { /* can not open TLOG file; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", (int)ocode,
		"can not open transaction log (TLOG) file for access");

      goto Abort_Init;
    }

  /* check that transaction log is regular file; check for log entries */

  if ((ocode = FS_fstat(TLOGinfo.fildes, &fstatus)) == PIOUS_OK)
    { /* successfully obtained status; check file type and size */
      if (!(fstatus.mode & PIOUS_ISREG))
	{ /* trans log file is not a regular file; log error and abort */
	  SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_ENOTREG,
		    "transaction log (TLOG) file not regular");

	  goto Abort_Init;
	}

      if (fstatus.size != 0)
	/* log file contains entries; MUST assume recovery required */
	SS_recover = TRUE;
    }

  else
    { /* unable to obtain status of TLOG file; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", (int)ocode,
		"unable to determine transaction log (TLOG) file status");

      goto Abort_Init;
    }

  /* set up FHDB pathname and open FHDB file for read/write access */

  if ((FHDBinfo.path = malloc((unsigned)(strlen(logpath) +
					 strlen(FHDB_NAME) +
					 (2 * idwidth) +
					 4))) == NULL)

    { /* can not alloc name space; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_EINSUF,
		"can not allocate file handle db (FHDB) pathname storage");

      goto Abort_Init;
    }

  sprintf(FHDBinfo.path, "%s/%s.%lx.%lx",
	  logpath, FHDB_NAME, myuid, myhostid);


  /* note: if recovery is not required then truncate FHDB. otherwise, must
   *       assume that last FHDB record is corrupt (since system only requires
   *       recovery in the case of a failure). in this case the FHDB is flagged
   *       as being full to prevent further writes; this is OK since recovery
   *       will not require writing to the FHDB since system will only access
   *       files previously accessed.
   */

  oflag = PIOUS_RDWR | PIOUS_CREAT;

  if (SS_recover)
    /* flag FHDB as full */
    FHDBfull = TRUE;
  else
    /* truncate FHDB */
    oflag |= PIOUS_TRUNC;

  if ((ocode = FS_open(FHDBinfo.path, oflag, FHDB_PERM)) >= 0)
    /* successfully opened FHDB file */
    FHDBinfo.fildes = ocode;

  else
    { /* can not open FHDB; log error */
      SS_errlog("pds_sstorage_manager", "SS_init()", (int)ocode,
		"can not open file handle database (FHDB) for access");

      goto Abort_Init;
    }

  /* check that FHDB is a regular file */

  if ((ocode = FS_fstat(FHDBinfo.fildes, &fstatus)) == PIOUS_OK)
    { /* successfully obtained status; check file type */
      if (!(fstatus.mode & PIOUS_ISREG))
	{ /* FHDB is not a regular file; log error and abort */
	  SS_errlog("pds_sstorage_manager", "SS_init()", PIOUS_ENOTREG,
		    "file handle database (FHDB) file not regular");

	  goto Abort_Init;
	}
    }

  else
    { /* unable to obtain status of FHDB file; log error and abort */
      SS_errlog("pds_sstorage_manager", "SS_init()", (int)ocode,
		"unable to determine file handle database (FHDB) file status");

      goto Abort_Init;
    }

  /* initialization completed without error; enable stable storage manager */

  SS_fatalerror = FALSE;

  /* indicate if recovery is required */
  if (SS_recover)
    return (PIOUS_ERECOV);
  else
    return (PIOUS_OK);

  /* initialization failed; exit point for all failures */

 Abort_Init:

  /* close TLOG and FHDB files, if opened */
  if (TLOGinfo.fildes != FILDES_INVALID)
    FS_close(TLOGinfo.fildes);

  if (FHDBinfo.fildes != FILDES_INVALID)
    FS_close(FHDBinfo.fildes);

  TLOGinfo.fildes = FHDBinfo.fildes = FILDES_INVALID;

  /* deallocate any allocated storage */
  if (TLOGinfo.path != NULL)
    free(TLOGinfo.path);

  if (FHDBinfo.path != NULL)
    free(FHDBinfo.path);

  if (ERRLOGinfo.path != NULL)
    free(ERRLOGinfo.path);

  ERRLOGinfo.path = TLOGinfo.path = FHDBinfo.path = NULL;

  if (fildes_table != NULL)
    free((char *)fildes_table);

  fildes_table = NULL;

  /* set result codes and exit */
  SS_fatalerror = TRUE;
  return(PIOUS_EFATAL);
}




/*
 * SS_lookup() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
int SS_lookup(char *path,
	      pds_fhandlet *fhandle,
	      int cflag,
	      pious_modet mode)
#else
int SS_lookup(path, fhandle, cflag, mode)
     char *path;
     pds_fhandlet *fhandle;
     int cflag;
     pious_modet mode;
#endif
{
  int rcode, lcode, fcode;
  int amode;
  fic_entryt *fic_entry;
  char *ficpath;
  fic_entryt fic_creat;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* validate 'cflag' argument */
  else if ((cflag & (~(PIOUS_CREAT | PIOUS_TRUNC))) != 0)
    rcode = PIOUS_EINVAL;

  else
    { /* attempt to map 'path' to corresponding file handle */

      lcode = path2fhandle(path, fhandle);

      if ((lcode == PIOUS_OK && (cflag & PIOUS_TRUNC)) ||
	  (lcode == PIOUS_ENOENT && (cflag & PIOUS_CREAT)))
	{
	  /* either file exists and PIOUS_TRUNC is specified, or file does
           * not exist and PIOUS_CREAT is specified.
	   */

	  /* set-up 'fic_creat', pseudo FIC entry for file creat/trunc */

	  fic_creat.path   = path;
	  fic_creat.amode  = PIOUS_W_OK;
	  fic_creat.fildes = FILDES_INVALID;

	  /* allocate a descriptor for 'fic_creat' with creation/truncation
	   * specified; see discussion in fildes_alloc()
	   */

	  if (cflag & PIOUS_CREAT)
	    lcode = fildes_alloc(&fic_creat, cflag, mode);
	  else
	    lcode = fildes_alloc(&fic_creat, cflag, (pious_modet)0);

	  if (lcode == PIOUS_OK)
	    { /* file creat/trunc; dealloc fildes and map path to fhandle */
	      fildes_free(&fic_creat);
	      lcode = path2fhandle(path, fhandle);
	    }
	}

      /* at this point lcode is return code of path2fhandle() or, if a file
       * creation/truncation attempt FAILED, fildes_alloc().
       */

      if (lcode != PIOUS_OK)
	rcode = lcode;

      else
	{ /* successful path2fhandle(); locate/insert entry in FHDB and FIC */

	  fcode = fhandle_locate(*fhandle, &fic_entry);

	  /* case: 'fhandle' located in either FIC or FHDB */
	  if (fcode == PIOUS_OK || fcode == PIOUS_EINSUF)
	    rcode = PIOUS_OK;

	  /* case: error encountered reading FHDB */
	  else if (fcode == PIOUS_EFATAL)
	    rcode = PIOUS_EFATAL;

	  /* case: 'fhandle' not in FIC or FHDB AND FHDB reached a limit */
	  else if (FHDBfull == TRUE) /* && fcode == PIOUS_EBADF */
	    rcode = PIOUS_EINSUF;

	  /* case: 'fhandle' not in FIC or FHDB AND FHDB still writeable */
	  else /* FHDBfull == FALSE && fcode == PIOUS_EBADF */
	    {
	      /* write to file handle database (FHDB).  must complete without
	       * error before placing in file information cache (FIC).
	       */

	      rcode = fhandle_db_write(*fhandle, path);

	      if (rcode == PIOUS_OK)
		{ /* write to FHDB successful; allocate FIC path name space */

		  ficpath = (char *)malloc((unsigned)strlen(path) + 1);

		  /* not a problem if malloc() fails; entry in FHDB */

		  if (ficpath != NULL)
		    { /* assign FIC path name */
		      strcpy(ficpath, path);

		      /* determine file accessability */
		      amode = 0;

		      if (FS_access(path, PIOUS_R_OK) == PIOUS_OK)
			amode |= PIOUS_R_OK;

		      if (FS_access(path, PIOUS_W_OK) == PIOUS_OK)
			amode |= PIOUS_W_OK;

		      if (FS_access(path, PIOUS_X_OK) == PIOUS_OK)
			amode |= PIOUS_X_OK;

		      /* insert information into FIC at MRU position */

		      fic_entry = fic_insert(*fhandle, ficpath, amode);
		    }
		}
	    }
	}
    }

  return rcode;
}

      


/*
 * SS_read() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
pious_ssizet SS_read(pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf)
#else
pious_ssizet SS_read(fhandle, offset, nbyte, buf)
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
#endif
{
  pious_ssizet rcode, acode;
  int fcode;
  int fd_valid;
  fic_entryt *fic_entry;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else
    { /* locate 'fhandle' */
      fcode = fhandle_locate(fhandle, &fic_entry);

      if (fcode != PIOUS_OK)
	/* 'fhandle' not located or error occured in accessing FHDB */
	rcode = fcode; /* PIOUS_EBADF or PIOUS_EINSUF or PIOUS_EFATAL */

      else
	{ /* 'fhandle' located and now in FIC; validate access mode */

	  if (!(fic_entry->amode & PIOUS_R_OK))
	    /* read not a valid access mode */
	    rcode = PIOUS_EACCES;
	  else
	    {
	      /* read is valid access mode; obtain file descriptor */

	      fd_valid = TRUE;

	      if (fic_entry->fildes == FILDES_INVALID)
		{ /* allocate a file descriptor */
		  acode = fildes_alloc(fic_entry, PIOUS_NOCREAT,
				       (pious_modet)0);

		  if (acode != PIOUS_OK)
		    { /* error allocating file descriptor */
		      fd_valid = FALSE;

		      /* PIOUS_EINSUF only possible error unless file status
		       * or path modified by entity other than PDS
		       */

		      if (acode == PIOUS_EINSUF)
			rcode = PIOUS_EINSUF;
		      else
			rcode = PIOUS_EUNXP;
		    }
		}

	      if (fd_valid)
		{ /* valid file descriptor; attempt to read from file */
		  acode = FS_read(fic_entry->fildes, offset, PIOUS_SEEK_SET,
				  nbyte, buf);

		  if (acode >= 0)
		    /* read from file without error */
		    rcode = acode;
		  else
		    /* error reading file; set rcode appropriately */
		    switch(acode)
		      {
		      case PIOUS_EINVAL:
			rcode = PIOUS_EINVAL;
			break;
		      default:
			rcode = PIOUS_EUNXP;
			break;
		      }
		}
	    }
	}
    }

  return rcode;
}




/*
 * SS_write() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
pious_ssizet SS_write(pds_fhandlet fhandle,
		      pious_offt offset,
		      pious_sizet nbyte,
		      char *buf,
		      int faultmode)
#else
pious_ssizet SS_write(fhandle, offset, nbyte, buf, faultmode)
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
     int faultmode;
#endif
{
  pious_ssizet rcode, acode;
  int fcode;
  int fd_valid;
  fic_entryt *fic_entry;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* validate 'faultmode' argument */
  else if (faultmode != PIOUS_VOLATILE && faultmode != PIOUS_STABLE)
    rcode = PIOUS_EINVAL;

  else
    { /* locate 'fhandle' */
      fcode = fhandle_locate(fhandle, &fic_entry);

      if (fcode != PIOUS_OK)
	/* 'fhandle' not located or error occured in accessing FHDB */
	rcode = fcode; /* PIOUS_EBADF or PIOUS_EINSUF or PIOUS_EFATAL */

      else
	{ /* 'fhandle' located and now in FIC; validate access mode */

	  if (!(fic_entry->amode & PIOUS_W_OK))
	    /* write not a valid access mode */
	    rcode = PIOUS_EACCES;
	  else
	    {
	      /* write is valid access mode; obtain file descriptor */

	      fd_valid = TRUE;

	      if (fic_entry->fildes == FILDES_INVALID)
		{ /* allocate a file descriptor */
		  acode = fildes_alloc(fic_entry, PIOUS_NOCREAT,
				       (pious_modet)0);

		  if (acode != PIOUS_OK)
		    { /* error allocating descriptor */
		      fd_valid = FALSE;

		      /* PIOUS_EINSUF only possible error unless file status
		       * or path modified by entity other than PDS
		       */

		      if (acode == PIOUS_EINSUF)
			rcode = PIOUS_EINSUF;
		      else
			rcode = PIOUS_EUNXP;
		    }
		}

	      if (fd_valid)
		{ /* valid file descriptor; attempt to write to file */
		  acode = FS_write(fic_entry->fildes, offset, PIOUS_SEEK_SET,
				   nbyte, buf);

		  if (acode >= 0)
		    /* file written without error */
		    if (faultmode == PIOUS_VOLATILE)
		      /* VOLATILE write; do not force to disk */
		      rcode = acode;
		    else
		      /* STABLE write; force to disk */
		      if (FS_fsync(fic_entry->fildes) == PIOUS_OK)
			rcode = acode;
		      else
			rcode = PIOUS_EUNXP;

		  else
		    /* error writting file; set rcode appropriately */
		    switch(acode)
		      {
		      case PIOUS_EINVAL:
		      case PIOUS_EFBIG:
		      case PIOUS_ENOSPC:
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

  return rcode;
}




/*
 * SS_faccess() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_faccess(pds_fhandlet fhandle,
	       int *amode)
#else
int SS_faccess(fhandle, amode)
     pds_fhandlet fhandle;
     int *amode;
#endif
{
  int rcode;
  fic_entryt *fic_entry;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* locate 'fhandle'; set 'amode' if found */
  else if ((rcode = fhandle_locate(fhandle, &fic_entry)) == PIOUS_OK)
    *amode = fic_entry->amode;

  return rcode;
}




/*
 * SS_stat() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_stat(char *path,
	    pious_modet *mode)
#else
int SS_stat(path, mode)
     char *path;
     pious_modet *mode;
#endif
{
  int rcode;
  struct FS_stat fstatus;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* determine file mode */
  else if ((rcode = FS_stat(path, &fstatus)) == PIOUS_OK)
    *mode = fstatus.mode;

  return rcode;
}




/*
 * SS_chmod() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
int SS_chmod(char *path,
	     pious_modet mode,
	     int *amode)
#else
int SS_chmod(path, mode, amode)
     char *path;
     pious_modet mode;
     int *amode;
#endif
{
  int rcode;
  pds_fhandlet fhandle;
  fic_entryt *fic_pos;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else
    { /* map 'path' to fhandle; search file information cache for fhandle and
       * deallocate file descriptor associated with file 'path', if any.
       */

      fic_pos = NULL;

      if (path2fhandle(path, &fhandle) == PIOUS_OK &&
	  (fic_pos = fic_locate(fhandle)) != NULL)
	/* 'path' fhandle located in file information cache; free descriptor */
	fildes_free(fic_pos);

      /* update file access permission bits, setting result code in process */
      rcode = FS_chmod(path, mode);

      /* establish file accessibility */
      *amode = 0;

      if (FS_access(path, PIOUS_R_OK) == PIOUS_OK)
	*amode |= PIOUS_R_OK;

      if (FS_access(path, PIOUS_W_OK) == PIOUS_OK)
	*amode |= PIOUS_W_OK;

      if (FS_access(path, PIOUS_X_OK) == PIOUS_OK)
	*amode |= PIOUS_X_OK;

      /* if FIC entry previously located, reset file accessibility there */
      if (fic_pos != NULL)
	fic_pos->amode = *amode;
    }

  return rcode;
}




/*
 * SS_unlink() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
int SS_unlink(char *path)
#else
int SS_unlink(path)
     char *path;
#endif
{
  int rcode;
  pds_fhandlet fhandle;
  fic_entryt *fic_pos;

  /* unlinking (removing) a file involves two distinct acts: removing
   * pds_sstorage_manager maintained information concerning the file,
   * and removing the file from the storage device; the first is logical
   * removal and the second is physical removal.
   *
   * ideally, logical and physical removal should be performed atomically.
   * unfortunately, this is not possible. physical removal is performed first.
   * the recovery algorithm has to deal with the resulting inconsistency
   * in the FHDB should failure occur between physical and logical removal.
   *
   * note: SS_unlink() applies to non-directory files only; this is enforced
   *       by FS_unlink() (and path2fhandle(), which only maps regular files).
   */

  /* check for previous fatal errors */

  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* check for FHDB full */

  else if (FHDBfull)
    rcode = PIOUS_EINSUF;

  /* perform physical/logical removal */

  else if (path2fhandle(path, &fhandle) != PIOUS_OK)
    /* no (path, fhandle) mapping; file is either not regular, does not
     * exist, or is inaccessable; logical removal NOT required
     */
    rcode = FS_unlink(path);

  else
    { /* (path, fhandle) mapping established; search file information cache
       * for fhandle and deallocate file descriptor associated with
       * file 'path', if any.
       */

      if ((fic_pos = fic_locate(fhandle)) != NULL)
	fildes_free(fic_pos);

      /* attempt physical removal */

      if ((rcode = FS_unlink(path)) == PIOUS_OK)
	{ /* physical removal succeeded; perform logical removal */

	  if (fhandle_db_write(fhandle, NULL) == PIOUS_OK)
	    { /* logical removal will succeed */
	      if (fic_pos != NULL)
		fic_invalidate(fic_pos);
	    }
	  else
	    { /* logical removal failed; FHDB is inconsistent; fatal error */
	      SS_fatalerror = TRUE;
	      rcode         = PIOUS_EFATAL;

	      SS_errlog("pds_sstorage_manager", "SS_unlink()", PIOUS_EFATAL,
			"logical removal failed; FHDB inconsistent");
	    }
	}
    }

  return rcode;
}




/*
 * SS_mkdir() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
int SS_mkdir(char *path,
	     pious_modet mode)
#else
int SS_mkdir(path, mode)
     char *path;
     pious_modet mode;
#endif
{
  int rcode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* creating a directory file will not effect the state of files currently
   * maintained by the pds_sstorage_manager; pass request directly to
   * FS_mkdir().
   */

  else
    rcode = FS_mkdir(path, mode);

  return rcode;
}




/*
 * SS_rmdir() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_rmdir(char *path)
#else
int SS_rmdir(path)
     char *path;
#endif
{
  int rcode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* since a directory file can be removed ONLY if it is empty, removing
   * directory will not effect the state of files currently maintained
   * by the pds_sstorage_manager; pass request directly to FS_rmdir().
   */
  else
    rcode = FS_rmdir(path);

  return rcode;
}




/*
 * SS_logread() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
pious_ssizet SS_logread(pious_offt offset,
			int whence,
			pious_sizet nbyte,
			char *buf)
#else
pious_ssizet SS_logread(offset, whence, nbyte, buf)
     pious_offt offset;
     int whence;
     pious_sizet nbyte;
     char *buf;
#endif
{
  pious_ssizet rcode, acode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else
    { /* attempt to read from log file */
      acode = FS_read(TLOGinfo.fildes, offset, whence, nbyte, buf);

      if (acode >= 0)
	/* read log file without error */
	rcode = acode;
      else
	/* error reading log file; set rcode appropriately */
	switch(acode)
	  {
	  case PIOUS_EBADF:
	    rcode         = PIOUS_EFATAL;
	    SS_fatalerror = TRUE;
	    SS_errlog("pds_sstorage_manager", "SS_logread()", 0,
		      "unable to access transaction log (TLOG) file");
	    break;
	  case PIOUS_EINVAL:
	    rcode = PIOUS_EINVAL;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  return rcode;
}




/*
 * SS_logwrite() - See pds_sstorage_manager.h for description.
 */

#ifdef __STDC__
pious_ssizet SS_logwrite(pious_offt offset,
			 int whence,
			 pious_sizet nbyte,
			 char *buf)
#else
pious_ssizet SS_logwrite(offset, whence, nbyte, buf)
     pious_offt offset;
     int whence;
     pious_sizet nbyte;
     char *buf;
#endif
{
  pious_ssizet rcode, acode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else
    { /* attempt to write to log file */
      acode = FS_write(TLOGinfo.fildes, offset, whence, nbyte, buf);

      if (acode >= 0)
	/* wrote log file without error */
	rcode = acode;
      else
	/* error writting log file; set rcode appropriately */
	switch(acode)
	  {
	  case PIOUS_EBADF:
	    rcode         = PIOUS_EFATAL;
	    SS_fatalerror = TRUE;
	    SS_errlog("pds_sstorage_manager", "SS_logwrite()", 0,
		      "unable to access transaction log (TLOG) file");
	    break;
	  case PIOUS_EINVAL:
	  case PIOUS_EFBIG:
	  case PIOUS_ENOSPC:
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
 * SS_logsync() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_logsync(void)
#else
int SS_logsync()
#endif
{
  int rcode, scode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else
    { /* attempt to force writes to disk */
      scode = FS_fsync(TLOGinfo.fildes);

      switch(scode)
	{
	case PIOUS_OK:
	  rcode = PIOUS_OK;
	  break;
	case PIOUS_EBADF:
	  rcode         = PIOUS_EFATAL;
	  SS_fatalerror = TRUE;
	  SS_errlog("pds_sstorage_manager", "SS_logsync()", 0,
		    "unable to access transaction log (TLOG) file");
	  break;
	default:
	  rcode = PIOUS_EUNXP;
	  break;
	}
    }

  return rcode;
}




/*
 * SS_logtrunc() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
int SS_logtrunc(void)
#else
int SS_logtrunc()
#endif
{
  int rcode, ocode;

  /* check for previous fatal errors */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  /* truncate transacation log and, if neccessary, FHDB file */
  else
    { /* close/re-open transaction log */
      FS_close(TLOGinfo.fildes);
      TLOGinfo.fildes = FILDES_INVALID;

      ocode = FS_open(TLOGinfo.path,
		      PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC, TLOG_PERM);

      if (ocode < 0)
	{ /* can not open TLOG file; log error */
	  SS_errlog("pds_sstorage_manager", "SS_logtrunc()", (int)ocode,
		    "can not open transaction log (TLOG) file for access");

	  SS_fatalerror = TRUE;
	  rcode         = PIOUS_EFATAL;
	}

      else
	{ /* successfully re-opened/truncated TLOG file */
	  TLOGinfo.fildes = ocode;

	  /* WARNING: hack to prevent FHDB trunc; see implementation notes */

	  if (TRUE /* !FHDBfull */)
	    /* FHDB does not require truncation */
	    rcode = PIOUS_OK;

	  else
	    { /* FHDB is full, truncate as well */
	      FS_close(FHDBinfo.fildes);
	      FHDBinfo.fildes = FILDES_INVALID;

	      ocode = FS_open(FHDBinfo.path,
			      PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
			      FHDB_PERM);

	      if (ocode < 0)
		{ /* can not open FHDB file; log error */
		  SS_errlog("pds_sstorage_manager", "SS_logtrunc()",
			    (int)ocode,
			    "can not open file handle db (FHDB) for access");

		  SS_fatalerror = TRUE;
		  rcode         = PIOUS_EFATAL;
		}

	      else
		{ /* successfully re-opened/truncated FHDB file */
		  FHDBinfo.fildes = ocode;
		  FHDBfull        = FALSE;
		  rcode           = PIOUS_OK;
		}
	    }
	}
    }

  return rcode;
}




/*
 * SS_errlog() - See pds_sstorage_manager.h for description
 */

#ifdef __STDC__
void SS_errlog(char *module,
	       char *func,
	       int errno,
	       char *errmsg)
#else
void SS_errlog(module, func, errno, errmsg)
     char *module;
     char *func;
     int errno;
     char *errmsg;
#endif
{
  char *errnotxt, *errtxt, msgout[512];
  long caltime;
  int errlen, errlogged;

  /* define complete error message for output */

  if (errno == 0)
    { /* no error code text required */
      errnotxt = "";
      errtxt   = "no error code provided";
    }
  else
    /* generate error code text */
    UTIL_errno2errtxt(errno, &errnotxt, &errtxt);

  caltime = time(NULL);

  sprintf(msgout, "\n%s\n%.25s/%.20s: %.20s\n\t[%.60s]\n\t%.60s\n",
	  ctime(&caltime), module, func, errnotxt, errtxt, errmsg);


  /* If ERRLOGinfo.path is defined, attempt to open file and write error
   * message.  If ERRLOGinfo.path is undefined, or if the file can not
   * be opened for writing, write error message to stderr.
   */

  errlogged = FALSE;
  errlen    = strlen(msgout);

  if (ERRLOGinfo.path != NULL)
    { /* open/creat error log; set-up pseudo FIC entry */
      ERRLOGinfo.amode  = PIOUS_W_OK;
      ERRLOGinfo.fildes = FILDES_INVALID;

      /* allocate a descriptor for ERRLOGinfo with creation specified */

      if (fildes_alloc(&ERRLOGinfo, PIOUS_CREAT, ERRLOG_PERM) == PIOUS_OK)
	{ /* descriptor allocated; attempt write */
	  if (FS_write(ERRLOGinfo.fildes, (pious_offt)0, PIOUS_SEEK_END,
		       (pious_sizet)errlen, msgout) == errlen)
	    errlogged = TRUE;

	  /* deallocate descriptor */
	  fildes_free(&ERRLOGinfo);
	}
    }

  /* output msgout to stderr, if necessary */
  if (!errlogged)
    {
      fprintf(stderr, "%s", msgout);
      fflush(stderr);
    }
}




/*
 * Function Definitions - Local Functions
 */




/*
 * path2fhandle()
 *
 * Parameters:
 *
 *   path    - file path name
 *   fhandle - file handle
 *
 * Map file 'path' to corresponding file handle and place in 'fhandle'.
 *
 * NOTE: To assign values of type pds_fhandlet, path2fhandle() must violate
 *       the pds_fhandlet type abstraction.  As discussed in
 *       pds/pds_fhandlet.h, pds_sstorage_manager.c is a "friend" of the
 *       pds_fhandlet ADT in the C++ sense.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully mapped path to file handle
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EACCES       - path search permission denied
 *       PIOUS_ENOENT       - file does not exist
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_ENOTREG      - file is not a regular file
 *       PIOUS_EUNXP        - unexpected error encountered
 */

#ifdef __STDC__
static int path2fhandle(char *path,
			pds_fhandlet *fhandle)
#else
static int path2fhandle(path, fhandle)
     char *path;
     pds_fhandlet *fhandle;
#endif
{
  int rcode, scode;
  struct FS_stat status;

  if ((scode = FS_stat(path, &status)) == PIOUS_OK)
    /* no errors accessing 'path' status */
    if (status.mode & PIOUS_ISREG)
      {
	fhandle->dev = status.dev;
	fhandle->ino = status.ino;
	rcode = PIOUS_OK;
      }
    else
      rcode = PIOUS_ENOTREG;
  else
    /* error accessing 'path' status */
    switch(scode)
      {
      case PIOUS_EACCES:
      case PIOUS_ENOENT:
      case PIOUS_ENAMETOOLONG:
      case PIOUS_ENOTDIR:
	rcode = scode;
	break;
      default:
	rcode = PIOUS_EUNXP;
	break;
      }

  return (rcode);
}




/*
 * fhandle_locate()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *   fic_entry - file information cache entry
 *
 * Search file information cache (FIC) and (if necessary) file handle database
 * (FHDB) for an entry with file handle 'fhandle'. If 'fhandle' is located
 * it is made the MRU file information cache entry, with a pointer to
 * that entry placed in 'fic_entry'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully located file handle 'fhandle'
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - file handle not located
 *       PIOUS_EINSUF - insufficient system resources; 'fhandle' record located
 *                      in FHDB but unable to allocate storage in FIC for
 *                      pathname
 *       PIOUS_EFATAL - FHDB detected to be corrupted or inaccessible
 */

#ifdef __STDC__
static int fhandle_locate(pds_fhandlet fhandle,
			  fic_entryt **fic_entry)
#else
static int fhandle_locate(fhandle, fic_entry)
     pds_fhandlet fhandle;
     fic_entryt **fic_entry;
#endif
{
  int rcode;
  fic_entryt *fic_pos;

  char *path;
  int amode;

  /* search file information cache for 'fhandle' */
  fic_pos = fic_locate(fhandle);

  if (fic_pos != NULL)
    { /* 'fhandle' located in cache and placed at MRU position */
      *fic_entry = fic_pos;
      rcode      = PIOUS_OK;
    }

  else
    { /* 'fhandle' not located in cache; check file handle database */
      rcode = fhandle_db_read(fhandle, &path);

      if (rcode == PIOUS_OK)
	{ /* 'fhandle' located in FHDB without error */

	  /* re-establish file accessability */
	  amode = 0;

	  if (FS_access(path, PIOUS_R_OK) == PIOUS_OK)
	    amode |= PIOUS_R_OK;

	  if (FS_access(path, PIOUS_W_OK) == PIOUS_OK)
	    amode |= PIOUS_W_OK;

	  if (FS_access(path, PIOUS_X_OK) == PIOUS_OK)
	    amode |= PIOUS_X_OK;

	  /* insert information into cache at MRU position; set *fic_entry */

	  *fic_entry = fic_insert(fhandle, path, amode);
	}
    }

  return rcode;
}




/*
 * fic_locate()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *
 * Search file information cache for an entry with file handle 'fhandle'.
 * If 'fhandle' is located it is made the MRU file information cache entry.
 *
 * Returns:
 *
 *   fic_entryt * - pointer to FIC entry with file handle 'fhandle'.
 *   NULL         - 'fhandle' not located.
 */

#ifdef __STDC__
static fic_entryt *fic_locate(pds_fhandlet fhandle)
#else
static fic_entryt *fic_locate(fhandle)
     pds_fhandlet fhandle;
#endif
{
  register fic_entryt *fic_pos;

  /* search file information cache for 'fhandle' */
  fic_pos = fh_table[fhandle_hash(fhandle, FH_TABLE_SZ)];

  while (fic_pos != NULL && !fhandle_eq(fic_pos->fhandle, fhandle))
    fic_pos = fic_pos->fhnext;

  /* if 'fhandle' located, move cache entry to MRU position */
  if (fic_pos != NULL)
    fic_make_mru(fic_pos);

  return fic_pos;
}




/*
 * fic_insert()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *   path      - file path name
 *   amode     - file accessibility
 *
 * Place file status information into file information cache as the MRU entry;
 * the LRU cache entry is deallocated, if necessary. The new MRU entry is
 * placed at the head of the appropriate file handle hash chain.
 *
 * Note: fic_insert() does NOT check the file information cache to see
 *       if an entry for 'fhandle' already exists.  Duplicates must be
 *       prevented at a higher level.
 *
 * Returns:
 *
 *   pds_fhandlet * - pointer to inserted cache entry
 */

#ifdef __STDC__
static fic_entryt *fic_insert(pds_fhandlet fhandle,
			      char *path,
			      int amode)
#else
static fic_entryt *fic_insert(fhandle, path, amode)
     pds_fhandlet fhandle;
     char *path;
     int amode;
#endif
{
  /* invalidate LRU entry */
  fic_invalidate(fic_lru);

  /* put new status information into LRU entry */
  fic_lru->fhandle   = fhandle;
  fic_lru->path      = path;
  fic_lru->amode     = amode;

  fic_lru->fildes    = FILDES_INVALID;
  fic_lru->valid     = TRUE;

  /* put LRU entry at head of appropriate file handle hash chain */
  fic_lru->fhprev = NULL;

  fic_lru->fhnext = fh_table[fhandle_hash(fhandle, FH_TABLE_SZ)];

  fh_table[fhandle_hash(fhandle, FH_TABLE_SZ)] = fic_lru;

  if (fic_lru->fhnext != NULL)
    fic_lru->fhnext->fhprev = fic_lru;

  /* move LRU entry to MRU position */
  fic_make_mru(fic_lru);

  return(fic_mru);
}




/*
 * fic_invalidate()
 *
 * Parameters:
 *
 *   fic_entry - file information cache (FIC) entry
 *
 * Invalidate the FIC entry 'fic_entry', deallocating an associated file
 * descriptor if necessary, and make the LRU cache entry.
 *
 * Returns:
 */

#ifdef __STDC__
static void fic_invalidate(fic_entryt *fic_entry)
#else
static void fic_invalidate(fic_entry)
     fic_entryt *fic_entry;
#endif
{
  /* invalidate cache entry */
  if (fic_entry->valid == TRUE)
    { /* deallocate file descriptor, if in use */
      if (fic_entry->fildes != FILDES_INVALID)
	fildes_free(fic_entry);

      /* remove from file handle hash chain */

      /* if not last in chain, reset 'prev' pointer of 'next' entry */
      if (fic_entry->fhnext != NULL)
	fic_entry->fhnext->fhprev = fic_entry->fhprev;

      /* if not first in chain, reset 'next' pointer of 'prev' entry */
      if (fic_entry->fhprev != NULL)
	fic_entry->fhprev->fhnext = fic_entry->fhnext;
      else
	/* first in chain, reset file handle hash table entry */
	fh_table[fhandle_hash(fic_entry->fhandle, FH_TABLE_SZ)] =
	  fic_entry->fhnext;

      /* deallocate path name storage */
      if (fic_entry->path != NULL)
	{
	  free(fic_entry->path);
	  fic_entry->path = NULL;
	}

      /* mark entry as invalid */
      fic_entry->valid = FALSE;
    }

  /* make LRU cache entry */
  fic_make_lru(fic_entry);
}




/*
 * fic_make_mru()
 *
 * Parameters:
 *
 *   fic_entry - file information cache (FIC) entry
 *
 * Make the FIC entry 'fic_entry' the MRU cache entry.
 *
 * NOTE: global fic_lru and fic_mru are altered; these values
 *       must be treated as volatile by any routine that calls
 *       fic_make_mru().
 *
 * Returns:
 */

#ifdef __STDC__
static void fic_make_mru(fic_entryt *fic_entry)
#else
static void fic_make_mru(fic_entry)
     fic_entryt *fic_entry;
#endif
{
  if (fic_entry != fic_mru)

    if (fic_entry == fic_lru)
      { /* special case; advance MRU and LRU around circular list */
	fic_mru = fic_lru;
	fic_lru = fic_lru->cprev;
      }
    else
      { /* remove fic_entry from middle of list and place in MRU position */
	    
	/* remove *fic_entry from cache list */
	fic_entry->cprev->cnext = fic_entry->cnext;
	fic_entry->cnext->cprev = fic_entry->cprev;

	/* set *fic_entry cache pointers as appropriate for MRU entry */
	fic_entry->cprev = fic_lru;
	fic_entry->cnext = fic_mru;

	/* set *fic_mru and *fic_lru pointers to new MRU entry (fic_entry) */
	fic_mru->cprev = fic_entry;
	fic_lru->cnext = fic_entry;

	/* set MRU pointer to fic_entry */
	fic_mru = fic_entry;
      }
}




/*
 * fic_make_lru()
 *
 * Parameters:
 *
 *   fic_entry - file information cache (FIC) entry
 *
 * Make the FIC entry 'fic_entry' the LRU cache entry.
 *
 * NOTE: global fic_lru and fic_mru are altered; these values
 *       must be treated as volatile by any routine that calls
 *       fic_make_lru().
 *
 * Returns:
 */

#ifdef __STDC__
static void fic_make_lru(fic_entryt *fic_entry)
#else
static void fic_make_lru(fic_entry)
     fic_entryt *fic_entry;
#endif
{
  if (fic_entry != fic_lru)

    if (fic_entry == fic_mru)
      { /* special case; advance MRU and LRU around circular list */
	fic_lru = fic_mru;
	fic_mru = fic_mru->cnext;
      }
    else
      { /* remove fic_entry from middle of list and place in LRU position */
	    
	/* remove *fic_entry from cache list */
	fic_entry->cprev->cnext = fic_entry->cnext;
	fic_entry->cnext->cprev = fic_entry->cprev;

	/* set *fic_entry cache pointers as appropriate for LRU entry */
	fic_entry->cprev = fic_lru;
	fic_entry->cnext = fic_mru;

	/* set *fic_lru and *fic_mru pointers to new LRU entry (fic_entry) */
	fic_lru->cnext = fic_entry;
	fic_mru->cprev = fic_entry;

	/* set LRU pointer to fic_entry */
	fic_lru = fic_entry;
      }
}



	  
/*
 * fhandle_db_write()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *   path      - file path name
 *
 * Write file handle database record required for mapping file handle
 * to path name.  If 'path' equals NULL, then a record "unmapping"
 * 'fhandle' is entered.
 *
 * Note:  If fatal error detected, a message is written to the PDS error log
 *        and the stable storage flag SS_fatalerror is set to TRUE.
 *
 *        If FHDB exceeds logical/physical constraints then the FHDBfull flag
 *        is set to TRUE.
 *
 *        To write values of type pds_fhandlet, fhandle_db_write() must
 *        violate the pds_fhandlet type abstraction.  As discussed in
 *        pds/pds_fhandlet.h, pds_sstorage_manager.c is a "friend" of the
 *        pds_fhandlet ADT in the C++ sense.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - sucessfully wrote file handle database record
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINSUF - FHDB has grown to exceed logical or physical system
 *                      constraints/resources; incomplete FHDB record may
 *                      have been written
 *       PIOUS_EFATAL - fatal error occured; an incomplete FHDB record may
 *                      have been written
 */

#ifdef __STDC__
static int fhandle_db_write(pds_fhandlet fhandle,
			    char *path)
#else
static int fhandle_db_write(fhandle, path)
     pds_fhandlet fhandle;
     char *path;
#endif
{
  pious_ssizet acode;
  int rcode, ic_transfer;
  struct fhdb_templatet tmp_rec;

  /* do not attempt write if FHDB is full */
  if (FHDBfull)
    return(PIOUS_EINSUF);

  /* Set-up record to write */

  tmp_rec                  = fhdb_template;

  tmp_rec.f[F_FHANDLE_DEV] = fhandle.dev;
  tmp_rec.f[F_FHANDLE_INO] = fhandle.ino;

  if (path != NULL)
    /* recording mapping */
    tmp_rec.f[F_PATHLEN] = strlen(path);
  else
    /* recording "unmapping" */
    tmp_rec.f[F_PATHLEN] = 0;

  ic_transfer = FALSE;

  /* write file path name, if extant */

  if (path != NULL &&
      (acode = FS_write(FHDBinfo.fildes, (pious_offt)0, PIOUS_SEEK_END,
			(pious_sizet)tmp_rec.f[F_PATHLEN],
			path)) < tmp_rec.f[F_PATHLEN])
    ic_transfer = TRUE;

  /* write fixed length record fields */

  else if ((acode = FS_write(FHDBinfo.fildes, (pious_offt)0, PIOUS_SEEK_END,
			     (pious_sizet)sizeof(fhdb_recordt),
			     (char *)tmp_rec.f)) < sizeof(fhdb_recordt))
    ic_transfer = TRUE;

  /* force writes to disk */

  /* WARNING: hack to avoid forced write to FHDB; see implementation notes */

  else if (FALSE /* (acode = FS_fsync(FHDBinfo.fildes)) != PIOUS_OK */)
    { /* fatal error, can't force write */
      SS_errlog("pds_sstorage_manager", "fhandle_db_write()", (int)acode,
		"can not force file handle database (FHDB) write to disk");

      rcode = PIOUS_EFATAL;
    }
  else
    rcode = PIOUS_OK;

  /* if incomplete transfer flagged, determine if a true error occured,
   * or if FHDB has grown to exceed physical or logical constraints;
   * set rcode accordingly.
   */

  if (ic_transfer == TRUE)
    {
      if (acode >= 0)
	/* partial transfer */
	rcode = PIOUS_EINSUF;
      else
	/* no transfer; examine error code */
	switch(acode)
	  {
	  case PIOUS_EINVAL: /* resulting file offset exceeds constraints */
	  case PIOUS_EFBIG:
	  case PIOUS_ENOSPC:
	    rcode = PIOUS_EINSUF;
	    break;
	  default:
	    SS_errlog("pds_sstorage_manager", "fhandle_db_write()", (int)acode,
		      "error writing file handle database (FHDB) record");

	    rcode = PIOUS_EFATAL;
	    break;
	  }
    }

  if (rcode == PIOUS_EFATAL)
    SS_fatalerror = TRUE;
  else if (rcode == PIOUS_EINSUF)
    FHDBfull = TRUE;

  return rcode;
}




/*
 * fhandle_db_read()
 *
 * Parameters:
 *
 *   fhandle     - file handle
 *   path        - file handle path name
 *
 * Locates MOST RECENTLY written file handle database record mapping
 * 'fhandle' to a pathname. If a valid mapping is found, storage is
 * allocated and 'path' is set to point to that pathname.
 *
 * NOTE:  Tolerates AT MOST corruption of last file handle data base record;
 *        further corruption may generate an error condition.
 *        After a system fault, recovery must take place prior to resumed
 *        operation.
 *
 *        If fatal error detected a message is written to the PDS error log
 *        and the stable storage flag SS_fatalerror is set to TRUE.
 *
 *        To read/assign values of type pds_fhandlet, fhandle_db_read() must
 *        violate the pds_fhandlet type abstraction.  As discussed in
 *        pds/pds_fhandlet.h, pds_sstorage_manager.c is a "friend" of the
 *        pds_fhandlet ADT in the C++ sense.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - successfully read file handle data base record
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - file handle not located.
 *       PIOUS_EINSUF - insufficient system resources; 'fhandle' record located
 *                      but unable to allocate storage for pathname
 *       PIOUS_EFATAL - FHDB corrupted or inaccessible
 */

#ifdef __STDC__
static int fhandle_db_read(pds_fhandlet fhandle,
			   char **path)
#else
static int fhandle_db_read(fhandle, path)
     pds_fhandlet fhandle;
     char **path;
#endif
{
  pious_ssizet acode;
  pious_offt offset;

  struct fhdb_templatet tmp_rec;
  pds_fhandlet tmp_fhandle;
  char *tmp_path;

  int rcode, done, found;

  struct FS_stat fhdb_status;

  /* determine size of file handle data base; used to detect db corruption
   * beyond the expected potentially corrupt last record.
   */

  if ((acode = FS_fstat(FHDBinfo.fildes, &fhdb_status)) != PIOUS_OK)
    { /* error accessing FHDB */
      SS_errlog("pds_sstorage_manager", "fhandle_db_read()", (int)acode,
		"unable to status file handle database (FHDB)");

      rcode = PIOUS_EFATAL; /* error accessing FHDB */
    }
  else if (fhdb_status.size == 0)
    /* FHDB contains no entries */
    rcode = PIOUS_EBADF;

  else
    { /* search BACKWARDS through file for first complete record as
       * signified by a complete EOR marker
       */

      offset = -(sizeof(fhdb_recordt));

      while ((acode = FS_read(FHDBinfo.fildes, offset, PIOUS_SEEK_END,
			      (pious_sizet)sizeof(fhdb_recordt),
			      (char *)tmp_rec.f)) == sizeof(fhdb_recordt) &&

	     ((tmp_rec.f[F_EORMARKER_0] ^ fhdb_template.f[F_EORMARKER_0]) ||
	      (tmp_rec.f[F_EORMARKER_1] ^ fhdb_template.f[F_EORMARKER_1]) ||
	      (tmp_rec.f[F_EORMARKER_2] ^ fhdb_template.f[F_EORMARKER_2])))
	offset--;

      if (acode != sizeof(fhdb_recordt))
	{ /* unable to locate a complete record */
	  SS_errlog("pds_sstorage_manager", "fhandle_db_read()", 0,
		    "can not locate last file handle database (FHDB) record");

	  rcode = PIOUS_EFATAL;
	}

      else
	{ /* located last complete record; search FHDB for 'fhandle' */
	  done  = FALSE;
	  found = FALSE;

	  while(!done)
	    {
	      tmp_fhandle.dev = tmp_rec.f[F_FHANDLE_DEV];
	      tmp_fhandle.ino = tmp_rec.f[F_FHANDLE_INO];
	      
	      if (fhandle_eq(tmp_fhandle, fhandle))
		{ /* located 'fhandle' in FHDB */
		  done  = TRUE;
		  found = TRUE;
		}

	      else
		{ /* record not 'fhandle'; determine if more to examine */
		  offset -= tmp_rec.f[F_PATHLEN];

		  if (fhdb_status.size + offset == 0)
		    { /* beginning of FHDB; no more records in FHDB */
		      rcode = PIOUS_EBADF;
		      done  = TRUE;
		    }

		  else if (fhdb_status.size + offset < sizeof(fhdb_recordt))
		    { /* FHDB corrupted */
		      SS_errlog("pds_sstorage_manager", "fhandle_db_read()", 0,
				"detected file handle db (FHDB) corruption");

		      rcode = PIOUS_EFATAL;
		      done  = TRUE;
		    }

		  else
		    { /* more records; prepare to examine next record */
		      offset -= sizeof(fhdb_recordt);

		      acode = FS_read(FHDBinfo.fildes, offset, PIOUS_SEEK_END,
				      (pious_sizet)sizeof(fhdb_recordt),
				      (char *)tmp_rec.f);

		      if (acode != sizeof(fhdb_recordt))
			{ /* FHDB corrupted */
			  SS_errlog("pds_sstorage_manager",
				    "fhandle_db_read()", 0,
				    "detected file handle db (FHDB) corrupt");

			  rcode = PIOUS_EFATAL;
			  done  = TRUE;
			}
		    }
		}
	    }

	  /* if 'fhandle' located and does not indicate "unmapping", allocate
           * space for and read path name.
           */

	  if (found)
	    if (tmp_rec.f[F_PATHLEN] == 0)
	      /* FHDB record indicates "unmapping" */
	      rcode = PIOUS_EBADF;

	    else
	      { /* FHDB contains mapping; allocate storage and read pathname */
		if ((tmp_path =
		     malloc((unsigned)tmp_rec.f[F_PATHLEN] + 1)) == NULL)
		  /* unable to allocate storage */
		  rcode = PIOUS_EINSUF;

		else
		  { /* read pathname from FHDB */
		    *(tmp_path + tmp_rec.f[F_PATHLEN]) = '\0';

		    offset -= tmp_rec.f[F_PATHLEN];

		    acode = FS_read(FHDBinfo.fildes, offset, PIOUS_SEEK_END,
				    (pious_sizet)tmp_rec.f[F_PATHLEN],
				    tmp_path);

		    if (acode != tmp_rec.f[F_PATHLEN])
		      { /* error reading path name; FHDB corrupted */
			SS_errlog("pds_sstorage_manager", "fhandle_db_read()",
				  0,
				  "detected file handle db (FHDB) corruption");

			rcode = PIOUS_EFATAL;
			free(tmp_path);
		      }
		    else
		      { /* pathname successfully read */
			*path = tmp_path;
			rcode = PIOUS_OK;
		      }
		  }
	      }
	}
    }

  if (rcode == PIOUS_EFATAL)
    SS_fatalerror = TRUE;

  return rcode;
}




/*
 * fildes_alloc()
 *
 * Parameters:
 *
 *   fic_entry - file information cache entry
 *   cflag     - file creation flag
 *   mode      - file creation access control (permission) bits
 *
 * Allocates file descriptor to specified file information cache (FIC) entry.
 *
 * 'cflag' is the inclusive OR of:
 *    exactly one of: PIOUS_NOCREAT, PIOUS_CREAT and
 *    any combination of: PIOUS_TRUNC
 *
 *
 * Note: Normally 'cflag' should be set to PIOUS_NOCREAT, since the FIC,
 *       by definition, contains information regarding existing files.
 *       However, SS_lookup() may require a file be created/truncated and
 *       hence a file descriptor allocated, since file creation/truncation
 *       is performed via FS_open(). Thus, either fildes_alloc() must provide
 *       creation/truncation ability, or SS_lookup() must alter the file
 *       descriptor mapping table; the former choice is the cleaner of the two
 *       kludges.
 *
 *       In the case that SS_lookup() is using fildes_alloc() for file
 *       creation/truncation, 'fic_entry' is not a true FIC entry but rather
 *       a pseudo entry provided by SS_lookup() only for this purpose.
 *
 *       Writing FS_creat() and FS_trunc() routines is NOT a viable
 *       alternative to the above, as creation/truncation will always fail
 *       whenever the pds_sstorage_manager is using all available descriptors.
 *       This solution allows descriptors to be deallocated for the
 *       purpose of creation/truncation.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - sucessfully allocated a file descriptor.
 *   < 0          - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL       - invalid cflag or file accessability
 *                            (fic_entry->amode) argument
 *       PIOUS_EACCES       - path search permission or access mode denied
 *       PIOUS_ENOENT       - file does not exist and PIOUS_NOCREAT, or
 *                            PIOUS_CREAT and path prefix component does not
 *                            exist or path argument empty
 *       PIOUS_ENAMETOOLONG - path or path component name too long
 *       PIOUS_ENOTDIR      - a component of the path prefix not a directory
 *       PIOUS_EINSUF       - insufficient system resources; unable to
 *                            obtain a file descriptor
 *       PIOUS_ENOSPC       - can not create file; no space to extend
 *                            directory or file system
 *       PIOUS_EUNXP        - unexpected error encountered
 */

#ifdef __STDC__
static int fildes_alloc(fic_entryt *fic_entry,
			int cflag,
			pious_modet mode)
#else
static int fildes_alloc(fic_entry, cflag, mode)
     fic_entryt *fic_entry;
     int cflag;
     pious_modet mode;
#endif
{
  int rcode, ocode, oflag;
  int done, fd_idx;

  /* validate 'cflag' argument */

  if ((cflag & (~(PIOUS_CREAT | PIOUS_TRUNC))) != 0)
    rcode = PIOUS_EINVAL;

  else
    {
      /* set FS_open() oflag value as determined by fic_entry accessability */
      oflag = 0;

      if ((fic_entry->amode & PIOUS_R_OK) && (fic_entry->amode & PIOUS_W_OK))
	oflag |= PIOUS_RDWR;
      else if (fic_entry->amode & PIOUS_R_OK)
	oflag |= PIOUS_RDONLY;
      else if (fic_entry->amode & PIOUS_W_OK)
	oflag |= PIOUS_WRONLY;

      /* set oflag for creation/truncation as determined by 'cflag' */
      oflag |= cflag;

      done    = FALSE;
      fd_idx  = 0;

      while (!done)
	{
	  /* attempt to open file */
	  ocode = FS_open(fic_entry->path, oflag, mode);

	  /* case: successfully opened file */
	  if (ocode >= 0)
	    { /* set fic_entry fildes and file descriptor table entry */
	      fic_entry->fildes   = ocode;
	      fildes_table[ocode] = fic_entry;

	      done  = TRUE;
	      rcode = PIOUS_OK;
	    }

	  /* case: too many file descriptors open */
	  else if (ocode == PIOUS_EINSUF)
	    { /* search fildes table for descriptor to deallocate */
	      while (fd_idx < fildes_total && fildes_table[fd_idx] == NULL)
		fd_idx++;

	      if (fd_idx < fildes_total)
		/* deallocate descriptor and try to open again */
		fildes_free(fildes_table[fd_idx]);
	      else
		{ /* none to deallocate */
		  done  = TRUE;
		  rcode = PIOUS_EINSUF;
		}
	    }
	  
	  /* case: error occured opening file
	   *   this should never happen unless file status changed by
           *   an entity other than PDS OR if performing file
	   *   creation/truncation for SS_lookup()
	   */
	  else
	    {
	      done = TRUE;
	      switch(ocode)
		{
		case PIOUS_EINVAL:
		case PIOUS_EACCES:
		case PIOUS_ENOENT:
		case PIOUS_ENAMETOOLONG:
		case PIOUS_ENOTDIR:
		case PIOUS_EINSUF:
		case PIOUS_ENOSPC:
		  rcode = ocode;
		  break;
		default:
		  rcode = PIOUS_EUNXP;
		  break;
		}
	    }
	}
    }

  return rcode;
}




/*
 * fildes_free()
 *
 * Parameters:
 *
 *   fic_entry - file information cache entry
 *
 * Deallocates file descriptor associated with the specified file information
 * cache (FIC) entry.
 *
 * Returns:
 */

#ifdef __STDC__
static void fildes_free(fic_entryt *fic_entry)
#else
static void fildes_free(fic_entry)
     fic_entryt *fic_entry;
#endif
{
  if (fic_entry->fildes != FILDES_INVALID)
    { /* close fildes */
      FS_close(fic_entry->fildes);

      /* reset corresponding file descriptor table entry */
      fildes_table[fic_entry->fildes] = NULL;

      /* indicate that file descriptor is now invalid */
      fic_entry->fildes = FILDES_INVALID;
    }
}
