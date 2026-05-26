#pragma once

#include "memory"
#include <vector>

#include <TopType.h>
#include <AST.h>

class TypeHelper {
public:
    static std::shared_ptr<TopType> intType();
    static std::shared_ptr<TopType> alphaType(ASTNode *node);
    static std::shared_ptr<TopType> ptrType(std::shared_ptr<TopType> t);
    static std::shared_ptr<TopType> funType(std::vector<std::shared_ptr<TopType>> p, std::shared_ptr<TopType> r);
};
