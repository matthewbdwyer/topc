#include "FrontEnd.h"

#include "ASTBuilder.h"
#include "ASTVisualizer.h"
#include "ParseError.h"
#include "PrettyPrinter.h"
#include "TOPLexer.h"
#include "TOPParser.h"

#include "loguru.hpp"

using namespace std;
using namespace antlr4;

void LexerErrorListener::syntaxError(Recognizer *recognizer,
                                     Token *offendingSymbol, size_t line,
                                     size_t charPositionInLine,
                                     const std::string &msg,
                                     std::exception_ptr e) {
  throw ParseError(msg + "@" + std::to_string(line) + ":" +
                   std::to_string(charPositionInLine));
}

void ParserErrorListener::syntaxError(Recognizer *recognizer,
                                      Token *offendingSymbol, size_t line,
                                      size_t charPositionInLine,
                                      const std::string &msg,
                                      std::exception_ptr e) {
  throw ParseError(msg + "@" + std::to_string(line) + ":" +
                   std::to_string(charPositionInLine));
}

std::shared_ptr<ASTProgram> FrontEnd::parse(std::istream &stream) {
  ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);
  LexerErrorListener lexerErrorListener;
  ParserErrorListener parserErrorListener;

  // Add error listeners
  lexer.removeErrorListeners();
  lexer.addErrorListener(&lexerErrorListener);

  parser.removeParseListeners();
  parser.removeErrorListeners();
  parser.addErrorListener(&parserErrorListener);

  LOG_S(1) << "Parsing program";

  TOPParser::ProgramContext *tree = parser.program();

  LOG_S(1) << "Building AST";

  ASTBuilder ab(&parser);
  return ab.build(tree);
}

void FrontEnd::prettyprint(ASTProgram *program, std::ostream &os) {
  PrettyPrinter::print(program, os, ' ', 2);
}

void FrontEnd::astVisualize(std::shared_ptr<ASTNode> node, std::ostream &os,
                            const std::string &format) {
  ASTVisualizer visualizer(os);
  SyntaxTree syntaxTree(node);
  if (format == "ascii") {
    visualizer.buildAscii(syntaxTree);
  } else {
    visualizer.buildGraph(syntaxTree);
  }
}
