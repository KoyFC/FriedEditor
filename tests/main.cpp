#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "environment.h"
#include "doctest/doctest.h"

#include <cstdlib>
#include <filesystem>

// The tests must not read or change what a real editor remembers.
static int s_noState = setEnvironment("FRIED_EDITOR_CONFIG", "none");
// Nor fill the user's trash: deleted assets go to one of the tests' own.
static int s_ownTrash = setEnvironment("FRIED_EDITOR_TRASH", (std::filesystem::temp_directory_path() / "fried-editor-tests-trash").string().c_str());
