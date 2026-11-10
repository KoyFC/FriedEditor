#include "fried/editor/transport.h"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace fried::editor
{
    namespace
    {
        class PosixSubprocess : public Subprocess
        {
        public:
            explicit PosixSubprocess(pid_t process)
                : m_process(process)
                , m_exitCode(-1)
                , m_hasExited(false)
            {
            }

            ~PosixSubprocess() override
            {
                terminate();
                if (!m_hasExited)
                {
                    waitpid(m_process, nullptr, 0);
                }
            }

            bool isRunning() override
            {
                if (m_hasExited)
                {
                    return false;
                }
                int status = 0;
                pid_t result = waitpid(m_process, &status, WNOHANG);
                if (result == 0)
                {
                    return true;
                }
                m_hasExited = true;
                m_exitCode = result == m_process && WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                return false;
            }

            int exitCode() const override { return m_exitCode; }

            void terminate() override
            {
                if (isRunning())
                {
                    kill(m_process, SIGTERM);
                }
            }

        private:
            pid_t m_process;
            int m_exitCode;
            bool m_hasExited;
        };
    }

    std::unique_ptr<Subprocess> startSubprocess(const std::vector<std::string> &arguments, const std::map<std::string, std::string> &environment,
                                                std::string *error)
    {
        if (arguments.empty())
        {
            *error = "There is no program to run.";
            return nullptr;
        }

        std::vector<std::string> variables;
        for (char **entry = environ; *entry; ++entry)
        {
            std::string text = *entry;
            if (environment.find(text.substr(0, text.find('='))) == environment.end())
            {
                variables.push_back(text);
            }
        }
        for (const auto &[name, value] : environment)
        {
            variables.push_back(name + "=" + value);
        }

        std::vector<char *> argv;
        for (const std::string &argument : arguments)
        {
            argv.push_back(const_cast<char *>(argument.c_str()));
        }
        argv.push_back(nullptr);
        std::vector<char *> envp;
        for (const std::string &variable : variables)
        {
            envp.push_back(const_cast<char *>(variable.c_str()));
        }
        envp.push_back(nullptr);

        pid_t process = 0;
        int result = posix_spawnp(&process, arguments[0].c_str(), nullptr, nullptr, argv.data(), envp.data());
        if (result != 0)
        {
            *error = "Cannot run " + arguments[0] + ": " + std::strerror(result);
            return nullptr;
        }
        return std::make_unique<PosixSubprocess>(process);
    }
}
