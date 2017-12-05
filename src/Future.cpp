#include "rpc/Future.h"
#include "rpc/Exceptions.h"
#include "Stream.h"

#include <google/protobuf/message.h>

#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>

namespace rpc
{

void details::ParseMessage(google::protobuf::Message& message, std::istream& s)
{
    details::ReadStream::Read(s, message);
}

namespace
{

class FutureImpl : public IFuture, public boost::enable_shared_from_this<FutureImpl>
{
public:

    FutureImpl(boost::asio::io_service& svc) : m_Service(svc) {}

    virtual StreamPtr GetData() override
    {
        boost::shared_ptr<boost::promise<StreamPtr>> promise;
        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            if (m_Exception)
                boost::rethrow_exception(m_Exception);

            if (m_Stream)
                return *m_Stream;

            promise = boost::make_shared<boost::promise<StreamPtr>>();
            const auto callback = [promise](const IFuture::Ptr& f)
            {
                if (f->GetException())
                    promise->set_exception(f->GetException());
                else
                    promise->set_value(f->GetData());
            };

            GetData(callback);
        }

        auto future = promise->get_future();
        while (!future.is_ready())
        {
            if (!m_Service.poll())
                boost::this_thread::sleep_for(boost::chrono::microseconds(1));
        }
        return future.get();
    }

    virtual void GetData(const Callback& c) override
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        if (m_Stream || m_Exception)
        {
            lock.unlock();
            c(shared_from_this());
        }
        else
        {
            m_Callback = c;
        }
    }

    virtual void SetData(const StreamPtr& stream) override
    {
        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            m_Stream = stream;
        }
        InvokeCallback();
    }

    virtual void SetException(const boost::exception_ptr& e) override
    {
        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            m_Exception = e;
        }
        InvokeCallback();
    }

    virtual boost::exception_ptr GetException() const override
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        return m_Exception;
    }

    virtual bool IsReady() const override
    {
        boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
        return m_Exception || m_Stream;
    }

    void InvokeCallback()
    {
        Callback cb;
        {
            boost::unique_lock<boost::recursive_mutex> lock(m_Mutex);
            m_Callback.swap(cb);
        }

        if (cb)
            cb(shared_from_this());
    }

    virtual const google::protobuf::Message* GetBase() const override
    {
        return m_Base.get();
    }

    virtual void SetBase(const google::protobuf::Message& base) override
    {
        m_Base.reset(base.New());
        m_Base->CopyFrom(base);
    }

private:
    boost::asio::io_service& m_Service;
    boost::optional<StreamPtr> m_Stream;
    mutable Callback m_Callback;
    boost::exception_ptr m_Exception;
    std::unique_ptr<google::protobuf::Message> m_Base;

    mutable boost::recursive_mutex m_Mutex;
};

} // anonymous namespace


IFuture::Ptr IFuture::Instance(boost::asio::io_service& svc)
{
    return boost::make_shared<FutureImpl>(svc);
}

} // namespace rpc