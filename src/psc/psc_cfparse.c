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




/* PIOUS Service Coordinator (PSC): Configuration File Parser
 *
 * @(#)psc_cfparse.c	2.2 28 Apr 1995 Moyer
 *
 * This module exports functions for parsing the PSC configuration file
 * containing PDS host information entries.
 *
 * The grammar that defines the configuration file syntax is:
 *
 *   PDS_SPEC   --> HOSTNAME OPT_LIST
 *   OPT_LIST   --> sp= PATHNAME |
 *                  sp= PATHNAME lp= PATHNAME |
 *                  lp= PATHNAME |
 *                  lp= PATHNAME sp= PATHNAME |
 *                  (null production)
 *   HOSTNAME   --> any whitespace delimited string
 *   PATHNAME   --> any whitespace delimited string where first char is '/'
 *
 *
 * Function Summary:
 *
 *   CF_parse();
 *
 */




/* Include Files */

#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "gpmacro.h"

#include "pious_errno.h"

#include "pious_sysconfig.h"

#include "psc_cfparse.h"




/*
 * Private Declarations - Type, Constants, and Macros
 */


/* file parsing tokens */

#define LPathToken     0         /* PDS host log directory path */
#define SPathToken     1         /* PDS host search root path */
#define IdentToken     2         /* identifier string */
#define EofToken       3         /* end of file */

/* PDS host information record */

typedef struct hinfo_entry{
  char *hname;                 /* PDS host name */
  char *spath;                 /* PDS host search root path */
  char *lpath;                 /* PDS host log directory path */
  struct hinfo_entry *next;    /* next host info entry */
} hinfo_entryt;




/*
 * Private Variable Definitions
 */

/* PSC host information file and file line number */
static FILE *hostfile;
static int hostfile_lineno;

/* parser lookahead buffer */
static int  lookahead_token;
static char lookahead_buf[PSC_INPUTBUF_MAX + 1];

/* OPT_LIST flags */
int sp_found, lp_found;




/*
 * Local Function Declarations
 */

#ifdef __STDC__
static int hlist_insert(hinfo_entryt **hlist,
			 hinfo_entryt *entry);

static int pds_spec(hinfo_entryt **host_entry);

static int opt_list(hinfo_entryt *host_entry);

static int lexan(char *tokenval);

static int getnonwhite(char *dest,
		       int max);

#else
static int hlist_insert();
static int pds_spec();
static int opt_list();
static int lexan();
static int getnonwhite();
#endif




/*
 * Function Definitions - Parser Operations
 */



/*
 * CF_parse() - See psc_cfparse.h for description
 */

#ifdef __STDC__
int CF_parse(char *psc_hostfile,
	     int *pds_cnt,
	     char ***pds_hname,
	     char ***pds_spath,
	     char ***pds_lpath)
#else
int CF_parse(psc_hostfile, pds_cnt, pds_hname, pds_spath, pds_lpath)
     char *psc_hostfile;
     int *pds_cnt;
     char ***pds_hname;
     char ***pds_spath;
     char ***pds_lpath;
#endif
{
  int rcode, pcode, idx, semantic_err;
  char *def_spath, *def_lpath;
  hinfo_entryt *pds_hosts, *host_entry, *host_entry_next;


  /* initialize PDS host information list and default paths */

  pds_hosts = NULL;
  *pds_cnt  = 0;

  def_spath = def_lpath = NULL;

  /* open PSC configuration file for reading */

  hostfile_lineno = 1;
  semantic_err    = FALSE;

  if ((hostfile = fopen(psc_hostfile, "r")) == NULL)
    rcode = PIOUS_EINVAL;

  /* allocate storage for default path parameters */

  else if ((def_spath = malloc((unsigned)(PSC_INPUTBUF_MAX + 1))) == NULL ||
	   (def_lpath = malloc((unsigned)(PSC_INPUTBUF_MAX + 1))) == NULL)

    rcode = PIOUS_EINSUF;

  /* parse configuration file */

  else
    { /* set default host search-root and log-directory paths to null */
      *def_spath = *def_lpath = '\0';

      /* parse each file entry and insert into PDS host info list */
      lookahead_token = lexan(lookahead_buf);
      pcode           = PIOUS_OK;

      while (lookahead_token != EofToken && pcode == PIOUS_OK)
	{
	  /* parse next host information entry */

	  if ((pcode = pds_spec(&host_entry)) == PIOUS_OK)
	    {
	      /* case: entry specifies new path defaults */

	      if (!strcmp(host_entry->hname, "*"))
		{ /* set new default search root path */
		  if (*host_entry->spath != '\0')
		    {
		      free(def_spath);
		      def_spath = host_entry->spath;
		    }
		  else
		    {
		      free(host_entry->spath);
		    }

		  /* set new default log directory path */
		  if (*host_entry->lpath != '\0')
		    {
		      free(def_lpath);
		      def_lpath = host_entry->lpath;
		    }
		  else
		    {
		      free(host_entry->lpath);
		    }

		  free(host_entry->hname);
		  free((char *)host_entry);
		}

	      /* case: new PDS host information entry */

	      else
		{ /* if search root path not defined then set to default */
		  if (*host_entry->spath == '\0')
		    {
		      if (*def_spath == '\0')
			pcode = PIOUS_EINVAL;
		      else
			strcpy(host_entry->spath, def_spath);
		    }

		  /* if log directory path not defined then set to default */
		  if (*host_entry->lpath == '\0')
		    {
		      if (*def_lpath == '\0')
			pcode = PIOUS_EINVAL;
		      else
			strcpy(host_entry->lpath, def_lpath);
		    }

		  /* insert entry into list in lexicographic order, checking
		   * for duplicate host names.
		   */
		  if (pcode == PIOUS_OK)
		    {
		      if (hlist_insert(&pds_hosts, host_entry))
			(*pds_cnt)++;
		      else
			pcode = PIOUS_EINVAL;
		    }


		  /* if semantic error occured, deallocate entry storage */
		  if (pcode != PIOUS_OK)
		    {
		      free(host_entry->hname);
		      free(host_entry->spath);
		      free(host_entry->lpath);

		      free((char *)host_entry);

		      semantic_err = TRUE;
		    }
		}
	    }
	}


      /* determine result code and set-up return parameters */

      if (pcode != PIOUS_OK || *pds_cnt == 0)
	{ /* error parsing file or file contained no host entries */
	  rcode = pcode;
	}

      else
	{ /* allocate storage for return parameters */

	  if ((*pds_hname = (char **)
	       malloc((unsigned)((*pds_cnt) * sizeof(char *)))) == NULL)
	    rcode = PIOUS_EINSUF;

	  else if ((*pds_spath = (char **)
		    malloc((unsigned)((*pds_cnt) * sizeof(char *)))) == NULL)
	    {
	      free((char *)pds_hname);
	      rcode = PIOUS_EINSUF;
	    }

	  else if ((*pds_lpath = (char **)
		    malloc((unsigned)((*pds_cnt) * sizeof(char *)))) == NULL)
	    {
	      free((char *)pds_hname);
	      free((char *)pds_spath);
	      rcode = PIOUS_EINSUF;
	    }

	  else
	    { /* set corresponding name and path parameters */
	      for (host_entry =  pds_hosts, idx = 0;
		   host_entry != NULL;
		   host_entry =  host_entry->next, idx++)
		{
		  (*pds_hname)[idx] = host_entry->hname;
		  (*pds_spath)[idx] = host_entry->spath;
		  (*pds_lpath)[idx] = host_entry->lpath;
		}

	      rcode = PIOUS_OK;
	    }
	}
    }


  /* report if parsing configuration file resulted in an error */

  if (rcode == PIOUS_EINVAL)
    {
      if (hostfile == NULL)
       	{ /* unable to open configuration file */
	  fprintf(stderr, "\npious: can not access %s\n", psc_hostfile);
	}

      else if (semantic_err)
	{ /* semantic error in host file entry */
	  fprintf(stderr, "\npious: error in %s at record preceding line %d;",
		  psc_hostfile, hostfile_lineno);
	  fprintf(stderr, "\n       missing path or duplicate host\n");
	}

      else
	{ /* configuration file contained a syntax error */
	  fprintf(stderr, "\npious: syntax error in %s at line %d\n",
		  psc_hostfile, hostfile_lineno);
	}
    }

  /* deallocate extraneous storage and close configuration file */

  if (def_spath != NULL)
    free(def_spath);

  if (def_lpath != NULL)
    free(def_lpath);

  for (host_entry =  pds_hosts;
       host_entry != NULL;
       host_entry =  host_entry_next)
    {
      /* must determine next before deallocating entry */
      host_entry_next = host_entry->next;

      if (rcode != PIOUS_OK)
	{ /* note: only deallocate list entries if there was an error! */
	  free(host_entry->hname);
	  free(host_entry->spath);
	  free(host_entry->lpath);
	}

      free((char *)host_entry);
    }

  if (hostfile != NULL)
    fclose(hostfile);

  return rcode;
}




/*
 * Function Definitions - Local Functions
 */




/*
 * hlist_insert()
 *
 * Parameters:
 *
 *   hlist - head of host information list
 *   entry - entry to insert
 *
 * Insert 'entry' into the host information list 'hlist', in lexicographic
 * order by host name, updating 'hlist' as appropriate.
 *
 * Returns:
 *
 *   TRUE  - entry inserted into host list
 *   FALSE - duplicate entry NOT inserted into host list
 */

#ifdef __STDC__
static int hlist_insert(hinfo_entryt **hlist,
			hinfo_entryt *entry)
#else
static int hlist_insert(hlist, entry)
     hinfo_entryt **hlist;
     hinfo_entryt *entry;
#endif
{
  int rcode, ccode;
  hinfo_entryt *hpos, *hpos_prev;

  /* search list to insert entry in lexicographic order by host name */

  hpos = hpos_prev = *hlist;

  while (hpos != NULL && (ccode = strcmp(hpos->hname, entry->hname)) < 0)
    {
      hpos_prev = hpos;
      hpos      = hpos->next;
    }


  if (hpos != NULL && ccode == 0)
    { /* duplicate entry found in list */
      rcode = FALSE;
    }

  else
    { /* insert entry prior to 'hpos', or at end of list if 'hpos' is NULL */
      entry->next = hpos;

      if (hpos_prev != hpos)
	/* 'entry' will NOT be first item in list */
	hpos_prev->next = entry;
      else
	/* 'entry' IS first item in list */
	*hlist = entry;

      rcode = TRUE;
    }

  return rcode;
}




/*
 * pds_spec()
 *
 * Parameters:
 *
 *   host_entry - PDS host information record
 *
 * Parse the next PDS host information record in the configuration file
 * and place result in 'host_entry'.
 *
 * Note: pds_spec() is the starting non-terminal in the grammar defining
 *       the syntax of a configuration file entry.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - host entry parsed without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - syntax error; configuration file invalid
 *       PIOUS_EINSUF - insufficient system resources to complete
 */

#ifdef __STDC__
static int pds_spec(hinfo_entryt **host_entry)
#else
static int pds_spec(host_entry)
     hinfo_entryt **host_entry;
#endif
{
  int rcode;

  /* allocate storage for host information record */
  rcode = PIOUS_OK;

  if ((*host_entry = (hinfo_entryt *)malloc(sizeof(hinfo_entryt))) == NULL)
    rcode = PIOUS_EINSUF;

  else
    {
      (*host_entry)->hname = NULL;
      (*host_entry)->spath = NULL;
      (*host_entry)->lpath = NULL;

      if (((*host_entry)->hname =
	   malloc((unsigned)(PSC_INPUTBUF_MAX + 1))) == NULL ||

	  ((*host_entry)->spath =
	   malloc((unsigned)(PSC_INPUTBUF_MAX + 1))) == NULL ||

	  ((*host_entry)->lpath =
	   malloc((unsigned)(PSC_INPUTBUF_MAX + 1))) == NULL)

	rcode = PIOUS_EINSUF;
    }

  /* parse next configuration file entry */
  if (rcode == PIOUS_OK)
    {
      /* match identifier terminal (hostname) */
      if (lookahead_token == IdentToken)
	{
	  /* set hostname and optional parameter values */
	  strcpy((*host_entry)->hname, lookahead_buf);

	  *(*host_entry)->spath = *(*host_entry)->lpath = '\0';

	  /* consume token */
	  lookahead_token = lexan(lookahead_buf);

	  /* mark log directory and search root paths as not defined */
	  lp_found = sp_found = FALSE;

	  /* parse opt_list non-terminal */
	  rcode = opt_list(*host_entry);
	}

      else
	{ /* invalid hostname */
	  rcode = PIOUS_EINVAL;
	}
    }

  /* deallocate storage if error */
  if (rcode != PIOUS_OK)
    if (*host_entry != NULL)
      {
	if ((*host_entry)->hname != NULL)
	  free((*host_entry)->hname);

	if ((*host_entry)->spath != NULL)
	  free((*host_entry)->spath);

	if ((*host_entry)->lpath != NULL)
	  free((*host_entry)->lpath);

	free((char *)(*host_entry));

	*host_entry = NULL;
      }

  return rcode;
}




/*
 * opt_list()
 *
 * Parameters:
 *
 *   host_entry - PDS host information record
 *
 * Parse the option list of the current PDS host information record and place
 * the result in 'host_entry'.
 *
 * Note: opt_list() is a non-terminal in the grammar defining the syntax
 *       of a configuration file entry.
 *
 * Returns:
 *
 *   PIOUS_OK (0) - option list parsed without error
 *   <  0         - error code defined in pious_errno.h; possible codes are:
 *
 *       PIOUS_EINVAL - syntax error; configuration file invalid
 */

#ifdef __STDC__
static int opt_list(hinfo_entryt *host_entry)
#else
static int opt_list(host_entry)
     hinfo_entryt *host_entry;
#endif
{
  int rcode;

  /* case: LPathToken */

  if (lookahead_token == LPathToken)
    {
      if (lp_found)
	{ /* redefined log directory path */
	  rcode = PIOUS_EINVAL;
	}

      else
	{ /* consume token */

	  lookahead_token = lexan(lookahead_buf);

	  /* match IdentToken with '/' as first character (pathname) */

	  if (lookahead_token == IdentToken && *lookahead_buf == '/')
	    { /* set log directory path */
	      strcpy(host_entry->lpath, lookahead_buf);

	      /* consume token */
	      lookahead_token = lexan(lookahead_buf);

	      /* mark log directory path as defined */
	      lp_found = TRUE;

	      /* parse remainder of opt_list non-terminal */
	      rcode = opt_list(host_entry);
	    }

	  else
	    { /* invalid pathname */
	      rcode = PIOUS_EINVAL;
	    }
	}
    }

  /* case: SPathToken */

  else if (lookahead_token == SPathToken)
    {
      if (sp_found)
	{ /* redefined search root path */
	  rcode = PIOUS_EINVAL;
	}

      else
	{ /* consume token */

	  lookahead_token = lexan(lookahead_buf);

	  /* match IdentToken with '/' as first character (pathname) */

	  if (lookahead_token == IdentToken && *lookahead_buf == '/')
	    { /* set search root path */
	      strcpy(host_entry->spath, lookahead_buf);

	      /* consume token */
	      lookahead_token = lexan(lookahead_buf);

	      /* mark search root path as defined */
	      sp_found = TRUE;

	      /* parse remainder of opt_list non-terminal */
	      rcode = opt_list(host_entry);
	    }

	  else
	    { /* invalid pathname */
	      rcode = PIOUS_EINVAL;
	    }
	}
    }

  /* case: empty production */

  else
    {
      rcode = PIOUS_OK;
    }

  return rcode;
}




/*
 * lexan()
 *
 * Parameters:
 *
 *   tokenval - token value
 *
 * Read the next token from the 'hostfile' input stream, returning the token
 * type.  If the token type has an associated value, this value is placed
 * in 'tokenval'.
 *
 * Returns:
 *
 *   int - token from input stream
 */

#ifdef __STDC__
static int lexan(char *tokenval)
#else
static int lexan(tokenval)
     char *tokenval;
#endif
{
  int token;
  int nextchar, charcnt;

  /* read next character from the input stream */
  nextchar = getc(hostfile);

  /* skip white space and comments, while incrementing line count */

  while (isspace(nextchar) || nextchar == '#')
    { /* skip comment */

      if (nextchar == '#')
	{
	  /* scan up to but not including EOL */
	  if (fscanf(hostfile, "%*[^\n]") == EOF)
	    nextchar = EOF;
	  else
	    nextchar = getc(hostfile);
	}

      /* skip white space */

      else
	{
	  if (nextchar == '\n')
	    hostfile_lineno++;

	  nextchar = getc(hostfile);
	}
    }


  /* form next token from input stream */

  /* case: EofToken */
  if (nextchar == EOF)
    token = EofToken;

  /* case: LPathToken || SPathToken || IdentToken */
  else
    { /* put 'nextchar' into buffer and begin checking rest of string */
      *tokenval = nextchar;

      /* read a total of 3 characters to distinguish tokens */
      charcnt = 1 + getnonwhite((tokenval + 1), 2);

      tokenval[charcnt] = '\0';

      /* determine token type */
      if (!strcmp(tokenval, "lp="))
	token = LPathToken;

      else if (!strcmp(tokenval, "sp="))
	token = SPathToken;

      else
	{ /* read rest of identifier, if applicable */
	  if (charcnt == 3)
	    {
	      charcnt += getnonwhite((tokenval + 3), (PSC_INPUTBUF_MAX - 3));
	      tokenval[charcnt] = '\0';
	    }

	  token = IdentToken;
	}
    }

  return token;
}




/*
 * getnonwhite()
 *
 * Parameters:
 *
 *   dest - character buffer
 *   max  - maximum character count
 *
 * Read up to 'max' non-whitespace characters from the 'hostfile' input
 * stream and place in buffer 'dest'.
 *
 * Returns:
 *
 *   int - number of characters read (0..max)
 */


#ifdef __STDC__
static int getnonwhite(char *dest,
		       int max)
#else
static int getnonwhite(dest, max)
     char *dest;
     int max;
#endif
{
  int nextchar, charcnt;

  charcnt = 0;

  while ((nextchar = getc(hostfile)) != EOF &&
	 !isspace(nextchar) &&
	 charcnt < max)
    {
      *dest++ = nextchar;
      charcnt++;
    }

  if (nextchar != EOF)
    ungetc(nextchar, hostfile);

  return charcnt;
}
