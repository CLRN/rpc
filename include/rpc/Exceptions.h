#pragma once

#include "log/log.h"

#include <boost/cstdint.hpp>
#include <boost/function.hpp>


namespace proto
{
    class BasePacket;
}
namespace google
{
    namespace protobuf
    {
        class Message;
    }
}

namespace rpc
{

struct Exception : public virtual boost::exception, std::runtime_error
{
    template <typename ... T>
    Exception(const std::string& text, const T&... args)
        : std::runtime_error(logging::MessageFormatter(text, args...).GetText().c_str())
    {}
};

typedef boost::shared_ptr<google::protobuf::Message> MessagePtr;
typedef boost::error_info<struct tag_proto_message, MessagePtr> ProtoErrorInfo;

namespace details
{
    typedef boost::function<MessagePtr()> CreateExceptionFn;

    void RegisterException(const boost::uint32_t id, const CreateExceptionFn& func);
    boost::uint32_t Crc32(const std::string& text);

} // namespace details


boost::exception_ptr MakeException(const proto::BasePacket& base);
boost::exception_ptr MakeException(const std::string& text);
std::string GetExceptionText(const boost::exception_ptr& e);


template<typename T>
void RegisterException()
{
    const auto id = details::Crc32(T::descriptor()->full_name());
    details::RegisterException(id, [](){ return new T(); });
}

// Extracts embedded google::protobuf::Message* from exception
template <typename TProtobufMsgPtr>
static TProtobufMsgPtr ProtobufMessageCast(const std::exception& e)
{
    const auto* protoErrorInfo = boost::get_error_info<rpc::ProtoErrorInfo>(e);
    if (!protoErrorInfo)
        return nullptr;

    return dynamic_cast<TProtobufMsgPtr>(protoErrorInfo->get());
}

void ThrowHttpError(boost::uint32_t code, const std::string& message);

} // namespace rpc
