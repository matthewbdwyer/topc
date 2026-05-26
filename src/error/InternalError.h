#pragma once

#include "Error.h"

/*! \brief Class for topc internal errors.
 *
 * Errors that are not related to the expected processing of a program.
 * \sa Error
 */
class InternalError : public Error {
public:
  InternalError(const std::string &msg)
      : Error(msg) {} //! Construct internal error with message
};
