#include "ParserHelper.h"
#include "ConsoleErrorListener.h"
#include "antlr4-runtime.h"
#include <TOPLexer.h>
#include <TOPParser.h>

// Handle parse errors
class ErrorListener : public antlr4::BaseErrorListener {
  std::shared_ptr<bool> error;

public:
  ErrorListener(std::shared_ptr<bool> e) : error(e) {}

  void syntaxError(antlr4::Recognizer *recognizer,
                   antlr4::Token *offendingSymbol, size_t line,
                   size_t charPositionInLine, const std::string &msg,
                   std::exception_ptr e) {
    *error = true;
  }
};

bool ParserHelper::is_parsable(std::istream &stream) {
  antlr4::ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);

  std::shared_ptr<bool> parseError = std::make_shared<bool>(false);
  ErrorListener errorListener(parseError);

  // Set error listeners
  lexer.addErrorListener(&errorListener);
  lexer.removeErrorListener(&antlr4::ConsoleErrorListener::INSTANCE);
  parser.addErrorListener(&errorListener);
  parser.removeErrorListener(&antlr4::ConsoleErrorListener::INSTANCE);

  TOPParser::ProgramContext *tree = parser.program();
  return !*parseError;
}

/* Assumes ParserHelper::is_parseable() is TRUE */
std::string ParserHelper::parsetree(std::istream &stream) {
  antlr4::ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);
  TOPParser::ProgramContext *tree = parser.program();
  // Return printed string
  return tree->toStringTree(&parser, false);
}
