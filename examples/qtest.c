/*
 * qtest.c - perform a quick test of PIOUS functionality
 *
 * Phase I  : single client test of all non-transaction functions (master)
 * Phase II : single client user-transaction test (master)
 * Phase III: multiple client consistency/deadlock test (master and cohort)
 *
 *
 * @(#)qtest.c	2.2  28 Apr 1995  Moyer
 */


#ifdef __STDC__
#include <stddef.h>
#include <stdlib.h>
#else
#include "nonansi.h"
#include <memory.h>
#endif

#include <stdio.h>
#include <string.h>

#include <pvm3.h>
#include <pious1.h>

#include "psc_cfparse.h"



#define COHORTFLAG  "-QtestCohortTaskFlag"

#define GROUPMASTER "qtestM"
#define GROUPCOHORT "qtestC"

#define FILENAME  "qtest.dat"

#define BUFSZ     1024   /* read/write buffer size - maximum */
#define FILESZ       8   /* file size, in number of buffers (FILESZ >= 3 ) */
#define SU           7   /* stripe unit size (SU << BUFSZ) */

#define BailOut() \
printf("\n\nqtest: Bailing - reset PVM to shutdown PIOUS as state may be "); \
printf("inconsistent\n"); \
fflush(stdout); \
pvm_exit(); exit(1)

#define REGMODE \
((pious_modet)(PIOUS_IRUSR | PIOUS_IWUSR | \
	       PIOUS_IRGRP | PIOUS_IWGRP | \
	       PIOUS_IROTH | PIOUS_IWOTH))

#define DIRMODE \
((pious_modet)(PIOUS_IRWXU | \
	       PIOUS_IRGRP | PIOUS_IXGRP | \
	       PIOUS_IROTH | PIOUS_IXOTH))


static int masterid, cohortid, iammaster;
static void mc_sync();


main(argc, argv)
     int argc;
     char **argv;
{
  int dscnt, fd[4], i, j, k, trial;
  long acode;
  char *cohortargv[3];
  char wbuf[BUFSZ], rbuf[BUFSZ], pbuf[BUFSZ], dirname[80], fullfilepath[160];
  pious_modet oldmask;
  pious_sizet bufsz, rdwrsz;
  pious_offt rdwroff;
  struct pious_stat statbuf;
  struct pious_dsvec *dsv;
  int pds_cnt;
  char **pds_hname, **pds_spath, **pds_lpath, *cptr;


  /* determine if master or cohort */

  if (argc == 3 && !strcmp(argv[1], COHORTFLAG))
    { /* cohort - enroll in PVM and goto CohortStart */
      if ((cohortid = pvm_mytid()) < 0)
	{
	  printf("\nqtest: cohort unable to enroll in PVM\n");
	  exit(1);
	}

      if ((masterid = pvm_parent()) < 0)
	{
	  printf("\nqtest: cohort unable to determine parent in PVM\n");
	  BailOut();
	}

      iammaster = 0;

      goto CohortStart;
    }


 MasterStart:

  if (argc != 2)
    { /* print usage and exit */
      printf("\nUsage: qtest dsfile\n");
      exit(1);
    }

  /* enroll in PVM and determine if PIOUS started with default servers */

  printf("\n\nQTEST - Perform a quick test of PIOUS functionality\n\n");
  printf("        Warning: dsfile must be correct or results are erroneous\n");

  if ((masterid = pvm_mytid()) < 0)
    {
      printf("\nqtest: unable to enroll in PVM\n");
      exit(1);
    }

  iammaster = 1;

  if ((dscnt = pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    {
      if (dscnt == 0)
	printf("\nqtest: configure PIOUS with default data servers\n");
      else
	printf("\nqtest: start PIOUS before executing application\n");

      BailOut();
    }


  /* parse configuration file, "unsort" results, and init dsv */

  if ((acode = CF_parse(argv[1],
			&pds_cnt,
			&pds_hname,
			&pds_spath, &pds_lpath)) != PIOUS_OK)
    { /* unable to parse configuration file; syntax/access errors reported */
      printf("\n");

      if (acode == PIOUS_EINVAL)
	printf("qtest: PIOUS parser can't access dsfile or dsfile invalid\n");
      else
	printf("qtest: insufficient memory\n");

      BailOut();
    }

  if (pds_cnt != dscnt)
    {
      printf("\nqtest: dsfile host count inconsistent with configuration\n");
      BailOut();
    }

  if (pds_cnt > 1)
    { /* switch first and last host information to be sure fns are sorting */

      cptr                   = pds_hname[0];
      pds_hname[0]           = pds_hname[pds_cnt - 1];
      pds_hname[pds_cnt - 1] = cptr;

      cptr                   = pds_spath[0];
      pds_spath[0]           = pds_spath[pds_cnt - 1];
      pds_spath[pds_cnt - 1] = cptr;

      cptr                   = pds_lpath[0];
      pds_lpath[0]           = pds_lpath[pds_cnt - 1];
      pds_lpath[pds_cnt - 1] = cptr;
    }

  if ((dsv = (struct pious_dsvec *)
       malloc(pds_cnt * sizeof(struct pious_dsvec))) == NULL)
    {
      printf("qtest: insufficient memory\n");
      BailOut();
    }

  for (i = 0; i < pds_cnt; i++)
    {
      dsv[i].hname = pds_hname[i];
      dsv[i].spath = pds_spath[i];
      dsv[i].lpath = pds_lpath[i];
    }


  /* get name of directory to work in */

  *dirname = '\0';

  printf("\n\nEnter a NEW directory name. ");
  printf("If directory is EXTANT, test will fail!\n");
  printf("\nDirectory: ");
  scanf("%79s", dirname);
  printf("\n\n");

  if (*dirname == '\0')
    {
      printf("\nqtest: invalid directory name\n");
      BailOut();
    }




  /*-----------------------------------------------------------------------
   * PHASE I: single client test of all non-transaction functions
   *
   * Note: pious_sysinfo() already called
   *-----------------------------------------------------------------------*/

  printf("\nPhase I   - test all non-transaction functions");
  fflush(stdout);

  /* set umask */

  printf(".");
  fflush(stdout);

  if ((acode = pious_umask((pious_modet)(PIOUS_IWGRP | PIOUS_IWOTH),
			   &oldmask)) != PIOUS_OK ||
      oldmask != 0)
    {
      printf("\n\nqtest: pious_umask() failed\n");
      BailOut();
    }

  /* create directory file */

  printf(".");
  fflush(stdout);

  if ((acode = pious_mkdir(dirname, DIRMODE)) != PIOUS_OK &&
      acode != PIOUS_EEXIST)
    {
      printf("\n\nqtest: pious_mkdir() failed - can't create user ");
      printf("supplied directory\n");

      BailOut();
    }

  if (pious_smkdir(dsv, dscnt, dirname, DIRMODE) != PIOUS_EEXIST)
    {
      printf("\n\nqtest: pious_smkdir() failed\n");
      BailOut();
    }

  /* set current working directory path */

  printf(".");
  fflush(stdout);

  if (pious_setcwd(dirname) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_setcwd() failed\n");
      BailOut();
    }

  /* open, read, and write file in all views with all read/write variants */

  bufsz = (BUFSZ / (SU * dscnt)) * (SU * dscnt);

  for (i = 0; i < bufsz; i++)
    wbuf[i] = '0' + (i % dscnt);

  for (trial = 0; trial < 4; trial++)
    {
      printf(".");
      fflush(stdout);

      switch (trial)
	{
	case 0:
	  /* global view - default servers */
	  if ((fd[trial] = pious_popen(GROUPMASTER,
				       FILENAME,
				       PIOUS_GLOBAL,
				       SU,
				       PIOUS_VOLATILE,
				       PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				       REGMODE,
				       dscnt)) < 0)
	    {
	      printf("\n\nqtest: pious_popen() failed\n");
	      BailOut();
	    }
	  break;

	case 1:
	  /* independent view - default servers */
	  if ((fd[trial] = pious_open(FILENAME,
				      PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				      REGMODE)) < 0)
	    {
	      printf("\n\nqtest: pious_open() failed\n");
	      BailOut();
	    }
	  break;

	case 2:
	  /* independent view - specified servers */
	  if ((fd[trial] = pious_sopen(dsv, dscnt,
				       FILENAME,
				       PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				       REGMODE)) < 0)
	    {
	      printf("\n\nqtest: pious_sopen() failed\n");
	      BailOut();
	    }
	  break;

	case 3:
	  /* segmented view - specified servers */
	  if ((fd[trial] = pious_spopen(dsv, dscnt,
					GROUPCOHORT,
					FILENAME,
					PIOUS_SEGMENTED,
					dscnt - 1,
					PIOUS_VOLATILE,
					PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
					REGMODE,
					dscnt + 5)) < 0)
	    {
	      printf("\n\nqtest: pious_spopen() failed\n");
	      BailOut();
	    }
	  break;
	}


      /* check file status */

      if (pious_fstat(fd[trial], &statbuf) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_fstat() failed (trial: %d)\n", trial);
	  BailOut();
	}

      if (statbuf.dscnt != dscnt || statbuf.segcnt != dscnt)
	{
	  printf("\n\nqtest: pious_fstat() values inconsistent (trial: %d)\n",
		 trial);
	  BailOut();
	}


      /* initialize file */

      if (trial == 0)
	{
	  for (i = 0; i < FILESZ; i++)
	    if ((acode = pious_write(fd[trial], wbuf, bufsz)) < bufsz)
	      {
		if (acode < 0)
		  printf("\n\nqtest: pious_write() failed (init)\n");
		else
		  printf("\n\nqtest: can not write full buffer (init)\n");

		BailOut();
	      }

	  if (pious_lseek(fd[trial], (pious_offt)0, PIOUS_SEEK_SET) != 0)
	    {
	      printf("\n\nqtest: pious_lseek() failed (init)\n");
	      BailOut();
	    }
	}

      /* set pattern buffer; expected result reading from previous iteration */

      if (trial == 0 || trial == 2)
	{
	  memcpy(pbuf, wbuf, bufsz);

	  rdwrsz = bufsz;
	}

      else if (trial == 1)
	{
	  cptr = pbuf;

	  for (i = 0; i < bufsz; i += (SU * dscnt))
	    for (j = 0; j < SU; j++)
	      for (k = 0; k < dscnt; k++)
		*cptr++ = wbuf[i + (j + (SU * k))];

	  rdwrsz = bufsz;
	}

      else /* trial == 3 */
	{
	  memset(pbuf, ('0' + (dscnt - 1)), bufsz / dscnt);

	  rdwrsz = bufsz / dscnt;
	}

      /* read/validate data */

      i = 0;

      do
	{
	  memset(rbuf, 'x', rdwrsz);

	  switch(i % 3)
	    {
	    case 0:
	      acode = pious_read(fd[trial], rbuf, rdwrsz);
	      break;

	    case 1:
	      acode = pious_oread(fd[trial], rbuf, rdwrsz, &rdwroff);
	      break;

	    case 2:
	      acode = pious_pread(fd[trial], rbuf, rdwrsz,
				  (pious_offt)(i * rdwrsz));
	      break;
	    }

	  if (acode != rdwrsz && (acode != 0 || i != FILESZ))
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_*read() failed (trial: %d)\n", trial);
	      else
		printf("\n\nqtest: unexpected EOF (trial: %d)\n", trial);

	      BailOut();
	    }

	  if (memcmp(rbuf, pbuf, acode))
	    {
	      printf("\n\nqtest: file data corrupted (trial: %d)\n", trial);
	      BailOut();
	    }


	  if (i % 3 == 1)
	    {
	      if (rdwroff != (i * rdwrsz))
		{
		  printf("\n\nqtest: pious_oread() bad offset (trial: %d)\n",
			 trial);
		  BailOut();
		}
	    }

	  else if (i % 3 == 2)
	    {
	      if (pious_lseek(fd[trial],
			      (pious_offt)rdwrsz,
			      PIOUS_SEEK_CUR) != ((i + 1) * rdwrsz))
		{
		  printf("\n\nqtest: pious_lseek() read adjust failed ");
		  printf("(trial: %d)\n", trial);
		  BailOut();
		}
	    }

	  i++;
	}
      while (acode != 0);


      /* seek back to beginning of file */

      if (pious_lseek(fd[trial], (pious_offt)0, PIOUS_SEEK_SET) != 0)
	{
	  printf("\n\nqtest: pious_lseek() failed (trial: %d)\n", trial);
	  BailOut();
	}


      /* write to file */

      for (i = 0; i < FILESZ; i++)
	{
	  switch(i % 3)
	    {
	    case 0:
	      acode = pious_write(fd[trial], wbuf, rdwrsz);
	      break;

	    case 1:
	      acode = pious_owrite(fd[trial], wbuf, rdwrsz, &rdwroff);
	      break;

	    case 2:
	      acode = pious_pwrite(fd[trial], wbuf, rdwrsz,
				  (pious_offt)(i * rdwrsz));
	      break;
	    }

	  if (acode != rdwrsz)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_*write() failed (trial: %d)\n",
		       trial);
	      else
		printf("\n\nqtest: can not write full buffer (trial: %d)\n",
		       trial);

	      BailOut();
	    }

	  if (i % 3 == 1)
	    {
	      if (rdwroff != (i * rdwrsz))
		{
		  printf("\n\nqtest: pious_owrite() bad offset (trial: %d)\n",
			 trial);
		  BailOut();
		}
	    }

	  else if (i % 3 == 2)
	    {
	      if (pious_lseek(fd[trial],
			      (pious_offt)rdwrsz,
			      PIOUS_SEEK_CUR) != ((i + 1) * rdwrsz))
		{
		  printf("\n\nqtest: pious_lseek() write adjust failed ");
		  printf("(trial: %d)\n", trial);
		  BailOut();
		}
	    }
	}
    }

  for (trial = 0; trial < 4; trial++)
    if (pious_close(fd[trial]) != PIOUS_OK)
      {
	printf("\n\nqtest: pious_close() failed (trial: %d)\n", trial);
	BailOut();
      }


  /* change file permission bits */

  printf(".");
  fflush(stdout);

  if (pious_chmod(FILENAME, (pious_modet)(PIOUS_IRUSR | PIOUS_IWUSR)) < 0)
    {
      printf("\n\nqtest: pious_chmod() failed\n");
      BailOut();
    }

  if (pious_schmod(dsv, dscnt, FILENAME, (pious_modet)(PIOUS_IRUSR)) < 0)
    {
      printf("\n\nqtest: pious_schmod() failed\n");
      BailOut();
    }


  /* check file pointer sharing */

  printf(".");
  fflush(stdout);

  if ((fd[0] = pious_popen(GROUPMASTER,
			   FILENAME,
			   PIOUS_GLOBAL,
			   SU,
			   PIOUS_VOLATILE,
			   PIOUS_RDONLY,
			   REGMODE,
			   dscnt)) < 0)
    {
      printf("\n\nqtest: pious_popen() failed (shared ptr test)\n");
      BailOut();
    }

  if ((fd[1] = pious_popen(GROUPMASTER,
			   FILENAME,
			   PIOUS_GLOBAL,
			   SU,
			   PIOUS_VOLATILE,
			   PIOUS_RDONLY,
			   REGMODE,
			   dscnt)) < 0)
    {
      printf("\n\nqtest: pious_popen() failed (shared ptr test)\n");
      BailOut();
    }

  if ((fd[2] = pious_popen(GROUPCOHORT,
			   FILENAME,
			   PIOUS_GLOBAL,
			   SU,
			   PIOUS_VOLATILE,
			   PIOUS_RDONLY,
			   REGMODE,
			   dscnt)) < 0)
    {
      printf("\n\nqtest: pious_popen() failed (shared ptr test)\n");
      BailOut();
    }

  if (pious_lseek(fd[0], (pious_offt)42, PIOUS_SEEK_SET) != 42)
    {
      printf("\n\nqtest: pious_lseek() failed (shared ptr test)\n");
      BailOut();
    }

  if ((acode = pious_lseek(fd[0], (pious_offt)0, PIOUS_SEEK_CUR)) != 42)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_lseek() failed (shared ptr test)\n");
      else
	printf("\n\nqtest: pious_lseek() value erroneous (shared ptr test)\n");

      BailOut();
    }

  if ((acode = pious_lseek(fd[1], (pious_offt)0, PIOUS_SEEK_CUR)) != 42)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_lseek() failed (shared ptr test)\n");
      else
	printf("\n\nqtest: pious_lseek() value erroneous (shared ptr test)\n");

      BailOut();
    }

  if ((acode = pious_lseek(fd[2], (pious_offt)0, PIOUS_SEEK_CUR)) != 0)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_lseek() failed (shared ptr test)\n");
      else
	printf("\n\nqtest: pious_lseek() value erroneous (shared ptr test)\n");

      BailOut();
    }

  for (i = 0; i < 3; i++)
    if (pious_close(fd[i]) != PIOUS_OK)
      {
	printf("\n\nqtest: pious_close() failed (shared ptr test)\n");
	BailOut();
      }


  /* reset current working directory path */

  printf(".");
  fflush(stdout);

  if (pious_getcwd(fullfilepath, (pious_sizet)80) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_getcwd() failed\n");
      BailOut();
    }

  if (strcmp(fullfilepath, dirname))
    {
      printf("\n\nqtest: pious_getcwd() value erroneous\n");
      BailOut();
    }

  if (pious_setcwd("") != PIOUS_OK)
    {
      printf("\n\nqtest: pious_setcwd() failed\n");
      BailOut();
    }

  if (pious_getcwd(fullfilepath, (pious_sizet)80) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_getcwd() failed\n");
      BailOut();
    }

  if (*fullfilepath != '\0')
    {
      printf("\n\nqtest: pious_getcwd() value erroneous\n");
      BailOut();
    }


  /* unlink working file, checking file placement */

  printf(".");
  fflush(stdout);

  sprintf(fullfilepath, "%s/%s", dirname, FILENAME);

  if (pious_sunlink(dsv, dscnt, fullfilepath) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_sunlink() failed\n");
      BailOut();
    }

  if ((acode = pious_unlink(fullfilepath)) != PIOUS_OK &&
      acode != PIOUS_ENOENT)
    {
      printf("\n\nqtest: pious_unlink() failed\n");
      BailOut();
    }



  /* change directory permission bits */

  printf(".");
  fflush(stdout);

  if (pious_chmoddir(dirname, (pious_modet)0) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_chmoddir() failed\n");
      BailOut();
    }

  if (pious_schmoddir(dsv, dscnt,
		      dirname, (pious_modet)PIOUS_IRWXU) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_schmoddir() failed\n");
      BailOut();
    }


  /* remove directory file */

  printf(".");
  fflush(stdout);

  if ((acode = pious_srmdir(dsv, dscnt, dirname)) != PIOUS_OK &&
      acode != PIOUS_ENOENT)
    {
      printf("\n\nqtest: pious_srmdir() failed\n");
      BailOut();
    }

  if (pious_rmdir(dirname) != PIOUS_ENOENT)
    {
      printf("\n\nqtest: pious_rmdir() failed\n");
      BailOut();
    }


  /* phase I is complete */

  printf("Completed\n");




  /*-----------------------------------------------------------------------
   * PHASE II: single client user-transaction test
   *-----------------------------------------------------------------------*/

  printf("\nPhase II  - test user-transaction functions");
  fflush(stdout);


  /* create directory file */

  printf(".");
  fflush(stdout);

  if ((acode = pious_mkdir(dirname, DIRMODE)) != PIOUS_OK &&
      acode != PIOUS_EEXIST)
    {
      printf("\n\nqtest: pious_mkdir() failed\n");
      BailOut();
    }

  /* set current working directory path */

  printf(".");
  fflush(stdout);

  if (pious_setcwd(dirname) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_setcwd() failed\n");
      BailOut();
    }


  /* transaction behavior test for all file views */

  bufsz = (BUFSZ / 2) * 2;

  for (trial = 0; trial < 3; trial++)
    {
      printf(".");
      fflush(stdout);

      switch (trial)
	{
	case 0:
	  /* global view - default servers */
	  if ((fd[0] = pious_popen(GROUPMASTER,
				   FILENAME,
				   PIOUS_GLOBAL,
				   SU,
				   PIOUS_VOLATILE,
				   PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				   REGMODE,
				   dscnt)) < 0)
	    {
	      printf("\n\nqtest: pious_popen() failed (trial: %d)\n", trial);
	      BailOut();
	    }
	  break;

	case 1:
	  /* independent view - default servers */
	  if ((fd[0] = pious_popen(GROUPMASTER,
				   FILENAME,
				   PIOUS_INDEPENDENT,
				   SU,
				   PIOUS_VOLATILE,
				   PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				   REGMODE,
				   dscnt)) < 0)
	    {
	      printf("\n\nqtest: pious_popen() failed (trial: %d)\n", trial);
	      BailOut();
	    }
	  break;

	case 2:
	  /* segmented view - default servers */
	  if ((fd[0] = pious_popen(GROUPMASTER,
				   FILENAME,
				   PIOUS_SEGMENTED,
				   0,
				   PIOUS_VOLATILE,
				   PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
				   REGMODE,
				   dscnt)) < 0)
	    {
	      printf("\n\nqtest: pious_popen() failed (trial: %d)\n", trial);
	      BailOut();
	    }
	  break;
	}


      /* initialize file */

      if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tbegin() failed (init: %d)\n", trial);
	  BailOut();
	}

      memset(wbuf, 'x', bufsz);

      for (i = 0; i < FILESZ; i++)
	if ((acode = pious_write(fd[0], wbuf, bufsz)) < bufsz)
	  {
	    if (acode < 0)
	      printf("\n\nqtest: pious_write() failed (init: %d)\n", trial);
	    else
	      printf("\n\nqtest: can not write full buffer (init: %d)\n",
		     trial);

	    BailOut();
	  }

      if (pious_tend() != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tend() failed (init: %d)\n", trial);
	  BailOut();
	}

      if ((acode = pious_lseek(fd[0],
			       (pious_offt)0,
			       PIOUS_SEEK_CUR)) != (bufsz * FILESZ))
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_lseek() failed (init: %d)\n", trial);
	  else
	    printf("\n\nqtest: pious_lseek() value erroneous (init: %d)\n",
		   trial);

	  BailOut();
	}


      /* update file */

      if (pious_lseek(fd[0], (pious_offt)0, PIOUS_SEEK_SET) != 0)
	{
	  printf("\n\nqtest: pious_lseek() failed (update: %d)\n", trial);
	  BailOut();
	}

      if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tbegin() failed (update: %d)\n", trial);
	  BailOut();
	}

      memset(wbuf, 'a', bufsz / 2);

      for (i = 0; i < FILESZ; i++)
	if ((acode = pious_pwrite(fd[0],
				  wbuf,
				  bufsz / 2,
				  (pious_offt)(i * bufsz))) < bufsz / 2)
	  {
	    if (acode < 0)
	      printf("\n\nqtest: pious_pwrite() failed (update: %d)\n", trial);
	    else
	      printf("\n\nqtest: can not write full buffer (update: %d)\n",
		     trial);

	    BailOut();
	  }

      /* check update before and after ending transaction */

      for (i = 0; i < FILESZ; i++)
	{
	  memset(rbuf, 'h', bufsz);

	  if ((acode = pious_pread(fd[0],
				   rbuf,
				   bufsz,
				   (pious_offt)(i * bufsz))) < bufsz)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_pread() failed (update: %d)\n",
		       trial);
	      else
		printf("\n\nqtest: did not read full buffer (update: %d)\n",
		       trial);

	      BailOut();
	    }

	  if (memcmp(rbuf, wbuf, acode))
	    {
	      printf("\n\nqtest: file data corrupted (update: %d)\n", trial);
	      BailOut();
	    }
	}


      if (pious_tend() != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tend() failed (update: %d)\n", trial);
	  BailOut();
	}

      if ((acode = pious_lseek(fd[0], (pious_offt)0, PIOUS_SEEK_CUR)) != 0)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_lseek() failed (update: %d)\n", trial);
	  else
	    printf("\n\nqtest: pious_lseek() value erroneous (update: %d)\n",
		   trial);

	  BailOut();
	}


      for (i = 0; i < FILESZ; i++)
	{
	  memset(rbuf, 'h', bufsz);

	  if ((acode = pious_read(fd[0], rbuf, bufsz)) < bufsz)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_read() failed (update: %d)\n", trial);
	      else
		printf("\n\nqtest: did not read full buffer (update: %d)\n",
		       trial);

	      BailOut();
	    }

	  if (memcmp(rbuf, wbuf, acode))
	    {
	      printf("\n\nqtest: file data corrupted (update: %d)\n", trial);
	      BailOut();
	    }
	}


      /* test aborted update, including file pointer */

      if (pious_lseek(fd[0], (pious_offt)0, PIOUS_SEEK_SET) != 0)
	{
	  printf("\n\nqtest: pious_lseek() failed (abort: %d)\n", trial);
	  BailOut();
	}

      if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tbegin() failed (abort: %d)\n", trial);
	  BailOut();
	}

      memset(wbuf, 'x', bufsz / 2);

      for (i = 0; i < FILESZ; i++)
	if ((acode = pious_pwrite(fd[0],
				  wbuf,
				  bufsz / 2,
				  (pious_offt)(i * bufsz))) < bufsz / 2)
	  {
	    if (acode < 0)
	      printf("\n\nqtest: pious_pwrite() failed (abort: %d)\n", trial);
	    else
	      printf("\n\nqtest: can not write full buffer (abort: %d)\n",
		     trial);

	    BailOut();
	  }

      /* check update before and after aborting transaction */

      for (i = 0; i < FILESZ; i++)
	{
	  memset(rbuf, 'h', bufsz);

	  if ((acode = pious_read(fd[0], rbuf, bufsz)) < bufsz)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_read() failed (abort: %d)\n", trial);
	      else
		printf("\n\nqtest: did not read full buffer (abort: %d)\n",
		       trial);

	      BailOut();
	    }

	  if (memcmp(rbuf, wbuf, acode))
	    {
	      printf("\n\nqtest: file data corrupted (abort: %d)\n", trial);
	      BailOut();
	    }
	}

      if ((acode = pious_lseek(fd[0],
			       (pious_offt)0,
			       PIOUS_SEEK_CUR)) != bufsz * FILESZ)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_lseek() failed (abort: %d)\n", trial);
	  else
	    printf("\n\nqtest: pious_lseek() value erroneous (abort: %d)\n",
		   trial);

	  BailOut();
	}

      if (pious_tabort() != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tabort() failed (abort: %d)\n", trial);
	  BailOut();
	}


      memset(wbuf, 'a', bufsz / 2);

      for (i = 0; i < FILESZ; i++)
	{
	  memset(rbuf, 'h', bufsz);

	  if ((acode = pious_pread(fd[0],
				   rbuf,
				   bufsz,
				   (pious_offt)(i * bufsz))) < bufsz)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_pread() failed (abort: %d)\n", trial);
	      else
		printf("\n\nqtest: did not read full buffer (abort: %d)\n",
		       trial);

	      BailOut();
	    }

	  if (memcmp(rbuf, wbuf, acode))
	    {
	      printf("\n\nqtest: file data corrupted (abort: %d)\n", trial);
	      BailOut();
	    }
	}

      if ((acode = pious_lseek(fd[0], (pious_offt)0, PIOUS_SEEK_CUR)) != 0)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_lseek() failed (abort: %d)\n", trial);
	  else
	    printf("\n\nqtest: pious_lseek() value erroneous (abort: %d)\n",
		   trial);

	  BailOut();
	}


      if (pious_close(fd[0]) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_close() failed (abort: %d)\n", trial);
	  BailOut();
	}
    }


  /* phase II is complete */

  printf("Completed\n");




  /*-----------------------------------------------------------------------
   * PHASE III: multiple client consistency/deadlock test
   *-----------------------------------------------------------------------*/


  printf("\nWarning: Phase III can hang as the result of an error.\n");
  printf("         If this occurs, check PVM log files for error messages.\n");
  printf("\n         Please be patient; test may take a short time.\n");

  printf("\nPhase III - multiple client consistency/deadlock test");
  fflush(stdout);


  /* spawn cohort */

  printf(".");
  fflush(stdout);

  cohortargv[0] = COHORTFLAG;
  cohortargv[1] = dirname;
  cohortargv[2] = NULL;

  if (pvm_spawn("qtest", cohortargv, PvmTaskDefault, NULL, 1, &cohortid) != 1)
    {
      printf("\n\nqtest: pvm_spawn() failed\n");
      BailOut();
    }


 CohortStart:

  /* cohort must set up shop */

  if (!iammaster)
    { /* set current working directory path */

      if (pious_setcwd(argv[2]) != PIOUS_OK)
	{
	  printf("\n\nqtest: cohort pious_setcwd() failed\n");
	  BailOut();
	}
    }

  mc_sync();


  /* sequential consistency test - access operations */


  if (iammaster)
    {
      printf(".");
      fflush(stdout);
    }

  bufsz = (BUFSZ / 2) * 2;

  if (iammaster)
    {
      memset(wbuf, 'M', bufsz);
      memset(pbuf, 'C', bufsz);
    }
  else
    {
      memset(wbuf, 'C', bufsz);
      memset(pbuf, 'M', bufsz);
    }


  if ((fd[0] = pious_open(FILENAME,
			  PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
			  REGMODE)) < 0)
    {
      printf("\n\nqtest: pious_popen() failed (seq access)\n");
      BailOut();
    }


  mc_sync();

  for (i = 0; i < 50; i++)
    {
      while ((acode =
	      pious_pwrite(fd[0],
			   wbuf, bufsz, (pious_offt)0)) == PIOUS_EABORT);

      if (acode != bufsz)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_pwrite() failed (seq access)\n");
	  else
	    printf("\n\nqtest: can not write full buffer (seq access)\n");

	  BailOut();
	}
    }


  memset(rbuf, 'x', bufsz);

  while ((acode = pious_pread(fd[0],
			      rbuf, bufsz, (pious_offt)0)) == PIOUS_EABORT);

  if (acode < bufsz)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_pread() failed (seq access)\n");
      else
	printf("\n\nqtest: can not read full buffer (seq access)\n");

      BailOut();
    }

  if (memcmp(rbuf, wbuf, acode) && memcmp(rbuf, pbuf, acode))
    {
      printf("\n\nqtest: file data corrupted (seq access)\n");
      BailOut();
    }



  /* sequential consistency test - user-transaction operations */

  if (iammaster)
    {
      printf(".");
      fflush(stdout);
    }

  mc_sync();

  for (i = 0; i < 50; i++)
    {
      if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tbegin() failed (seq trans)\n");
	  BailOut();
	}

      if ((acode = pious_pwrite(fd[0],
				wbuf,
				bufsz / 2,
				(pious_offt)0)) != bufsz / 2 &&
	  acode != PIOUS_EABORT)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_pwrite() failed (seq trans)\n");
	  else
	    printf("\n\nqtest: can not write full buffer (seq trans)\n");

	  BailOut();
	}

      if (acode != PIOUS_EABORT)
	{
	  if ((acode = pious_pwrite(fd[0],
				    wbuf + (bufsz / 2),
				    bufsz / 2,
				    (pious_offt)(bufsz / 2))) != bufsz / 2 &&
	      acode != PIOUS_EABORT)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_pwrite() failed (seq trans)\n");
	      else
		printf("\n\nqtest: can not write full buffer (seq trans)\n");

	      BailOut();
	    }

	  if (acode != PIOUS_EABORT)
	    {
	      if ((acode = pious_tend()) != PIOUS_OK && acode != PIOUS_EABORT)
		{
		  printf("\n\nqtest: pious_tend() failed (seq trans)\n");
		  BailOut();
		}
	    }
	}
    }


  do
    {
      memset(rbuf, 'x', bufsz);

      if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_tbegin() failed (seq trans)\n");
	  BailOut();
	}

      if ((acode = pious_pread(fd[0],
			       rbuf,
			       bufsz / 2,
			       (pious_offt)0)) != bufsz / 2 &&
	  acode != PIOUS_EABORT)
	{
	  if (acode < 0)
	    printf("\n\nqtest: pious_pread() failed (seq trans)\n");
	  else
	    printf("\n\nqtest: can not read full buffer (seq trans)\n");

	  BailOut();
	}

      if (acode != PIOUS_EABORT)
	{
	  if ((acode = pious_pread(fd[0],
				   rbuf + (bufsz / 2),
				   bufsz / 2,
				   (pious_offt)(bufsz / 2))) != bufsz / 2 &&
	      acode != PIOUS_EABORT)
	    {
	      if (acode < 0)
		printf("\n\nqtest: pious_pread() failed (seq trans)\n");
	      else
		printf("\n\nqtest: can not read full buffer (seq trans)\n");

	      BailOut();
	    }

	  if (acode != PIOUS_EABORT)
	    {
	      if ((acode = pious_tend()) != PIOUS_OK && acode != PIOUS_EABORT)
		{
		  printf("\n\nqtest: pious_tend() failed (seq trans)\n");
		  BailOut();
		}
	    }
	}
    }
  while (acode == PIOUS_EABORT);


  if (memcmp(rbuf, wbuf, bufsz) && memcmp(rbuf, pbuf, bufsz))
    {
      printf("\n\nqtest: file data corrupted (seq trans)\n");
      BailOut();
    }



  /* deadlock detection test */

  if (iammaster)
    {
      printf(".");
      fflush(stdout);
    }

  mc_sync();

  if (pious_tbegin(PIOUS_VOLATILE) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_tbegin() failed (dead)\n");
      BailOut();
    }

  if (iammaster)
    rdwroff = 0;
  else
    rdwroff = bufsz / 2;

  if ((acode = pious_pwrite(fd[0],
			    wbuf,
			    bufsz / 2,
			    rdwroff)) < bufsz / 2)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_pwrite() failed (dead)\n");
      else
	printf("\n\nqtest: can not write full buffer (dead)\n");

      BailOut();
    }

  mc_sync();

  if (iammaster)
    rdwroff = bufsz / 2;
  else
    rdwroff = 0;

  if ((acode = pious_pwrite(fd[0],
			    wbuf + (bufsz / 2),
			    bufsz / 2,
			    rdwroff)) != bufsz / 2 &&
      acode != PIOUS_EABORT)
    {
      if (acode < 0)
	printf("\n\nqtest: pious_pwrite() failed (dead)\n");
      else
	printf("\n\nqtest: can not write full buffer (dead)\n");

      BailOut();
    }

  {
    int msgtag, cohortacode;

    if (pious_sysinfo(PIOUS_TAG_LOW) > 0)
      msgtag = 0;
    else
      msgtag = pious_sysinfo(PIOUS_TAG_HIGH) + 1;

    cohortacode = acode;

    if (iammaster)
      {
	pvm_recv(cohortid, msgtag);
	pvm_upkint(&cohortacode, 1, 1);

	if (!((acode == bufsz / 2 && cohortacode == PIOUS_EABORT) ||
	      (acode == PIOUS_EABORT && cohortacode == bufsz / 2)))
	  {
	    printf("\n\nqtest: pious_pwrite() result unexpected (dead)\n");

	    BailOut();
	  }
      }

    else
      {
	pvm_initsend(PvmDataDefault);
	pvm_pkint(&cohortacode, 1, 1);
	pvm_send(masterid, msgtag);
      }
  }


  if (acode != PIOUS_EABORT)
    {
      if ((acode = pious_tend()) != PIOUS_OK && acode != PIOUS_EABORT)
	{
	  printf("\n\nqtest: pious_tend() failed (dead)\n");
	  BailOut();
	}
    }


  /* close file */

  if (pious_close(fd[0]) != PIOUS_OK)
    {
      printf("\n\nqtest: pious_close() failed (dead)\n");
      BailOut();
    }

  mc_sync();


  /* phase III is complete */

  if (iammaster)
    printf("Completed\n");


  /*-----------------------------------------------------------------------
   * Cleanup
   *-----------------------------------------------------------------------*/


  if (iammaster)
    { /* unlink working file */

      if (pious_unlink(FILENAME) != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_unlink() failed\n");
	  BailOut();
	}

      /* reset current working directory path */

      if (pious_setcwd("") != PIOUS_OK)
	{
	  printf("\n\nqtest: pious_setcwd() failed\n");
	  BailOut();
	}

      /* remove directory file */

      if ((acode = pious_rmdir(dirname)) != PIOUS_OK &&
	  acode != PIOUS_ENOENT)
	{
	  printf("\n\nqtest: pious_rmdir() failed\n");
	  BailOut();
	}

      printf("\n\nPIOUS passed functionality test for this system\n\n");
    }

  pvm_exit();
}




/* master-cohort synchronization function */

static void mc_sync()
{
  int msgtag;

  if (pious_sysinfo(PIOUS_TAG_LOW) > 0)
    msgtag = 0;
  else
    msgtag = pious_sysinfo(PIOUS_TAG_HIGH) + 1;

  if (iammaster)
    {
      pvm_recv(cohortid, msgtag);

      pvm_initsend(PvmDataRaw);
      pvm_send(cohortid, msgtag);
    }

  else
    {
      pvm_initsend(PvmDataRaw);
      pvm_send(masterid, msgtag);
      pvm_recv(masterid, msgtag);
    }
}
