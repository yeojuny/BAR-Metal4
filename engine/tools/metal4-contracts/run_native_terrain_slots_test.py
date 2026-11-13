#!/usr/bin/env python3
"""Host-test actual snapshot and GPU value readers, with SIMD storage stubs only."""
import argparse, subprocess, pathlib, os
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
r=pathlib.Path(__file__).resolve().parents[2]
h=(r/'rts/Rendering/Backend/NativeWaterGroundSourceData.h').read_text()
s=(r/'rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm').read_text()
unit='''#include <bit>
#include <algorithm>
#include <cstring>
#include <random>
#include <array>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <cassert>
#include <iostream>
#include <limits>
using simd_float4=std::array<float,4>;using simd_int4=std::array<int32_t,4>;
struct simd_float4x4 {std::array<simd_float4,4> columns;};
void Require(bool c,const std::string& m){if(!c)throw std::runtime_error(m);}
'''+h[h.index('struct NativeTerrainUniformValue'):h.index('// A draw retains')]+s[s.index('void RequireNamedTerrainInput'):s.index('simd_float4 Vec(')]+'''
int main(){
 using S=NativeTerrainUniformSlot;unsigned checks=0;
 auto check=[&](bool x){++checks;assert(x);};
 for(size_t i=1;i<NativeTerrainUniformNames.size();++i)check(std::string(NativeTerrainUniformNames[i-1])<NativeTerrainUniformNames[i]);
 NativeTerrainUniformSnapshot a;
 for(size_t i=0;i<NativeTerrainUniformNames.size();++i){
  NativeTerrainUniformValue v;v.values=std::vector<float>{float(i)};
  a.values.emplace(NativeTerrainUniformNames[i],v);
 }
 a.values["unrelated"].values=std::vector<float>{99.f};
 a.SealSlots();
 for(size_t i=0;i<NativeTerrainUniformNames.size();++i){
  auto slot=S(i);check(a.Find(slot)==&a.values.at(NativeTerrainUniformNames[i]));
  check(Float(a,slot,1)[0]==float(i));
 }
 NativeTerrainUniformSnapshot b=a;
 b.values["borderMinHeight"].values=std::vector<float>{-0.f};b.SealSlots();
 check(b.Find(S::borderMinHeight)!=a.Find(S::borderMinHeight));
 check(std::signbit(Float(b,S::borderMinHeight,1)[0]));
 a=NativeTerrainUniformSnapshot{};check(Float(b,S::cameraPos,1)[0]==2.f);
 NativeTerrainUniformSnapshot c;c=b;c.SealSlots();
 check(c.Find(S::cameraPos)!=b.Find(S::cameraPos));
 auto fails=[&](auto fn,const char* expected){try{fn();check(false);}catch(const std::runtime_error& e){check(std::string(e.what()).find(expected)!=std::string::npos);}};
 c.InvalidateSlots();c.values.erase("cameraPos");c.SealSlots();
 fails([&]{Float(c,S::cameraPos,3);},"unobserved");
 c.InvalidateSlots();auto& v=c.values["cameraPos"];v.rows=2;v.values=std::vector<float>{1,2};c.SealSlots();
 fails([&]{Float(c,S::cameraPos,3);},"shape differs");
 c.InvalidateSlots();v.rows=3;v.values=std::vector<int32_t>{1,2,3};c.SealSlots();
 fails([&]{Float(c,S::cameraPos,3);},"type differs");
 check(Int(c,S::cameraPos,3)[2]==3);
 c.InvalidateSlots();v.values=std::vector<float>{1,2,std::numeric_limits<float>::quiet_NaN()};c.SealSlots();
 fails([&]{Float(c,S::cameraPos,3);},"nonfinite");
 c.InvalidateSlots();v.transpose=true;c.SealSlots();
 fails([&]{Float(c,S::cameraPos,3);},"type differs");
 c.InvalidateSlots();auto& m=c.values["shadowMat"];m.rows=m.columns=4;m.values=std::vector<float>(16);m.transpose=true;
 auto& data=std::get<std::vector<float>>(m.values);for(unsigned i=0;i<16;++i)data[i]=i;c.SealSlots();
 auto matrix=Matrix(c,S::shadowMat);for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)check(matrix.columns[col][row]==float(row*4+col));
 check(c.Find(S::Count)==nullptr);
 // Unsealed copies take the unchanged validating reader path. Compare every
 // packed byte, including padding components and signed zero, against it.
 std::mt19937 rng(1234);
 for(unsigned trial=0;trial<2000;++trial){
  NativeTerrainUniformSnapshot packed;auto& f=packed.values["cameraPos"];
  f.rows=1+rng()%4;std::vector<float> vf(f.rows);
  for(auto& x:vf){uint32_t bits=rng();if((bits&0x7f800000u)==0x7f800000u)bits=0x80000000u;std::memcpy(&x,&bits,4);}
  f.values=vf;auto& iv=packed.values["texSquare"];iv.rows=1+rng()%4;
  std::vector<int32_t> vi(iv.rows);for(auto& x:vi)x=int32_t(rng());iv.values=vi;
  auto& mv=packed.values["shadowMat"];mv.rows=mv.columns=4;mv.transpose=bool(rng()%2);
  std::vector<float> vm(16);for(auto& x:vm)x=float(int(rng()%200)-100);vm[0]=-0.f;mv.values=vm;
  NativeTerrainUniformSnapshot reference=packed;packed.SealSlots();
  check(packed.PackedFloat(S::cameraPos,f.rows)!=nullptr);
  check(packed.PackedInt(S::texSquare,iv.rows)!=nullptr);
  check(packed.PackedMatrix(S::shadowMat)!=nullptr);
  const auto pf=Float(packed,S::cameraPos,f.rows),rf=Float(reference,S::cameraPos,f.rows);
  const auto pi=Int(packed,S::texSquare,iv.rows),ri=Int(reference,S::texSquare,iv.rows);
  const auto pm=Matrix(packed,S::shadowMat),rm=Matrix(reference,S::shadowMat);
  check(!std::memcmp(&pf,&rf,sizeof pf));check(!std::memcmp(&pi,&ri,sizeof pi));check(!std::memcmp(&pm,&rm,sizeof pm));
  NativeTerrainUniformSnapshot next=packed;
  next.values["texSquare"].values=std::vector<int32_t>(iv.rows,-17);
  next.ReuseTypedBaseAfterSingleWrite(packed,"texSquare");next.SealSlots();
  const auto nf=Float(next,S::cameraPos,f.rows);const auto nm=Matrix(next,S::shadowMat);
  check(!std::memcmp(&nf,&pf,sizeof pf));check(!std::memcmp(&nm,&pm,sizeof pm));
  check(Int(next,S::texSquare,iv.rows)[0]==-17);
  check(Int(packed,S::texSquare,iv.rows)[0]==vi[0]);
  next.InvalidateWrittenSlot("cameraPos");next.values["cameraPos"].values=std::vector<float>(f.rows,-0.f);next.SealSlots();
  check(std::signbit(Float(next,S::cameraPos,f.rows)[0]));
  next.InvalidateWrittenSlot("shadowMat");std::get<std::vector<float>>(next.values["shadowMat"].values)[0]=std::numeric_limits<float>::infinity();
  next.SealSlots();check(!next.PackedMatrix(S::shadowMat));fails([&]{Matrix(next,S::shadowMat);},"nonfinite");
  packed.InvalidateSlots();check(!packed.PackedFloat(S::cameraPos,f.rows));check(!packed.PackedMatrix(S::shadowMat));
  f.values=std::vector<float>(f.rows,std::numeric_limits<float>::infinity());packed.SealSlots();
  check(!packed.PackedFloat(S::cameraPos,f.rows));fails([&]{Float(packed,S::cameraPos,f.rows);},"nonfinite");
  NativeTerrainUniformSnapshot assigned;assigned=packed;check(!assigned.PackedInt(S::texSquare,iv.rows));assigned.SealSlots();check(assigned.PackedInt(S::texSquare,iv.rows));
 }
 check(sizeof(NativeTerrainTypedVector)==20);
 std::cout<<"typed vector bytes="<<sizeof(NativeTerrainTypedVector)<<" snapshot bytes="<<sizeof(NativeTerrainUniformSnapshot)<<"\\n";

 std::cout<<"PASS "<<checks<<" actual terrain slot/value-reader checks; Metal execution not tested\\n";
}
'''
f=a.output/'test.cpp';f.write_text(unit)
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined',str(f),'-o',str(a.output/'test')]
subprocess.run(cmd,check=True)
subprocess.run([str(a.output/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
