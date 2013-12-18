/*
 * rdwr.c - a simple PIOUS demonstration program
 *
 * Performs the following actions:
 *   1) open/create a PIOUS file
 *   2) write data to the file
 *   3) read data from the file and validate
 *   4) close the file
 *   5) unlink the file
 *
 *
 * @(#)rdwr.c	2.2  28 Apr 1995  Moyer
 */

#ifdef __STDC__
#include <string.h>
#else
#include <memory.h>
#endif

#include <stdio.h>

#include <pvm3.h>
#include <pious1.h>


#define BUFSZ   1024   /* read/write buffer size */
#define FILESZ    16   /* file size, in number of buffers */

#define BailOut() \
printf("\n\nrdwr: Bailing - reset PVM to shutdown PIOUS as state may be "); \
printf("inconsistent\n"); \
pvm_exit(); exit(1)

main()
{
  int dscnt, fd, i, acode;
  char wbuf[BUFSZ], rbuf[BUFSZ], filename[80];

  printf("\n\nRDWR - Create and access a PIOUS file\n\n");

  /* enroll in PVM */

  if (pvm_mytid() < 0)
    {
      printf("\nrdwr: unable to enroll in PVM\n");
      exit(1);
    }


  /* determine if PIOUS started with a default set of data servers */

  if ((dscnt = pious_sysinfo(PIOUS_DS_DFLT)) <= 0)
    {
      if (dscnt == 0)
	printf("\nrdwr: configure PIOUS with default data servers\n");
      else
	printf("\nrdwr: start PIOUS before executing application\n");

      BailOut();
    }


  /* get name of file to work with */

  *filename = '\0';

  printf("Enter file name: ");
  scanf("%79s", filename);
  printf("\n\n");

  if (*filename == '\0')
    {
      printf("\nrdwr: invalid file name\n");
      BailOut();
    }


  /* open/create PIOUS file */

  printf("\nOpening PIOUS file...\n");

  if ((fd = pious_open(filename,
		       PIOUS_RDWR | PIOUS_CREAT | PIOUS_TRUNC,
		       PIOUS_IRUSR | PIOUS_IWUSR)) < 0)
    {
      printf("\nrdwr: can not open/create PIOUS file\n");
      BailOut();
    }


  /* write PIOUS file */

  printf("\nWriting PIOUS file...\n");

  for (i = 0; i < FILESZ; i++)
    if ((acode = pious_write(fd, wbuf, (pious_sizet)BUFSZ)) < BUFSZ)
      {
	if (acode < 0)
	  printf("\nrdwr: error writing PIOUS file\n");
	else
	  printf("\nrdwr: can not write full buffer to PIOUS file\n");

	BailOut();
      }


  /* seek to beginning of PIOUS file */

  if (pious_lseek(fd, (pious_offt)0, PIOUS_SEEK_SET) < 0)
    {
      printf("\nrdwr: error seeking in PIOUS file\n");
      BailOut();
    }


  /* read PIOUS file and validate contents */

  printf("\nReading PIOUS file...\n");

  for (i = 0; i < FILESZ; i++)
    {
      memset(rbuf, 'x', BUFSZ);

      if ((acode = pious_read(fd, rbuf, (pious_sizet)BUFSZ)) < BUFSZ)
	{
	  if (acode < 0)
	    printf("\nrdwr: error reading PIOUS file\n");
	  else
	    printf("\nrdwr: can not read full buffer from PIOUS file\n");

	  BailOut();
	}

      if (memcmp(rbuf, wbuf, BUFSZ))
	{
	  printf("\nrdwr: PIOUS file data corrupted\n");
	  BailOut();
	}
    }


  /* close/unlink PIOUS file */

  printf("\nClosing PIOUS file...\n");

  pious_close(fd);

  printf("\nUnlinking PIOUS file...\n");

  pious_unlink(filename);


  /* exit PVM */

  pvm_exit();
}
