#pragma once

#include "Type.h"

/*! \brief Base class for TOP type visitors.
 *
 * The type visitor class abstracts the traversal of an type.  It works
 * in concert with the accept methods of TopType subtypes.  Each of those
 * nodes performs a traversal of its children in order.  This class defines
 * default behavior for the processing performed when the traversal reaches
 * a node of a given type.
 *
 * By default the visit method returns true, indicating that the children of
 * the node should also be visited, and the endVisit method does nothing.
 *
 * Subtypes of TopTypeVisitor will selectively override these default methods.
 * Overriding a visit permits a pre-order processing during traversal and
 * overriding endVisit permits a post-order processing.
 *
 * Subtype of TopTypeVisitor will also define member data that can be
 * referenced by overridden methods to communicate information along
 * the traversal to future method invocations.
 */
class TopTypeVisitor {
public:
  virtual bool visit(TopAlpha *element) { return true; }
  virtual void endVisit(TopAlpha *element) {}
  virtual bool visit(TopAbsentField *element) { return true; }
  virtual void endVisit(TopAbsentField *element) {}
  virtual bool visit(TopFunction *element) { return true; }
  virtual void endVisit(TopFunction *element) {}
  virtual bool visit(TopInt *element) { return true; }
  virtual void endVisit(TopInt *element) {}
  virtual bool visit(TopMu *element) { return true; }
  virtual void endVisit(TopMu *element) {}
  virtual bool visit(TopRecord *element) { return true; }
  virtual void endVisit(TopRecord *element) {}
  virtual bool visit(TopRef *element) { return true; }
  virtual void endVisit(TopRef *element) {}
  virtual bool visit(TopOwningRef *element) { return true; }
  virtual void endVisit(TopOwningRef *element) {}
  virtual bool visit(TopBorrowRef *element) { return true; }
  virtual void endVisit(TopBorrowRef *element) {}
  virtual bool visit(TopSumType *element) { return true; }
  virtual void endVisit(TopSumType *element) {}
  virtual bool visit(TopVar *element) { return true; }
  virtual void endVisit(TopVar *element) {}
};
