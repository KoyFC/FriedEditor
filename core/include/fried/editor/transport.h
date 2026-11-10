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

    struct SubprocessOptions
    {
        std::vector<std::string> m_arguments;

        // Added to the environment of the editor, which the program inherits.
        std::map<std::string, std::string> m_environment;
        std::string m_workDirectory;

        // Receives what the program prints, standard output and error together. Empty discards it.
        std::string m_logFile;
    };

    std::unique_ptr<Subprocess> startSubprocess(const SubprocessOptions &options, std::string *error);
}
