#include "fried/editor/transport.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

namespace fried::editor
{
    namespace
    {
        bool startWinsock()
        {
            static bool s_started = [] {
                WSADATA data;
                return WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
            return s_started;
        }

        std::string describe(int code)
        {
            char *buffer = nullptr;
            DWORD length = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                                          static_cast<DWORD>(code), 0, reinterpret_cast<char *>(&buffer), 0, nullptr);
            std::string message(buffer ? buffer : "", length);
            if (buffer)
            {
                LocalFree(buffer);
            }
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r' || message.back() == ' '))
            {
                message.pop_back();
            }
            return message;
        }

        // Not inheritable, so no program the editor starts, such as the player or a build tool, keeps the port bound after the editor ends.
        SOCKET openSocket()
        {
            return WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        }

        bool makeNonBlocking(SOCKET descriptor)
        {
            u_long enabled = 1;
            return ioctlsocket(descriptor, FIONBIO, &enabled) == 0;
        }

        class SocketTransport : public Transport
        {
        public:
            explicit SocketTransport(SOCKET descriptor)
                : m_descriptor(descriptor)
            {
            }

            ~SocketTransport() override { close(); }

            long receive(void *buffer, std::size_t capacity) override
            {
                if (m_descriptor == INVALID_SOCKET)
                {
                    return -1;
                }
                int count = recv(m_descriptor, static_cast<char *>(buffer), static_cast<int>(capacity), 0);
                if (count > 0)
                {
                    return count;
                }
                if (count == SOCKET_ERROR && (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAEINTR))
                {
                    return 0;
                }
                return -1;
            }

            bool send(const void *data, std::size_t size) override
            {
                const char *bytes = static_cast<const char *>(data);
                while (size > 0 && m_descriptor != INVALID_SOCKET)
                {
                    int count = ::send(m_descriptor, bytes, static_cast<int>(size), 0);
                    if (count > 0)
                    {
                        bytes += count;
                        size -= static_cast<std::size_t>(count);
                    }
                    else if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
                    {
                        WSAPOLLFD waiting = {m_descriptor, POLLWRNORM, 0};
                        WSAPoll(&waiting, 1, 1000);
                    }
                    else if (count == SOCKET_ERROR && WSAGetLastError() != WSAEINTR)
                    {
                        return false;
                    }
                }
                return size == 0;
            }

            void close() override
            {
                if (m_descriptor != INVALID_SOCKET)
                {
                    closesocket(m_descriptor);
                    m_descriptor = INVALID_SOCKET;
                }
            }

        private:
            SOCKET m_descriptor;
        };

        class SocketListener : public Listener
        {
        public:
            SocketListener(SOCKET descriptor, int port)
                : m_descriptor(descriptor)
                , m_port(port)
            {
            }

            ~SocketListener() override { closesocket(m_descriptor); }

            int port() const override { return m_port; }

            std::unique_ptr<Transport> accept() override
            {
                SOCKET accepted = ::accept(m_descriptor, nullptr, nullptr);
                if (accepted == INVALID_SOCKET || !makeNonBlocking(accepted))
                {
                    if (accepted != INVALID_SOCKET)
                    {
                        closesocket(accepted);
                    }
                    return nullptr;
                }
                // A small message is sent at once instead of waiting to be joined by the next, which would cost a delayed acknowledgement, about 40 ms.
                BOOL enabled = TRUE;
                setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&enabled), sizeof(enabled));
                return std::make_unique<SocketTransport>(accepted);
            }

        private:
            SOCKET m_descriptor;
            int m_port;
        };

        sockaddr_in loopbackAddress(int port)
        {
            sockaddr_in address = {};
            address.sin_family = AF_INET;
            address.sin_port = htons(static_cast<u_short>(port));
            inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
            return address;
        }
    }

    std::unique_ptr<Listener> listenOnLoopback(std::string *error)
    {
        if (!startWinsock())
        {
            *error = "Cannot listen on the loopback interface: Winsock did not start.";
            return nullptr;
        }

        SOCKET descriptor = openSocket();
        sockaddr_in address = loopbackAddress(0);
        int size = sizeof(address);
        if (descriptor == INVALID_SOCKET || bind(descriptor, reinterpret_cast<sockaddr *>(&address), size) != 0 || listen(descriptor, 1) != 0
            || getsockname(descriptor, reinterpret_cast<sockaddr *>(&address), &size) != 0 || !makeNonBlocking(descriptor))
        {
            *error = "Cannot listen on the loopback interface: " + describe(WSAGetLastError());
            if (descriptor != INVALID_SOCKET)
            {
                closesocket(descriptor);
            }
            return nullptr;
        }
        return std::make_unique<SocketListener>(descriptor, ntohs(address.sin_port));
    }

    std::unique_ptr<Transport> connectToLoopback(int port, std::string *error)
    {
        if (!startWinsock())
        {
            *error = "Cannot connect to port " + std::to_string(port) + ": Winsock did not start.";
            return nullptr;
        }

        SOCKET descriptor = openSocket();
        sockaddr_in address = loopbackAddress(port);
        if (descriptor == INVALID_SOCKET || connect(descriptor, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 || !makeNonBlocking(descriptor))
        {
            *error = "Cannot connect to port " + std::to_string(port) + ": " + describe(WSAGetLastError());
            if (descriptor != INVALID_SOCKET)
            {
                closesocket(descriptor);
            }
            return nullptr;
        }
        return std::make_unique<SocketTransport>(descriptor);
    }
}
