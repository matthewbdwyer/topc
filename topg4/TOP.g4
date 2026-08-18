grammar TOP;
// Grammar for TOP: Typed Ownership Programming language.
// Extended from Moeller and Schwartzbach's TIP with ownership, algebraic sum types, and case expressions.

////////////////////// TOP Programs ////////////////////////// 

program : (typeDecl | function)+
;

function : nameDeclaration 
           '(' (nameDeclaration (',' nameDeclaration)*)? ')'
           '{' (declaration*) (statement*) returnStmt '}' 
;

////////////////////// TOP Type Declarations ////////////////////////// 

// Sum type declarations (top-level only).
// Type names and constructor names must begin with an uppercase letter (CONID).
typeDecl : KTYPE CONID '=' sumVariant ('|' sumVariant)* ';' ;

sumVariant : CONID ('(' nameDeclaration (',' nameDeclaration)* ')')? ;

////////////////////// TOP Declarations ///////////////////////// 

declaration : KVAR nameDeclaration (',' nameDeclaration)* ';' ;

nameDeclaration : IDENTIFIER ;

////////////////////// TOP Expressions ////////////////////////// 

// Expressions in TOP are ordered to capture precedence.
// We adopt the C convention that orders operators as:
//   postfix, unary, multiplicative, additive, relational, and equality 
//
// NB: # creates rule label that can be accessed in visitor
//
// ANTLR4 can automatically refactor direct left-recursion so we
// place all recursive rules as options in a single rule.  This
// means that we have some complex rules here that might otherwise
// be separated out, e.g., funAppExpr, and that we can't factor out
// other useful concepts, e.g., defining a rule for the subset of
// expressions that can be used as an l-value.  We prefer a clean 
// grammar, which simplifies AST construction, and work around these
// issues elsewhere in the compiler, e.g.,  introducing an assignable expr
// weeding pass. 
//
expr : expr '(' (expr (',' expr)*)? ')' 	#funAppExpr
     | '*' expr 				#deRefExpr
     | SUB NUMBER				#negNumber
     | '&' expr					#refExpr
     | expr op=(MUL | DIV) expr 		#multiplicativeExpr
     | expr op=(ADD | SUB) expr 		#additiveExpr
     | expr op=GT expr 				#relationalExpr
     | expr op=(EQ | NE) expr 			#equalityExpr
     | CONID '(' expr (',' expr)* ')'		#sumCtorExprWithArgs
     | CONID					#sumCtorExprNoArgs
     | IDENTIFIER				#varExpr
     | NUMBER					#numExpr
     | KINPUT					#inputExpr
     | KALLOC expr				#allocExpr
     | '(' expr ')'				#parenExpr
;

////////////////////// TOP Statements ////////////////////////// 

// NOTE: returnStmt is deliberately NOT a statement.  A 'return' may appear only
// as a function's mandatory final statement (see the function rule); there is no
// nested or early-return form in TOP, so nested returns fail to parse.
statement : blockStmt
    | assignStmt
    | whileStmt
    | ifStmt
    | outputStmt
    | errorStmt
    | caseStmt
;

assignStmt : expr '=' expr ';' ;

blockStmt : '{' (statement*) '}' ;

whileStmt : KWHILE '(' expr ')' statement ;

ifStmt : KIF '(' expr ')' statement (KELSE statement)? ;

outputStmt : KOUTPUT expr ';'  ;

errorStmt : KERROR expr ';'  ;

returnStmt : KRETURN expr ';'  ;

// TOP case statement (pattern-match on sum type)
caseStmt : KCASE expr KOF '{' caseArm+ '}' ;
caseArm  : CONID ('(' pattern (',' pattern)* ')')? ARROW statement ;

// Patterns appear in case-arm payloads and may be nested.
pattern
    : KWILDCARD                                                     // wildcard
    | CONID '(' pattern (',' pattern)* ')'                          // ctor with payload
    | CONID                                                         // no-arg ctor
    | IDENTIFIER                                                    // variable binding
    ;

////////////////////// TOP Lexicon ////////////////////////// 

// By convention ANTLR4 lexical elements use all caps

MUL : '*' ;
DIV : '/' ;
ADD : '+' ;
SUB : '-' ;
GT  : '>' ;
EQ  : '==' ;
NE  : '!=' ;

NUMBER : [0-9]+ ;

// Existing TOP keywords
KALLOC  : 'alloc' ;
KINPUT  : 'input' ;
KWHILE  : 'while' ;
KIF     : 'if' ;
KELSE   : 'else' ;
KVAR    : 'var' ;
KRETURN : 'return' ;
KNULL   : 'null' ;
KOUTPUT : 'output' ;
KERROR  : 'error' ;

// TOP keywords
KTYPE   : 'type' ;
KCASE   : 'case' ;
KOF     : 'of' ;

// TOP operator tokens (must precede SUB so maximal-munch applies)
ARROW   : '->' ;

// Constructor/type identifiers must begin with an uppercase letter.
// CONID must precede IDENTIFIER so ANTLR4 assigns uppercase-starting words
// to CONID via the first-match rule.
CONID : [A-Z][a-zA-Z0-9_]* ;

// KWILDCARD must precede IDENTIFIER: the single underscore `_` is the wildcard
// pattern and must not be lexed as an IDENTIFIER.
KWILDCARD : '_' ;

IDENTIFIER : [a-zA-Z][a-zA-Z0-9_]* | '_' [a-zA-Z0-9_]+ ;

// ANTLR4 has a nice mechanism for specifying the characters that should
// skipped during parsing.  You write "-> skip" after the pattern and
// let ANTLR4s pattern matching do the rest.

// Ignore whitespace
WS : [ \t\n\r]+ -> skip ;

// This does not handle nested block comments.
BLOCKCOMMENT: '/*' .*? '*/' -> skip ;

COMMENT : '//' ~[\n\r]* -> skip ;
