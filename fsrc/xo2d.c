/* xo2d - hex and octal string to decimal string filter
 *
 * @(#)xo2d.c	2.2  28 Apr 1995  Moyer
 *
 * Simple filter for converting hex and octal strings to decimal strings.
 * Character strings and decimal strings are passed through unchanged.
 *
 * Intended for use with piousC2Ftool; it is not general purpose.
 *
 * Assumes:
 *   1) all input strings contain at most 79 characters.
 *   2) character strings do not begin with a digit, '+', or '-' character.
 *   2) decimal, hex, and octal input strings are syntactically correct.
 */

#include <stdio.h>
#include <ctype.h>

main ()
{
  char buf[80];
  int decival;

  while (scanf("%79s", buf) == 1)
    {
      if (!isdigit(*buf) && *buf != '-' && *buf != '+')
	{ /* character string */
	  printf("%s\n", buf);
	}

      else
	{ /* numeric string */
	  sscanf(buf, "%i", &decival);
	  printf("%d\n", decival);
	}
    }
}
