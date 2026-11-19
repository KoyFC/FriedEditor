#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "environment.h"
#include "doctest/doctest.h"

#include <cstdlib>

// The tests must not read or change what a real editor remembers.
static int s_noState = setEnvironment("FRIED_EDITOR_CONFIG", "none");
