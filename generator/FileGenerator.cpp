#include "FileGenerator.h"
#include "ServiceGenerator.h"
#include <google/protobuf/descriptor.pb.h>
//#include <google/protobuf/compiler/cpp/cpp_helpers.h>

#include <map>
#include <sstream>

#include <boost/lexical_cast.hpp>


namespace clrn
{
namespace rpc
{
namespace compiler
{

using google::protobuf::MethodOptions;
using google::protobuf::FieldOptions;

FileGenerator::FileGenerator(const FileDescriptor* file)
    : m_FileDescriptor(file)
{
}

FileGenerator::~FileGenerator()
{ 
}

void FileGenerator::GenerateHeaderNamespaceScope(Printer* printer)
{
    int serviceCount = m_FileDescriptor->service_count();
    for (int i = 0; i < serviceCount; i++)
    {
        google::protobuf::compiler::cpp::ServiceGenerator g(m_FileDescriptor->service(i));
        g.GenerateDeclarations(printer);
    }
}

void FileGenerator::GenerateHeaderIncludes(Printer* printer)
{
    printer->Print(
        "#include \"rpc/Future.h\"\n"
        "#include \"rpc/Base.h\"\n"
    );
}



void FileGenerator::GenerateSourceNamespaceScope(Printer* printer)
{
    if (!m_FileDescriptor->service_count())
        return;

    int serviceCount = m_FileDescriptor->service_count();
    for (int i = 0; i < serviceCount; i++)
    {
        const ServiceDescriptor* desc = m_FileDescriptor->service(i);

        google::protobuf::compiler::cpp::ServiceGenerator g(desc);
        g.GenerateDescriptorInitializer(printer, i);
        g.GenerateImplementation(printer);
    }
}

void FileGenerator::GenerateSourceIncludes(Printer* printer)
{
    printer->Print(
        "#include \"rpc/Exceptions.h\"\n"
        "#include \"rpc_base.pb.h\"\n"
        "#include <assert.h>\n"
        "#include <boost/shared_ptr.hpp>\n"
    );
}
} // namespace compiler
} // namespace rpc
} // namespace clrn