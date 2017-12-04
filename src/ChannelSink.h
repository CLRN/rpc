#pragma once

#include "rpc/Channel.h"
#include "net/connection.hpp"

#include "rpc_base.pb.h"

#include <deque>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/asio/io_service.hpp>

namespace rpc
{
namespace details
{

class IChannelSink
{
public:
    typedef boost::shared_ptr<IChannelSink> Ptr;

    //! Factory function, used if we need to create connection object for write transaction
    typedef boost::function<net::IConnection::Ptr(const net::IConnection::Ptr&)> WrapConnectionFn;
    
    virtual ~IChannelSink() {}

    virtual IFuture::Ptr Push(const proto::BasePacket& base, const gp::Message* request, const IStream& stream) = 0;
    virtual void Pop(const proto::BasePacket& base, const IStream& stream) = 0;
    virtual void SetConnection(const net::IConnection::Ptr& connection) = 0;
    virtual void SetConnectionWrapper(const WrapConnectionFn& wrapper) = 0;
    virtual void Close(const boost::exception_ptr& e) = 0;
    virtual void AddHandler(const details::IRequestHandler::Ptr& handler) = 0;

    //! Instance
    static Ptr Instance(boost::asio::io_service& svc, const boost::weak_ptr<rpc::details::IChannel>& channel);
};

} // namespace details
} // namespace rpc
