#pragma once

#include <string>
#include <vector>

// A program that ends with an exit code, in the shell the system has.
inline std::vector<std::string> exitingWith(const std::string &code)
{
#ifdef _WIN32
    return {"cmd.exe", "/d", "/c", "exit " + code};
#else
    return {"sh", "-c", "exit " + code};
#endif
}

// The same, with the code read from an environment variable.
inline std::vector<std::string> exitingWithVariable(const std::string &name)
{
#ifdef _WIN32
    return exitingWith("%" + name + "%");
#else
    return exitingWith("$" + name);
#endif
}
