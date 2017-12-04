#ifndef RpcBase_h__
#define RpcBase_h__

#include "Future.h"

#include <iosfwd>
#include <string>
#include <initializer_list>
#include <set>

#include <boost/noncopyable.hpp>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

namespace boost
{
namespace asio
{
    class io_service;
}
}

namespace google
{
namespace protobuf
{
    class MethodDescriptor;
    class Message;
    class ServiceDescriptor;
} // namespace protobuf
} // namespace google

namespace rpc
{

namespace gp = google::protobuf;
typedef boost::shared_ptr<std::istream> IStream;
typedef boost::shared_ptr<std::ostream> OStream;
typedef boost::shared_ptr<gp::Message> MessagePtr;
typedef std::string InstanceId;
typedef boost::uint64_t UserId;
typedef std::string NetworkId;

namespace choose
{

static const unsigned MAX_HOPS = unsigned(-1);

enum CallType
{
    Any         = 0,
    All         = 1,
    Unloaded    = 2,
    Closest     = 3,
    Event       = 4
};

struct ById
{
    ById() : m_Value() {}
    ById(const InstanceId& id)
        : m_Value(id)
    {}
    InstanceId m_Value;
};

struct ByNet
{
    ByNet() : m_Value() {}
    ByNet(const InstanceId& id)
        : m_Value(id)
    {}
    NetworkId m_Value;
};

}// namespace choose

namespace details
{

class IChannel;

class PacketHolder
{
public:
    PacketHolder() : m_Channel() {}
    virtual ~PacketHolder() {}

    const boost::exception_ptr& GetException() const { return m_Exception; }
    void SetException(const boost::exception_ptr& e) { m_Exception = e; }
    IChannel& GetChannel() { return *m_Channel; }
protected:
    boost::exception_ptr m_Exception;
    boost::shared_ptr<IChannel> m_Channel;
};

class RequestAndInfoHolder : public PacketHolder
{
public:
    RequestAndInfoHolder() : m_IsResponseRequired(), m_MethodDescriptor() {}

    const rpc::InstanceId& GetCaller() const { return m_InstanceId; }
    bool IsResponseRequired() const { return m_IsResponseRequired; }
    const gp::MethodDescriptor& GetMethodDescriptor() const { return *m_MethodDescriptor; }
protected:
    rpc::InstanceId m_InstanceId;
    bool m_IsResponseRequired;
    const gp::MethodDescriptor* m_MethodDescriptor;
};

class StreamHolder
{
public:
    StreamHolder(const IStream& s = IStream()) : m_Stream(s) {}

    const IStream& Stream() const { return m_Stream; }
    IStream& Stream() { return m_Stream; }
    void Stream(const IStream& stream) { m_Stream = stream; }
private:
    IStream m_Stream;
};

class ResponseHolder : public PacketHolder
{
public:
    ResponseHolder();
protected:
    void Send(const gp::Message& message, const IStream& stream);
protected:
    bool m_IsSent;
    std::unique_ptr<gp::Message> m_Base;
    const gp::MethodDescriptor* m_Method;
    const gp::ServiceDescriptor* m_Service;
};

struct CallParams
{
    template<typename ... T>
    explicit CallParams(const T&... args) 
        : m_Type()
        , m_Id()
        , m_Hops(choose::MAX_HOPS)
        , m_UserId()
        , m_Network()
        , m_NoResponse(false)
    {
        Assign(args...);
    }
    void Assign()
    {
    }
    template<typename ... Args>
    void Assign(const choose::CallType type, const Args&... args)
    {
        m_Type = type;
        Assign(args...);
    }
    template<typename ... Args>
    void Assign(const choose::ById& id, const Args&... args)
    {
        m_Id = id;
        Assign(args...);
    }
    template<typename ... Args>
    void Assign(const unsigned hops, const Args&... args)
    {
        m_Hops = hops;
        Assign(args...);
    }
    template<typename ... Args>
    void Assign(const choose::ByNet& net, const Args&... args)
    {
        m_Network = net;
        Assign(args...);
    }

    choose::CallType m_Type;
    choose::ById m_Id;
    unsigned m_Hops;
    UserId m_UserId;
    choose::ByNet m_Network;
    bool m_NoResponse;
    std::string m_UserIp;
    std::set<InstanceId> m_Visited;
};

} // namespace details

template<typename T>
class Request : public T, public details::RequestAndInfoHolder
{
public:
    template<typename ... Arg>
    Request(Arg... var) : T(var...) {}

    typedef boost::shared_ptr<Request<T> > Ptr;
};

template<typename T>
class Response : public T, public details::ResponseHolder
{
public:
    typedef boost::shared_ptr<Response<T> > Ptr;
    ~Response()
    {
        Send();
    }
    void Send()
    {
        ResponseHolder::Send(*this, IStream());
    }
};

template<typename T>
class StreamRequest : public T, public details::RequestAndInfoHolder, public details::StreamHolder
{
public:
    template<typename ...Arg>
    StreamRequest(const IStream& s = IStream(), Arg... var) : T(var...), details::StreamHolder(s) {}
    typedef boost::shared_ptr<StreamRequest<T> > Ptr;
};

template<typename T>
class StreamResponse : public T, public details::ResponseHolder, public details::StreamHolder
{
public:
    typedef boost::shared_ptr<StreamResponse<T> > Ptr;

    ~StreamResponse()
    {
        Send();
    }
    void Send()
    {
        ResponseHolder::Send(*this, Stream());
    }
};

namespace details
{

class IChannel
{
public:
    virtual ~IChannel() {}
    virtual IFuture::Ptr CallMethod(const gp::MethodDescriptor& method, 
                                    const MessagePtr& request,
                                    const IStream& stream,
                                    const CallParams& params) = 0;
    virtual const InstanceId& GetRemoteId() const = 0;
};

} // namespace details

class IService : boost::noncopyable
{
public:
    virtual ~IService() {}

    typedef unsigned Id;
    typedef boost::shared_ptr<IService> Ptr;

    //! Auto generated implementation
    virtual void CallMethod(const gp::MethodDescriptor& method,
                            const MessagePtr& request,
                            const MessagePtr& response) = 0;

    virtual const gp::ServiceDescriptor& GetDescriptor() = 0;
    virtual Id GetId() const = 0;
    virtual const InstanceId& GetName() const = 0;

    virtual gp::Message* CreateRequest(const gp::MethodDescriptor& method) const = 0;
    virtual gp::Message* CreateResponse(const gp::MethodDescriptor& method) const = 0;
};

} // namespace rpc

#endif // RpcBase_h__
