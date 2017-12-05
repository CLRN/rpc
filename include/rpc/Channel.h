#pragma once

#include "Base.h"
#include "net/connection.hpp"

#include <iosfwd>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <boost/asio/io_service.hpp>

namespace rpc
{

namespace details
{
struct IRequestHandler;
class IChannelSink;
} // namespace details
    
class ISequencedChannel : public rpc::details::IChannel
{
public:
    typedef boost::shared_ptr<ISequencedChannel> Ptr;

    typedef std::set<unsigned> Services;

    virtual ~ISequencedChannel() {}
    virtual IFuture::Ptr CallMethod(unsigned service, unsigned method, const MessagePtr& request, const IStream& stream) = 0;
    virtual void AddHandler(const boost::shared_ptr<details::IRequestHandler>& handler) = 0;
    virtual void OnIncomingData(const IStream& stream, const boost::exception_ptr& e) = 0;
    virtual void Close(const boost::exception_ptr& e) = 0;
    virtual void SetRemoteId(const InstanceId& id) = 0;
    virtual boost::shared_ptr<details::IChannelSink> GetSink() const = 0;
    virtual void SetConnection(const net::IConnection::Ptr& connection) = 0;

    static Ptr Instance(boost::asio::io_service& svc);
};

class IChannel : virtual public ISequencedChannel
{
public:
    typedef boost::shared_ptr<IChannel> Ptr;

    static Ptr Instance(boost::asio::io_service& svc);
};

namespace details
{

class IQueue;

//! RPC request handler, may be set as additional request handler for rpc channel
struct IRequestHandler
{
    typedef boost::shared_ptr<IRequestHandler> Ptr;

    virtual ~IRequestHandler() {}

    //! Handle request
    //!\return true if request is handled, false otherwise
    virtual bool HandleRequest(const gp::Message& base, const IStream& stream, const rpc::ISequencedChannel::Ptr& channel) = 0;

    //! Handle response
    virtual void HandleResponse(const gp::Message& base, const rpc::InstanceId& instance) = 0;
};

class IChannelCallback
{
public:
    typedef boost::shared_ptr<IChannelCallback> Ptr;
    struct ChannelDesc
    {
        ISequencedChannel::Ptr m_Channel;
        InstanceId m_Id;
    };
    typedef std::vector<ChannelDesc> Channels;
    typedef boost::shared_ptr<IQueue> QueuePtr;

    virtual ~IChannelCallback() {}
    virtual ISequencedChannel::Ptr GetChannelByInstanceId(const InstanceId& id) const = 0;
    virtual Channels GetAllChannels() const = 0;
    virtual bool CanWeHandleThis(rpc::IService::Id service) const = 0;
};

} // namespace details
} // namespace rpc
