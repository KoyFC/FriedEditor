#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <cstdlib>

// The tests must not read or change what a real editor remembers.
static int s_noState = setenv("FRIED_EDITOR_CONFIG", "none", 1);
