

#ifndef CGEN_H
#define CGEN_H





#include "Globals.h"
#include "SymTab.h"
/*
 * Constant definitions that need to be visible elsewhere.
 */

#define WORDSIZE 1


/*
 * externs that need to be visible here
 */

extern int TraceCode;


/*
 * NAME:    codeGen()
 * PURPOSE: Generates code from the program's abstract syntax tree, and
 *           sends the resulting dcodes to the file named by "fileName".
 */

void codeGen(TreeNode *syntaxTree, char *fileName, char *moduleName);

#endif

/* END OF FILE */
