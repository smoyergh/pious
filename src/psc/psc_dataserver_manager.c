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




/* PIOUS Service Coordinator (PSC): Data Server Manager
 *
 * @(#)psc_dataserver_manager.c	2.2 28 Apr 1995 Moyer
 *
 * The psc_dataserver_manager exports functions for managing the set
 * of PIOUS data servers (PDS) that provide the mechanism for accessing
 * user data.
 *
 *
 * Function Summary:
 *
 *   SM_add_pds();
 *   SM_reset_pds();
 *   SM_shutdown_pds();
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

#include "pious_errno.h"
#include "pious_types.h"

#include "pious_sysconfig.h"

#include "pds_fhandlet.h"
#include "pds_transidt.h"

#include "pdce_msgtagt.h"
#include "pdce_srcdestt.h"
#include "pdce.h"

#include "pds.h"

#include "psc_cmsgidt.h"

#include "psc_dataserver_manager.h"




/*
 * Private Declarations - Type, Constants, and Macros
 */


/* PDS host information record */

typedef struct hinfo_entry{
  char *hname;                 /* PDS host name */
  dce_srcdestt id;             /* PDS message passing id */
  struct hinfo_entry *next;    /* next host info entry */
} hinfo_entryt;


/* PDS control operation request state */

struct cntrlop_state{
  int code;    /* result code of request send/recv */
  int id;      /* control message id */
};




/*
 * Private Variable Definitions
 */


/* PDS host list
 *
 *   The set of hosts on which PDS have been spawned is maintained as a
 *   simple linked list.  This list is rarely modified and is likely to
 *   contain few entries, so search performance is not an issue.
 */

static hinfo_entryt *pds_hosts;
static int pds_hostcnt = 0;




/*
 * Local Function Declarations
 */


#ifdef __STDC__
static hinfo_entryt *hlist_lookup(char *hname);
static void hlist_insert(hinfo_entryt *hentry);
#else
static hinfo_entryt *hlist_lookup();
static void hlist_insert();
#endif




/*
 * Function Definitions - Data Server Manager Functions
 */



/*
 * SM_add_pds() - See psc_dataserver_manager.h for description
 */

#ifdef __STDC__
int SM_add_pds(char *pds_hname,
	       char *pds_lpath,
	       dce_srcdestt *pds_id)
#else
int SM_add_pds(pds_hname, pds_lpath, pds_id)
     char *pds_hname;
     char *pds_lpath;
     dce_srcdestt *pds_id;
#endif
{
  int rcode, scode;
  hinfo_entryt *host_entry;
  char *pds_argv[2];

  /* spawn PDS on specified host */

  if ((host_entry = hlist_lookup(pds_hname)) != NULL)
    { /* PDS previously spawned on host; set PDS message passing id */
      *pds_id = host_entry->id;
      rcode   = PIOUS_OK;
    }

  else
    { /* set PDS arguments */

      pds_argv[0] = pds_lpath;
      pds_argv[1] = NULL;

      /* allocate a host information record */

      if ((host_entry =
	   (hinfo_entryt *)malloc(sizeof(hinfo_entryt))) == NULL ||

	  (host_entry->hname =
	   malloc((unsigned)(strlen(pds_hname) + 1))) == NULL)
	{ /* unable to allocate storage */
	  rcode = PIOUS_EINSUF;
	}

      /* spawn PDS on host */
	
      else if ((scode = DCE_spawn(PDS_DAEMON_EXEC,
				  pds_argv,
				  pds_hname,
				  pds_id)) != PIOUS_OK)
	{ /* unable to spawn PDS */
	  switch(scode)
	    {
	    case PIOUS_EINVAL:
	    case PIOUS_EINSUF:
	      rcode = scode;
	      break;
	    default:
	      rcode = PIOUS_ETPORT;
	      break;
	    }
	}

      /* fill in host information record and insert into list */

      else
	{
	  strcpy(host_entry->hname, pds_hname);
	  host_entry->id = *pds_id;

	  hlist_insert(host_entry);

	  rcode = PIOUS_OK;
	}
    }

  /* deallocate extraneous storage */

  if (rcode != PIOUS_OK && host_entry != NULL)
    {
      if (host_entry->hname != NULL)
	free(host_entry->hname);

      free((char *)host_entry);
    }

  return rcode;
}




/*
 * SM_reset_pds() - See psc_dataserver_manager.h for description
 *
 */

#ifdef __STDC__
void SM_reset_pds(void)
#else
void SM_reset_pds()
#endif
{
  int i;
  struct cntrlop_state *cntrlop;
  hinfo_entryt *host_entry;

  if (pds_hostcnt > 0)
    { /* reset PDS on all hosts */
      if ((cntrlop = (struct cntrlop_state *)
	   malloc((unsigned)
		  (pds_hostcnt * sizeof(struct cntrlop_state)))) == NULL)

	{ /* perform blocking PDS reset */
	  for (host_entry =  pds_hosts;
	       host_entry != NULL;
	       host_entry =  host_entry->next)

	    PDS_reset(host_entry->id, CntrlIdNext);
	}

      else
	{ /* perform pipelined PDS reset */
	  for (host_entry =  pds_hosts, i = 0;
	       host_entry != NULL;
	       host_entry =  host_entry->next, i++)
	    {
	      cntrlop[i].id   = CntrlIdNext;
	      cntrlop[i].code = PDS_reset_send(host_entry->id, CntrlIdLast);
	    }

	  for (host_entry =  pds_hosts, i = 0;
	       host_entry != NULL;
	       host_entry =  host_entry->next, i++)
	    {
	      if (cntrlop[i].code == PIOUS_OK)
		PDS_reset_recv(host_entry->id, cntrlop[i].id);
	    }

	  /* deallocate extraneous storage */
	  free((char *)cntrlop);
	}
    }
}




/*
 * SM_shutdown_pds() - See psc_dataserver_manager.h for description
 *
 */

#ifdef __STDC__
void SM_shutdown_pds(void)
#else
void SM_shutdown_pds()
#endif
{
  int i;
  struct cntrlop_state *cntrlop;
  hinfo_entryt *host_entry, *host_entry_next;

  if (pds_hostcnt > 0)
    { /* shutdown PDS on all hosts */
      if ((cntrlop = (struct cntrlop_state *)
	   malloc((unsigned)
		  (pds_hostcnt * sizeof(struct cntrlop_state)))) == NULL)

	{ /* perform blocking PDS shutdown */
	  host_entry = pds_hosts;

	  while (host_entry != NULL)
	    {
	      PDS_shutdown(host_entry->id, CntrlIdNext);

	      /* free host entry */
	      host_entry_next = host_entry->next;

	      free(host_entry->hname);
	      free((char *)host_entry);

	      host_entry = host_entry_next;
	    }
	}

      else
	{ /* perform pipelined PDS shutdown */
	  for (host_entry =  pds_hosts, i = 0;
	       host_entry != NULL;
	       host_entry =  host_entry->next, i++)
	    {
	      cntrlop[i].id   = CntrlIdNext;
	      cntrlop[i].code = PDS_shutdown_send(host_entry->id, CntrlIdLast);
	    }

	  for (host_entry = pds_hosts, i = 0; host_entry != NULL; i++)
	    {
	      if (cntrlop[i].code == PIOUS_OK)
		PDS_shutdown_recv(host_entry->id, cntrlop[i].id);

	      /* free host entry */
	      host_entry_next = host_entry->next;

	      free(host_entry->hname);
	      free((char *)host_entry);

	      host_entry = host_entry_next;
	    }

	  /* deallocate extraneous storage */
	  free((char *)cntrlop);
	}

      /* indicate that host entry list deallocated */
      pds_hostcnt = 0;
    }
}




/*
 * Function Definitions - Local Functions
 */


/*
 * hlist_lookup()
 *
 * Parameters:
 *
 *   hname - host name
 *
 * Look up a host name in the PDS host list.
 *
 * Returns:
 *
 *   hinfo_entryt * - pointer to PDS host list entry 'hname'
 *   NULL           - 'hname' not located
 */

#ifdef __STDC__
static hinfo_entryt *hlist_lookup(char *hname)
#else
static hinfo_entryt *hlist_lookup(hname)
     char *hname;
#endif
{
  hinfo_entryt *host_entry;

  host_entry = pds_hosts;

  while (host_entry != NULL && strcmp(host_entry->hname, hname))
    host_entry = host_entry->next;

  return host_entry;
}




/*
 * hlist_insert()
 *
 * Parameters:
 *
 *   hentry - host information record entry
 *
 * Insert host information record 'hentry' into PDS host list and increment
 * list entry count.
 *
 * Returns:
 */

#ifdef __STDC__
static void hlist_insert(hinfo_entryt *hentry)
#else
static void hlist_insert(hentry)
     hinfo_entryt *hentry;
#endif
{
  /* insert entry at head of list */
  hentry->next = pds_hosts;
  pds_hosts    = hentry;

  /* increment entry count */
  pds_hostcnt++;
}
