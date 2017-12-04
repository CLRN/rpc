#ifndef ServiceGenerator_h__
#define ServiceGenerator_h__

#include <map>
#include <string>
#include <google/protobuf/stubs/common.h>
//#include <google/protobuf/compiler/cpp/cpp_options.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
  namespace io {
    class Printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace cpp {

class ServiceGenerator {
 public:
  // See generator.cc for the meaning of dllexport_decl.
  explicit ServiceGenerator(const ServiceDescriptor* descriptor);
  ~ServiceGenerator();

  // Header stuff.

  // Generate the class definitions for the service's interface and the
  // stub implementation.
  void GenerateDeclarations(io::Printer* printer);

  // Source file stuff.

  // Generate code that initializes the global variable storing the service's
  // descriptor.
  void GenerateDescriptorInitializer(io::Printer* printer, int index);

  // Generate implementations of everything declared by GenerateDeclarations().
  void GenerateImplementation(io::Printer* printer);

 private:
  enum RequestOrResponse { REQUEST, RESPONSE };
  enum VirtualOrNon { VIRTUAL, NON_VIRTUAL };

  // Header stuff.

  // Generate the service abstract interface.
  void GenerateInterface(io::Printer* printer);

  // Generate the stub class definition.
  void GenerateStubDefinition(io::Printer* printer);

  // Prints signatures for all methods in the
  void GenerateMethodSignatures(VirtualOrNon virtual_or_non,
                                io::Printer* printer);

  // Source file stuff.

  // Generate the default implementations of the service methods, which
  // produce a "not implemented" error.
  void GenerateNotImplementedMethods(io::Printer* printer);

  // Generate the CallMethod() method of the service.
  void GenerateCallMethod(io::Printer* printer);

  // Generate the Get{Request,Response}Prototype() methods.
  void GenerateGetPrototype(RequestOrResponse which, io::Printer* printer);

  // Generate the stub's implementations of the service methods.
  void GenerateStubMethods(io::Printer* printer);

  // Test if method has input stream
  bool IsInputStreamPresent(const MethodDescriptor& method);

  // Test if method has output stream
  bool IsOutStreamPresent(const MethodDescriptor& method);

  // Get request wrapper type
  std::string GetRequestWrapper(const MethodDescriptor& method);

  // Get response wrapper type
  std::string GetResponseWrapper(const MethodDescriptor& method);

  // Get method signature
  std::string GetMethodSignature(const MethodDescriptor& method, const std::string& inType);

  const ServiceDescriptor* descriptor_;
  std::map<string, string> vars_;

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ServiceGenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif // ServiceGenerator_h__
