


#ifndef SCAN_H
#define SCAN_H


/* Define the maximum token length (use Turbo Pascal conventions) */
#define MAXTOKENLEN     64

/* tokenString holds the lexeme being scanned */
extern char tokenString[MAXTOKENLEN+1];

/*
 * NAME:     getToken()
 * PURPOSE:  Returns the next token in the source file.
 */

TokenType getToken(void);

#endif

/* END OF FILE */
