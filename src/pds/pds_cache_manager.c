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




/* PIOUS Data Server (PDS): Cache Manager
 *
 * @(#)pds_cache_manager.c	2.2  28 Apr 1995  Moyer
 *
 * The pds_cache_manager exports all functions necessary to access
 * data from a cached data file.  All stable storage access operations,
 * except those required for logging, must pass through the pds_cache_manager
 * to maintain cache coherence.
 *
 * The pds_cache_manager implements a segmented least-recently used (SLRU)
 * caching policy, which is depicted by the following logic diagram:
 *
 *                  ----------------------
 *                  |         MRU        |<--------
 *                  ----------------------        |
 *                  |  PROTECTED Segment |-- Hit -|
 *                  ----------------------        |
 *                  |         LRU        |        |
 *                  ----------------------        |
 *                            |                   |
 *         --------------------                   |
 *         |                                      |
 *         |        ----------------------        |
 *  Miss ---------> |         MRU        |        |
 *                  ----------------------        |
 *                  |PROBATIONARY Segment|-- Hit --
 *                  ----------------------
 *                  |         LRU        |
 *                  ----------------------
 *                             |
 *                             ---- Discarded Lines -------->
 *
 *
 * A SLRU policy keeps lines that have been accessed two or more times in
 * the PROTECTED segment of the cache, provided there is sufficient space.
 * Lines that are accessed only once are kept in the PROBATIONARY segment,
 * given sufficient space, to prevent the cache from being flooded when such
 * lines comprise a sizeable fraction of the workload.
 *
 * The pds_cache_manager also implements a dual write policy: volatile
 * writes operate under a write-through policy while stable writes operate
 * under a write-back (delayed write) policy.  Write operations do not
 * allocate cache blocks.
 *
 * The dual write policy insures data integrity for volatile mode access
 * when client processes fail to properly close files before shutting
 * down the PIOUS system.  Since volatile writes are asynchronous (effectively
 * double-buffered) this should not impose too great of a performance penalty,
 * particulary given that requests must be of a moderate size to get reasonable
 * performance in a distributed environment.
 *
 * The pds_cache_manager operates as follows.  Each CM_read()/CM_write()
 * operation is broken into a number of data block read (read_dblk())/
 * data block write (write_dblk()) operations, where a data block is
 * the unit of caching. A read_dblk() operation that results in a
 * cache-miss causes the appropriate file data block to be loaded into cache,
 * while a read_dblk() operation that results in a cache-hit is satisfied
 * from cache.  A write_dblk() operation that results in a cache-miss is
 * written to stable storage without allocating a cache entry, while a
 * write_dblk() operation that cache hits only accesses stable store if
 * the access is in volatile mode.
 *
 * Note that the semantics of a write operation cause one exceptional condition
 * during a read_dblk() operation. An explicit write operation that begins
 * past the end of file results in an implicit write of zeros (0) to all
 * bytes between the EOF and the start of the explicit write; these semantics
 * are supported in the underlying FS_read()/FS_write() calls and are
 * standard in Unix-type systems.  When a cached entry contains an EOF, by
 * virtue of being an incomplete block, there is a possibility that the EOF
 * is stale as a result of a write that occured past that block. Thus, for
 * read_dblk() operations that cache-hit, if the cached block is incomplete
 * it must be (possibly flushed and) re-read.
 *
 *
 * Function Summary:
 *
 * CM_read();
 * CM_write();
 * CM_flush();
 * CM_fflush();
 * CM_invalidate();
 * CM_finvalidate();
 *
 *
 *
 * ----------------------------------------------------------------------------
 * Implementation Notes:
 *
 *   1) Can increase disk efficiency of CM_flush() if performed on a
 *      per-file basis.
 */

/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#include <memory.h>
#endif

#include <string.h>

#include "gpmacro.h"

#include "pious_types.h"
#include "pious_errno.h"
#include "pious_std.h"
#include "pious_sysconfig.h"

#include "pds_fhandlet.h"
#include "pds_sstorage_manager.h"

#include "pds_cache_manager.h"


/*
 * Private Declarations - Types and Constants
 *
 */


/* Data Block, Cache, and Data Block and File Handle Hash Table Sizes */

/* data block size in bytes; see config/pious_sysconfig.h */
#define DBLK_SZ    PDS_CM_DBLK_SZ

/* cache size in number of data blocks; see config/pious_sysconfig.h */
#if PDS_CM_CACHE_SZ == 1
#define CACHE_SZ   2
#else
#define CACHE_SZ   PDS_CM_CACHE_SZ
#endif

/* data block hash table size; choose prime not near a power of 2 */
#define DBLK_TABLE_SZ     103

/* file handle hash table size; choose prime not near a power of 2 */
#define FH_TABLE_SZ       103


/* Segmented LRU parameters */

/* cache segment in which data block resides */
#define PROTECTED     0
#define PROBATIONARY  1

/* ratio of protected segment size to full cache size (0 < ratio < 1) */
#define PROT_SEG_RATIO    0.70


/* Cache Entry: A cache container for a data block */

typedef struct cache_entry{
  int valid;                  /* cache entry allocated flag */
  int dirty;                  /* cache entry dirty flag */
  int segment;                /* cache segment in which entry resides */
  int faultmode;              /* PIOUS_STABLE or PIOUS_VOLATILE */
  pds_fhandlet fhandle;       /* data block file handle */
  pious_offt db_nmbr;         /* data block number */
  pious_sizet db_nbyte;       /* data block valid byte count */
  char dblk[DBLK_SZ];         /* data block */
  struct cache_entry *cnext;  /* next cache entry in list (towards LRU) */
  struct cache_entry *cprev;  /* prev cache entry in list (towards MRU) */
  struct cache_entry *dbnext; /* next cache entry in data block hash chain */
  struct cache_entry *dbprev; /* prev cache entry in data block hash chain */
  struct cache_entry *fhnext; /* next cache entry in file handle hash chain */
  struct cache_entry *fhprev; /* prev cache entry in file handle hash chain */
} cache_entryt;


/*
 * Private Variable Definitions
 */


/* data block cache */

#if CACHE_SZ == 0
/* for compilation purposes, declare cache even if it will not be used */
static cache_entryt cache[1];
#else
static cache_entryt cache[CACHE_SZ];
#endif

static cache_entryt *cache_mru_pt;      /* MRU protected seg cache entry */
static cache_entryt *cache_mru_pb;      /* MRU probationary seg cache entry */
static cache_entryt *cache_lru_pb;      /* LRU probationary seg cache entry */

/* data block hash table - for general location of data blocks */
static cache_entryt *dblk_table[DBLK_TABLE_SZ];

/* file handle hash table - for locating data blocks associated with fhandle */
static cache_entryt *fh_table[FH_TABLE_SZ];


/* data block cache initilization flag */
static int cache_initialized = FALSE;



/* Local Function Declarations */

#ifdef __STDC__
static pious_ssizet read_dblk(pds_fhandlet fhandle,
			      pious_offt db_nmbr,
			      pious_offt offset,
			      pious_sizet nbyte,
			      char *buf);

static int write_dblk(pds_fhandlet fhandle,
		      pious_offt db_nmbr,
		      pious_offt offset,
		      pious_sizet nbyte,
		      char *buf,
		      int faultmode);

static int cache_alloc(pds_fhandlet fhandle,
		       pious_offt db_nmbr,
		       cache_entryt **cache_entry);

static void entry_validate(cache_entryt *cache_entry);

static void entry_invalidate(cache_entryt *cache_entry);

static void make_mru_pt(cache_entryt *cache_entry);

static void make_mru_pb(cache_entryt *cache_entry);

static void cachemanager_init(void);
#else
static pious_ssizet read_dblk();
static int write_dblk();
static int cache_alloc();
static void entry_validate();
static void entry_invalidate();
static void make_mru_pt();
static void make_mru_pb();
static void cachemanager_init();
#endif




/* Function Definitions - Cache Manager Operations */


/*
 * CM_read() - See pds_cache_manager.h for description
 */

#ifdef __STDC__
pious_ssizet CM_read(pds_fhandlet fhandle,
		     pious_offt offset,
		     pious_sizet nbyte,
		     char *buf)
#else
pious_ssizet CM_read(fhandle, offset, nbyte, buf)
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
#endif
{
  int done;
  pious_ssizet rcode, acode;
  pious_sizet readcount, db_nbyte;
  pious_offt db_nmbr, db_offset;

  /* check for previous fatal error and recover flags */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else if (SS_recover)
    rcode = PIOUS_ERECOV;

  /* validate 'offset' and 'nbyte' arguments */
  else if (offset < 0 || nbyte < 0)
    rcode = PIOUS_EINVAL;

  /* if CACHE_SZ == 0 (no cache), access stable storage directly */
  else if (CACHE_SZ == 0)
    {
      acode = SS_read(fhandle, offset, nbyte, buf);

      if (acode >= 0)
	/* data read successfully */
	rcode = acode;
      else
	/* data read unsuccessful; set return code */
	switch(acode)
	  {
	  case PIOUS_EBADF:
	  case PIOUS_EACCES:
	  case PIOUS_EINVAL:
	  case PIOUS_EINSUF:
	  case PIOUS_EFATAL:
	    rcode = acode;
	    break;
	  default:
	    rcode = PIOUS_EUNXP;
	    break;
	  }
    }

  /* read data from cache */
  else
    { /* initialize cache, if required */
      if (!cache_initialized)
	cachemanager_init();

      /* calculate first data block number, offset, and byte count */
  
      db_nmbr   = offset / DBLK_SZ;
      db_offset = offset % DBLK_SZ;
      db_nbyte  = Min(DBLK_SZ - db_offset, nbyte);

      readcount = 0;
      done      = FALSE;

      /* read data blocks from cache, one block at a time */

      while (!done)
	{
	  /* read appropriate (portion of) data block */
	  acode = read_dblk(fhandle, db_nmbr, db_offset, db_nbyte, buf);

	  if (acode < 0)
	    { /* error, set return code appropriately and exit */
	      rcode = acode;
	      done  = TRUE;
	    }

	  else
	    { /* read next data block, if required */
	      readcount += acode;
	      nbyte     -= acode;

	      if (nbyte == 0 || acode < db_nbyte)
		{ /* transfer complete or hit EOF */
		  rcode = readcount;
		  done  = TRUE;
		}

	      else
		{ /* transfer incomplete; set next data block/offset to read */
		  db_nmbr++;
		  db_offset = 0;
		  db_nbyte  = Min(DBLK_SZ, nbyte);

		  /* increment read buffer pointer for next transfer */
		  buf += acode;
		}
	    }
	}
    }

  return rcode;
}




/*
 * CM_write() - See pds_cache_manager.h for description
 */

#ifdef __STDC__
int CM_write(pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte,
	     char *buf,
	     int faultmode)
#else
int CM_write(fhandle, offset, nbyte, buf, faultmode)
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
     int faultmode;
#endif
{
  int done, rcode, acode;
  pious_sizet writecount, db_nbyte;
  pious_offt db_nmbr, db_offset;

  /* validate 'faultmode' argument; if invalid set to PIOUS_STABLE */
  if (faultmode != PIOUS_STABLE && faultmode != PIOUS_VOLATILE)
    faultmode = PIOUS_STABLE;

  /* check for previous fatal error and recover flags */
  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else if (SS_recover)
    rcode = PIOUS_ERECOV;

  /* validate 'offset' and 'nbyte' arguments; if invalid specify recovery */
  else if (offset < 0 || nbyte < 0)
    { /* note: should NEVER happen if write args validated upon receipt */
      SS_recover = TRUE;
      rcode      = PIOUS_ERECOV;
    }

  /* if CACHE_SZ == 0 (no cache), access stable storage directly */
  else if (CACHE_SZ == 0)
    {
      acode = SS_write(fhandle, offset, nbyte, buf, faultmode);

      if (acode == nbyte)
	{ /* write completed successfully */
	  rcode = PIOUS_OK;
	}
      else
	{ /* write error, or incomplete transfer of data */
	  if (acode == PIOUS_EFATAL)
	    {
	      rcode = PIOUS_EFATAL;
	    }
	  else
	    {
	      SS_recover = TRUE;
	      rcode      = PIOUS_ERECOV;
	    }
	}
    }

  /* write data to cache */
  else
    { /* initialize cache, if required */
      if (!cache_initialized)
	cachemanager_init();

      /* calculate first data block number, offset, and byte count */
  
      db_nmbr    = offset / DBLK_SZ;
      db_offset  = offset % DBLK_SZ;
      db_nbyte   = Min(DBLK_SZ - db_offset, nbyte);

      writecount = 0;
      done       = FALSE;

      /* write data blocks to cache, one block at a time */

      while (!done)
	{
	  /* write appropriate (portion of) data block */
	  acode = write_dblk(fhandle, db_nmbr, db_offset, db_nbyte, buf,
			     faultmode);

	  if (acode != PIOUS_OK)
	    { /* error, set return code appropriately and exit */
	      rcode = acode;
	      done  = TRUE;
	    }

	  else
	    { /* write next data block, if required */
	      writecount += db_nbyte;
	      nbyte      -= db_nbyte;

	      if (nbyte == 0)
		{ /* transfer complete */
		  rcode = PIOUS_OK;
		  done  = TRUE;
		}

	      else
		{ /* transfer incomplete; increment write buffer pointer */
		  buf += db_nbyte;

		  /* compute next data block/offset to write */
		  db_nmbr++;
		  db_offset = 0;
		  db_nbyte  = Min(DBLK_SZ, nbyte);
		}
	    }
	}
    }

  return rcode;
}




/*
 * CM_flush() - See pds_cache_manager.h for description.
 */

#ifdef __STDC__
int CM_flush(void)
#else
int CM_flush()
#endif
{
  int rcode;
  pious_ssizet acode;
  register cache_entryt *cache_pos;

  /* check for previous fatal error and recover flags */

  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else if (SS_recover)
    rcode = PIOUS_ERECOV;

  /* flush cached data blocks */

  else if (CACHE_SZ == 0)
    /* if no cache then flushed by definition */
    rcode = PIOUS_OK;

  else if (!cache_initialized)
    { /* newly initialized cache is flushed by definition */
      cachemanager_init();
      rcode = PIOUS_OK;
    }

  else
    { /* search through entire cache flushing dirty entries */
      rcode     = PIOUS_OK;
      cache_pos = cache_mru_pt;

      do
	{
	  if (cache_pos->valid && cache_pos->dirty)
	    { /* flush cache block */
	      acode = SS_write(cache_pos->fhandle,
			       (pious_offt)(cache_pos->db_nmbr * DBLK_SZ),
			       cache_pos->db_nbyte,
			       cache_pos->dblk,
			       cache_pos->faultmode);

	      if (acode < cache_pos->db_nbyte)
		{ /* error, or incomplete transfer of data */
		  if (acode == PIOUS_EFATAL)
		    rcode = PIOUS_EFATAL;
		  else
		    rcode = PIOUS_EUNXP;
		}
	      else
		{ /* block flushed; mark as clean */
		  cache_pos->dirty     = FALSE;
		  cache_pos->faultmode = PIOUS_VOLATILE;
		}
	    }

	  cache_pos = cache_pos->cnext;
	}
      while (cache_pos != cache_mru_pt && rcode == PIOUS_OK);
    }

  return rcode;
}




/*
 * CM_fflush() - See pds_cache_manager.h for description.
 */

#ifdef __STDC__
int CM_fflush(pds_fhandlet fhandle)
#else
int CM_fflush(fhandle)
     pds_fhandlet fhandle;
#endif
{
  int rcode;
  pious_ssizet acode;
  register cache_entryt *cache_entry;

  /* check for previous fatal error and recover flags */

  if (SS_fatalerror)
    rcode = PIOUS_EFATAL;

  else if (SS_recover)
    rcode = PIOUS_ERECOV;

  /* flush cached data blocks associated with 'fhandle' */

  else if (CACHE_SZ == 0)
    /* if no cache then flushed by definition */
    rcode = PIOUS_OK;

  else if (!cache_initialized)
    { /* newly initialized cache is flushed by definition */
      cachemanager_init();
      rcode = PIOUS_OK;
    }

  else
    { /* search file handle hash chain for 'fhandle' data blocks */
      rcode       = PIOUS_OK;
      cache_entry = fh_table[fhandle_hash(fhandle, FH_TABLE_SZ)];

      while (cache_entry != NULL && rcode == PIOUS_OK)
	{
	  if (fhandle_eq(cache_entry->fhandle, fhandle) &&
	      cache_entry->dirty)
	    { /* block belongs to 'fhandle' and is dirty; flush */

	      acode = SS_write(cache_entry->fhandle,
			       (pious_offt)(cache_entry->db_nmbr * DBLK_SZ),
			       cache_entry->db_nbyte,
			       cache_entry->dblk,
			       cache_entry->faultmode);

	      if (acode < cache_entry->db_nbyte)
		{ /* error, or incomplete transfer of data */
		  if (acode == PIOUS_EFATAL)
		    rcode = PIOUS_EFATAL;
		  else
		    rcode = PIOUS_EUNXP;
		}
	      else
		{ /* block flushed; mark as clean */
		  cache_entry->dirty     = FALSE;
		  cache_entry->faultmode = PIOUS_VOLATILE;
		}
	    }

	  cache_entry = cache_entry->fhnext;
	}
    }

  return rcode;
}




/*
 * CM_invalidate() - See pds_cache_manager.h for description.
 */

#ifdef __STDC__
void CM_invalidate(void)
#else
void CM_invalidate()
#endif
{
  int i;

  if (CACHE_SZ != 0)
    {
      if (!cache_initialized)
	{ /* newly initialized cache is invalidated by definition */
	  cachemanager_init();
	}

      else
	{ /* scan cache, invalidating all entries */
	  for (i = 0; i < CACHE_SZ; i++)
	    cache[i].valid = FALSE;

	  /* reset data block and file handle hash tables */
	  for (i = 0; i < DBLK_TABLE_SZ; i++)
	    dblk_table[i] = NULL;

	  for (i = 0; i < FH_TABLE_SZ; i++)
	    fh_table[i] = NULL;
	}
    }
}




/*
 * CM_finvalidate() - See pds_cache_manager.h for description.
 */

#ifdef __STDC__
void CM_finvalidate(pds_fhandlet fhandle)
#else
void CM_finvalidate(fhandle)
     pds_fhandlet fhandle;
#endif
{
  register cache_entryt *cache_entry, *next_entry;

  if (CACHE_SZ != 0)
    {
      if (!cache_initialized)
	{ /* newly initialized cache is invalidated by definition */
	  cachemanager_init();
	}

      else
	{ /* invalidate all 'fhandle' entries */
	  cache_entry = fh_table[fhandle_hash(fhandle, FH_TABLE_SZ)];

	  while (cache_entry != NULL)
	    { /* search 'fhandle' hash chain */
	      next_entry = cache_entry->fhnext;

	      if (fhandle_eq(cache_entry->fhandle, fhandle))
		/* block belongs to 'fhandle'; invalidate */
		entry_invalidate(cache_entry);

	      cache_entry = next_entry;
	    }
	}
    }
}




/* Function Defintions - Local Functions */




/*
 * hash_dblk()
 *
 * Parameters:
 *
 *   db_nmbr - data block number
 *
 * Calculates data block hash value in range 0..(DBLK_TABLE_SZ - 1).
 *
 * Returns:
 *
 *   0..(DBLK_TABLE_SZ - 1) - if db_nmbr > 0
 *   0                      - if db_nmbr <= 0
 */

/*
static int hash_dblk(pious_offt db_nmbr)
*/

#define hash_dblk(db_nmbr) \
((int)(((db_nmbr) <= 0) ? 0 : ((db_nmbr) % DBLK_TABLE_SZ)))




/*
 * read_dblk()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *   db_nmbr - data block number
 *   offset  - data block offset (offset from start of data block)
 *   nbyte   - byte count
 *   buf     - buffer
 *
 * Read data block 'db_nmbr' of file 'fhandle' starting at 'offset' bytes
 * from the beginning of the block and proceeding for 'nbyte' bytes; place
 * result in buffer 'buf'.
 *
 * Returns:
 *
 *   >= 0 - number of bytes read and placed in buffer (<= nbyte)
 *   <  0 - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EBADF  - invalid/stale 'fhandle' argument
 *       PIOUS_EACCES - read is invalid access mode for 'fhandle'
 *       PIOUS_EINVAL - file offset or nbyte is not a proper value
 *                      or exceeds SYSTEM constraints
 *       PIOUS_EINSUF - insufficient system resources; retry operation
 *       PIOUS_EUNXP  - unexpected error condition encountered
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
static pious_ssizet read_dblk(pds_fhandlet fhandle,
			      pious_offt db_nmbr,
			      pious_offt offset,
			      pious_sizet nbyte,
			      char *buf)
#else
static pious_ssizet read_dblk(fhandle, db_nmbr, offset, nbyte, buf)
     pds_fhandlet fhandle;
     pious_offt db_nmbr;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
#endif
{
  int copyout, badflush, cachehit;
  pious_ssizet rcode, acode;
  cache_entryt *cache_entry;

  /* a null read always succeeds without perturbing cache */
  if (nbyte == 0)
    {
      rcode = 0;
    }

  else
    { /* allocate a cache entry for (fhandle, db_nmbr) */

      acode = cache_alloc(fhandle, db_nmbr, &cache_entry);

      if (acode != PIOUS_OK)
	{ /* alloc failed; acode == PIOUS_EFATAL || acode == PIOUS_ERECOV */
	  rcode = acode;
	}

      else
	{ /* cache entry allocated; read data block and/or copy out data */

	  if (cache_entry->valid)
	    cachehit = TRUE;
	  else
	    cachehit = FALSE;

	  copyout = TRUE;

	  if (!cache_entry->valid || cache_entry->db_nbyte < DBLK_SZ)
	    { /* cache entry invalid or contains a potentially stale EOF
	       * resulting from an implied write; see discussion at top.
	       * (re)read data from file in either case; if valid and dirty,
               * must flush first.
	       */

	      badflush = FALSE;

	      if (cache_entry->valid && cache_entry->dirty)
		{ /* cache entry is valid and dirty; attempt to flush */
		  acode = SS_write(fhandle,
				   (pious_offt)(db_nmbr * DBLK_SZ),
				   cache_entry->db_nbyte,
				   cache_entry->dblk,
				   cache_entry->faultmode);

		  if (acode != cache_entry->db_nbyte)
		    { /* block flush unsuccessful */
		      badflush = TRUE;
		      copyout  = FALSE;
		      rcode    = PIOUS_EUNXP;
		    }
		}

	      if (!badflush)
		{ /* read data block from stable storage; at this point the
                   * alloced cache entry is either invalid, or valid and clean
                   */

		  acode = SS_read(fhandle,
				  (pious_offt)(db_nmbr * DBLK_SZ),
				  DBLK_SZ,
				  cache_entry->dblk);

		  if (acode > 0)
		    { /* data block read successful; set cache entry fields */
		      cache_entry->db_nbyte  = acode;
		      cache_entry->dirty     = FALSE;
		      cache_entry->faultmode = PIOUS_VOLATILE;

		      if (!cache_entry->valid)
			/* mark valid, put on data block/fhandle hash chains */
			entry_validate(cache_entry);
		    }

		  else
		    { /* data block read unsuccessful; set return code */
		      copyout = FALSE;

		      switch(acode)
			{
			case 0: /* data block read at/past EOF */
			case PIOUS_EBADF:
			case PIOUS_EACCES:
			case PIOUS_EINVAL:
			case PIOUS_EINSUF:
			case PIOUS_EFATAL:
			  rcode = acode;
			  break;
			default:
			  rcode = PIOUS_EUNXP;
			  break;
			}

		      if (cache_entry->valid)
			/* invalidate cache entry, if valid */
			entry_invalidate(cache_entry);
		    }
		}
	    }

	  if (copyout)
	    { /* copy requested data to buffer as available */

	      if (offset >= cache_entry->db_nbyte)
		{ /* request starts at/past EOF; no data transfered */
		  rcode = 0;
		}

	      else
		{ /* request starts within valid data; transfer */
		  rcode = Min(nbyte, cache_entry->db_nbyte - offset);

		  memcpy(buf, (cache_entry->dblk) + offset, (int)rcode);
		}

	      /* move cache entry to MRU position of appropriate segment */
	      if (cachehit)
		make_mru_pt(cache_entry);
	      else
		make_mru_pb(cache_entry);
	    }
	}
    }

  return rcode;
}




/*
 * write_dblk()
 *
 * Parameters:
 *
 *   fhandle   - file handle
 *   db_nmbr   - data block number
 *   offset    - data block offset (offset from start of data block)
 *   nbyte     - byte count
 *   buf       - buffer
 *   faultmode - PIOUS_STABLE or PIOUS_VOLATILE
 *
 * Write to data block 'db_nmbr' of file 'fhandle' starting at 'offset' bytes
 * from the beginning of the block and proceeding for 'nbyte' bytes the data
 * in buffer 'buf'.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - file data block successfully written
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
static int write_dblk(pds_fhandlet fhandle,
		      pious_offt db_nmbr,
		      pious_offt offset,
		      pious_sizet nbyte,
		      char *buf,
		      int faultmode)
#else
static int write_dblk(fhandle, db_nmbr, offset, nbyte, buf, faultmode)
     pds_fhandlet fhandle;
     pious_offt db_nmbr;
     pious_offt offset;
     pious_sizet nbyte;
     char *buf;
     int faultmode;
#endif
{
  int rcode;
  pious_ssizet acode;
  register cache_entryt *cache_entry;

  /* a null write always succeeds without perturbing cache */
  if (nbyte == 0)
    {
      rcode = PIOUS_OK;
    }

  else
    { /* search cache via data block hash chain for (fhandle, db_nmbr) pair */

      cache_entry = dblk_table[hash_dblk(db_nmbr)];

      while (cache_entry != NULL &&
	     (cache_entry->db_nmbr != db_nmbr ||
	      !fhandle_eq(cache_entry->fhandle, fhandle)))
	cache_entry = cache_entry->dbnext;

      /* if cache-miss or volatile write then write-through w/o allocation */

      rcode = PIOUS_OK;

      if (cache_entry == NULL || faultmode == PIOUS_VOLATILE)
	{
	  if ((acode = SS_write(fhandle,
				(pious_offt)((db_nmbr * DBLK_SZ) + offset),
				nbyte,
				buf,
				faultmode)) != nbyte)
	    { /* write error, or incomplete transfer of data */
	      if (acode == PIOUS_EFATAL)
		{ /* fatal error occured */
		  rcode = PIOUS_EFATAL;
		}
	      else
		{ /* non-fatal error; mark for recovery */
		  SS_recover = TRUE;
		  rcode      = PIOUS_ERECOV;
		}
	    }
	}

      /* if cache-hit then update cached block */

      if (cache_entry != NULL && rcode == PIOUS_OK)
	{ /* fill void between last valid byte and start of write */
	  if (offset > cache_entry->db_nbyte)
	    memset((cache_entry->dblk) + (cache_entry->db_nbyte), 0,
		   (int)(offset - cache_entry->db_nbyte));

	  /* copy updated data into cached block */
	  memcpy((cache_entry->dblk) + offset, buf, (int)nbyte);

	  /* reset data block byte count */
	  cache_entry->db_nbyte = Max(cache_entry->db_nbyte,
				      offset + nbyte);

	  /* if write faultmode is stable, mark block as dirty and stable */
	  if (faultmode == PIOUS_STABLE)
	    {
	      cache_entry->dirty     = TRUE;
	      cache_entry->faultmode = PIOUS_STABLE;
	    }

	  /* move cache entry to MRU position of protected segment */
	  make_mru_pt(cache_entry);
	}
    }

  return rcode;
}




/*
 * cache_alloc()
 *
 * Parameters:
 *
 *   fhandle     - file handle
 *   db_nmbr     - data block number
 *   cache_entry - allocated cache entry
 *
 * Allocate a cache entry for data block 'db_nmbr' of file 'fhandle', placing
 * a pointer to the cache entry in 'cache_entry'.
 *
 * If the requested data block resided in the cache prior to calling
 * cache_alloc(), then 'cache_entry' contains valid data.  Otherwise,
 * a cache entry is allocated from the PROBATIONARY segment of the
 * cache (cache_lru_pb in all but exceptional circumstances) and
 * 'cache_entry' is marked invalid.
 *
 * NOTE: if successful then upon return:
 *         1) cache_entry->fhandle == fhandle and
 *         2) cache_entry->db_nmbr == db_nmbr
 *       even if cache_entry->valid == FALSE
 *
 * Returns:
 *
 *   PIOUS_OK (0) - data block successfully allocated
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_ERECOV - recovery required
 *       PIOUS_EFATAL - fatal error; check PDS error log
 */

#ifdef __STDC__
static int cache_alloc(pds_fhandlet fhandle,
		       pious_offt db_nmbr,
		       cache_entryt **cache_entry)
#else
static int cache_alloc(fhandle, db_nmbr, cache_entry)
     pds_fhandlet fhandle;
     pious_offt db_nmbr;
     cache_entryt **cache_entry;
#endif
{
  int rcode, done, found;
  pious_ssizet fcode;
  register cache_entryt* cache_pos;

  /* search cache via data block hash chain for (fhandle, db_nmbr) pair */

  cache_pos = dblk_table[hash_dblk(db_nmbr)];

  while (cache_pos != NULL && (cache_pos->db_nmbr != db_nmbr ||
			       !fhandle_eq(cache_pos->fhandle, fhandle)))
    cache_pos = cache_pos->dbnext;

  if (cache_pos != NULL)
    { /* located (fhandle, db_nmbr) pair in cache */
      *cache_entry = cache_pos;
      rcode        = PIOUS_OK;
    }
  else
    { /* did NOT locate (fhandle, db_nmbr) pair in cache; allocate entry */

      done  = FALSE;
      found = FALSE;

      cache_pos = cache_lru_pb;

      do
	{ /* working from LRU to MRU entry of probationary segment, find the
           * first cache entry such that:
           *   1) entry is not valid, or
           *   2) entry is valid but not dirty, or
           *   3) entry is valid and dirty but can be successfully flushed
           *      to stable storage.
           * if no such entry can be found then PDS can not continue and
           * recovery will be required.
           *
	   */

	  if (cache_pos->valid == FALSE || cache_pos->dirty == FALSE)
	    { /* cache entry is not valid, or is valid but not dirty */
	      found = done = TRUE;
	    }

	  else
	    { /* cache entry is valid and dirty; attempt to flush */
	      fcode = SS_write(cache_pos->fhandle,
			       (pious_offt)(cache_pos->db_nmbr * DBLK_SZ),
			       cache_pos->db_nbyte,
			       cache_pos->dblk,
			       cache_pos->faultmode);

	      if (fcode == cache_pos->db_nbyte)
		/* block flush successful */
		found = done = TRUE;

	      else if (fcode == PIOUS_EFATAL)
		/* PDS can not continue; SS_fatalerror set by Stable Storage
                 * Manager.
		 */
		done = TRUE;
	      else
		/* block flush unsuccessful but not fatal, go to next block */
		cache_pos = cache_pos->cprev;
	    }
	}
      while (!done && cache_pos->segment == PROBATIONARY);

      if (SS_fatalerror)
	{ /* indicate that a fatal error occured */
	  rcode = PIOUS_EFATAL;
	}

      else if (!found)
	{ /* indicate that a cache entry can not be successfully allocated */
	  SS_recover = TRUE;
	  rcode      = PIOUS_ERECOV;
	}

      else
	{ /* successfully located/flushed a cache entry to allocate */

	  /* if cache entry to replace is valid, invalidate it */
	  if (cache_pos->valid)
	    entry_invalidate(cache_pos);

	  /* update relevant cache entry fields */
	  cache_pos->fhandle  = fhandle;
	  cache_pos->db_nmbr  = db_nmbr;

	  /* set return values */
	  *cache_entry = cache_pos;
	  rcode        = PIOUS_OK;
	}
    }

  return rcode;
}




/*
 * entry_validate()
 *
 * Parameters:
 *
 *   cache_entry - invalid cache entry
 *
 * Place INVALID cache entry 'cache_entry' on appripriate data block and
 * file handle hash chains and mark as valid.  Does not move 'cache_entry'
 * in cache list.
 *
 * NOTE: Assumes:
 *         1) cache_entry->db_nmbr and
 *         2) cache_entry->fhandle
 *       have been assigned the appropriate (valid) data block number and
 *       file handle values, respectively.
 *
 * Returns:
 */

#ifdef __STDC__
static void entry_validate(cache_entryt *cache_entry)
#else
static void entry_validate(cache_entry)
     cache_entryt *cache_entry;
#endif
{
  if (!cache_entry->valid)
    { /* place 'cache_entry' at head of data block hash chain */

      cache_entry->dbprev = NULL;
      cache_entry->dbnext = dblk_table[hash_dblk(cache_entry->db_nmbr)];
      dblk_table[hash_dblk(cache_entry->db_nmbr)] = cache_entry;

      /* set 'prev' pointer of former head of hash chain, if extant */
      if (cache_entry->dbnext != NULL)
	cache_entry->dbnext->dbprev = cache_entry;

      /* place 'cache_entry' at head of fhandle hash chain */

      cache_entry->fhprev = NULL;
      cache_entry->fhnext = fh_table[fhandle_hash(cache_entry->fhandle,
						  FH_TABLE_SZ)];
      fh_table[fhandle_hash(cache_entry->fhandle, FH_TABLE_SZ)] = cache_entry;

      /* set 'prev' pointer of former head of hash chain, if extant */
      if (cache_entry->fhnext != NULL)
	cache_entry->fhnext->fhprev = cache_entry;


      /* mark entry as valid */
      cache_entry->valid = TRUE;
    }
}




/*
 * entry_invalidate()
 *
 * Parameters:
 *
 *   cache_entry - valid cache entry
 *
 * Remove VALID cache entry 'cache_entry' from associated data block and
 * file handle hash chains and mark as invalid.  Does not move 'cache_entry'
 * in cache list.
 *
 * Returns:
 */

#ifdef __STDC__
static void entry_invalidate(cache_entryt *cache_entry)
#else
static void entry_invalidate(cache_entry)
     cache_entryt *cache_entry;
#endif
{
  if (cache_entry->valid)
    { /* remove 'cache_entry' from data block hash chain */

      /* if not last in chain, reset 'prev' pointer of 'next' entry */
      if (cache_entry->dbnext != NULL)
	cache_entry->dbnext->dbprev = cache_entry->dbprev;

      /* if not first in chain, reset 'next' pointer of 'prev' entry */
      if (cache_entry->dbprev != NULL)
	cache_entry->dbprev->dbnext = cache_entry->dbnext;
      else
	/* first in chain, reset data block hash table entry */
	dblk_table[hash_dblk(cache_entry->db_nmbr)] = cache_entry->dbnext;

      /* remove 'cache_entry' from fhandle hash chain */

      /* if not last in chain, reset 'prev' pointer of 'next' entry */
      if (cache_entry->fhnext != NULL)
	cache_entry->fhnext->fhprev = cache_entry->fhprev;

      /* if not first in chain, reset 'next' pointer of 'prev' entry */
      if (cache_entry->fhprev != NULL)
	cache_entry->fhprev->fhnext = cache_entry->fhnext;
      else
	/* first in chain, reset fhandle hash table entry */
	fh_table[fhandle_hash(cache_entry->fhandle, FH_TABLE_SZ)] =
	  cache_entry->fhnext;


      /* mark 'cache_entry' as invalid */
      cache_entry->valid = FALSE;
    }
}




/*
 * make_mru_pt()
 *
 * Parameters:
 *
 *   cache_entry - cache entry
 *
 * Insert the data block cache entry 'cache_entry' at the MRU position of
 * the PROTECTED segment.  Prior to calling make_mru_pt(), 'cache_entry' may
 * reside in either the protected or the probationary segment of the cache.
 *
 * Note: global variables 'cache_mru_pt', 'cache_mru_pb', and 'cache_lru_pb'
 *       may be altered as a side-effect
 *
 * Returns:
 */

#ifdef __STDC__
static void make_mru_pt(cache_entryt *cache_entry)
#else
static void make_mru_pt(cache_entry)
     cache_entryt *cache_entry;
#endif
{
  /* put 'cache_entry' at MRU position of the protected segment */
  if (cache_entry != cache_mru_pt)
    {
      if (cache_entry == cache_lru_pb)
	{ /* special case; advance MRU and LRU pointers around circular list */
	  cache_mru_pt = cache_lru_pb;
	  cache_lru_pb = cache_lru_pb->cprev;
	  cache_mru_pb = cache_mru_pb->cprev;

	  /* set segment types appropriately */
	  cache_mru_pt->segment = PROTECTED;
	  cache_mru_pb->segment = PROBATIONARY;
	}
      else
	{ /* remove from list and place in MRU position of protected segment */

	  /* if removing entry from probationary seg, make LRU entry of
           * protected seg the new MRU entry of the probationary seg
	   */
	  if (cache_entry->segment == PROBATIONARY)
	    {
	      cache_mru_pb          = cache_mru_pb->cprev;
	      cache_mru_pb->segment = PROBATIONARY;
	    }
	    
	  /* remove *cache_entry from cache block list */
	  cache_entry->cprev->cnext = cache_entry->cnext;
	  cache_entry->cnext->cprev = cache_entry->cprev;

	  /* set *cache_entry pointers/seg-type appropriately for MRU entry */
	  cache_entry->cprev   = cache_lru_pb;
	  cache_entry->cnext   = cache_mru_pt;
	  cache_entry->segment = PROTECTED;

	  /* set *cache_mru_pt and *cache_lru_pb pointers to new MRU entry */
	  cache_mru_pt->cprev = cache_entry;
	  cache_lru_pb->cnext = cache_entry;

	  /* set MRU pointer to cache_entry */
	  cache_mru_pt = cache_entry;
	}
    }
}




/*
 * make_mru_pb()
 *
 * Parameters:
 *
 *   cache_entry - cache entry
 *
 * Insert the data block cache entry 'cache_entry' at the MRU position of
 * the PROBATIONARY segment.  Prior to calling make_mru_pb(), 'cache_entry'
 * MUST reside in the probationary segment of the cache.
 *
 * Note: global variables 'cache_mru_pb' and 'cache_lru_pb' may be altered
 *       as a side-effect
 *
 * Returns:
 */

#ifdef __STDC__
static void make_mru_pb(cache_entryt *cache_entry)
#else
static void make_mru_pb(cache_entry)
     cache_entryt *cache_entry;
#endif
{
  /* put 'cache_entry' at MRU position of the probationary segment */
  if (cache_entry != cache_mru_pb && cache_entry->segment == PROBATIONARY)
    {
      /* remove from list and place in MRU position of probationary segment */

      /* remove *cache_entry from cache block list */
      cache_entry->cprev->cnext = cache_entry->cnext;
      cache_entry->cnext->cprev = cache_entry->cprev;

      if (cache_entry == cache_lru_pb)
	cache_lru_pb = cache_entry->cprev;

      /* set *cache_entry pointers appropriately for MRU entry */
      cache_entry->cprev = cache_mru_pb->cprev;
      cache_entry->cnext = cache_mru_pb;

      /* set *cache_mru_pb and *(cache_mru_pb->cprev) ptrs to new MRU entry */
      cache_mru_pb->cprev->cnext = cache_entry;
      cache_mru_pb->cprev        = cache_entry;

      /* set MRU pointer to cache_entry */
      cache_mru_pb = cache_entry;
    }
}




/*
 * cachemanager_init()
 *
 * Parameters:
 *
 * Initializes the data block cache in preparation for access.
 *
 * Returns:
 */

#ifdef __STDC__
static void cachemanager_init(void)
#else
static void cachemanager_init()
#endif
{
#if CACHE_SZ > 0 /* no initialization if no cache */
  register cache_entryt *cache_pos;
  int prot_seg_sz;

  /* initialize data block cache as a doubly linked circular list.
   * presumes CACHE_SZ >= 2, as set when CACHE_SZ is defined above.
   */

  /* determine number of protected segment cache entries:
   *   1 <= prot_seg_sz <= (CACHE_SZ - 1)
   */

  prot_seg_sz = Min(Max((int)(CACHE_SZ * PROT_SEG_RATIO), 1), CACHE_SZ - 1);

  /* initialize all but first and last */

  for (cache_pos = cache + 1; cache_pos < cache + (CACHE_SZ - 1); cache_pos++)
    {
      cache_pos->cnext = cache_pos + 1;
      cache_pos->cprev = cache_pos - 1;
      cache_pos->valid = FALSE;

      if (cache_pos < cache + prot_seg_sz)
	cache_pos->segment = PROTECTED;
      else
	cache_pos->segment = PROBATIONARY;
    }

  /* initialize first */
  cache[0].cnext   = cache + 1;
  cache[0].cprev   = cache + (CACHE_SZ - 1);
  cache[0].valid   = FALSE;
  cache[0].segment = PROTECTED;

  /* initialize last */
  cache[CACHE_SZ - 1].cnext   = cache;
  cache[CACHE_SZ - 1].cprev   = cache + (CACHE_SZ - 2);
  cache[CACHE_SZ - 1].valid   = FALSE;
  cache[CACHE_SZ - 1].segment = PROBATIONARY;

  /* set MRU protected/probationary and LRU probationary seg pointers */

  cache_mru_pt = cache;
  cache_mru_pb = cache + prot_seg_sz;
  cache_lru_pb = cache + (CACHE_SZ - 1);
#endif

  /* indicate that initilization has taken place */

  cache_initialized = TRUE;
}
