#include "RpcGenerator.h"
#include "FileGenerator.h"

#include <tuple>
#include <exception>
#include <sstream>
#include <memory>

namespace clrn
{
namespace rpc
{
namespace compiler
{

using google::protobuf::io::ZeroCopyOutputStream;
using namespace std;

namespace
{
    string StripProto(const string& name)
    {
        string::size_type pos = name.find_last_of('.');
        if (pos != string::npos && !name.substr(pos + 1).compare("proto"))
            return name.substr(0, pos);

        return name;
    }

    typedef void (FileGenerator::*GenFunc)(Printer*);
    typedef tuple<string, string, GenFunc> GenDataItem;

}

RpcGenerator::RpcGenerator(void)
{
}

RpcGenerator::~RpcGenerator(void)
{
}

bool RpcGenerator::Generate(const FileDescriptor * file, const string & /*parameter*/, GeneratorContext* context, string * error) const
{
    try
    {
        string basename = StripProto(file->name());
        basename.append(".pb");

        FileGenerator fileGenerator(file);

        GenDataItem patchData[] =
        {
            make_tuple(".h", "namespace_scope", &FileGenerator::GenerateHeaderNamespaceScope),
            //make_tuple(".h", "global_scope", &FileGenerator::GenerateHeaderGlobaleScope),
            make_tuple(".h", "includes", &FileGenerator::GenerateHeaderIncludes),
            make_tuple(".cc", "namespace_scope", &FileGenerator::GenerateSourceNamespaceScope),
            //make_tuple(".cc", "global_scope", &FileGenerator::GenerateSourceGlobaleScope),
            make_tuple(".cc", "includes", &FileGenerator::GenerateSourceIncludes),
        };

        for (size_t i = 0; i < sizeof(patchData)/sizeof(patchData[0]); i++)
        {
            const GenDataItem& patch = patchData[i];

            const string& fileName = basename + get<0>(patch);
            const string& insertPoint = get<1>(patch);
            GenFunc func = get<2>(patch);

            std::unique_ptr<ZeroCopyOutputStream> output(context->OpenForInsert(fileName, insertPoint));
            if (!output.get())
            {
                ostringstream oss;
                oss << "insertion_point '" << insertPoint << "' not found in file '" << fileName << "'";
                throw runtime_error(oss.str().c_str());
            }

            Printer printer(output.get(), '$');
            (fileGenerator.*func)(&printer);
        }
    }
    catch (const exception& ex)
    {
        error->assign(ex.what());
        return false;
    }

    return true;
}

} // namespace Compiler
} // namespace Rpc
} // namespace cmn
