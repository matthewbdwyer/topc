#include "ASTHelper.h"
#include "ConsoleErrorListener.h"
#include "antlr4-runtime.h"
#include <TOPLexer.h>
#include <TOPParser.h>

std::shared_ptr<ASTProgram> ASTHelper::build_ast(std::stringstream &stream) {
    antlr4::ANTLRInputStream input(stream);
    TOPLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);
    TOPParser parser(&tokens);
    TOPParser::ProgramContext *tree = parser.program();
    ASTBuilder tb(&parser);
    return tb.build(tree);
}



