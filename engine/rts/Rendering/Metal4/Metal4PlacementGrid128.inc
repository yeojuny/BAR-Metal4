// Source GPU Building Grid. One uniform buffer and one ordered instanced draw.
struct PlacementGridProbe128 {
 const char* path=std::getenv("BAR_NATIVE_PLACEMENT_GRID_PROBE_FILE");std::atomic<uint64_t> gpu{0},fallback{0},bytes{0};unsigned seen=0;
 bool Enabled()const{return path&&*path;}
 void Example(const NativeBuildingGridDraw& d,const PlacementGridHeight128&,const std::array<double,4>& vp,unsigned width,unsigned height,unsigned samples,bool admitted){
  if(!Enabled())return;const unsigned bit=1u<<(unsigned(samples>1)*2+unsigned(admitted));if(seen&bit)return;seen|=bit;
  const auto name=std::string(path)+"."+std::to_string(samples)+(admitted?".gpu.json":".fallback.json");if(auto f=std::fopen(name.c_str(),"w")){std::fprintf(f,"{\"samples\":%u,\"width\":%u,\"height\":%u,\"vertexCount\":%u,\"lineWidth\":%.9g,\"viewport\":[%.17g,%.17g,%.17g,%.17g],\"mouse\":[%.9g,%.9g,%.9g],\"matrix\":[",samples,width,height,d.vertexCount,d.world.lineWidth,vp[0],vp[1],vp[2],vp[3],d.mousePosition[0],d.mousePosition[1],d.mousePosition[2]);for(unsigned i=0;i<16;++i)std::fprintf(f,"%s%.9g",i?",":"",d.view.viewProjection[i]);std::fprintf(f,"]}\n");std::fclose(f);}
 }
 ~PlacementGridProbe128(){if(Enabled())if(auto f=std::fopen(path,"w")){std::fprintf(f,"gpuDraws,cpuFallbackDraws,metadataBytes\n%llu,%llu,%llu\n",(unsigned long long)gpu.load(),(unsigned long long)fallback.load(),(unsigned long long)bytes.load());std::fclose(f);}}
};
PlacementGridProbe128& GridProbe128(){static PlacementGridProbe128 p;return p;}
struct PlacementGridJob128 {
 __strong id<MTL4ArgumentTable> vertexArguments=nil;
 __strong id<MTLBuffer> input=nil;
 __strong M4PlacementLineLifetime125* lifetime=nil;
 std::shared_ptr<const WorldTerrainHeights> heightOwner;
 uint32_t segments=0;bool encoded=false;
 void DidComplete()const {Require(encoded,"Building Grid completion precedes drawing");}
};
struct PlacementGrid128 {
 ScreenUploadPool uploads;__strong id<MTLLibrary> library=nil;bool validation=false;
 void Compile(RuntimeLease& lease,bool validate,double& ms){
  if(library&&validation==validate)return;const auto start=Clock::now();NSError* error=nil;auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;
  // Retain the original fragment's Safe/defaultFast compilation options.
  auto lib=lease.NewLibraryWithSource(String(std::string(NativePlacementShaderSource)+PlacementGridShader128),options,&error);Require(lib,"Cannot compile source Building Grid: "+Error(error));library=lib;validation=validate;ms+=Milliseconds(start);
 }
 std::shared_ptr<PlacementGridJob128> Prepare(RuntimeLease& lease,const NativeBuildingGridDraw& source,const PlacementGridHeight128& heights,const std::array<double,4>& vp,uint32_t width,uint32_t height,unsigned samples,bool validate,double& ms){
  if(!heights.texture||heights.owner!=source.heights||heights.texture.device!=lease.Device()||heights.texture.pixelFormat!=MTLPixelFormatR32Float||heights.texture.width!=source.heights->width||heights.texture.height!=source.heights->height)return {};
  auto u=placementGrid128::Admit(source,vp,width,height,samples);GridProbe128().Example(source,heights,vp,width,height,samples,bool(u));if(!u)return {};
  Compile(lease,validate,ms);lease.Retain(library);lease.AddAllocation(heights.texture);
  uploads.BeginBatch(lease.Device());auto job=std::make_shared<PlacementGridJob128>();job->lifetime=[M4PlacementLineLifetime125 new];lease.Retain(job->lifetime);job->lifetime->storage=uploads.Acquire(lease.Device(),256);job->input=job->lifetime->storage?job->lifetime->storage->buffer:[lease.Device()newBufferWithLength:256 options:MTLResourceStorageModeShared];Require(job->input&&job->input.contents,"Cannot retain immutable Building Grid inputs");std::memcpy(job->input.contents,&*u,sizeof(*u));lease.AddAllocation(job->input);
  job->segments=source.vertexCount/2;job->heightOwner=heights.owner;
  auto d=[MTL4ArgumentTableDescriptor new];d.maxBufferBindCount=2;d.maxTextureBindCount=1;d.initializeBindings=YES;NSError* error=nil;job->vertexArguments=[lease.Device()newArgumentTableWithDescriptor:d error:&error];Require(job->vertexArguments,"Cannot allocate Building Grid arguments: "+Error(error));[job->vertexArguments setAddress:job->input.gpuAddress atIndex:0];[job->vertexArguments setTexture:heights.texture.gpuResourceID atIndex:0];lease.Retain(job->vertexArguments);
  if(GridProbe128().Enabled()){GridProbe128().gpu.fetch_add(1,std::memory_order_relaxed);GridProbe128().bytes.fetch_add(256,std::memory_order_relaxed);}return job;
 }
};
