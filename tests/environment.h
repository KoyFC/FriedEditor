#pragma once

#include <cstdlib>

// An environment variable set for the rest of the test run, which Windows spells differently.
inline int setEnvironment(const char *name, const char *value)
{
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}
