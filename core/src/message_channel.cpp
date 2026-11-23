#include "fried/editor/message_channel.h"

#include <algorithm>
#include <cstdint>

namespace fried::editor
{
    namespace
    {
        constexpr std::size_t s_lengthSize = 4;
        constexpr std::size_t s_readSize = 65536;

        std::uint32_t readLength(const unsigned char *bytes)
        {
            return (std::uint32_t)bytes[0] | ((std::uint32_t)bytes[1] << 8) | ((std::uint32_t)bytes[2] << 16) | ((std::uint32_t)bytes[3] << 24);
        }
    }

    MessageChannel::MessageChannel(std::unique_ptr<Transport> transport)
        : m_transport(std::move(transport))
        , m_isOpen(true)
    {
    }

    bool MessageChannel::send(const nlohmann::json &header)
    {
        if (!m_isOpen)
        {
            return false;
        }
        std::string text = header.dump();
        std::vector<unsigned char> framed(s_lengthSize + text.size());
        std::uint32_t length = (std::uint32_t)text.size();
        for (std::size_t index = 0; index < s_lengthSize; ++index)
        {
            framed[index] = (unsigned char)(length >> (8 * index));
        }
        std::copy(text.begin(), text.end(), framed.begin() + s_lengthSize);

        if (!m_transport->send(framed.data(), framed.size()))
        {
            m_isOpen = false;
        }
        return m_isOpen;
    }

    std::vector<Message> MessageChannel::poll()
    {
        for (;;)
        {
            if (m_receivedCapacity - m_receivedSize < s_readSize)
            {
                std::size_t capacity = std::max(m_receivedCapacity * 2, m_receivedSize + s_readSize);
                std::unique_ptr<unsigned char[]> grown(new unsigned char[capacity]);
                std::copy(m_received.get(), m_received.get() + m_receivedSize, grown.get());
                m_received = std::move(grown);
                m_receivedCapacity = capacity;
            }
            long count = m_transport->receive(m_received.get() + m_receivedSize, m_receivedCapacity - m_receivedSize);
            if (count < 0)
            {
                m_isOpen = false;
                break;
            }
            if (count == 0)
            {
                break;
            }
            m_receivedSize += (std::size_t)count;
        }

        std::vector<Message> messages;
        const unsigned char *bytes = m_received.get();
        std::size_t position = 0;
        while (m_receivedSize - position >= s_lengthSize)
        {
            std::size_t length = readLength(bytes + position);
            std::size_t headerEnd = position + s_lengthSize + length;
            if (m_receivedSize < headerEnd)
            {
                break;
            }

            Message message;
            message.m_header = nlohmann::json::parse(bytes + position + s_lengthSize, bytes + headerEnd, nullptr, false);
            std::size_t pixelCount = message.m_header.is_object() && message.m_header.value("pixels", 0) > 0 ? message.m_header.value("pixels", 0) : 0;
            if (m_receivedSize < headerEnd + pixelCount)
            {
                break;
            }
            message.m_pixels.assign(bytes + headerEnd, bytes + headerEnd + pixelCount);
            position = headerEnd + pixelCount;
            if (!message.m_header.is_discarded())
            {
                messages.push_back(std::move(message));
            }
        }

        // What is left is the start of a message still on its way, moved to the front for the next read to finish.
        std::copy(m_received.get() + position, m_received.get() + m_receivedSize, m_received.get());
        m_receivedSize -= position;
        return messages;
    }
}
