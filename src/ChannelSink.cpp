#include "ChannelSink.h"
#include "rpc/Exceptions.h"
#include "log/log.h"

#include "Stream.h"

#include <atomic>

#include <boost/make_shared.hpp>
#include <boost/bind.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/thread.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace rpc
{
namespace details
{

namespace
{

SET_LOGGING_MODULE("Rpc");

class ChannelSink : public IChannelSink, public boost::enable_shared_from_this<ChannelSink>
{
    struct FutureResponse
    {
        FutureResponse()  {}
        IFuture::Ptr m_Future;
    };

    typedef std::map<boost::uint32_t, FutureResponse> PacketMap;
    typedef std::deque<details::IRequestHandler::Ptr> Handlers;

public:
    ChannelSink(boost::asio::io_service& svc, const boost::weak_ptr<rpc::details::IChannel>& channel)
        : m_Service(svc)
        , m_Channel(channel)
    {
    }

    ~ChannelSink()
    {
    }

    virtual IFuture::Ptr Push(const proto::BasePacket& base, const gp::Message* request, const IStream& stream) override
    {
        FutureResponse packet;
        if (base.packetid() && base.direction() == proto::BasePacket::Request)
        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            packet.m_Future = IFuture::Instance(m_Service);
            if (!m_OutgoingRequests.insert(std::make_pair(base.packetid(), packet)).second)
                BOOST_THROW_EXCEPTION(Exception("Duplicated packet id: %s", base.ShortDebugString()));
        }

        Write(base, request, stream);
        return packet.m_Future;
    }

    virtual void Pop(const proto::BasePacket& base, const IStream& stream) override
    {
        boost::for_each(m_Handlers, boost::bind(&details::IRequestHandler::HandleResponse, _1, boost::ref(base), GetRemoteId()));

        FutureResponse future;

        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            const auto it = m_OutgoingRequests.find(base.packetid());
            if (it == m_OutgoingRequests.end())
            {
                LOG_ERROR("<-[%s] Unknown packet id: %s", GetRemoteId(), base.DebugString());
                return;
            }

            future = it->second;
            m_OutgoingRequests.erase(it);
        }

        future.m_Future->SetBase(base);

        if (!base.error().empty() || base.errorid())
            future.m_Future->SetException(MakeException(base));
        else
            future.m_Future->SetData(stream);
    }

    virtual void SetConnection(const net::IConnection::Ptr& connection) override
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        const auto previous = m_Connection;
        m_Connection = connection;
        if (connection)
        {
            m_Exception = boost::exception_ptr();

            if (previous && previous != connection)
            {
                LOG_WARNING("--[%s] Closing previous connection", GetRemoteId());
                previous->Receive([](const net::IConnection::StreamPtr& s){}); // ignore everything from the old connection
                previous->Close();
            }
        }
    }

    void Write(const proto::BasePacket& base, const gp::Message* request, const IStream& stream)
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        const auto connection = m_Connection;
        if (m_Exception || !connection)
        {
            LOG_WARNING("->[%s] Channel has been closed, exception: %s, connection: %s", GetRemoteId(), (m_Exception ? "yes" : "no"), (connection ? "yes" : "no"));
            return;
        }

        const auto wrapped = m_WrapConnection ? m_WrapConnection(connection) : connection;
        lock.unlock();

        LOG_TRACE("->[%s] Writing packet: %s", GetRemoteId(), base.ShortDebugString());

        details::WriteStream writer(wrapped);
        writer.Write(base, request, stream);
    }

    virtual void Close(const boost::exception_ptr& e) override
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        if (m_Connection)
            m_Connection->Close();
        m_Connection.reset();
        if (!m_Exception)
            m_Exception = e;

        if (!m_OutgoingRequests.empty())
        {
            PacketMap responses;
            responses.swap(m_OutgoingRequests);
            lock.unlock();

            const auto exception = e ? e : rpc::MakeException("Channel closed by local side");
            boost::for_each(responses, [&exception](const PacketMap::value_type& pair){
                try
                {
                    pair.second.m_Future->SetException(exception);
                }
                catch (const std::exception&)
                {
                }
            });
        }
    }

    virtual void SetConnectionWrapper(const WrapConnectionFn& wrapper) override
    {
        m_WrapConnection = wrapper;
    }

    virtual void AddHandler(const details::IRequestHandler::Ptr& handler) override
    {
        m_Handlers.emplace_back(handler);
    }

    std::string GetRemoteId() const
    {
        if (const auto lock = m_Channel.lock())
            return lock->GetRemoteId();
        return std::string("destroyed channel");
    }

private:
    const boost::weak_ptr<rpc::details::IChannel> m_Channel;
    boost::asio::io_service& m_Service;
    WrapConnectionFn m_WrapConnection;
    Handlers m_Handlers;

    mutable boost::recursive_mutex m_Mutex;
    PacketMap m_OutgoingRequests;

    net::IConnection::Ptr m_Connection;
    boost::exception_ptr m_Exception;
};

} // anonymous namespace


IChannelSink::Ptr IChannelSink::Instance(boost::asio::io_service& svc, const boost::weak_ptr<rpc::details::IChannel>& channel)
{
    return boost::make_shared<ChannelSink>(svc, channel);
}

} // namespace details
} // namespace rpc
