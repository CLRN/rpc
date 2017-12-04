#pragma once

#include "Channel.h"

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/asio/io_service.hpp>

namespace rpc
{

class ILocalHandler : public details::IRequestHandler
{
public:
    typedef boost::shared_ptr<ILocalHandler> Ptr;

    virtual ~ILocalHandler() {}

    virtual void ProvideService(const boost::weak_ptr<IService>& svc) = 0;
    virtual void RemoveService(const boost::weak_ptr<IService>& svc) = 0;
    virtual bool HasService(const rpc::IService::Id& id) const = 0;
    
    //! Instance
    static Ptr Instance(boost::asio::io_service& svc);
};

} // namespace rpc
