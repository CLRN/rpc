#pragma once

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>

#include <string>
#include <vector>

using google::protobuf::FileDescriptor;
using google::protobuf::ServiceDescriptor;
using google::protobuf::MethodDescriptor;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::io::Printer;

namespace clrn
{
namespace rpc
{
namespace compiler
{

class FileGenerator
{
public:
    // See generator.cc for the meaning of dllexport_decl.
    explicit FileGenerator(const FileDescriptor* file);
    ~FileGenerator();

    void GenerateHeaderNamespaceScope(Printer* printer);
    void GenerateHeaderIncludes(Printer* printer);
    void GenerateSourceNamespaceScope(Printer* printer);
    void GenerateSourceIncludes(Printer* printer);

private:
    const FileDescriptor* m_FileDescriptor;
};


} // namespace Compiler
} // namespace Rpc
} // namespace cmn
