#pragma once

#pragma warning(push)
#pragma warning(disable:4244)
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#pragma warning(pop)

namespace clrn
{
namespace rpc
{
namespace compiler
{

using google::protobuf::FileDescriptor;
using google::protobuf::compiler::CodeGenerator;
using google::protobuf::compiler::GeneratorContext;

class RpcGenerator : public CodeGenerator
{
public:
    RpcGenerator();
    ~RpcGenerator();

    virtual bool Generate(const FileDescriptor * file, const std::string & parameter, GeneratorContext* context, std::string * error) const override;
};

} // namespace compiler
} // namespace rpc
} // namespace clrn
