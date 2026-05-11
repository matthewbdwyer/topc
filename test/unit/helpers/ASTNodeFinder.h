#pragma once
#include "ASTVisitor.h"

template<class N>
class NodeFinder : public ASTVisitor {
public:
    static N *find_node(ASTProgram *p) {
        NodeFinder<N> visitor;
        p->accept(&visitor);
        return visitor.found_node;
    }

    void endVisit(N *element) override { found_node = element; }

private:
    N *found_node = nullptr;
};
