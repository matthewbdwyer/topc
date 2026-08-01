#include "TypeHelper.h"

#include "TopAlpha.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopOwningRef.h"


std::shared_ptr<TopType> TypeHelper::intType() {
   return std::make_shared<TopInt>();
}

std::shared_ptr<TopType> TypeHelper::alphaType(ASTNode *node) {
    return std::make_shared<TopAlpha>(node);
}

std::shared_ptr<TopType> TypeHelper::ptrType(std::shared_ptr<TopType> t) {
    return std::make_shared<TopOwningRef>(t);
}

std::shared_ptr<TopType> TypeHelper::funType(std::vector<std::shared_ptr<TopType>> p, std::shared_ptr<TopType> r) {
    return std::make_shared<TopFunction>(p,r);
}
