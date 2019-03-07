#ifndef GETOPT_H
#define GETOPT_H

#include <stdio.h>                  /* for EOF */
#include <string.h>                 /* for strchr() */


/* static (global) variables that are specified as exported by getopt() */
extern char *optarg;    /* pointer to the start of the option argument  */
extern int   optind;       /* number of the next argv[] to be evaluated    */
extern int   opterr;       /* non-zero if a question mark should be returned
                           when a non-valid option character is detected */

/* handle possible future character set concerns by putting this in a macro */
#define _next_char(string)  (char)(*(string+1))

int getopt(int argc, char *argv[], char *opstring);

#endif