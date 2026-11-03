// Included in the family-local anonymous namespace. Product exact-world kernel
// and packet format are shared; only the Placement renderer's resource binding
// is new. Pool token is retained by both the prepared draw and RuntimeLease.
struct PlacementLineProbe125 {
 const char* path=std::getenv("BAR_NATIVE_PLACEMENT_LINE_PROBE_FILE");
 std::atomic<uint64_t> draws{0},vertices{0},bytes{0},cpu{0};
 bool Enabled()const{return path&&*path;}
 ~PlacementLineProbe125(){if(!Enabled())return;if(auto f=std::fopen(path,"w")){std::fprintf(f,"gpuDraws,gpuVertices,sharedPacketBytes,cpuDraws\n%llu,%llu,%llu,%llu\n",(unsigned long long)draws.load(),(unsigned long long)vertices.load(),(unsigned long long)bytes.load(),(unsigned long long)cpu.load());std::fclose(f);}}
};
PlacementLineProbe125& PlacementProbe125(){static PlacementLineProbe125 p;return p;}
struct PlacementLineJob125 {
 __strong id<MTLComputePipelineState> pipeline=nil;
 __strong id<MTL4ArgumentTable> arguments=nil;
 __strong id<MTLBuffer> vertices=nil,indices=nil;
 __strong M4PlacementLineLifetime125* lifetime=nil;
 uint32_t quads=0;bool encoded=false;
};
struct PlacementLines125 {
 ScreenUploadPool uploads;
 __strong id<MTLLibrary> library=nil;
 __strong id<MTLComputePipelineState> pipeline=nil;
 bool validation=false;
 void Compile(RuntimeLease& lease,bool validate,double& ms){
  if(pipeline&&validation==validate)return;
  const auto start=Clock::now();NSError* error=nil;auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;
  auto lib=lease.NewLibraryWithSource(String(ExactWorldLineShader),options,&error);Require(lib,"Cannot compile exact placement line arithmetic: "+Error(error));
  auto function=[MTL4LibraryFunctionDescriptor new];function.library=lib;function.name=@"ExactWorldLineGeometry";
  auto descriptor=[MTL4ComputePipelineDescriptor new];descriptor.computeFunctionDescriptor=function;descriptor.options=[MTL4PipelineOptions new];descriptor.options.shaderValidation=ShaderValidationMode(validate);
  auto p=[lease.Compiler() newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];Require(p,"Cannot compile exact placement lines: "+Error(error));library=lib;pipeline=p;validation=validate;ms+=Milliseconds(start);
 }
 std::shared_ptr<PlacementLineJob125> Prepare(RuntimeLease& lease,const exactWorldLine::Packet& packet,bool validate,double& ms){
  Compile(lease,validate,ms);lease.Retain(library);lease.Retain(pipeline);
  // BeginBatch selects the device and rewinds the search; live owner tokens
  // still forbid reuse, including earlier draws in this submission.
  uploads.BeginBatch(lease.Device());
  const auto align=[](size_t n){return (n+255)&~size_t(255);};
  const size_t spanOffset=align(packet.segments.size()*sizeof(exactWorldLine::Line120Segment)),countOffset=align(spanOffset+packet.spans.size()*sizeof(exactWorldLine::Line120Span)),length=countOffset+sizeof(uint32_t);
  auto job=std::make_shared<PlacementLineJob125>();job->pipeline=pipeline;job->quads=packet.quads;job->lifetime=[M4PlacementLineLifetime125 new];lease.Retain(job->lifetime);
  job->lifetime->storage=uploads.Acquire(lease.Device(),length);auto input=job->lifetime->storage?job->lifetime->storage->buffer:[lease.Device()newBufferWithLength:length options:MTLResourceStorageModeShared];Require(input&&input.contents,"Cannot retain immutable placement line input");
  std::memcpy(input.contents,packet.segments.data(),packet.segments.size()*sizeof(exactWorldLine::Line120Segment));std::memcpy(static_cast<uint8_t*>(input.contents)+spanOffset,packet.spans.data(),packet.spans.size()*sizeof(exactWorldLine::Line120Span));const uint32_t count=packet.spans.size();std::memcpy(static_cast<uint8_t*>(input.contents)+countOffset,&count,4);lease.AddAllocation(input);
  job->vertices=[lease.Device()newBufferWithLength:size_t(packet.quads)*4*sizeof(NativeWorldDrawVertex) options:MTLResourceStorageModePrivate];job->indices=[lease.Device()newBufferWithLength:size_t(packet.quads)*6*sizeof(uint32_t) options:MTLResourceStorageModePrivate];Require(job->vertices&&job->indices,"Cannot allocate exact placement GPU output");lease.AddAllocation(job->vertices);lease.AddAllocation(job->indices);
  auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=5;desc.initializeBindings=YES;NSError* error=nil;job->arguments=[lease.Device()newArgumentTableWithDescriptor:desc error:&error];Require(job->arguments,"Cannot allocate placement line arguments: "+Error(error));lease.Retain(job->arguments);
  [job->arguments setAddress:input.gpuAddress atIndex:0];[job->arguments setAddress:input.gpuAddress+spanOffset atIndex:1];[job->arguments setAddress:job->vertices.gpuAddress atIndex:2];[job->arguments setAddress:input.gpuAddress+countOffset atIndex:3];[job->arguments setAddress:job->indices.gpuAddress atIndex:4];
  if(PlacementProbe125().Enabled()){auto& p=PlacementProbe125();p.draws.fetch_add(1,std::memory_order_relaxed);p.vertices.fetch_add(uint64_t(packet.quads)*4,std::memory_order_relaxed);p.bytes.fetch_add(length,std::memory_order_relaxed);}return job;
 }
};
