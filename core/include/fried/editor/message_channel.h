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
        std::vector<unsigned char> m_received;
        bool m_isOpen;
    };
}
