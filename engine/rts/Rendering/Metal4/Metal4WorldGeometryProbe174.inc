// Bounded observations of this Prepare invocation. No source or GPU objects are retained.
struct WorldGeometryProbe174 {
 using Clock=std::chrono::steady_clock;
 using Bytes=unsigned long long;
 enum Family {Fill,Font,LinesPacket,Triangle,Line,Point,Grass,Spots,Spotlight,Reclaim,Sensor,Effect,Glass,Distortion,Water,Los,Terraform,Hull,Facing,Splat,Shadow,Count};
 static Family Classify(const NativeWorldDrawCommand& c) {
  if(c.unitGlass)return Glass;
  if(c.unitEffect)return Effect;
  if(c.distortion)return Distortion;
  if(c.objectSpotlight)return Spotlight;
  if(c.grass)return Grass;
  if(c.resourceSpots)return Spots;
  if(c.sensorLOS)return Sensor;
  if(c.reclaimField)return Reclaim;
  if(c.sourceLines)return LinesPacket;
  if(c.waterOverlay)return Water;
  if(c.losView)return Los;
  if(c.terraformOverlay)return Terraform;
  if(c.squadHull)return Hull;
  if(c.easyFacing)return Facing;
  if(c.surface.splatPainter)return Splat;
  if(c.originalShadowInput)return Shadow;
  if(c.sourcePrimitive==NativeDrawPrimitive::GPUFillTriangles)return Fill;
  if(c.sourcePrimitive==NativeDrawPrimitive::GPUFontTriangles)return Font;
  return c.primitive==NativeDrawPrimitive::Lines?Line:c.primitive==NativeDrawPrimitive::Points?Point:Triangle;
 }
 struct Row {
  Bytes selected=0,first=0,last=0,vertices=0,indices=0;
  Bytes typed=0,borrow=0,rebuild=0,hit=0,exact=0,sourceLines=0,meshV=0,meshI=0;
  Bytes uploadV=0,uploadI=0,packet=0,uniforms=0,disks=0,directAllocs=0,privateMesh=0,privateAllocs=0;
  Bytes sharedPacked=0,validationNS=0,meshNS=0;
 };
 std::array<Row,Count> rows{};
 Bytes matrixBufferRequest=0,matrixNewBufferBytes=0;
 Clock::time_point began=Clock::now();
 static Bytes Since(Clock::time_point t){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-t).count();}
 Row& Select(const NativeWorldDrawCommand& c){
  const auto f=Classify(c);auto& r=rows[f];if(!r.selected)r.first=c.sequence;
  r.last=c.sequence;++r.selected;r.vertices+=c.vertices.size();r.indices+=c.indices.size();
  if(f==Grass||f==Spots||f==Spotlight||f==Reclaim||f==Sensor||f==Effect||f==Glass||f==Distortion)++r.typed;
  return r;
 }
 void Report(const NativeWorldDrawSelection167& frame,std::uint32_t samples)const {
  static constexpr const char* names[Count]={"matrix_fill","matrix_font","source_lines","triangles","lines","points","grass","resources","spotlight","reclaim","sensor","unit_effect","glass","distortion","water","los","terraform","squad_hull","easy_facing","splat","shadow"};
  LOG("[NativeWorldPrepare174] frame=%llu serial=%llu selected=%zu samples=%u prepare_ns=%llu matrix_buffer_request=%llu matrix_new_buffer_bytes=%llu",
      (Bytes)frame.SourceDrawFrame(),(Bytes)frame.Serial(),frame.size(),samples,Since(began),matrixBufferRequest,matrixNewBufferBytes);
  for(unsigned f=0;f<Count;++f){const auto& r=rows[f];if(!r.selected)continue;
   LOG("[NativeWorldMesh174] frame=%llu serial=%llu family=%s selected=%llu seq_first=%llu seq_last=%llu src_v=%llu src_i=%llu dedicated=%llu borrow=%llu rebuild=%llu cache_hit=%llu exact=%llu source_lines=%llu mesh_v=%llu mesh_i=%llu uploaded_v_bytes=%llu uploaded_i_bytes=%llu packet_payload_bytes=%llu uniform_bytes=%llu disk_bytes=%llu direct_allocs=%llu private_mesh_bytes=%llu private_allocs=%llu packed_draws=%llu validate_ns=%llu mesh_ns=%llu",
    (Bytes)frame.SourceDrawFrame(),(Bytes)frame.Serial(),names[f],r.selected,r.first,r.last,r.vertices,r.indices,r.typed,r.borrow,r.rebuild,r.hit,r.exact,r.sourceLines,r.meshV,r.meshI,r.uploadV,r.uploadI,r.packet,r.uniforms,r.disks,r.directAllocs,r.privateMesh,r.privateAllocs,r.sharedPacked,r.validationNS,r.meshNS);
  }
 }
};
