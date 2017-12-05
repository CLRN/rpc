#include "ServiceGenerator.h"
#include "cpp_helpers.h"

#include <google/protobuf/io/printer.h>

namespace google
{
namespace protobuf
{
namespace compiler
{
namespace cpp
{

ServiceGenerator::ServiceGenerator(const ServiceDescriptor* descriptor) : descriptor_(descriptor)
{
    vars_["classname"] = descriptor_->name();
    vars_["full_name"] = descriptor_->full_name();
    vars_["dllexport"] = "";
}

ServiceGenerator::~ServiceGenerator()
{
}

void ServiceGenerator::GenerateDeclarations(io::Printer* printer)
{
    // Forward-declare the stub type.
    printer->Print(vars_, "class $classname$_Stub;\n"
        "\n");

    GenerateInterface(printer);
    GenerateStubDefinition(printer);
}

void ServiceGenerator::GenerateInterface(io::Printer* printer)
{
    printer->Print(vars_, "class $dllexport$$classname$ : public rpc::IService {\n"
        " protected:\n"
        "  // This class should be treated as an abstract interface.\n"
        "  inline $classname$(const rpc::InstanceId& name = rpc::InstanceId()) : m_Name(name) {};\n"
        " public:\n"
        "  virtual ~$classname$();\n");
    printer->Indent();

    printer->Print(vars_, "\n"
        "typedef $classname$_Stub Stub;\n"
        "\n"
        "static const ::google::protobuf::ServiceDescriptor& descriptor();\n"
        "\n");

    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        std::map<string, string> sub_vars;
        sub_vars["name"]          = method->name();
        sub_vars["input_type"]    = ClassName(method->input_type(), true);
        sub_vars["output_type"]   = ClassName(method->output_type(), true);
        sub_vars["request_type"]  = GetRequestWrapper(*method);
        sub_vars["response_type"] = GetResponseWrapper(*method);

        printer->Print(sub_vars, "virtual void $name$(const rpc::$request_type$<$input_type$>::Ptr& request,\n"
            "                    const rpc::$response_type$<$output_type$>::Ptr& response);\n");
    }

    printer->Print("\n"
                       "// implements Service ----------------------------------------------\n"
                       "\n"
                       "const ::google::protobuf::ServiceDescriptor& GetDescriptor();\n"
                       "virtual void CallMethod(const ::google::protobuf::MethodDescriptor& method,\n"
                       "                        const rpc::MessagePtr& request,\n"
                       "                        const rpc::MessagePtr& response) override;\n"

                       "virtual google::protobuf::Message* CreateRequest(\n"
                       "  const ::google::protobuf::MethodDescriptor& method) const override;\n"
                       "virtual google::protobuf::Message* CreateResponse(\n"
                       "  const ::google::protobuf::MethodDescriptor& method) const override;\n"
                       "virtual const rpc::InstanceId& GetName() const override;\n"
                       "virtual Id GetId() const override;\n");

    printer->Outdent();
    printer->Print(vars_, "\n"
        " private:\n"
        "  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS($classname$);\n"
        "  const rpc::InstanceId m_Name;\n"
        "};\n"
        "\n");
}

void ServiceGenerator::GenerateStubDefinition(io::Printer* printer)
{
    printer->Print(vars_, "class $dllexport$$classname$_Stub : public $classname$ {\n"
        " public:\n");

    printer->Indent();

    printer->Print(vars_, "$classname$_Stub(const $classname$_Stub& other); \n"
        "$classname$_Stub(rpc::details::IChannel& channel); \n"
        "~$classname$_Stub();\n"
        "\n"
        "// implements $classname$ ------------------------------------------\n"
        "\n");

    GenerateMethodSignatures(NON_VIRTUAL, printer);

    printer->Outdent();
    printer->Print(vars_, " private:\n"
        "  rpc::details::IChannel* channel_;\n"
        "};\n"
        "\n");
}

void ServiceGenerator::GenerateMethodSignatures(VirtualOrNon virtual_or_non, io::Printer* printer)
{
    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        std::map<string, string> sub_vars;
        sub_vars["name"]        = method->name();
        sub_vars["input_type"]  = ClassName(method->input_type(), true);
        sub_vars["output_type"] = ClassName(method->output_type(), true);
        sub_vars["virtual"]     = virtual_or_non == VIRTUAL ? "virtual " : "";
        sub_vars["signature"]   = GetMethodSignature(*method, ClassName(method->input_type(), true));

        printer->Print(sub_vars, "$virtual$rpc::Future<$output_type$> $name$($signature$);\n");
    }
}

// ===================================================================

void ServiceGenerator::GenerateDescriptorInitializer(io::Printer* printer, int index)
{
    std::map<string, string> vars;
    vars["classname"] = descriptor_->name();
    vars["index"]     = SimpleItoa(index);
    vars["file"]      = descriptor_->file()->name();

    printer->Print(vars,
                   "const ::google::protobuf::FileDescriptor* file$classname$_descriptor_ = ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName(\"$file$\");\n"
                   "const ::google::protobuf::ServiceDescriptor* $classname$_descriptor_ = file$classname$_descriptor_->service($index$);\n");
}

// ===================================================================

void ServiceGenerator::GenerateImplementation(io::Printer* printer)
{
    printer->Print(vars_, "$classname$::~$classname$() {}\n"
        "\n"
        "const ::google::protobuf::ServiceDescriptor& $classname$::descriptor() {\n"
        //"  protobuf_AssignDescriptorsOnce();\n"
        "  return *$classname$_descriptor_;\n"
        "}\n"
        "\n"
        "const ::google::protobuf::ServiceDescriptor& $classname$::GetDescriptor() {\n"
        //"  protobuf_AssignDescriptorsOnce();\n"
        "  return *$classname$_descriptor_;\n"
        "}\n"
        "\n"

        "rpc::IService::Id $classname$::GetId() const {\n"
        "  return descriptor().options().GetExtension(proto::ServiceId); \n"
        "}\n"
        "\n"

        "const rpc::InstanceId& $classname$::GetName() const {\n"
        "  return m_Name; \n"
        "}\n"
        "\n");

    // Generate methods of the interface.
    GenerateNotImplementedMethods(printer);
    GenerateCallMethod(printer);
    GenerateGetPrototype(REQUEST, printer);
    GenerateGetPrototype(RESPONSE, printer);

    // Generate stub implementation.
    printer->Print(vars_, "$classname$_Stub::$classname$_Stub(const $classname$_Stub& other)\n"
        "  : channel_(other.channel_) {}\n"

        "$classname$_Stub::$classname$_Stub(rpc::details::IChannel& channel)\n"
        "  : channel_(&channel) {}\n"
        "$classname$_Stub::~$classname$_Stub() {\n"
        "}\n"
        "\n");

    GenerateStubMethods(printer);
}

void ServiceGenerator::GenerateNotImplementedMethods(io::Printer* printer)
{
    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        std::map<string, string> sub_vars;
        sub_vars["classname"]     = descriptor_->name();
        sub_vars["name"]          = method->name();
        sub_vars["index"]         = SimpleItoa(i);
        sub_vars["input_type"]    = ClassName(method->input_type(), true);
        sub_vars["output_type"]   = ClassName(method->output_type(), true);
        sub_vars["request_type"]  = GetRequestWrapper(*method);
        sub_vars["response_type"] = GetResponseWrapper(*method);

        printer->Print(sub_vars, "void $classname$::$name$(const rpc::$request_type$<$input_type$>::Ptr&,\n"
            "                         const rpc::$response_type$<$output_type$>::Ptr&) {\n"
            "  BOOST_THROW_EXCEPTION(rpc::Exception(\"Method not implemented\"));\n"
            "}\n"
            "\n");
    }
}

void ServiceGenerator::GenerateCallMethod(io::Printer* printer)
{
    printer->Print(vars_, "void $classname$::CallMethod(const google::protobuf::MethodDescriptor& method,\n"
        "                             const rpc::MessagePtr& request,\n"
        "                             const rpc::MessagePtr& response) {\n"
        "  GOOGLE_DCHECK_EQ(method.service(), $classname$_descriptor_);\n"
        "  switch(method.index()) {\n");

    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        std::map<string, string> sub_vars;
        sub_vars["name"]          = method->name();
        sub_vars["index"]         = SimpleItoa(i);
        sub_vars["input_type"]    = ClassName(method->input_type(), true);
        sub_vars["output_type"]   = ClassName(method->output_type(), true);
        sub_vars["request_type"]  = GetRequestWrapper(*method);
        sub_vars["response_type"] = GetResponseWrapper(*method);

        // Note:  down_cast does not work here because it only works on pointers,
        //   not references.
        printer->Print(sub_vars, "    case $index$:\n"
            "      $name$(boost::dynamic_pointer_cast<rpc::$request_type$<$input_type$> >(request),\n"
            "             boost::dynamic_pointer_cast<rpc::$response_type$<$output_type$> >(response));\n"
            "      break;\n");
    }

    printer->Print(vars_, "    default:\n"
        "      GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n"
        "      break;\n"
        "  }\n"
        "}\n"
        "\n");
}

void ServiceGenerator::GenerateGetPrototype(RequestOrResponse which, io::Printer* printer)
{
    if (which == REQUEST)
    {
        printer->Print(vars_, "google::protobuf::Message* $classname$::CreateRequest(\n");
    }
    else
    {
        printer->Print(vars_, "google::protobuf::Message* $classname$::CreateResponse(\n");
    }

    printer->Print(vars_, "    const ::google::protobuf::MethodDescriptor& method) const {\n"
        "  GOOGLE_DCHECK_EQ(method.service(), &descriptor());\n"
        "  switch(method.index()) {\n");

    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        const Descriptor      * type   = (which == REQUEST) ? method->input_type() : method->output_type();

        std::map<string, string> sub_vars;
        sub_vars["index"]         = SimpleItoa(i);
        sub_vars["type"]          = ClassName(type, true);
        sub_vars["request_type"]  = GetRequestWrapper(*method);
        sub_vars["response_type"] = GetResponseWrapper(*method);


        if (which == REQUEST)
        {
            printer->Print(sub_vars, "    case $index$:\n"
                "      return new rpc::$request_type$<$type$>;\n");
        }
        else
        {
            printer->Print(sub_vars, "    case $index$:\n"
                "      return new rpc::$response_type$<$type$>;\n");
        }
    }

    printer->Print(vars_, "    default:\n"
        "      GOOGLE_LOG(FATAL) << \"Bad method index; this should never happen.\";\n"
        "      return reinterpret_cast< ::google::protobuf::Message*>(NULL);\n"
        "  }\n"
        "}\n"
        "\n");
}

void ServiceGenerator::GenerateStubMethods(io::Printer* printer)
{
    for (int i = 0; i < descriptor_->method_count(); i++)
    {
        const MethodDescriptor* method = descriptor_->method(i);
        std::map<string, string> sub_vars;
        sub_vars["classname"]   = descriptor_->name();
        sub_vars["name"]        = method->name();
        sub_vars["index"]       = SimpleItoa(i);
        sub_vars["input_type"]  = ClassName(method->input_type(), true);
        sub_vars["output_type"] = ClassName(method->output_type(), true);

        if (IsInputStreamPresent(*method))
        {
            if (method->input_type()->field_count())
            {
                printer->Print(sub_vars,
                               "rpc::Future<$output_type$> $classname$_Stub::$name$(const $input_type$& request, \n"
                               "                                                    const rpc::IStream& stream) {\n"
                               "    return rpc::Future<$output_type$>(channel_->CallMethod(*descriptor().method($index$), \n"
                               "                                                           boost::make_shared<rpc::StreamRequest<$input_type$>>(stream, request), \n"
                               "                                                           stream));\n"
                               "}\n");
            }
            else
            {
                printer->Print(sub_vars,
                               "rpc::Future<$output_type$> $classname$_Stub::$name$(const rpc::IStream& stream) {\n"
                                   "   return rpc::Future<$output_type$>(channel_->CallMethod(*descriptor().method($index$), \n"
                                   "                                                          boost::make_shared<rpc::StreamRequest<$input_type$>>(stream), \n"
                                   "                                                          stream));\n"
                                   "}\n");
            }
        }
        else
        {
            if (method->input_type()->field_count())
            {
                printer->Print(sub_vars,
                               "rpc::Future<$output_type$> $classname$_Stub::$name$(const $input_type$& request) {\n"
                               "    return rpc::Future<$output_type$>(channel_->CallMethod(*descriptor().method($index$), \n"
                               "                                      boost::make_shared<rpc::Request<$input_type$>>(request), \n"
                               "                                      rpc::IStream()));\n"
                               "}\n");
            }
            else
            {
                printer->Print(sub_vars, "rpc::Future<$output_type$> $classname$_Stub::$name$() {\n"
                    "   return rpc::Future<$output_type$>(channel_->CallMethod(*descriptor().method($index$), \n"
                    "                                                          boost::make_shared<rpc::Request<$input_type$>>(), \n"
                    "                                                          rpc::IStream()));\n"
                    "}\n");
            }
        }
    }
}


enum MethodStreamType
{
    No = 0, In = 1, Out = 2, InOut = 3
};

static const int STREAM_FIELD = 60002;

bool ServiceGenerator::IsInputStreamPresent(const MethodDescriptor& method)
{
    for (int i = 0; i < method.options().unknown_fields().field_count(); ++i)
    {
        const auto& field = method.options().unknown_fields().field(i);
        if (field.number() == STREAM_FIELD && field.type() == 0)
            return static_cast<MethodStreamType>(field.varint()) == In ||
                   static_cast<MethodStreamType>(field.varint()) == InOut;
    }
    return false;
}

bool ServiceGenerator::IsOutStreamPresent(const MethodDescriptor& method)
{
    for (int i = 0; i < method.options().unknown_fields().field_count(); ++i)
    {
        const auto& field = method.options().unknown_fields().field(i);
        if (field.number() == STREAM_FIELD && field.type() == 0)
            return static_cast<MethodStreamType>(field.varint()) == Out ||
                   static_cast<MethodStreamType>(field.varint()) == InOut;
    }
    return false;
}

std::string ServiceGenerator::GetRequestWrapper(const MethodDescriptor& method)
{
    return IsInputStreamPresent(method) ? "StreamRequest" : "Request";
}

std::string ServiceGenerator::GetResponseWrapper(const MethodDescriptor& method)
{
    return IsOutStreamPresent(method) ? "StreamResponse" : "Response";
}

std::string ServiceGenerator::GetMethodSignature(const MethodDescriptor& method, const std::string& inType)
{
    if (IsInputStreamPresent(method))
    {
        if (method.input_type()->field_count())
            return std::string("const ") + inType + "& request, const rpc::IStream& stream";
        else
            return "const rpc::IStream& stream";
    }
    else
    {
        if (method.input_type()->field_count())
            return std::string("const ") + inType + "& request";
        else
            return "";
    }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
