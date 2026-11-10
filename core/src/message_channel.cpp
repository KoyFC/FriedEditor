#include "fried/editor/message_channel.h"

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
        std::vector<unsigned char> chunk(s_readSize);
        for (;;)
        {
            long count = m_transport->receive(chunk.data(), chunk.size());
            if (count < 0)
            {
                m_isOpen = false;
                break;
            }
            if (count == 0)
            {
                break;
            }
            m_received.insert(m_received.end(), chunk.begin(), chunk.begin() + count);
        }

        std::vector<Message> messages;
        std::size_t position = 0;
        while (m_received.size() - position >= s_lengthSize)
        {
            std::size_t length = readLength(m_received.data() + position);
            std::size_t headerEnd = position + s_lengthSize + length;
            if (m_received.size() < headerEnd)
            {
                break;
            }

            Message message;
            message.m_header = nlohmann::json::parse(m_received.begin() + position + s_lengthSize, m_received.begin() + headerEnd, nullptr, false);
            std::size_t pixelCount = message.m_header.is_object() && message.m_header.value("pixels", 0) > 0 ? message.m_header.value("pixels", 0) : 0;
            if (m_received.size() < headerEnd + pixelCount)
            {
                break;
            }
            message.m_pixels.assign(m_received.begin() + headerEnd, m_received.begin() + headerEnd + pixelCount);
            position = headerEnd + pixelCount;
            if (!message.m_header.is_discarded())
            {
                messages.push_back(std::move(message));
            }
        }
        m_received.erase(m_received.begin(), m_received.begin() + position);
        return messages;
    }
}
