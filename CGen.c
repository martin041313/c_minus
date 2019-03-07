

#include <stdio.h>
#include <stdlib.h>


#include "CGen.h"
#include "Globals.h"
#include "Util.h"
#include "Code.h"

static int tmpOffset=0;

/*********************************************************************
 *  Module-static function declarations
 */



/*
 * Traverses the AST calculating the "size" attribute for each
 *  function found - the "size" is the size of the local variables allocated
 *  on the top of the stack.
 */
void calcSizeAttribute(TreeNode *syntaxTree);


/*
 * Traverses the AST calculating AP/LP offsets for function parameters/
 *  function locals respectively.
 */
void calcStackOffsets(TreeNode *syntaxTree);


/* emitComment(): emit a DCode comment to the output file. */
void emitComment(char *comment);


/* emitCommentSeparator(): emit a cosmetic "seperator" to the output file */
void emitCommentSeparator(void);


/*
 * genProgram(): given a abstract syntax tree, generates the appropriate
 *  DCode.  Delegates to other routines as required.
 */
void genProgram(TreeNode *tree, char *fileName, char *moduleName);


/*
 * genTopLevelDecl(): given a declaration AST node, generates the
 *  appropriate DCode.  Delegates to other routines to fully handle
 *  code generation of functions.
 *
 * PRE: "output" must be an open file descriptor
 */
void genTopLevelDecl(TreeNode *tree);


/*
 * genFunction(): given a function declaration AST node, generates the
 *  appropriate DCode.  Delegates to other routines to fully handle
 *  code generation of function locals and bodies.
 *
 * PRE: "output" must be an open file descriptor
 */
void genFunction(TreeNode *tree);


/*
 * genFunctionLocals(): given a function declaration AST node, generates the
 *  appropriate DCode.
 *
 * PRE: "output" must be an open file descriptor
 */
void genFunctionLocals(TreeNode *tree);

/* used by genFunctionLocals() */
void genFunctionLocals2(TreeNode *tree);


/*
 *  genStatement(): given a statement AST node, generates
 *   appropriate DCode. Delegates as necessary.
 */
void genStatement();


/*
 * Generate DCode for an Expression.
 */
void genExpression(TreeNode *tree, int addressNeeded);


/*
 * Generate and return a new unique label.
 */
char *genNewLabel(void);





/*
 * genIfStmtt(): generates DCode to evaluate an IF statement.
 */
void genIfStmt(TreeNode *tree);

void genAssStmt(TreeNode *tree);
/*
 * genWhileStmt(): generate DCode to evaluate a WHILE statement.
 */
void genWhileStmt(TreeNode *tree);


/*
 * genReturnStatement(): generate DCode to evaluate a RETURN statement.
 */
void genReturnStmt(TreeNode *tree);


/*
 * genCallStmt(): generates DCode to evaluate CALL statements.
 */
void genCallStmt(TreeNode *tree);


/*
 * varSize(): given a scalar/array declaration, computes the size of the
 *  variable, or returns 0 otherwise.
 */
int varSize(TreeNode *tree);


/*********************************************************************
 *  Public function definitions
 */

void codeGen(TreeNode *syntaxTree, char *fileName, char *moduleName)
{
    /* attempt to open output file for writing */
    output = fopen(fileName, "w");
    if (output == NULL)
    {
        Error = TRUE;
        fprintf(listing, ">>> Unable to open output file for writing.\n");
    }
    else
    {
        /* begin code generation */

        /*
         * There are three attributes that need to be synthesised before
         *  code generation can begin.  They're the locals-on-the-stack
         *  size, and the "assembly-area" size (assembly area is where
         *  parameters for function calls are set up before execution), as
         *  well as AP/LP stack-offsets for locals/parameters.
         */

        calcSizeAttribute(syntaxTree);
        calcStackOffsets(syntaxTree);

        genProgram(syntaxTree, fileName, moduleName);
    }
}

/*********************************************************************
 *  Static function definitions
 */




void calcSizeAttribute(TreeNode *syntaxTree)
{
    int i;      /* used for iterating over tree-node children */
    static int size;   /* amount of space locals are taking up on stack */

    while (syntaxTree != NULL)
    {
        /*
         * entering a function call?  the local area size obviously has
         *  to be set to 0.
         */

        if ((syntaxTree->nodekind == DecK)
                && (syntaxTree->kind.dec == FuncDecK) || (syntaxTree->isGlobal==TRUE))
        {
            size = 0;
        }



        /* visit children nodes */
        for (i=0; i < MAXCHILDREN; ++i)
            if(syntaxTree->child[i]!=NULL) calcSizeAttribute(syntaxTree->child[i]);

        /* have we hit a local declaration? increase "size" */
        if ((syntaxTree->nodekind == DecK)
                && ((syntaxTree->kind.dec == ScalarDecK)
                    || (syntaxTree->kind.dec == ArrayDecK))
           )
        {
            if (syntaxTree->kind.dec == ScalarDecK)
                size += WORDSIZE;
            else if (syntaxTree->kind.dec == ArrayDecK && syntaxTree->isParameter==TRUE)
                size += WORDSIZE;
            else if (syntaxTree->kind.dec == ArrayDecK && syntaxTree->isParameter==FALSE)
                size += (WORDSIZE * syntaxTree->val);
            /* record the attribute */
            syntaxTree->localSize = size;
        }

        /* leaving a function? record attribute in function-dec node */
        if ((syntaxTree->nodekind == DecK)
                && (syntaxTree->kind.dec == FuncDecK))
        {
            /* debug output */
            DEBUG_ONLY( fprintf(listing,
                                "*** Calculated localSize attribute for %s() as %d.\n",
                                syntaxTree->name, size+3); );

            syntaxTree->localSize = size+3;
        }

        syntaxTree = syntaxTree->sibling;
        if (syntaxTree==NULL)
        {
            return;
        }

    }
}


/*
 * Traverses the AST calculating AP/LP offsets for function parameters/
 *  function locals respectively.
 */
void calcStackOffsets(TreeNode *syntaxTree)
{
    int i;      /* used for iterating over tree-node children */

    /* offsets for LP and AP respectively */
    static int GP=0;
    static int LP=0;


    while (syntaxTree != NULL)
    {
        /* If we're entering a function, reset offset counters to 0 */
        if ((syntaxTree->nodekind==DecK) && (syntaxTree->kind.dec==FuncDecK))
        {

            LP = -2;
        }

        /* visit children nodes */
        for (i=0; i < MAXCHILDREN; ++i)
            if(syntaxTree->child[i]!=NULL) calcStackOffsets(syntaxTree->child[i]);

        /* post-order stuff goes here */
        if ((syntaxTree->nodekind == DecK) &&
                ((syntaxTree->kind.dec==ArrayDecK)
                 ||(syntaxTree->kind.dec==ScalarDecK)))
        {
            if (syntaxTree->isGlobal)
            {
                syntaxTree->offset = GP;
                GP += varSize(syntaxTree);

                DEBUG_ONLY(
                    fprintf(listing,
                            "*** computed offset attribute for %s as %d\n",
                            syntaxTree->name, syntaxTree->offset); );
            }
            else
            {

                LP -= varSize(syntaxTree);
                syntaxTree->offset = LP;

                DEBUG_ONLY(
                    fprintf(listing,
                            "*** computed offset attribute for %s as %d\n",
                            syntaxTree->name, syntaxTree->offset); );

            }
        }

        syntaxTree = syntaxTree->sibling;
    }
}






void emitCommentSeparator(void)
{
    if (TraceCode)
        emitComment("************************************************************\n");
}


/*
 * genTopLevelDecl(): given a declaration AST node, generates the
 *  appropriate DCode.  Delegates to other routines to fully handle
 *  code generation of functions.
 *
 * PRE: "output" must be an open file descriptor
 */
void genTopLevelDecl(TreeNode *tree)
{
    /*
     * There are 3 types of top level declarations in C-: scalar declarations,
     *  array declarations and functions.
     */

    TreeNode *current;
    char commentBuffer[80];  /* used for formatting comments */

    current = tree;

    while (current != NULL)
    {
        if ((current->nodekind == DecK) && (current->kind.dec == ScalarDecK))
        {
            /* scalar */
            if (TraceCode)
            {
                emitCommentSeparator();
                sprintf(commentBuffer,
                        "Variable \"%s\" is a scalar of type %s\n",
                        current->name, typeName(current->variableDataType));
                emitComment(commentBuffer);
            }

        }
        else if ((current->nodekind==DecK)&&(current->kind.dec==ArrayDecK))
        {
            /* array */
            if (TraceCode)
            {
                emitCommentSeparator();
                if (current->val==0)
                {
                    assert(current->isParameter==TRUE);
                    sprintf(commentBuffer,
                            "Variable \"%s\" is an array of type %s and size unknow and is parameter.\n",
                            current->name, typeName(current->variableDataType)
                           );
                }
                else
                    sprintf(commentBuffer,
                            "Variable \"%s\" is an array of type %s and size %d\n",
                            current->name, typeName(current->variableDataType),
                            current->val);
                emitComment(commentBuffer);
            }


        }
        else if ((current->nodekind==DecK)&&(current->kind.dec==FuncDecK))
        {
            /* function */
            genFunction(current);
        }

        current = current->sibling;
    }
}


/*
 * genFunction(): given a function declaration AST node, generates the
 *  appropriate DCode.  Delegates to other routines to fully handle
 *  code generation of function locals and bodies.
 *
 * PRE: "output" must be an open file descriptor AND "tree" must be a
 *       function declaration node.
 */
void genFunction(TreeNode *tree)
{
    char commentBuffer[80];  /* used for formatting comments */

    /*
     * Generate headers, call genFunctionLocals(), and then
     * genStatement().
     */

    /* node must be a function declaration */
    assert((tree->nodekind == DecK) && (tree->kind.dec == FuncDecK));

    if (TraceCode)
    {
        emitCommentSeparator();
        sprintf(commentBuffer,
                "Function declaration for \"%s()\"", tree->name);
        emitComment(commentBuffer);


    }
    /* emit function header */

    sprintf(commentBuffer,"%s",tree->name);
    emitLabel(commentBuffer,"function entry");
    /* make sure all local variables get declared */
    genFunctionLocals(tree);
    tmpOffset = -tree->localSize;

    /* begin of procedure, make it so */
    emitRM("ST",ac,retFO,mp,"ret from call");
    emitRO("LDC",ac,tmpOffset,ac,"get function stack size");
    emitRM("ST",ac,initFO,mp,"set stack size");
    genStatement(tree->child[1]);

    /* end of procedure, make it so */
    if (strcmp(tree->name,"main")==0)
    {
        emitRO("HALT",0,0,0,"halt");
    }
    else
        emitRM("LD",pc,retFO,mp,"ret from call");

}


/*
 * genFunctionLocals(): given a function declaration AST node, generates the
 *  appropriate DCode.
 *
 * PRE: "output" must be an open file descriptor
 */


void genFunctionLocals(TreeNode *tree)
{
    /*
     * We don't want to traverse the sibling nodes of "tree", so we
     *  just iteratively call getFunctionLocals2() on "tree"s children.
     */
    int i;

    for (i=0; i<MAXCHILDREN; ++i)
        if (tree->child[i] != NULL)
            genFunctionLocals2(tree->child[i]);
}

void genFunctionLocals2(TreeNode *tree)
{
    /*
     * Do a postorder traversal of the syntax tree looking for non-parameter
     * declarations, and emit code to declare them.
     */

    int i;
    int offset; /* "offset" for .LOCAL declaration */
    char commentBuffer[80];

    while (tree != NULL)
    {
        /* preorder operations go here */

        for (i=0; i < MAXCHILDREN; ++i)
            genFunctionLocals2(tree->child[i]);

        /* postorder operations go here */
        if (tree->nodekind==DecK)
        {


            /* calculate offset and emit .LOCAL directive */




            offset = tree->offset;
            sprintf(commentBuffer,"LOCAL _%s %d,%d",tree->name, offset, varSize(tree));

            emitComment(commentBuffer);
            /* insert a \n to make output more readable */


        }

        tree = tree->sibling;
    }
}


void genStatement(TreeNode *tree)
{
    TreeNode *current;

    current = tree;

    while (current != NULL)
    {
        /* assignment */
        if ((current->nodekind==ExpK) && (current->kind.exp==AssignK))
        {
            genAssStmt(current);

        }
        else if (current->nodekind==StmtK)
        {

            /* statements */

            switch(current->kind.stmt)
            {
            case IfK:

                genIfStmt(current);
                break;

            case WhileK:

                genWhileStmt(current);
                break;

            case ReturnK:

                genReturnStmt(current);
                break;

            case CallK:

                genCallStmt(current);
                break;

            case CompoundK:

                genStatement(current->child[1]);
                break;
            }
        }

        current = current->sibling;
    }
}


/*
 * Generate DCode for an Expression.
 */
void genExpression(TreeNode *tree, int addressNeeded)
{
    char scratch[80];   /* used for assembling arguments to instructions */
    int loc=0;
    TreeNode * p1=NULL, * p2=NULL;

    /* if it's an expression, eval as expression... */
    if (tree->nodekind == ExpK)
    {

        switch (tree->kind.exp)
        {
        case IdK:
            if (tree->declaration->kind.dec == ArrayDecK)
            {

                /* are we just passing the name of an array as parameter?
                   Pass as reference */
                if (tree->child[0]==NULL)
                {
                    emitComment("leave address of array on stack");
                    if (tree->declaration->isGlobal) /* GLOBAL VARIABLE */
                    {
                        emitComment("push address of global variable");


                        emitRM("LDA",ac,tree->declaration->offset,gp,"get the value");



                    }
                    else if (tree->declaration->isParameter)
                    {


                        emitRM("LD",ac,tree->declaration->offset,mp,"get the value");


                    }
                    else
                    {
                        emitRM("LDA",ac,tree->declaration->offset,mp,"get the value");
                    }

                }
                else
                {
                    /* calculate array offset */
                    emitComment("calculate array offset");
                    genExpression(tree->child[0], FALSE);

                    emitComment("get address of array onto stack");
                    if (tree->declaration->isGlobal || tree->declaration->isParameter) /* GLOBAL VARIABLE */
                    {
                        emitComment("push address of global variable");

                        if (addressNeeded)
                        {
                            emitRO("ADD",ac,ac,gp,"op: load left");
                            emitRM("LDA",ac,0,ac,"get the value");
                        }
                        else
                        {
                            emitRO("ADD",ac,ac,gp,"op: load left");
                            emitRM("LD",ac,0,ac,"get the value");
                        }

                    }
                    else
                    {


                        if (addressNeeded)
                        {
                            emitRO("ADD",ac,ac,mp,"op: load left");
                            emitRM("LDA",ac,tree->declaration->offset,ac,"get the value");
                        }
                        else
                        {
                            emitRO("ADD",ac,ac,mp,"op: load left");
                            emitRM("LD",ac,tree->declaration->offset,ac,"get the value");
                        }
                    }


                }

            }
            else if (tree->declaration->kind.dec == ScalarDecK)
            {

                /* get effective address */
                emitComment("calculate effective address of variable");
                if (tree->declaration->isGlobal)   /* GLOBAL VARIABLE */
                {
                    if (TraceCode) emitComment("->global Id") ;


                    if (addressNeeded)
                    {

                        emitRM("LDA",ac,tree->declaration->offset,mp,"get the value");
                    }
                    else
                    {

                        emitRM("LD",ac,tree->declaration->offset,mp,"get the value");
                    }
                    if (TraceCode)  emitComment("<- Id") ;
                }
                else
                {


                    if (addressNeeded)
                    {

                        emitRM("LDA",ac,tree->declaration->offset,mp,"get the value");
                    }
                    else
                    {

                        emitRM("LD",ac,tree->declaration->offset,mp,"get the value");
                    }
                }





            }

            break;

        case OpK:

            /* compute operands */

            if (TraceCode) emitComment("-> Op") ;
            p1 = tree->child[0];
            p2 = tree->child[1];
            /* gen code for ac = left arg */
            genExpression(p1,FALSE);
            /* gen code to push left operand */
            //emitRO("ADD",ac1,gp,ac,"move ac to ac1");
            emitRM("ST",ac,tmpOffset--,mp,"push");
            /* gen code for ac = right operand */
            genExpression(p2,FALSE);
            /* now load left operand */
            emitRM("LD",ac1,++tmpOffset,mp,"pop");
            switch (tree->op)
            {

            case PLUS:


                emitRO("ADD",ac,ac1,ac,"*op +");
                break;

            case MINUS:
                emitRO("SUB",ac,ac1,ac,"*op -");

                break;

            case TIMES:
                emitRO("MUL",ac,ac1,ac,"*op *");

                break;

            case DIVIDE:
                emitRO("DIV",ac,ac1,ac,"*op /");

                break;

            case LT:
                emitRO("SUB",ac,ac1,ac,"op <") ; // ac = ac1-ac(left-right)
                emitRM("JLT",ac,2,pc,"br if true") ; //ac<0 (left<right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            case GT:
                emitRO("SUB",ac,ac1,ac,"op >") ; // ac = ac1-ac(left-right)
                emitRM("JGT",ac,2,pc,"br if true") ; //a>0 (left>right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            case NE:
                emitRO("SUB",ac,ac1,ac,"op !=") ; // ac = ac1-ac(left-right)
                emitRM("JNE",ac,2,pc,"br if true") ; //ac!=0 (left!=right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            case LTE:
                emitRO("SUB",ac,ac1,ac,"op <=") ; // ac = ac1-ac(left-right)
                emitRM("JLE",ac,2,pc,"br if true") ; //ac<=0 (left<=right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            case GTE:
                emitRO("SUB",ac,ac1,ac,"op >=") ; // ac = ac1-ac(left-right)
                emitRM("JGE",ac,2,pc,"br if true") ; //ac>=0 (left>=right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            case EQ:
                emitRO("SUB",ac,ac1,ac,"op ==") ; // ac = ac1-ac(left-right)
                emitRM("JEQ",ac,2,pc,"br if true") ; //ac==0 (left==right) goto pc+3
                emitRM("LDC",ac,0,ac,"false case") ; //else ac=0
                emitRM("LDA",pc,1,pc,"unconditional jmp") ; //pc=pc+2
                emitRM("LDC",ac,1,ac,"true case") ; //ac=1

                break;

            default:
                abort();
            }

            break;

        case ConstK:

            sprintf(scratch, "* pshLit  %d", tree->val);
            emitRM("LDC",ac,tree->val,ac,"get the value");

            break;

        case AssignK:

            genAssStmt(tree);
            break;

        }
    }
    else if (tree->nodekind == StmtK)
    {
        if (tree->kind.stmt == CallK)
        {
            genCallStmt(tree);
        }
    }
}


/*
 * Generate and return a new unique label.
 */
char *genNewLabel(void)
{
    static int nextLabel = 0;
    char labelBuffer[40];


    sprintf(labelBuffer, "label%d", nextLabel++);
    return copyString(labelBuffer);
}





/*
 * genIfStmtt(): generates DCode to evaluate an IF statement.
 */
void genIfStmt(TreeNode *tree)
{
    char *elseLabel;
    char *endLabel;

    elseLabel = genNewLabel();
    endLabel = genNewLabel();

    /* test expression */
    emitComment("IF statement");
    emitComment("if false, jump to else-part");
    genExpression(tree->child[0], FALSE);


    emitGoto("JEQ",ac,elseLabel,gp,"else label");


    genStatement(tree->child[1]); /* then-part */

    emitGoto("LDA",pc,endLabel,gp,"endLabel");


    /* emit else-part label */


    emitLabel(elseLabel,"elseLabel");
    genStatement(tree->child[2]);  /* else-part */

    /* emit end-label */


    emitLabel(endLabel,"endLabel");
}


/*
 * genWhileStmt(): generate DCode to evaluate a WHILE statement.
 */
void genWhileStmt(TreeNode *tree)
{
    char *startLabel;
    char *endLabel;


    startLabel = genNewLabel();
    endLabel = genNewLabel();

    emitComment("WHILE statement");
    emitComment("if expression evaluates to FALSE, exit loop");

    /* emit start label */


    emitLabel(startLabel,"startLabel");
    genExpression(tree->child[0], FALSE);

    /* generate conditional branch */

    emitGoto("JEQ",ac,endLabel,gp,"endLabel");
    /* emit body */
    genStatement(tree->child[1]);

    /* emit branch to start */

    emitGoto("LDA",pc,startLabel,gp,"startLabel");


    /* generate end label */

    emitLabel(endLabel,"endLabel");
}


/*
 * genReturnStatement(): generate DCode to evaluate a RETURN statement.
 */
void genReturnStmt(TreeNode *tree)
{
    if (tree->declaration->functionReturnType != Void)
    {

        if (tree->child[0] != NULL)
            genExpression(tree->child[0], FALSE);
        else

            emitRO("LDC",ac,0,ac,"return 0");


    }
    emitRO("LD",pc,retFO,mp,"return to call place");
}


/*
 * genCallStmt(): generates DCode to evaluate CALL statements.
 */
void genCallStmt(TreeNode *tree)
{
    int numPars = 0;
    int offset;
    TreeNode *argPtr;   /* iterate over function arguments */
    TreeNode* calledFunc;
    HashNodePtr calledFuncHash;
    argPtr = tree->child[0];
    offset=tmpOffset;
    emitRM("ST",mp,tmpOffset--,mp,"save ofp"); //ofp
    tmpOffset--;//for ret
    calledFuncHash = lookupSymbol(tree->name);
    assert(calledFuncHash!=NULL);
    calledFunc = calledFuncHash->declaration;
    emitRO("LDC",ac,-calledFunc->localSize,ac,"the func stack size");
    emitRM("ST",ac,tmpOffset--,mp,"save init");

    while (argPtr != NULL)
    {
        genExpression(argPtr, FALSE);

        emitRM("ST",ac,tmpOffset--,mp,"push args");
        ++numPars;
        argPtr = argPtr->sibling;
    }
    emitRM("LDA",mp,offset,mp,"change the fp");
    emitRO("LDA",ac,1,pc,"save ret in ac");

    emitGoto("LDA",pc,tree->name,gp,"func call");
    emitRM("LD",mp,ofpFO,mp,"restore old fp");
    tmpOffset=offset;

}
void genAssStmt(TreeNode *tree)
{
    /* generate code to find rvalue (value) */
    emitComment("calculate the rvalue of the assignment");
    genExpression(tree->child[1], FALSE);

    /* gen code to push left operand */
    emitRM("ST",ac,tmpOffset--,mp,"push");

    /* find lvalue (address) */
    emitComment("calculate the lvalue of the assignment");
    genExpression(tree->child[0], TRUE);
    /* now load left operand */
    emitRM("LD",ac1,++tmpOffset,mp,"pop");
    /* do assignment */
    emitComment("perform assignment");

    emitRM("ST",ac1,0,ac,"assign");
}

/*
 * genProgram(): given a abstract syntax tree, generates the appropriate
 *  DCode.  Delegates to other routines as required.
 */
void genProgram(TreeNode *tree, char *fileName, char *moduleName)
{

    emitRM("LD",mp,0,0,"load max address from mem[0]");
    emitRM("ST",0,0,0,"clear mem[0]");
    emitGoto("LDA",pc,"main",gp,"goto main");
    emitLabel("input","input methond");
    emitRM("ST",ac,retFO,mp,"save ret in ac");
    emitRO("IN",ac,0,0,"input");
    emitRM("LD",pc,retFO,mp,"load pc back");

    emitLabel("output","output methond");
    emitRM("ST",ac,retFO,mp,"save ret in ac");
    emitRM("LD",ac,-3,mp,"load args");
    emitRO("OUT",ac,0,0,"output");
    emitRM("LD",pc,retFO,mp,"load pc back");


    /* generate the rest of the program */
    genTopLevelDecl(tree);
    emitRO("HALT",0,0,0,"halt");
}


int varSize(TreeNode *tree)
{
    int size;

    if (tree->nodekind == DecK)
    {
        if (tree->kind.dec == ScalarDecK)
            size = WORDSIZE;
        else if (tree->kind.dec == ArrayDecK)
        {
            /* array parameters are passed by reference! */
            if (tree->isParameter)
                size = WORDSIZE;
            else
                size = WORDSIZE * (tree->val);
        }
        else
            /* if is anything else, then we can't take the size of it */
            size = 0;
    }
    else
        size = 0;  /* it's not something we can take the size of... */

    return size;
}



/* END OF FILE */
