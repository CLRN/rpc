#ifndef Future_h__
#define Future_h__

#include <iosfwd>

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/exception_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

namespace google
{
namespace protobuf
{
    class Message;
}
}

namespace boost
{
namespace asio
{
    class io_service;
}
}

namespace rpc
{

class IFuture
{
public:
    typedef boost::shared_ptr<IFuture> Ptr;
    typedef boost::shared_ptr<std::istream> StreamPtr;
    typedef boost::function<void(const Ptr& future)> Callback;
    typedef boost::posix_time::time_duration Duration;

    virtual ~IFuture() {}

    virtual StreamPtr GetData() = 0;
    virtual void GetData(const Callback& c) = 0;
    virtual void SetData(const StreamPtr& stream) = 0;
    virtual void SetException(const boost::exception_ptr& e) = 0;
    virtual boost::exception_ptr GetException() const = 0;
    virtual bool IsReady() const = 0;
    virtual const google::protobuf::Message* GetBase() const = 0;
    virtual void SetBase(const google::protobuf::Message& base) = 0;

    static Ptr Instance(boost::asio::io_service& svc);
};

namespace details
{

void ParseMessage(google::protobuf::Message& message, std::istream& s);

} // namespace details


template<typename T>
class Future : public boost::enable_shared_from_this<Future<T>>
{
    typedef boost::function<void(const Future<T>&)> UserCallbackFn;
public:
    Future(const IFuture::Ptr& f) : m_Future(f), m_Parsed() {}

    operator const T& () const
    {
        return Response();
    }

    const T& Response() const
    {
        // parse message here
        if (!m_Parsed)
        {
            const auto stream = m_Future->GetData();
            if (stream)
                details::ParseMessage(m_Message, *stream);
            m_Parsed = true;
        }
        return m_Message;
    }

    IFuture::StreamPtr Stream() const
    {
        Response(); // ensure that message already parsed
        return m_Future->GetData();
    }

    template<typename C>
    void Async(const C& callback) const
    {
        const UserCallbackFn cb(callback);
        m_Future->GetData(boost::bind(&Future::Callback, _1, cb));
    }

    template<typename Response, typename Callback>
    void Async(const Response& response, const Callback& callback) const
    {
        m_Future->GetData([callback, response](const Future<T>& future)
        {
            try
            {
                callback(future.Response());
            }
            catch (const std::exception&)
            {
                response->SetException(boost::current_exception());
            }
        });
    }

    bool IsReady() const
    {
        return m_Future->IsReady();
    }

private:
    static void Callback(const IFuture::Ptr& future, const UserCallbackFn& cb)
    {
        Future<T> copy(future);
        cb(copy);
    }

private:

    IFuture::Ptr m_Future;
    mutable T m_Message;
    mutable bool m_Parsed;
};


} // namespace rpc


#endif // Future_h__
