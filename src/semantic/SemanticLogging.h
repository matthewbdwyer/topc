#pragma once

#include "loguru.hpp"

#define SEMANTIC_LOG(level, phase)                                             \
  LOG_S(level) << "[semantic][" << phase << "] "
