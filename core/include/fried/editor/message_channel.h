#pragma once

#include "fried/editor/transport.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <vector>

namespace fried::editor
{
    struct Message
    {
        nlohmann::json m_header;
        std::vector<unsigned char> m_pixels;
    };

    // Messages are a 32-bit little-endian length and then JSON. A header with a "pixels" count is followed by that many raw bytes.
    class MessageChannel
    {
    public:
        explicit MessageChannel(std::unique_ptr<Transport> transport);

        bool send(const nlohmann::json &header);

        // Every message that has arrived in full since the last call.
        std::vector<Message> poll();

        bool isOpen() const { return m_isOpen; }

    private:
        std::unique_ptr<Transport> m_transport;
        // Bytes read but not yet made into messages. Frames are megabytes, so they are read straight into it, and it keeps its size between reads rather than being cleared to zeros.
        std::unique_ptr<unsigned char[]> m_received;
        std::size_t m_receivedSize = 0;
        std::size_t m_receivedCapacity = 0;
        bool m_isOpen;
    };
}
