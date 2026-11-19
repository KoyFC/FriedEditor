#pragma once

#include "fried/core/toolchains.h"

// Machines described by hand, so what a build runs does not depend on what the one running the tests has installed.
inline fried::core::Toolchains linuxMachine()
{
    fried::core::Toolchains toolchains;
    toolchains.m_host = fried::core::HostSystem::Linux;
    toolchains.m_haxe = "haxe";
    toolchains.m_cmake = "cmake";
    return toolchains;
}

inline fried::core::Toolchains windowsMachineWithVisualStudio()
{
    fried::core::Toolchains toolchains = linuxMachine();
    toolchains.m_host = fried::core::HostSystem::Windows;
    toolchains.m_visualStudioCmake = "C:\\VS\\cmake.exe";
    return toolchains;
}
