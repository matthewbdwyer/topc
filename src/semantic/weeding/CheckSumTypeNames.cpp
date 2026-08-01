#include "CheckSumTypeNames.h"
#include "../SemanticLogging.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <map>
#include <set>
#include <sstream>

#include "loguru.hpp"

void CheckSumTypeNames::endVisit(ASTProgram *element) {
  std::set<std::string> typeNames;
  std::map<std::string, std::string> ctorToType; // ctor tag -> type name

  for (auto td : element->getTypedecls()) {
    const std::string &tname = td->getName();

    // Rule 1: no duplicate type names
    if (typeNames.count(tname)) {
      std::ostringstream oss;
      oss << "Type name error on line " << td->getLine()
          << ": duplicate sum type name '" << tname << "'\n";
      throw SemanticError(oss.str());
    }
    typeNames.insert(tname);

    // Rule 2: no duplicate constructor tags (global uniqueness)
    for (auto sv : td->getVariants()) {
      const std::string &tag = sv->getTag();
      if (ctorToType.count(tag)) {
        std::ostringstream oss;
        oss << "Constructor name error on line " << sv->getLine()
            << ": constructor '" << tag
            << "' is already declared in type '" << ctorToType.at(tag) << "'\n";
        throw SemanticError(oss.str());
      }
      ctorToType[tag] = tname;
    }
  }
}

void CheckSumTypeNames::check(ASTProgram *p) {
  SEMANTIC_LOG(1, "sum-type-names") << "start";
  CheckSumTypeNames visitor;
  p->accept(&visitor);
  SEMANTIC_LOG(1, "sum-type-names") << "complete";
}
