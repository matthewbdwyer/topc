#include "DestructionPass.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTBlockStmt.h"
#include "ASTCaseArm.h"
#include "ASTDestroyStmt.h"
#include "ASTFunction.h"

#include <memory>
#include <vector>

void DestructionPass::run(ASTProgram *ast,
                          const MoveAnalysis::DestructionPlan &plan) {
  SEMANTIC_LOG(1, "destruction") << "start";

  if (RuleToggles::enabled("destroy-at-exit")) {
    for (const auto &[function, owners] : plan.atExit) {
      for (auto *decl : owners) {
        SEMANTIC_LOG(2, "destruction")
            << "function=" << function->getName() << " insert variable="
            << decl->getName();
        function->insertBeforeReturn(std::make_shared<ASTDestroyStmt>(decl));
      }
    }
  }

  if (RuleToggles::enabled("destroy-arm-binders")) {
    for (const auto &[arm, binders] : plan.atArmEnd) {
      // The binders are in scope only inside the arm, so their destroys go at
      // the end of the arm's body: wrap the body in a block that ends with them.
      std::shared_ptr<ASTStmt> body;
      for (auto &child : arm->getChildren()) {
        if (child.get() == arm->getBody()) {
          body = std::dynamic_pointer_cast<ASTStmt>(child);
        }
      }
      std::vector<std::shared_ptr<ASTStmt>> stmts{body};
      for (auto *binder : binders) {
        SEMANTIC_LOG(2, "destruction")
            << "arm line=" << arm->getLine()
            << " insert arm-binder=" << binder->getName();
        stmts.push_back(std::make_shared<ASTDestroyStmt>(binder));
      }
      arm->replaceChild(arm->getBody(),
                        std::make_shared<ASTBlockStmt>(std::move(stmts)));
    }
  }

  SEMANTIC_LOG(1, "destruction") << "complete";
}
