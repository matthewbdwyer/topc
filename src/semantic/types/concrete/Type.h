#pragma once

/*
 * This include is for convenience when defining algorithms spanning
 * the TOP type hierarchy, e.g., visitors.
 *
 * It should be used sparingly as it introduces coupling to the entire
 * TOP type hierarchy.
 */

#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopOwningRef.h"
#include "TopRef.h"
#include "TopSumType.h"
#include "TopType.h"
#include "TopVar.h"
