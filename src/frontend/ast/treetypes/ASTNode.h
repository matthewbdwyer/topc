#pragma once

#include <cassert>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

// Forward declare the visitor to resolve circular dependency
class ASTVisitor;

/*! \brief Abstract base class for all AST nodes.
 *
 * ASTNodes define the elements of the program in a tree structured form.
 * A key concept to undestand is that of "ownership".   Every node in the
 * tree is "owned" by its parent.  The parent maintains a "shared pointer"
 * to each of its children.  This greatly simplifies memory management as
 * the tree can be reclaimed once its root is no longer needed.  It is
 * important to understand that the "shared pointer" is only used to manage
 * the lifetime of the child nodes.  The values of those pointers can be
 * passed around freely to other parts of the compiler that want to view
 * the ASTNodes.
 *
 * Each node has information about the line and column in the source
 * program on which the element begins.  Subtypes define "<<" operator
 * that print a custom representation of the ASTNode, by overriding
 * the print method.
 *
 * There are two virtual methods that are used in a generic visitor
 * and for code generation pass.
 */
class ASTNode {
  int line = 0;
  int column = 0;

public:
  virtual ~ASTNode() = default;

  /*! \fn accept
   *  \brief Visit the children of this node and apply the visitor.
   *
   * This virtual function is overridden by each ASTNode subtype
   * in order to visit each of its child nodes, by calling their accept
   * methods.  The visitor parameter defines the operations that are
   * applied to each node that is visited; a subtype of ASTVisitor defines
   * those operations.
   *
   * \param visitor The subtype of ASTVisitor that carries out per-ASTNode work.
   */
  virtual void accept(ASTVisitor *visitor) = 0;

  /*! \fn getChildren
   *  \brief Return all of the children for the node.
   *
   * This is not a pure virtual function so subclasses can selectively override
   * it.
   *
   * \return a collection of the nodes children.
   */
  virtual std::vector<std::shared_ptr<ASTNode>> getChildren() { return {}; }

  /*! \fn replaceChild
   *  \brief Replace a direct child of this node, identified by raw pointer.
   *
   * Used by AST-lowering passes to swap a child in place. Overridden by any node
   * that holds an expression or statement child. The default does nothing and
   * returns false so nodes without replaceable children need no override.
   *
   * \param oldChild raw pointer to the current child to replace.
   * \param newChild the replacement node (must be assignable to the slot's type).
   * \return true if a child was replaced.
   */
  virtual bool replaceChild(ASTNode *oldChild,
                            std::shared_ptr<ASTNode> newChild) {
    (void)oldChild;
    (void)newChild;
    return false;
  }

  void setLocation(int l, int c) {
    line = l;
    column = c;
  }
  int getLine() { return line; }
  int getColumn() { return column; }

  friend std::ostream &operator<<(std::ostream &os, const ASTNode &obj) {
    return obj.print(os);
  }

protected:
  virtual std::ostream &print(std::ostream &out) const = 0;
};

/*! \brief Replace a single typed child slot if it holds \p oldChild.
 *
 * Helper for ASTNode::replaceChild overrides. \p T is the slot's element type
 * (e.g. ASTExpr or ASTStmt); the incoming replacement is retyped to match. The
 * replacement is slot-compatible by construction in the lowering passes, so a
 * failed cast is a bug -- assert it rather than silently null the slot.
 */
template <class T>
inline bool astReplaceSlot(std::shared_ptr<T> &slot, ASTNode *oldChild,
                           const std::shared_ptr<ASTNode> &newChild) {
  if (slot.get() != oldChild)
    return false;
  auto retyped = std::dynamic_pointer_cast<T>(newChild);
  assert(retyped && "replaceChild: replacement has incompatible node type");
  slot = std::move(retyped);
  return true;
}

/*! \brief Replace a child within a vector of typed slots if one holds \p oldChild. */
template <class T>
inline bool astReplaceVec(std::vector<std::shared_ptr<T>> &vec,
                          ASTNode *oldChild,
                          const std::shared_ptr<ASTNode> &newChild) {
  for (auto &slot : vec)
    if (slot.get() == oldChild) {
      auto retyped = std::dynamic_pointer_cast<T>(newChild);
      assert(retyped && "replaceChild: replacement has incompatible node type");
      slot = std::move(retyped);
      return true;
    }
  return false;
}
