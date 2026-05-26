#pragma once

#include <TopType.h>
#include <memory>

/*!
 * \class ConstraintHandler
 *
 * \brief Abstract class for handling type constraints as they are generated.
 */
class ConstraintHandler {
public:
  virtual ~ConstraintHandler() = default;
  virtual void handle(std::shared_ptr<TopType> t1,
                      std::shared_ptr<TopType> t2) = 0;
};