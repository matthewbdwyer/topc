grammar TIP;
// Grammar for Moeller and Schwartzbach's Tiny Imperative Language (TIP)
// Extended with TOP (Typed Ownership Programming) constructs.

////////////////////// TIP Programs ////////////////////////// 

program : (typeDecl | function)+
;

function : nameDeclaration 
           '(' (nameDeclaration (',' nameDeclaration)*)? ')'
           KPOLY?
           '{' (declaration*) (statement*) returnStmt '}' 
;

////////////////////// TOP Type Declarations ////////////////////////// 

// Sum type declarations (top-level only).
// Type names and constructor names must begin with an uppercase letter (CONID).
typeDecl : KTYPE CONID '=' sumVariant ('|' sumVariant)* ';' ;

sumVariant : CONID ('(' nameDeclaration (',' nameDeclaration)* ')')? ;

////////////////////// TIP Declarations ///////////////////////// 

declaration : KVAR nameDeclaration (',' nameDeclaration)* ';' ;

nameDeclaration : IDENTIFIER ;

////////////////////// TIP Expressions ////////////////////////// 

// Expressions in TIP are ordered to capture precedence.
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
     | expr '.' IDENTIFIER 			#accessExpr
     | '*' expr 				#deRefExpr
     | SUB NUMBER				#negNumber
     | '&' expr					#refExpr
     | expr op=(MUL | DIV) expr 		#multiplicativeExpr
     | expr op=(ADD | SUB) expr 		#additiveExpr
     | expr op=GT expr 				#relationalExpr
     | expr op=(EQ | NE) expr 			#equalityExpr
     | expr DOTDOT expr (KBY expr)?             #rangeExpr
     | IDENTIFIER				#varExpr
     | NUMBER					#numExpr
     | KINPUT					#inputExpr
     | KALLOC expr				#allocExpr
     | KNULL					#nullExpr
     | recordExpr				#recordRule
     | '(' expr ')'				#parenExpr
;

recordExpr : '{' (fieldExpr (',' fieldExpr)*)? '}' ;

fieldExpr : IDENTIFIER ':' expr ;

////////////////////// TIP Statements ////////////////////////// 

statement : blockStmt
    | assignStmt
    | whileStmt
    | ifStmt
    | outputStmt
    | errorStmt
    | caseStmt
    | forStmt
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
caseArm  : CONID ('(' IDENTIFIER (',' IDENTIFIER)* ')')? ARROW statement ;

// SOP for-loop stub (AST building deferred to sopc)
forStmt : KFOR '(' nameDeclaration ':' expr ')' statement ;

////////////////////// TIP Lexicon ////////////////////////// 

// By convention ANTLR4 lexical elements use all caps

MUL : '*' ;
DIV : '/' ;
ADD : '+' ;
SUB : '-' ;
GT  : '>' ;
EQ  : '==' ;
NE  : '!=' ;

NUMBER : [0-9]+ ;

// Existing TIP keywords
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

// Keyword to declare functions as polymorphic (ignored in Phase 7; removed after)
KPOLY   : 'poly' ;

// TOP keywords
KTYPE   : 'type' ;
KCASE   : 'case' ;
KOF     : 'of' ;
KFOR    : 'for' ;
KBY     : 'by' ;

// TOP operator tokens (must precede SUB and GT so maximal-munch applies)
ARROW   : '->' ;
DOTDOT  : '..' ;

// Constructor/type identifiers must begin with an uppercase letter.
// CONID must precede IDENTIFIER so ANTLR4 assigns uppercase-starting words
// to CONID via the first-match rule.
CONID : [A-Z][a-zA-Z0-9_]* ;

IDENTIFIER : [a-zA-Z_][a-zA-Z0-9_]* ;

// ANTLR4 has a nice mechanism for specifying the characters that should
// skipped during parsing.  You write "-> skip" after the pattern and
// let ANTLR4s pattern matching do the rest.

// Ignore whitespace
WS : [ \t\n\r]+ -> skip ;

// This does not handle nested block comments.
BLOCKCOMMENT: '/*' .*? '*/' -> skip ;

COMMENT : '//' ~[\n\r]* -> skip ;
