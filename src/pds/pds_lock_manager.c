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




/* PIOUS Data Server (PDS): Lock Manager
 * 
 * @(#)pds_lock_manager.c	2.2  28 Apr 1995  Moyer
 *
 * The pds_lock_manager exports all locking functions necessary to implement
 * a Strict Two-Phase Locking protocol (2PL) for transaction operations.
 *
 * Function Summary:
 *
 *   LM_rlock();
 *   LM_wlock();
 *   LM_rfree();
 *   LM_wfree();
 *
 */

/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include "gpmacro.h"

#include "pious_types.h"

#include "pds_transidt.h"
#include "pds_fhandlet.h"

#include "pds_lock_manager.h"


/*
 * Private Declarations - Functions, Types, and Constants
 *
 */


#define READ     0 /* Read lock */
#define WRITE    1 /* Write lock */
#define INSERT   0 /* Perform insert during lookup operations */
#define NOINSERT 1 /* Do NOT perform insert during lookup operations */


/* File Handle and Transaction Id hash table sizes */

#define FH_TABLE_SZ 103 /* Choose prime not near a power of 2 */
#define TI_TABLE_SZ 103 /* Choose prime not near a power of 2 */

/* Lock Table Entry: Lock associated with a (transaction id, file handle) */

struct fh_entry; /* defined below */
struct ti_entry; /* defined below */

typedef struct lock_entry{
  pds_transidt transid;      /* transaction id of lock owner */
  pious_offt start;          /* lock start position */
  pious_offt stop;           /* lock stop position */
  int lock;                  /* lock type */
  struct lock_entry *lnext;  /* next lock associated with file */
  struct lock_entry *lprev;  /* previous lock associated with file */
  struct lock_entry *onext;  /* next lock owned by transaction, any file */
  struct fh_entry *fhchain;  /* reference back to file handle hash chain */
} lock_entryt;

/* File Handle Hash Table: File handle hash chain entry */

typedef struct fh_entry{
  pds_fhandlet fhandle;     /* file handle */
  lock_entryt *locks;       /* locks associated with fhandle */
  struct fh_entry *fhnext;  /* next entry in fhandle hash chain */
  struct fh_entry *fhprev;  /* previous entry in fhandle hash chain */
} fh_entryt;

/* Transaction Id Hash Table: Transaction Id hash chain entry */

typedef struct ti_entry{
  pds_transidt transid;    /* transaction id */
  lock_entryt *rlocks;     /* Read lock list */
  lock_entryt *wlocks;     /* Write lock list */
  struct ti_entry *tinext; /* next entry in transid hash chain */
  struct ti_entry *tiprev; /* previous entry in transid hash chain */
} ti_entryt;

/* Local Functions */

#ifdef __STDC__
static int getlock(pds_transidt transid,
		   pds_fhandlet fhandle,
		   pious_offt start,
		   pious_offt stop,
		   int lock);

static void ti_freelocks(pds_transidt transid,
			 int lock);

static fh_entryt *fh_lookup(pds_fhandlet fhandle,
			    int action);

static void fh_rm(fh_entryt *fh_entry);

static ti_entryt *ti_lookup(pds_transidt transid,
			    int action);

static void ti_rm(ti_entryt *ti_entry);

static lock_entryt *lock_insert(fh_entryt *fh_entry,
				ti_entryt *ti_entry,
				pds_transidt transid,
				pious_offt start,
				pious_offt stop,
				int lock);

static void lock_rm(lock_entryt *lock_entry);
#else
static int getlock();
static void ti_freelocks();

static fh_entryt *fh_lookup();
static void fh_rm();

static ti_entryt *ti_lookup();
static void ti_rm();

static lock_entryt *lock_insert();
static void lock_rm();
#endif


/*
 * Private Variable Definitions
 *
 */

static fh_entryt *fh_table[FH_TABLE_SZ]; /* File handle hash table */
static ti_entryt *ti_table[TI_TABLE_SZ]; /* Transaction Id hash table */




/* Function Definitions */




/*
 * LM_rlock() - See pds_lock_manager.h for description.
 */

#ifdef __STDC__
int LM_rlock(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte)
#else
int LM_rlock(transid, fhandle, offset, nbyte)
     pds_transidt transid;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
#endif
{
  return getlock(transid, fhandle, offset,
		 offset + (pious_offt)(nbyte - 1), READ);
}




/*
 * LM_wlock() - See pds_lock_manager.h for description.
 */

#ifdef __STDC__
int LM_wlock(pds_transidt transid,
	     pds_fhandlet fhandle,
	     pious_offt offset,
	     pious_sizet nbyte)
#else
int LM_wlock(transid, fhandle, offset, nbyte)
     pds_transidt transid;
     pds_fhandlet fhandle;
     pious_offt offset;
     pious_sizet nbyte;
#endif
{
  return getlock(transid, fhandle, offset,
		 offset + (pious_offt)(nbyte - 1), WRITE);
}




/*
 * LM_rfree() - See pds_lock_manager.h for description.
 *
 */

#ifdef __STDC__
void LM_rfree(pds_transidt transid)
#else
void LM_rfree(transid)
     pds_transidt transid;
#endif
{
  ti_freelocks(transid, READ);
}




/*
 * LM_wfree() - See pds_lock_manager.h for description.
 *
 */

#ifdef __STDC__
void LM_wfree(pds_transidt transid)
#else
void LM_wfree(transid)
     pds_transidt transid;
#endif
{
  ti_freelocks(transid, WRITE);
}




/*
 * getlock()
 *
 * Parameters:
 *   transid  - transaction id
 *   fhandle  - file handle
 *   start    - start byte
 *   stop     - stop byte
 *   lock     - lock type
 *
 * Obtain requested lock and place in table, if no conflicting locks.
 *
 * Returns:
 *
 *   LM_GRANT - lock granted
 *   LM_DENY  - lock denied
 */

#ifdef __STDC__
static int getlock(pds_transidt transid,
		   pds_fhandlet fhandle,
		   pious_offt start,
		   pious_offt stop,
		   int lock)
#else
static int getlock(transid, fhandle, start, stop, lock)
     pds_transidt transid;
     pds_fhandlet fhandle;
     pious_offt start;
     pious_offt stop;
     int lock;
#endif
{
  int owned, conflict, result;
  fh_entryt *fh_entry;
  ti_entryt *ti_entry;
  register lock_entryt *lock_entry;

  /* Validate 'start', 'stop', and 'lock' arguments */

  if ((lock != READ && lock != WRITE) || stop < start)
    return(LM_DENY);

  /* Locate/insert fhandle in lock table (fhandle hash chain) */
  fh_entry = fh_lookup(fhandle, INSERT);

  if (fh_entry == NULL) /* fh_lookup() couldn't allocate space */
    result = LM_DENY;

  else /* check for conflicting locks */
    {
      /* locks maintained in ascending order based on start position.
       * can stop looking at locks when stop < lock_entry->start
       *
       * NOTE: Currently locks maintained in a simple linked list.
       *       This provides sufficient performance for PIOUS calls,
       *       but may prove inadequate if the number of locks for
       *       a file becomes large, such as in using the PDS as a
       *       "standard" transaction-oriented database system.
       *
       *       The semantics of lock ownership will no doubt require update
       *       when inter-group concurrency control mechanisms, beyond
       *       sequential consistency, are implemented.
       */

      lock_entry  = fh_entry->locks; /* first file lock entry */
      owned       = FALSE; /* flag if equivalent lock already held */
      conflict    = FALSE; /* flag if there exists a conflicting lock */

      while (!conflict && !owned &&
	     lock_entry != NULL && stop >= lock_entry->start)
	{ /* check if requested lock overlaps granted lock */

	  if (start <= lock_entry->stop) /* && stop >= lock_entry->start */
	    /* overlap; check for lock ownership or conflict */

	    if (transid_eq(transid, lock_entry->transid))
	      { /* owned; determine if granted lock subsumes requested lock */

		if (start >= lock_entry->start && stop <= lock_entry->stop &&
		    (lock == READ || lock_entry->lock == WRITE))
		  owned = TRUE;
	      }

	    else /* overlapping lock does not belong to transid; confict? */

	      if (lock == WRITE || lock_entry->lock == WRITE)
		conflict = TRUE;
	  
	  lock_entry = lock_entry->lnext; /* advance to next granted lock */
	}

      if (owned)
	result = LM_GRANT;

      else if (conflict)
	result = LM_DENY;

      else /* allocate and grant lock */
	{
	  /* Locate/insert transid in transaction id table */
	  ti_entry = ti_lookup(transid, INSERT);

	  if (ti_entry == NULL) /* ti_lookup() couldn't allocate space */
	    result = LM_DENY;
	  else
	    {
	      /* Allocate and insert lock into lock table */
	      lock_entry = lock_insert(fh_entry, ti_entry,
				       transid, start, stop, lock);

	      if (lock_entry == NULL) /* lock_insert() can't allocate space */
		result = LM_DENY;
	      else
		result = LM_GRANT;
	    }

	  if (result == LM_DENY)
	    {
	      /* deallocate created entries */
	      if (fh_entry->locks == NULL)
		fh_rm(fh_entry);

	      if (ti_entry != NULL && ti_entry->rlocks == NULL &&
		  ti_entry->wlocks == NULL)
		ti_rm(ti_entry);
	    }
	}
    }

  return (result);
}




/*
 * ti_freelocks()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   lock     - lock type
 *
 * Free all locks of type 'lock' held by transaction 'transid'.
 *
 * Returns:
 *
 */

#ifdef __STDC__
static void ti_freelocks(pds_transidt transid,
			 int lock)
#else
static void ti_freelocks(transid, lock)
     pds_transidt transid;
     int lock;
#endif
{
  ti_entryt *ti_entry;
  register lock_entryt *lock_entry, *next_lock;

  /* Validate arguments */

  if (lock != READ && lock != WRITE)
    return;

  /* locate transid in transaction id table */
  ti_entry = ti_lookup(transid, NOINSERT);

  if (ti_entry != NULL)
    { /* there exists locks owned by transid */

      if (lock == READ)
	{
	  lock_entry = ti_entry->rlocks;
	  ti_entry->rlocks = NULL; /* entire chain to be deallocated */
	}
      else /* lock == WRITE */
	{
	  lock_entry = ti_entry->wlocks;
	  ti_entry->wlocks = NULL; /* entire chain to be deallocated */
	}
      
      /* remove all 'lock' locks owned by transid */
      while (lock_entry != NULL)
	{
	  next_lock = lock_entry->onext;

	  /* NOTE: file handle hash chains updated by lock_rm(); see
           * description of lock_rm() for details.
	   */
	  lock_rm(lock_entry);

	  lock_entry = next_lock;
	}

      /* deallocate *ti_entry if no more associated locks */
      if (ti_entry->rlocks == NULL && ti_entry->wlocks == NULL)
	ti_rm(ti_entry);
    }
}




/*
 * fh_lookup()
 *
 * Parameters:
 *
 *   fhandle - file handle
 *   action  - INSERT | NOINSERT
 *
 * Locate fhandle in the file handle hash table. If file handle not
 * found and INSERT, then place file handle into table.
 *
 * Returns:
 *
 *   fh_entryt * - pointer to fh_entry if fhandle found/inserted
 *   NULL        - fhandle not found/inserted
 */

#ifdef __STDC__
static fh_entryt *fh_lookup(pds_fhandlet fhandle,
			    int action)
#else
static fh_entryt *fh_lookup(fhandle, action)
     pds_fhandlet fhandle;
     int action;
#endif
{
  int index;
  register fh_entryt *fh_entry;

  /* perform hash function on fhandle */
  index = fhandle_hash(fhandle, FH_TABLE_SZ);

  fh_entry = fh_table[index];

  /* search appropriate file handle hash chain for fhandle */
  while (fh_entry != NULL && !fhandle_eq(fhandle, fh_entry->fhandle))
    fh_entry = fh_entry->fhnext;

  if (fh_entry == NULL && action == INSERT)
    { /* fhandle not located; insert into appropriate file handle hash chain */

      fh_entry = (fh_entryt *)malloc((unsigned)sizeof(fh_entryt));

      if (fh_entry != NULL)
	{ /* initialize and place at head of hash chain */
	  fh_entry->fhandle = fhandle;
	  fh_entry->locks   = NULL;
	  fh_entry->fhnext  = fh_table[index];
	  fh_entry->fhprev  = NULL;

	  fh_table[index] = fh_entry;

	  /* reset 'previous' pointer of former head, if extant */
	  if (fh_entry->fhnext != NULL)
	    fh_entry->fhnext->fhprev = fh_entry;
	}
    }

  return(fh_entry);
}




/*
 * fh_rm()
 *
 * Parameters:
 *
 *   fh_entry - file handle hash chain entry
 *
 * Remove and deallocate file handle hash chain entry.
 *
 * Returns:
 */

#ifdef __STDC__
static void fh_rm(fh_entryt *fh_entry)
#else
static void fh_rm(fh_entry)
     fh_entryt *fh_entry;
#endif
{
  if (fh_entry != NULL)
    {
      /* if not last in chain, reset 'previous' pointer of 'next' entry */
      if (fh_entry->fhnext != NULL)
	fh_entry->fhnext->fhprev = fh_entry->fhprev;

      /* if not first in chain, reset 'next' pointer of 'previous' entry */
      if (fh_entry->fhprev != NULL)
	fh_entry->fhprev->fhnext = fh_entry->fhnext;
      else
	/* first in chain, reset file handle hash table entry */
	fh_table[fhandle_hash(fh_entry->fhandle, FH_TABLE_SZ)] =
	  fh_entry->fhnext;

      free((char *)fh_entry); /* deallocate storage for fh_entry */
    }
}




/*
 * ti_lookup()
 *
 * Parameters:
 *
 *   transid  - transaction id
 *   action   - INSERT | NOINSERT
 *
 * Locate transid in the transaction id hash table. If transaction id not
 * found and INSERT, then place transaction id into table.
 *
 * Returns:
 *
 *   ti_entryt * - pointer to ti_entry if transid found/inserted
 *   NULL        - transid not found/inserted
 */

#ifdef __STDC__
static ti_entryt *ti_lookup(pds_transidt transid,
			    int action)
#else
static ti_entryt *ti_lookup(transid, action)
     pds_transidt transid;
     int action;
#endif
{
  int index;
  register ti_entryt *ti_entry;

  /* perform hash function on transid */
  index = transid_hash(transid, TI_TABLE_SZ);

  ti_entry = ti_table[index];

  /* search appropriate transaction id hash chain for transid */
  while (ti_entry != NULL && !transid_eq(transid, ti_entry->transid))
    ti_entry = ti_entry->tinext;

  if (ti_entry == NULL && action == INSERT)
    { /* transid not located; insert into transaction id hash chain */

      ti_entry = (ti_entryt *)malloc((unsigned)sizeof(ti_entryt));

      if (ti_entry != NULL)
	{ /* initialize and place at head of hash chain */
	  ti_entry->transid = transid;
	  ti_entry->rlocks   = NULL;
	  ti_entry->wlocks   = NULL;
	  ti_entry->tinext   = ti_table[index];
	  ti_entry->tiprev   = NULL;

	  ti_table[index] = ti_entry;

	  /* reset 'previous' pointer of former head, if extant */
	  if (ti_entry->tinext != NULL)
	    ti_entry->tinext->tiprev = ti_entry;
	}
    }

  return(ti_entry);
}




/*
 * ti_rm()
 *
 * Parameters:
 *
 *   ti_entry - transaction id hash chain entry
 *
 * Remove and deallocate transaction id hash chain entry.
 *
 * Returns:
 */

#ifdef __STDC__
static void ti_rm(ti_entryt *ti_entry)
#else
static void ti_rm(ti_entry)
     ti_entryt *ti_entry;
#endif
{
  if (ti_entry != NULL)
    {
      /* if not last in chain, reset 'previous' pointer of 'next' entry */
      if (ti_entry->tinext != NULL)
	ti_entry->tinext->tiprev = ti_entry->tiprev;

      /* if not first in chain, reset 'next' pointer of 'previous' entry */
      if (ti_entry->tiprev != NULL)
	ti_entry->tiprev->tinext = ti_entry->tinext;
      else
	/* first in chain, reset transaction id hash table entry */
	ti_table[transid_hash(ti_entry->transid, TI_TABLE_SZ)] =
	  ti_entry->tinext;

      free((char *)ti_entry); /* deallocate storage for ti_entry */
    }
}



/*
 * lock_insert()
 *
 * Parameters:
 *
 *   fh_entry - file handle hash chain entry associated with lock
 *   ti_entry - transaction id hash chain entry associated with lock
 *   transid  - transaction id
 *   start    - start byte
 *   stop     - stop byte
 *   lock     - lock type
 *
 * Allocate and insert a lock into the lock table.  The lock is to
 * be associate with the file 'fh_entry' and the transaction 'ti_entry'.
 *
 * Returns:
 *   lock_entryt * - pointer to lock_entry
 *   NULL          - lock can not be placed in table
 *
 * Note: ti_entry->(rlocks | wlocks) and (possibly) fh_entry->locks altered.
 */

#ifdef __STDC__
static lock_entryt *lock_insert(fh_entryt *fh_entry,
				ti_entryt *ti_entry,
				pds_transidt transid,
				pious_offt start,
				pious_offt stop,
				int lock)
#else
static lock_entryt *lock_insert(fh_entry, ti_entry, transid, start, stop, lock)
     fh_entryt *fh_entry;
     ti_entryt *ti_entry;
     pds_transidt transid;
     pious_offt start;
     pious_offt stop;
     int lock;
#endif
{
  register lock_entryt *lock_entry, *cur_lpos, *last_lpos;

  /* Check that arguments are valid */

  if (fh_entry == NULL || ti_entry == NULL ||
      (lock != READ && lock != WRITE))
    return(NULL);

  /* Valid arguments; begin function */

  lock_entry = (lock_entryt *)malloc((unsigned)sizeof(lock_entryt));

  if (lock_entry != NULL) /* lock space allocated */
    {
      /* Step 1: initialize constant lock information */
      lock_entry->transid  = transid;
      lock_entry->start    = start;
      lock_entry->stop     = stop;
      lock_entry->lock     = lock;
      lock_entry->fhchain  = fh_entry;

      /* Step 2: place lock at head of *ti_entry 'rlocks' or 'wlocks' chain */

      if (lock == READ) /* place at head of rlocks chain */
	{
	  lock_entry->onext = ti_entry->rlocks;
	  ti_entry->rlocks  = lock_entry;
	}
      else /* lock == WRITE so place at head of wlocks chain */
	{
	  lock_entry->onext = ti_entry->wlocks;
	  ti_entry->wlocks  = lock_entry;
	}

      /* Step 3: place lock at appropriate position in *fh_entry 'locks'
       * chain.  locks maintained in ascending order based on start position.
       */

      last_lpos = NULL;
      cur_lpos  = fh_entry->locks;

      /* search lock chain for insert position */

      while (cur_lpos != NULL && start > cur_lpos->start)
	{
	  last_lpos = cur_lpos;
	  cur_lpos  = cur_lpos->lnext;
	}

      /* insert lock between last_lpos and cur_lpos */

      lock_entry->lprev = last_lpos;
      lock_entry->lnext = cur_lpos;

      /* reset 'prev' pointer of 'next' entry relative to lock_entry */
      if (lock_entry->lnext != NULL)
	lock_entry->lnext->lprev = lock_entry;

      /* reset 'next' pointer of 'prev' entry relative to lock_entry */
      if (lock_entry->lprev != NULL)
	lock_entry->lprev->lnext = lock_entry;
      else
	fh_entry->locks = lock_entry;
    }

  return (lock_entry);
}




/*
 * lock_rm()
 *
 * Parameters:
 *
 *   lock_entry - lock table entry
 *
 * Remove lock_entry from chain of locks associated with a given file handle
 * and deallocate space.
 *
 * Returns:
 *
 * Note:
 *
 *   If lock_entry is the last lock associated with a given file handle,
 *   then that file handle entry is removed. Thus lock_rm() can have the
 *   side effect of altering a file handle hash chain via a call to fh_rm().
 *
 *   Lock_entry is also an entry in a chain of locks associated with a given
 *   transaction id. This chain of locks is NOT updated when lock_entry
 *   is deallocated, breaking the chain.  However, this is not a problem
 *   since lock_rm() is ONLY called when ALL locks in this chain associated
 *   with a given transaction are being deallocated. Thus this is a
 *   performance enhancement.
 *
 */

#ifdef __STDC__
static void lock_rm(lock_entryt *lock_entry)
#else
static void lock_rm(lock_entry)
     lock_entryt *lock_entry;
#endif
{
  if (lock_entry != NULL)
    {
      /* remove lock_entry from file handle locks chain */

      /* if not last in chain, reset 'previous' pointer of 'next' entry */
      if (lock_entry->lnext != NULL)
	lock_entry->lnext->lprev = lock_entry->lprev;

      /* if not first in chain, reset 'next' pointer of 'previous' entry */
      if (lock_entry->lprev != NULL)
	lock_entry->lprev->lnext = lock_entry->lnext;
      else
	/* first; reset associated fhchain->locks entry; noted side effect  */
	lock_entry->fhchain->locks = lock_entry->lnext;

      /* Deallocate file handle entry if no more locks on file */
      if  (lock_entry->fhchain->locks == NULL)
	fh_rm(lock_entry->fhchain);

      /* Deallocate lock space */
      free((char *)lock_entry);
    }
}
