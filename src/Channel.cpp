#include "rpc/Channel.h"
#include "rpc/Exceptions.h"
#include "conversion/cast.hpp"
#include "Stream.h"
#include "net/sequence.hpp"
#include "log/log.h"
#include "ChannelSink.h"

#include "rpc_base.pb.h"

#include <vector>
#include <atomic>

#include <google/protobuf/descriptor.h>

#include <boost/make_shared.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/function.hpp>
#include <boost/asio/io_service.hpp>

namespace rpc
{

namespace
{

SET_LOGGING_MODULE("Rpc");

class SequencedChannel
    : virtual public rpc::ISequencedChannel
    , boost::noncopyable
    , public boost::enable_shared_from_this<SequencedChannel>
{
public:

    typedef std::deque<details::IRequestHandler::Ptr> Handlers;

    SequencedChannel(boost::asio::io_service& svc)
        : m_Service(svc)
        , m_PacketCounter()
    {
    }

    ~SequencedChannel()
    {
    }

    virtual void Init()
    {
        m_Sink = details::IChannelSink::Instance(m_Service, shared_from_this());
    }

protected:

    virtual void OnIncomingData(const IStream& stream, const boost::exception_ptr& e)
    {
        if (!stream)
            Close(e);
        else
            HandleBasePacket(stream);
    }

    IFuture::Ptr CallMethodImpl(const proto::BasePacket& base, const gp::Message* request, const IStream& stream)
    {
        LOG_DEBUG("->[%s]: Pushing packet: %s", m_RemoteId, base.ShortDebugString());
        if (request && !request->IsInitialized())
            BOOST_THROW_EXCEPTION(Exception("Can't call method because request is not initialized: base: %s, request: %s, errors: %s", base.ShortDebugString(), request->ShortDebugString(), request->InitializationErrorString()));

        return m_Sink->Push(base, request, stream);
    }

    virtual IFuture::Ptr CallMethod(unsigned service,
                                    unsigned method,
                                    const MessagePtr& request,
                                    const IStream& stream) override
    {
        // make base request packet
        proto::BasePacket base;

        {
            base.set_method(method);
            base.set_serviceid(service);
            base.set_packetid(GetNextPacketId());
            base.set_direction(proto::BasePacket::Request);
        }

        return CallMethodImpl(base, request.get(), stream);
    }

    virtual IFuture::Ptr CallMethod(const gp::MethodDescriptor& method,
                                    const MessagePtr& request,
                                    const IStream& stream) override
    {
        const IService::Id id = method.service()->options().GetExtension(proto::ServiceId);
        return CallMethod(id, method.index(), request, stream);
    }

    boost::uint32_t GetNextPacketId() const
    {
        boost::unique_lock<boost::mutex> lock(m_PacketIdMutex);
        while (m_PacketCounter++ == 0); // ensure the packet id is not equal to zero
        return m_PacketCounter;
    }

private:

    void HandleRequest(proto::BasePacket& basePacket, const IStream& stream)
    {
        try
        {
            LOG_DEBUG("<-[%s]: Handling request: [%s]", m_RemoteId, basePacket.ShortDebugString());

            if (basePacket.callerid().empty() && !m_RemoteId.empty())
                basePacket.set_callerid(m_RemoteId);

            HandleRawRequestData(basePacket, stream);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to process request: %s", boost::diagnostic_information(e));

        	// send error response
            basePacket.set_direction(proto::BasePacket::Response);
            basePacket.set_error(conv::cast<std::string>(boost::diagnostic_information(e)));

            m_Sink->Push(basePacket, nullptr, IStream());
        }
    }

    void HandleResponse(proto::BasePacket& basePacket, const IStream& stream)
    {
        if (!basePacket.packetid())
            return;

        LOG_DEBUG("<-[%s]: Handling response: [%s]", m_RemoteId, basePacket.ShortDebugString());

        m_Sink->Pop(basePacket, stream);
    }

    void HandleBasePacket(const IStream& stream)
    {
        proto::BasePacket basePacket;
        try
        {
            details::ReadStream::Read(*stream, basePacket);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to parse base packet: %s", boost::diagnostic_information(e));
            return;
        }

        if (basePacket.direction() == proto::BasePacket::Request)
            HandleRequest(basePacket, stream);
        else
        if (basePacket.direction() == proto::BasePacket::Response)
            HandleResponse(basePacket, stream);
    }

    void HandleRawRequestData(proto::BasePacket& basePacket, const IStream& stream)
    {
        for (const auto& handler : m_RequestHandlers)
        {
            if (handler->HandleRequest(basePacket, stream, shared_from_this()))
                return;
        }
        assert(!"this should never happen, because local handler matches everything");
    }

    virtual void AddHandler(const details::IRequestHandler::Ptr& handler) override
    {
        m_Sink->AddHandler(handler);
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        m_RequestHandlers.emplace_front(handler);
    }

    virtual void Close(const boost::exception_ptr& e) override
    {
        m_Sink->Close(e);
    }

    virtual void SetRemoteId(const InstanceId& id) override
    {
        m_RemoteId = id;
    }

    virtual const InstanceId& GetRemoteId() const override
    {
        return m_RemoteId;
    }

    net::IConnection::Ptr WrapConnection(const net::IConnection::Ptr& connection) const
    {
        return connection;
    }

    virtual boost::shared_ptr<details::IChannelSink> GetSink() const override
    {
        return m_Sink;
    }

protected:
    boost::asio::io_service& m_Service;
    details::IChannelSink::Ptr m_Sink;
private:
    Handlers m_RequestHandlers;
    mutable boost::recursive_mutex m_Mutex;
    InstanceId m_RemoteId;
    mutable std::atomic<boost::uint32_t> m_PacketCounter;
    mutable boost::mutex m_PacketIdMutex;
};

#pragma warning(push)
#pragma warning(disable:4250) // warning C4250: 'rpc::`anonymous-namespace'::Channel' : inherits 'rpc::`anonymous-namespace'::SequencedChannel::rpc::`anonymous-namespace'::SequencedChannel::CallMethod' via dominance

class Channel 
    : public SequencedChannel
    , public IChannel
    , public net::ISequenceCollector::ICallback
{
public:
    Channel(boost::asio::io_service& svc) : SequencedChannel(svc)
    {
    }

    virtual void Init() override
    {
        SequencedChannel::Init();
        m_Sink->SetConnectionWrapper(boost::bind(&Channel::WrapConnection, this, _1));
    }

    virtual void OnIncomingData(const IStream& stream, const boost::exception_ptr& e) override
    {
        try
        {
            if (e)
            {
                SequencedChannel::OnIncomingData(stream, e);
            }
            else
            {
                if (!m_Collector)
                    m_Collector = net::ISequenceCollector::Instance(boost::dynamic_pointer_cast<net::ISequenceCollector::ICallback>(shared_from_this()));

                // parse streams, collect all sequence data
                m_Collector->OnNewStream(stream);
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to process incoming data: %s", boost::diagnostic_information(e));
            BOOST_THROW_EXCEPTION(Exception("Failed to process incoming data"));
        }
    }

    virtual void OnFullStreamCollected(const net::ReadSequence::StreamPtr& stream) override
    {
        SequencedChannel::OnIncomingData(stream, boost::exception_ptr());
    }


    net::IConnection::Ptr WrapConnection(const net::IConnection::Ptr& connection) const
    {
        return boost::make_shared<net::SequencedConnection>(connection);
    }

private:
    net::ISequenceCollector::Ptr m_Collector;
};

#pragma warning(pop)

} // anonymous namespace

ISequencedChannel::Ptr ISequencedChannel::Instance(boost::asio::io_service& svc)
{
    const auto instance = boost::make_shared<SequencedChannel>(svc);
    instance->Init();
    return instance;
}

IChannel::Ptr IChannel::Instance(boost::asio::io_service& svc)
{
    const auto instance = boost::make_shared<Channel>(svc);
    instance->Init();
    return instance;
}


} // namespace rpc