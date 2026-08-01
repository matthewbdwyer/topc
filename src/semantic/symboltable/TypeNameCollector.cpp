#include "TypeNameCollector.h"
#include "SemanticError.h"
#include "../SemanticLogging.h"

void TypeNameCollector::endVisit(ASTSumTypeDecl *element) {
  const std::string &typeName = element->getName();
  // type name uniqueness was already checked by CheckSumTypeNames; just record.
  typeMap[typeName] = element;

  for (auto sv : element->getVariants()) {
    const std::string &tag = sv->getTag();
    // constructor uniqueness within this type (across types: checked by weeding)
    constructorMap[tag] = sv;
    SEMANTIC_LOG(2, "symbol-table")
      << "add constructor=" << tag << " type=" << typeName;
  }
}

std::pair<std::map<std::string, ASTSumTypeDecl *>,
          std::map<std::string, ASTSumVariant *>>
TypeNameCollector::build(ASTProgram *p) {
  SEMANTIC_LOG(2, "symbol-table") << "build type-name table";
  TypeNameCollector visitor;
  p->accept(&visitor);
  return {visitor.typeMap, visitor.constructorMap};
}
