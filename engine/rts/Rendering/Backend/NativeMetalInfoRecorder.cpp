/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeMetalInfoRecorder.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
std::atomic<bool> gpuReady{false},maintenanceReady{false};
std::atomic<std::uint64_t> nextMap{1};
void Check(bool valid,const char* text){if(!valid)throw std::invalid_argument(text);}
size_t Count(std::uint32_t x,std::uint32_t z){Check(x&&z&&size_t(x)<=std::numeric_limits<size_t>::max()/z,"Native metal source extent overflow/empty");return size_t(x)*z;}
}
void SetNativeMetalInfoGPUReady(bool value) noexcept{gpuReady.store(value);}
void SetNativeMetalInfoMaintenanceReady(bool value) noexcept{maintenanceReady.store(value);}
bool IsNativeMetalInfoConsumerAvailable() noexcept{return gpuReady.load()&&maintenanceReady.load();}
void ValidateNativeMetalDensity(const NativeMetalDensity& d){Check(d.values.size()==Count(d.width,d.height),"Native metal density byte extent differs");}
void ValidateNativeMetalExtraction(const NativeMetalExtraction& e){Check(e.depth.size()==Count(e.width,e.height)&&e.los.size()==Count(e.losWidth,e.losHeight),"Native metal extraction/ground LOS extent differs");for(float value:e.depth)Check(std::isfinite(value),"Native metal extraction is nonfinite");}
void ValidateNativeMetalInfoFrame(const NativeMetalInfoFrame& f){Check((!f.active||(f.modeGeneration&&f.serial))&&f.mapIdentity&&f.squaresX&&f.squaresZ&&f.paddedSquaresX>=f.squaresX&&f.paddedSquaresZ>=f.squaresZ&&std::has_single_bit(f.paddedSquaresX)&&std::has_single_bit(f.paddedSquaresZ),"Native metal map/padded extents differ");std::uint64_t previous=0;for(const auto& u:f.updates){Check(u.firstSerial>previous&&(!previous||u.firstSerial==previous+1)&&(!u.clear||!previous)&&u.lastSerial>=u.firstSerial&&u.lastSerial<=f.serial&&std::isfinite(u.brightness),"Native metal update order/brightness differs");Check(u.clear?(u.repetitions==0&&u.firstSerial==u.lastSerial):(u.repetitions&&u.lastSerial-u.firstSerial+1==u.repetitions),"Native metal repeated update count differs");if(!u.clear){Check(u.density&&u.extraction,"Native metal update lacks actual original inputs");ValidateNativeMetalDensity(*u.density);ValidateNativeMetalExtraction(*u.extraction);Check(std::uint64_t(u.density->width)*2==f.squaresX&&std::uint64_t(u.density->height)*2==f.squaresZ&&u.extraction->width==u.density->width&&u.extraction->height==u.density->height,"Native metal half-resolution source schema differs");}previous=u.lastSerial;}Check(f.updates.empty()||previous==f.serial,"Native metal queued history does not reach its declared completion serial");}
NativeMetalInfoRecorder::NativeMetalInfoRecorder(){Reset();}
void NativeMetalInfoRecorder::Reset(){auto identity=nextMap.load();for(;;){if(identity==0||identity==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native metal map identity exhausted");if(nextMap.compare_exchange_weak(identity,identity+1))break;}frame={};frame.mapIdentity=identity;priorDensity.reset();priorExtraction.reset();firstUpdate=true;extractionUpdateCounter=0;brightness=1;}
std::uint64_t NativeMetalInfoRecorder::NextSerial(){if(frame.serial==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native metal update serial exhausted");return ++frame.serial;}
void NativeMetalInfoRecorder::SetMode(bool active,float value,std::uint32_t x,std::uint32_t z,std::uint32_t px,std::uint32_t pz){Check(std::isfinite(value),"Native metal brightness is nonfinite");Check(x&&z&&px>=x&&pz>=z&&std::has_single_bit(px)&&std::has_single_bit(pz),"Native metal selected map bounds differ");if(frame.modeGeneration==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native metal mode generation exhausted");auto candidate=frame;candidate.squaresX=x;candidate.squaresZ=z;candidate.paddedSquaresX=px;candidate.paddedSquaresZ=pz;candidate.active=active;++candidate.modeGeneration;candidate.updates.clear();if(active){if(candidate.serial==std::numeric_limits<std::uint64_t>::max())throw std::overflow_error("Native metal update serial exhausted");const auto serial=++candidate.serial;candidate.updates.push_back({serial,serial,0,true,value,{},{}});}frame=std::move(candidate);brightness=value;}
bool NativeMetalInfoRecorder::NeedsExtractionUpdate()const{return firstUpdate||extractionUpdateCounter%30==0;}
void NativeMetalInfoRecorder::Update(std::shared_ptr<const NativeMetalDensity> density,std::shared_ptr<const NativeMetalExtraction> extraction){Check(bool(density),"Native metal density unavailable");ValidateNativeMetalDensity(*density);const bool update=NeedsExtractionUpdate();if(update){Check(bool(extraction),"Native metal extraction cadence source unavailable");ValidateNativeMetalExtraction(*extraction);}else Check(bool(priorExtraction),"Native metal prior extraction unavailable");
	// The legacy sorted info map updates its combiner before metal/metalextraction.
	// Initialize first valid auxiliary data explicitly; never expose uninitialized GPU memory.
	const auto& drawDensity=priorDensity?priorDensity:density;const auto& drawExtraction=priorExtraction?priorExtraction:extraction;
	if(frame.active){NativeMetalInfoFrame check=frame;check.updates={{1,1,1,false,brightness,drawDensity,drawExtraction}};check.serial=1;ValidateNativeMetalInfoFrame(check);const auto serial=NextSerial();if(!frame.updates.empty()){auto& last=frame.updates.back();if(!last.clear&&last.density==drawDensity&&last.extraction==drawExtraction&&last.brightness==brightness&&last.repetitions<std::numeric_limits<std::uint32_t>::max()){last.lastSerial=serial;++last.repetitions;}else frame.updates.push_back({serial,serial,1,false,brightness,drawDensity,drawExtraction});}else frame.updates.push_back({serial,serial,1,false,brightness,drawDensity,drawExtraction});}
	priorDensity=std::move(density);if(update)priorExtraction=std::move(extraction);if(firstUpdate)firstUpdate=false;else extractionUpdateCounter=(extractionUpdateCounter+1)%30;
}
std::shared_ptr<const NativeMetalInfoFrame> NativeMetalInfoRecorder::Capture()const{return std::make_shared<const NativeMetalInfoFrame>(frame);}
void NativeMetalInfoRecorder::Retire(std::uint64_t serial){Check(serial==frame.serial,"Native metal completion must acknowledge the entire captured transaction");frame.updates.clear();}
}
