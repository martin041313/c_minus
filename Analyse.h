


#ifndef ANALYSE_H
#define ANALYSE_H

#include "Globals.h"

/*
 * NAME:    buildSymboltable()
 * PURPOSE: Takes a syntax tree and traverses it, building a symbol table
 *           and decorating all identifiers with references to their
 *           declarations.
 */

void buildSymbolTable(TreeNode *syntaxTree);


/*
 * NAME:    typeCheck()
 * PURPOSE: Traverses the syntax tree and type checks the program.
 */

void typeCheck(TreeNode *syntaxTree);

#endif

/* END OF FILE */

