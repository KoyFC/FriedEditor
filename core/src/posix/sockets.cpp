#include "fried/editor/transport.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace fried::editor
{
    namespace
    {
        bool makeNonBlocking(int descriptor)
        {
            int flags = fcntl(descriptor, F_GETFL, 0);
            return flags >= 0 && fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
        }

        class SocketTransport : public Transport
        {
        public:
            explicit SocketTransport(int descriptor)
                : m_descriptor(descriptor)
            {
            }

            ~SocketTransport() override { close(); }

            long receive(void *buffer, std::size_t capacity) override
            {
                if (m_descriptor < 0)
                {
                    return -1;
                }
                ssize_t count = recv(m_descriptor, buffer, capacity, MSG_DONTWAIT);
                if (count > 0)
                {
                    return (long)count;
                }
                if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
                {
                    return 0;
                }
                return -1;
            }

            bool send(const void *data, std::size_t size) override
            {
                const char *bytes = static_cast<const char *>(data);
                while (size > 0 && m_descriptor >= 0)
                {
                    ssize_t count = ::send(m_descriptor, bytes, size, MSG_NOSIGNAL);
                    if (count > 0)
                    {
                        bytes += count;
                        size -= (std::size_t)count;
                    }
                    else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        pollfd waiting = {m_descriptor, POLLOUT, 0};
                        ::poll(&waiting, 1, 1000);
                    }
                    else if (count < 0 && errno != EINTR)
                    {
                        return false;
                    }
                }
                return size == 0;
            }

            void close() override
            {
                if (m_descriptor >= 0)
                {
                    ::close(m_descriptor);
                    m_descriptor = -1;
                }
            }

        private:
            int m_descriptor;
        };

        class SocketListener : public Listener
        {
        public:
            SocketListener(int descriptor, int port)
                : m_descriptor(descriptor)
                , m_port(port)
            {
            }

            ~SocketListener() override { ::close(m_descriptor); }

            int port() const override { return m_port; }

            std::unique_ptr<Transport> accept() override
            {
                int accepted = ::accept(m_descriptor, nullptr, nullptr);
                if (accepted < 0 || !makeNonBlocking(accepted))
                {
                    if (accepted >= 0)
                    {
                        ::close(accepted);
                    }
                    return nullptr;
                }
                return std::make_unique<SocketTransport>(accepted);
            }

        private:
            int m_descriptor;
            int m_port;
        };

        sockaddr_in loopbackAddress(int port)
        {
            sockaddr_in address = {};
            address.sin_family = AF_INET;
            address.sin_port = htons((uint16_t)port);
            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
            return address;
        }
    }

    std::unique_ptr<Listener> listenOnLoopback(std::string *error)
    {
        int descriptor = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address = loopbackAddress(0);
        socklen_t size = sizeof(address);
        if (descriptor < 0 || bind(descriptor, (sockaddr *)&address, size) != 0 || listen(descriptor, 1) != 0
            || getsockname(descriptor, (sockaddr *)&address, &size) != 0 || !makeNonBlocking(descriptor))
        {
            *error = std::string("Cannot listen on the loopback interface: ") + std::strerror(errno);
            if (descriptor >= 0)
            {
                ::close(descriptor);
            }
            return nullptr;
        }
        return std::make_unique<SocketListener>(descriptor, ntohs(address.sin_port));
    }

    std::unique_ptr<Transport> connectToLoopback(int port, std::string *error)
    {
        int descriptor = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address = loopbackAddress(port);
        if (descriptor < 0 || connect(descriptor, (sockaddr *)&address, sizeof(address)) != 0 || !makeNonBlocking(descriptor))
        {
            *error = std::string("Cannot connect to port ") + std::to_string(port) + ": " + std::strerror(errno);
            if (descriptor >= 0)
            {
                ::close(descriptor);
            }
            return nullptr;
        }
        return std::make_unique<SocketTransport>(descriptor);
    }
}
