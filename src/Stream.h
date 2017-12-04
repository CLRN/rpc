#pragma once

#include "net/connection.hpp"
#include "net/details/memory.hpp"
#include "net/details/stream.hpp"
#include "rpc/Exceptions.h"

#include "rpc/Base.h"

#include <google/protobuf/message.h>

#include <boost/cstdint.hpp>
#include <boost/iostreams/stream.hpp>


namespace rpc
{
namespace details
{

class ReadStream
{
public:
    static boost::uint32_t Read(std::istream& s, gp::Message& message)
    {
        boost::uint32_t size = 0;
        s.read(reinterpret_cast<char*>(&size), sizeof(boost::uint32_t));
        if (!size)
            return size;

        if (size <= 4096)
        {
            char buffer[4096];
            s.read(buffer, size);
            if (!message.ParseFromArray(buffer, size))
                BOOST_THROW_EXCEPTION(Exception("Failed to parse incoming packet"));
        }
        else
        {
            std::unique_ptr<char[]> buffer(new char[size]);
            s.read(buffer.get(), size);
            if (!message.ParseFromArray(buffer.get(), size))
                BOOST_THROW_EXCEPTION(Exception("Failed to parse incoming packet"));
        }
        return size;
    }
};

class WriteStream
{
public:

    enum { MAX_IN_MEMORY_STREAM_SIZE = 1024 * 100 };
    WriteStream(const net::IConnection::Ptr& stream)
        : m_NextLayer(stream)
    {
    }

    void Write(const gp::Message& base, const gp::Message* request = nullptr, const rpc::IStream& stream = rpc::IStream())
    {
        if (!m_NextLayer)
            return;

        assert(base.IsInitialized());

        const uint32_t baseSize = base.ByteSize();
        const uint32_t requestSize = request ? request->ByteSize() : 0;
        const uint64_t streamSize = stream ? net::StreamSize(*stream) : 0;
        uint64_t totalSize = baseSize + requestSize + sizeof(baseSize) + (request ? sizeof(requestSize) : 0);

        if (streamSize + totalSize < MAX_IN_MEMORY_STREAM_SIZE)
        {
            totalSize += streamSize;

            char buffer[MAX_IN_MEMORY_STREAM_SIZE];

            *reinterpret_cast<boost::uint32_t*>(buffer) = baseSize;
            if (!base.SerializeToArray(buffer + sizeof(baseSize), baseSize))
                BOOST_THROW_EXCEPTION(Exception("Failed to serialize base packet"));

            if (request)
            {
                *reinterpret_cast<boost::uint32_t*>(buffer + baseSize + sizeof(baseSize)) = requestSize;
                if (!request->SerializeToArray(buffer + baseSize + sizeof(baseSize) + sizeof(requestSize), requestSize))
                    BOOST_THROW_EXCEPTION(Exception("Failed to serialize base packet"));
            }

            if (streamSize)
                stream->read(buffer + totalSize - streamSize, streamSize);

            m_NextLayer->Prepare(static_cast<std::size_t>(totalSize))->Write(buffer, static_cast<std::size_t>(totalSize));
        }
        else
        {
            {
                const auto data = m_NextLayer->Prepare(static_cast<std::size_t>(totalSize));
                boost::iostreams::stream<net::details::StreamWrapper> out(data);

                out.write(reinterpret_cast<const char*>(&baseSize), sizeof(baseSize));
                if (!base.SerializeToOstream(&out))
                    BOOST_THROW_EXCEPTION(Exception("Failed to serialize base packet"));

                if (request)
                {
                    out.write(reinterpret_cast<const char*>(&requestSize), sizeof(requestSize));
                    if (!request->SerializeToOstream(&out))
                        BOOST_THROW_EXCEPTION(Exception("Failed to serialize base packet"));
                }
            }

            if (stream)
                SendStream(stream, streamSize);
        }

    }

private:

    void SendStream(const rpc::IStream& stream, uint64_t size)
    {
        // write stream data if available
        uint64_t sent = 0;
        static const uint64_t bufferSize = MAX_IN_MEMORY_STREAM_SIZE;
        char buffer[bufferSize];

        for (;;)
        {
            const auto remaining = size - sent;
            const auto toSend = std::min(remaining, bufferSize);

            const auto read = static_cast<std::size_t>(stream->read(buffer, toSend).gcount());
            if (!read)
                break;

            m_NextLayer->Prepare(read)->Write(buffer, read);
            sent += read;
        }
    }

private:
    const net::IConnection::Ptr m_NextLayer;
};

} // namespace details
} // namespace rpc
