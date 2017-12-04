#include "rpc/Channel.h"
#include "test_service.pb.h"
#include "rpc/LocalHandler.h"
#include "../src/ChannelSink.h"
#include "net/details/memory.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <google/protobuf/descriptor.h>

#include <iostream>
#include <sstream>

#include <boost/assign.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/make_shared.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/thread/future.hpp>
#include <boost/range/algorithm.hpp>

using testing::Range;
using testing::Combine;
using ::testing::_;
using ::testing::Return;
using ::testing::Exactly;
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Expectation;

void TestContent(std::istream& s)
{
    // copy data
    std::vector<char> testData;
    std::copy(std::istream_iterator<char>(s), std::istream_iterator<char>(), std::back_inserter(testData));
}

class Service : public proto::test::TestService
{
public:

    virtual void TestMethod(const rpc::StreamRequest<::proto::test::Request>::Ptr& request, const rpc::StreamResponse<::proto::test::Response>::Ptr& response) override
    {
        response->set_data(request->data() + 1);

        // copy stream back
        if (request->Stream())
        {
            const auto out = boost::make_shared<std::stringstream>();
            std::copy(std::istream_iterator<char>(*request->Stream()), std::istream_iterator<char>(), std::ostream_iterator<char>(*out));
            response->Stream(out);
        }
    }

};

class SimpleLocalConnection : public net::IConnection
{
public:
    class LocalData : public net::details::IData
    {
    public:
        LocalData(SimpleLocalConnection& connection) : m_Parent(connection) {}
        ~LocalData()
        {
        }

        virtual void Write(const void* data, std::size_t size) override
        {
            m_Parent.m_Stream->write(reinterpret_cast<const char*>(data), size);
        }

        virtual const std::vector<boost::asio::mutable_buffer>& GetBuffers() override
        {
            static const std::vector<boost::asio::mutable_buffer> res;
            return res;
        }


    private:
        std::string m_Buffer;
        SimpleLocalConnection& m_Parent;
    };

    SimpleLocalConnection() : m_Stream(boost::make_shared<std::stringstream>()) {}

    virtual void Receive(const Callback& callback)
    {
        throw std::runtime_error("The method or operation is not implemented.");
    }
    virtual void Close()
    {
        throw std::runtime_error("The method or operation is not implemented.");
    }
    virtual net::details::IData::Ptr Prepare(std::size_t size) override
    {
        return boost::make_shared<LocalData>(*this);
    }

    template<typename T>
    void WriteToChannel(T& channel)
    {
        channel.OnIncomingData(m_Stream, boost::exception_ptr());
    }

    virtual void Flush() override
    {
        throw std::logic_error("The method or operation is not implemented.");
    }
    virtual std::string GetInfo() const
    {
        return "";
    }

private:
    boost::shared_ptr<std::stringstream> m_Stream;
};

class SequencedLocalConnection : public net::IConnection
{
public:
    class LocalData : public net::details::IData
    {
    public:
        LocalData(SequencedLocalConnection& connection) : m_Parent(connection) {}
        ~LocalData()
        {
            const auto stream = boost::make_shared<std::stringstream>();
            stream->write(m_Buffer.c_str(), m_Buffer.size());
            m_Parent.m_Streams.push_back(stream);
        }

        virtual void Write(const void* data, std::size_t size) override
        {
            std::copy(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data)+size, std::back_inserter(m_Buffer));
        }

        virtual const std::vector<boost::asio::mutable_buffer>& GetBuffers() override
        {
            static const std::vector<boost::asio::mutable_buffer> res;
            return res;
        }


    private:
        std::string m_Buffer;
        SequencedLocalConnection& m_Parent;
    };

    virtual void Receive(const Callback& callback)
    {
        throw std::runtime_error("The method or operation is not implemented.");
    }
    virtual void Close()
    {
        throw std::runtime_error("The method or operation is not implemented.");
    }

    virtual net::details::IData::Ptr Prepare(std::size_t size) override
    {
        return boost::make_shared<LocalData>(*this);
    }

    template<typename T>
    void WriteToChannel(T& channel)
    {
        boost::for_each(m_Streams, boost::bind(&T::OnIncomingData, &channel, _1, boost::exception_ptr()));
    }

    virtual void Flush() override
    {
        throw std::logic_error("The method or operation is not implemented.");
    }

    virtual std::string GetInfo() const
    {
        return "";
    }

private:
    std::vector<rpc::IStream> m_Streams;
};

template<typename T>
struct ConnectionGetter;

template<>
struct ConnectionGetter<rpc::ISequencedChannel>
{
    typedef SimpleLocalConnection Type;
};

template<>
struct ConnectionGetter<rpc::IChannel>
{
    typedef SequencedLocalConnection Type;
};

template<typename T>
void SynchronousWithoutStreamTest()
{
    boost::asio::io_service service;

    // initialize client
    const auto clientConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto client = T::Instance(service);
    client->GetSink()->SetConnection(clientConnection);

    // send request
    proto::test::Request request;
    request.set_data(99);
    const auto future = proto::test::TestService::Stub(*client).TestMethod(request, rpc::IStream());

    // initialize server
    const auto serverConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto server = T::Instance(service);
    server->GetSink()->SetConnection(serverConnection);

    // initialize local handler
    const auto handler = rpc::ILocalHandler::Instance(service);
    const auto svc = boost::make_shared<Service>();
    handler->ProvideService(svc);
    server->AddHandler(handler);

    // parse client output stream by server channel
    clientConnection->WriteToChannel(*server);

    // process request by service
    service.poll();

    // parse server output stream by client channel
    serverConnection->WriteToChannel(*client);

    // obtain and validate result from future
    EXPECT_EQ(future.Response().data(), 100);
}

template<typename T>
void SynchronousWithStreamTest()
{
    boost::asio::io_service service;

    // initialize client
    const auto clientConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto client = T::Instance(service);
    client->GetSink()->SetConnection(clientConnection);

    // send request
    proto::test::Request request;
    request.set_data(1);
    const auto streamData = boost::make_shared<std::stringstream>("sometext");
    const auto future = proto::test::TestService::Stub(*client).TestMethod(request, streamData);

    // initialize server
    const auto serverConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto server = T::Instance(service);
    server->GetSink()->SetConnection(serverConnection);

    // initialize local handler
    const auto handler = rpc::ILocalHandler::Instance(service);
    const auto svc = boost::make_shared<Service>();
    handler->ProvideService(svc);
    server->AddHandler(handler);

    // parse client output stream by server channel
    clientConnection->WriteToChannel(*server);

    // process request by service
    service.poll();

    // parse server output stream by client channel
    serverConnection->WriteToChannel(*client);

    // obtain and validate result from future
    EXPECT_EQ(future.Response().data(), 2);

    std::string out;
    *future.Stream() >> out;

    EXPECT_EQ(out, "sometext");
}


template<typename T>
void AsynchronousWithoutStream()
{
    boost::asio::io_service service;

    // initialize client
    const auto clientConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto client = T::Instance(service);
    client->GetSink()->SetConnection(clientConnection);

    // set up future callback
    const auto callback = [](const rpc::Future<proto::test::Response>& future){

        // obtain and validate result from future
        EXPECT_EQ(future.Response().data(), 100);
    };

    // send request
    proto::test::Request request;
    request.set_data(99);
    proto::test::TestService::Stub(*client).TestMethod(request, rpc::IStream()).Async(callback);

    // initialize server
    const auto serverConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto server = T::Instance(service);
    server->GetSink()->SetConnection(serverConnection);

    // initialize local handler
    const auto handler = rpc::ILocalHandler::Instance(service);
    const auto svc = boost::make_shared<Service>();
    handler->ProvideService(svc);
    server->AddHandler(handler);

    // parse client output stream by server channel
    clientConnection->WriteToChannel(*server);

    // process request by service
    service.poll();

    // parse server output stream by client channel
    serverConnection->WriteToChannel(*client);
}


template<typename T>
void AsynchronousWithStreamTest()
{
    boost::asio::io_service service;

    // initialize client
    const auto clientConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto client = T::Instance(service);
    client->GetSink()->SetConnection(clientConnection);

    // set up future callback
    const auto callback = [](const rpc::Future<proto::test::Response>& future){

        // obtain and validate result from future
        EXPECT_EQ(future.Response().data(), 2);

        std::string out;
        *future.Stream() >> out;

        EXPECT_EQ(out, "sometext");
    };

    // send request
    proto::test::Request request;
    request.set_data(1);
    const auto streamData = boost::make_shared<std::stringstream>("sometext");
    proto::test::TestService::Stub(*client).TestMethod(request, streamData).Async(callback);

    // initialize server
    const auto serverConnection = boost::make_shared<typename ConnectionGetter<T>::Type>();
    const auto server = T::Instance(service);
    server->GetSink()->SetConnection(serverConnection);

    // initialize local handler
    const auto handler = rpc::ILocalHandler::Instance(service);
    const auto svc = boost::make_shared<Service>();
    handler->ProvideService(svc);
    server->AddHandler(handler);

    // parse client output stream by server channel
    clientConnection->WriteToChannel(*server);

    // process request by service
    service.poll();

    // parse server output stream by client channel
    serverConnection->WriteToChannel(*client);
}

TEST(SequencedRpcChannel, SynchronousWithoutStream)
{
    SynchronousWithoutStreamTest<rpc::ISequencedChannel>();
}

TEST(SequencedRpcChannel, SynchronousWithStream)
{
    SynchronousWithStreamTest<rpc::ISequencedChannel>();
}

TEST(SequencedRpcChannel, AsynchronousWithoutStream)
{
    AsynchronousWithoutStream<rpc::ISequencedChannel>();
}

TEST(SequencedRpcChannel, AsynchronousWithStream)
{
    AsynchronousWithStreamTest<rpc::ISequencedChannel>();
}

TEST(RpcChannel, SynchronousWithoutStream)
{
    SynchronousWithoutStreamTest<rpc::IChannel>();
}

TEST(RpcChannel, SynchronousWithStream)
{
    SynchronousWithStreamTest<rpc::IChannel>();
}

TEST(RpcChannel, AsynchronousWithoutStream)
{
    AsynchronousWithoutStream<rpc::IChannel>();
}

TEST(RpcChannel, AsynchronousWithStream)
{
    AsynchronousWithStreamTest<rpc::IChannel>();
}
