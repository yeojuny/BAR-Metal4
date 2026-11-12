// Separate intervention: portable policy checks, not a GPU correctness test.
#include "Rendering/Metal4/Metal4Pass160Policy.h"
#include "Rendering/Metal4/Metal4AlphaPass166Policy.h"
#include "Rendering/Metal4/Metal4ModelPhaseExecution165.h"
#include <cassert>
#include <string>
using namespace rendering::metal4;
int main(int argc,char** argv) {
 const bool expected=argc==2&&std::string(argv[1])=="1";
 assert(EmptyModelPassFusionInterventionEnabled()==expected);
 assert(ModelPass160Enabled()==expected);
 assert(ModelPass165Enabled()==expected);
 assert(ModelPass166Enabled()==expected);
 ModelPhaseExecution165 unknown;
 assert(!unknown.HasOnlyCPULifecycle());
 ModelPhaseExecution165 cpu{SourceDependencyWork160::NoCommands,ModelPhaseBody165::CPULifecycleOnly};
 assert(cpu.HasOnlyCPULifecycle());
 cpu.dependencies=SourceDependencyWork160::CommandsRequired;
 assert(!cpu.HasOnlyCPULifecycle());
 AlphaCallbackExecution166 alpha{SourceDependencyWork160::NoCommands,AlphaCallbackBody166::CPULifecycleOnly,true,true};
 assert(alpha.HasOnlyCPULifecycle());
 alpha.noResourceReads=false; assert(!alpha.HasOnlyCPULifecycle());
 alpha.noResourceReads=true; alpha.preservesEncoderState=false; assert(!alpha.HasOnlyCPULifecycle());
 alpha.preservesEncoderState=true; alpha.dependencies=SourceDependencyWork160::Unknown; assert(!alpha.HasOnlyCPULifecycle());
 alpha.dependencies=SourceDependencyWork160::NoCommands; alpha.body=AlphaCallbackBody166::OrderedCommands; assert(!alpha.HasOnlyCPULifecycle());
}
