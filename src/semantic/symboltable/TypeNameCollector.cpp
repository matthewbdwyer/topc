#include "TypeNameCollector.h"
#include "SemanticError.h"

#include "loguru.hpp"

void TypeNameCollector::endVisit(ASTSumTypeDecl *element) {
  const std::string &typeName = element->getName();
  // type name uniqueness was already checked by CheckSumTypeNames; just record.
  typeMap[typeName] = element;

  for (auto sv : element->getVariants()) {
    const std::string &tag = sv->getTag();
    // constructor uniqueness within this type (across types: checked by weeding)
    constructorMap[tag] = sv;
    LOG_S(1) << "Collecting constructor " << tag << " of type " << typeName;
  }
}

std::pair<std::map<std::string, ASTSumTypeDecl *>,
          std::map<std::string, ASTSumVariant *>>
TypeNameCollector::build(ASTProgram *p) {
  LOG_S(1) << "Building type name table";
  TypeNameCollector visitor;
  p->accept(&visitor);
  return {visitor.typeMap, visitor.constructorMap};
}
