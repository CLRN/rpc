
#include <google/protobuf/compiler/plugin.h>
#include "RpcGenerator.h"

//#define DEBUG_WITH_COMPILER

#ifdef DEBUG_WITH_COMPILER
#include <windows.h>
#endif // DEBUG_WITH_COMPILER

int main(int argc, char* argv[])
{
#ifdef DEBUG_WITH_COMPILER
#pragma comment(lib, "user32.lib")
    MessageBoxW(NULL, L"Waiting debugger ...", L"Debug", MB_ICONQUESTION|MB_OK|MB_DEFBUTTON1);
#endif
    clrn::rpc::compiler::RpcGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
