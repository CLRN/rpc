#include "rpc/LocalHandler.h"
#include "conversion/cast.hpp"
#include "Stream.h"
#include "log/log.h"
#include "ChannelSink.h"

#include <boost/make_shared.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/thread.hpp>

namespace rpc
{
    

SET_LOGGING_MODULE("Rpc");
namespace details
{

static void BindProtobufException(proto::BasePacket& base, const google::protobuf::Message* protoMessage)
{
    const auto id = details::Crc32(protoMessage->GetDescriptor()->full_name());
    base.set_errorid(id);
    protoMessage->SerializeToString(base.mutable_error());
}

static void BindOtherExceptionFormatted(proto::BasePacket& base, const std::string& method, const std::string& service)
{
    if (!base.error().empty())
        *base.mutable_error() += "\n";

    std::ostringstream oss;
    oss << "Method [" << method
        << "] failed with: " << GetExceptionText(boost::current_exception());

    *base.mutable_error() += oss.str();
}

static void ProcessAbstractException(const std::exception& e, proto::BasePacket& base, const std::string& method, const std::string& service)
{
    if (auto protoMessage = rpc::ProtobufMessageCast<const gp::Message*>(e))
        BindProtobufException(base, protoMessage);
    else
        BindOtherExceptionFormatted(base, method, service);
}

ResponseHolder::ResponseHolder() 
    : m_IsSent(false)
    , m_Method()
    , m_Service()
{

}

void ResponseHolder::Send(const gp::Message& message, const IStream& stream)
{
    if (!m_Base)
        return; // response is not initialized

    auto& base = static_cast<proto::BasePacket&>(*m_Base);
    if (!base.packetid() || !m_Channel)
        return; // response is not required

    if (m_IsSent)
        return; // already sent

    // this method will be invoked when response is out of user code scope
    // and there is no more references. so we are ready to send it.
    const auto* m = &message;
    if (base.error().empty())
    {
        try
        {
            if (GetException())
                boost::rethrow_exception(GetException());
            if (!m->IsInitialized())
                BOOST_THROW_EXCEPTION(Exception("Failed to send response, not initialized: %s", m->InitializationErrorString()));
        }
        catch (const std::exception& e)
        {
            m = nullptr;
            ProcessAbstractException(e, base, m_Method->full_name(), m_Service->full_name());
        }
    }
    else
    {
        m = nullptr;
    }

    auto& channel = static_cast<ISequencedChannel&>(*m_Channel);

    try
    {
        // log error if exists
        if (!base.error().empty())
        {
            LOG_ERROR("Sending error response to [%s]: %s", channel.GetRemoteId(), base.DebugString());
        }
        else
        {
            LOG_TRACE("->[%s]: Sending response packet: %s", channel.GetRemoteId(), base.ShortDebugString());
        }

        // serialize response
        const auto sink = channel.GetSink();
        sink->Push(base, m, stream);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to write response: %s", boost::diagnostic_information(e));
    }
    m_IsSent = true;
}

} // namespace details

namespace
{

class LocalHandler 
    : public ILocalHandler
    , public boost::enable_shared_from_this<LocalHandler>
{
public:
    LocalHandler(boost::asio::io_service& svc)
        : m_Service(svc)
    {    
    }

    virtual bool HandleRequest(const gp::Message& baseMessage, const IStream& stream, const rpc::ISequencedChannel::Ptr& channel) override
    {
        const auto& currentBase = static_cast<const proto::BasePacket&>(baseMessage);

        std::vector<IService::Ptr> services;

        {
            boost::unique_lock<boost::mutex> lock(m_ServiceMutex);
            for (const auto& service : m_Services)
            {
                if (const auto s = service.lock())
                {
                    if (s->GetId() == currentBase.serviceid())
                        services.emplace_back(s);
                }
            }
        }

        if (services.empty())
            BOOST_THROW_EXCEPTION(Exception("Unable to handle request, service is not supported: %s", currentBase.ShortDebugString()));

        // get method description from service
        const auto* methodDesc = services.front()->GetDescriptor().method(currentBase.method());
        assert(methodDesc);

        // prepare request and response
        std::unique_ptr<gp::Message> rawRequest(services.front()->CreateRequest(*methodDesc));
        std::unique_ptr<gp::Message> rawResponse(services.front()->CreateResponse(*methodDesc));

        // parse request from stream
        details::ReadStream::Read(*stream, *rawRequest);

        // assign stream if more data exist
        const auto pos = stream->tellg();
        stream->seekg(0, std::ios::end);
        if (pos != stream->tellg())
        {
            stream->clear();
            stream->seekg(pos);
            dynamic_cast<details::StreamHolder&>(*rawRequest).Stream(stream);
        }

        struct ResponseAccess : public details::ResponseHolder
        {
            void SetChannel(const rpc::ISequencedChannel::Ptr& c) { m_Channel = c; }
            void SetBase(const proto::BasePacket& in)
            {
                m_Base = std::make_unique<proto::BasePacket>(in);
                auto& base = static_cast<proto::BasePacket&>(*m_Base);
                base.set_direction(proto::BasePacket::Response);
            }
            void SetMethod(const gp::MethodDescriptor* desc) { m_Method = desc; }
            void SetService(const gp::ServiceDescriptor* desc) { m_Service = desc; }
            proto::BasePacket& GetBase() const { return static_cast<proto::BasePacket&>(*m_Base); }
        };

        struct RequestAccess : public details::RequestAndInfoHolder
        {
            void SetChannel(const rpc::ISequencedChannel::Ptr& c) { m_Channel = c; }
            void SetInstance(const rpc::InstanceId& id) { m_InstanceId = id; }
            void SetIsResponseRequired(bool value) { m_IsResponseRequired = value; }
            void SetMethodDescriptor(const gp::MethodDescriptor* value) { m_MethodDescriptor = value; }
        };

        // set up request and response additional data
        auto responseHolder = dynamic_cast<details::PacketHolder*>(rawResponse.get());
        assert(responseHolder && "Can't extract stream holder from response, generated service is invalid");
        auto* responseAccessor = static_cast<ResponseAccess*>(responseHolder);

        auto requestHolder = dynamic_cast<details::RequestAndInfoHolder*>(rawRequest.get());
        assert(requestHolder && "Can't extract info holder from request, generated service is invalid");
        auto* requestAccessor = static_cast<RequestAccess*>(requestHolder);

        // initialize request and response with current channel
        responseAccessor->SetChannel(channel);
        requestAccessor->SetChannel(channel);

        // set up caller info
        requestAccessor->SetInstance(currentBase.callerid());

        // when packet id is specified response is required
        requestAccessor->SetIsResponseRequired(currentBase.packetid() != 0);

        // set up method descriptor
        requestAccessor->SetMethodDescriptor(methodDesc);

        // initialize response
        responseAccessor->SetBase(currentBase);
        responseAccessor->SetMethod(methodDesc);
        responseAccessor->SetService(&services.front()->GetDescriptor());

        LOG_TRACE("Handling request [%s] by local handler", methodDesc->full_name());

        const auto instance(shared_from_this());
        const MessagePtr request(rawRequest.release());
        const MessagePtr response(rawResponse.release());

        for (auto& service : services)
        {
            try
            {
                service->CallMethod(*methodDesc, request, response);
            }
            catch (const std::exception& e)
            {
                details::ProcessAbstractException(e,
                                                  responseAccessor->GetBase(),
                                                  methodDesc->full_name(),
                                                  service->GetDescriptor().full_name());
            }
        }

        return true;
    }

    virtual void HandleResponse(const gp::Message&, const InstanceId&) override {}

    virtual void ProvideService(const boost::weak_ptr<IService>& svc) override
    {
        boost::unique_lock<boost::mutex> lock(m_ServiceMutex);
        m_Services.emplace_back(svc);
    }

    virtual void RemoveService(const boost::weak_ptr<IService>& svc) override
    {
        boost::unique_lock<boost::mutex> lock(m_ServiceMutex);
        const auto it = boost::find_if(m_Services, boost::bind(&boost::weak_ptr<IService>::lock, _1) == svc.lock());
        if (it != m_Services.end())
            m_Services.erase(it);
    }

    virtual bool HasService(const rpc::IService::Id& id) const override
    {
        boost::unique_lock<boost::mutex> lock(m_ServiceMutex);
        for (const auto& svc : m_Services)
        {
            if (const auto s = svc.lock())
            {
                if (s->GetId() == id)
                    return true;
            }
        }
        return false;
    }

private:
    boost::asio::io_service& m_Service;
    std::vector<boost::weak_ptr<IService>> m_Services;
    mutable boost::mutex m_ServiceMutex;
};


} // anonymous namespace


ILocalHandler::Ptr ILocalHandler::Instance(boost::asio::io_service& svc)
{
    return boost::make_shared<LocalHandler>(svc);
}

} // namespace rpc
