#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace fried::editor
{
    // One end of a byte stream to the player. Reading never blocks.
    class Transport
    {
    public:
        virtual ~Transport() = default;

        // Returns the bytes read, 0 when nothing has arrived, and -1 once the other end has gone.
        virtual long receive(void *buffer, std::size_t capacity) = 0;
        virtual bool send(const void *data, std::size_t size) = 0;
        virtual void close() = 0;
    };

    // Waits on loopback for the player to connect.
    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual int port() const = 0;

        // The player's end once it has connected, and null until then.
        virtual std::unique_ptr<Transport> accept() = 0;
    };

    class Subprocess
    {
    public:
        virtual ~Subprocess() = default;

        virtual bool isRunning() = 0;
        virtual int exitCode() const = 0;
        virtual void terminate() = 0;
    };

    // Opens a listener on a port the system picks, on the loopback interface only.
    std::unique_ptr<Listener> listenOnLoopback(std::string *error);

    // Connects to a loopback port, which is what the player does from the other side.
    std::unique_ptr<Transport> connectToLoopback(int port, std::string *error);

    // Runs a program with the given variables added to the environment.
    std::unique_ptr<Subprocess> startSubprocess(const std::vector<std::string> &arguments, const std::map<std::string, std::string> &environment,
                                                std::string *error);
}
