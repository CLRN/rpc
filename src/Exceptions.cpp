#include "rpc/Exceptions.h"

#include "rpc_base.pb.h"

#include <boost/crc.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>

namespace rpc
{

std::map<uint32_t, rpc::details::CreateExceptionFn> g_Factory;

boost::exception_ptr MakeException(const std::string& text)
{
    try
    {
        BOOST_THROW_EXCEPTION(Exception(text.c_str()));
    }
    catch (const std::exception&)
    {
        return boost::current_exception();
    }
}

boost::exception_ptr MakeException(const proto::BasePacket& base)
{
    try
    {
        if (base.errorid())
        {
            const auto it = g_Factory.find(base.errorid());
            if (it != g_Factory.end())
            {
                const auto e = it->second();
                if (!e->ParseFromString(base.error()))
                    BOOST_THROW_EXCEPTION(Exception("Failed to parse proto exception"));
                BOOST_THROW_EXCEPTION(Exception("Protobuf exception") << ProtoErrorInfo(e));
            }
        }

        BOOST_THROW_EXCEPTION(Exception(base.error().c_str()));
    }
    catch (const std::exception&)
    {
        return boost::current_exception();
    }
}

std::string GetExceptionText(const boost::exception_ptr& e)
{
    if (!e)
        return std::string();

    try
    {
        boost::rethrow_exception(e);
    }
    catch (const std::exception& e)
    {
        return boost::diagnostic_information(e);
    }
}

namespace details
{

boost::uint32_t Crc32(const std::string& text)
{
    boost::crc_32_type result;
    result.process_bytes(text.c_str(), text.size());
    return result.checksum();
}

void RegisterException(const boost::uint32_t id, const CreateExceptionFn& func)
{
    g_Factory.emplace(id, func);
}

} // namespace details


} // namespace rpc
