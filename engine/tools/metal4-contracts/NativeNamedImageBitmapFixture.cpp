/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Actual original CBitmap/DevIL and pool; file lookup alone uses verified extracts.
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "Rendering/Textures/Bitmap.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "lib/tracy/profiler/src/stb_image.h"
#pragma GCC diagnostic pop
bool fixtureBuffered=true;
CFileHandler::CFileHandler(const char* n,const char* m){Open(n,m);}
CFileHandler::CFileHandler(const std::string& n,const std::string& m){Open(n,m);}
void CFileHandler::Open(const std::string& name,const std::string&){Close();fileName=name;std::ifstream in(name,std::ios::binary);if(!in)return;std::vector<uint8_t>b((std::istreambuf_iterator<char>(in)),{});fileSize=int(b.size());filePos=0;if(fixtureBuffered)fileBuffer=std::move(b);else ifs.open(name,std::ios::binary);}
void CFileHandler::Close(){if(ifs.is_open())ifs.close();fileBuffer.clear();fileName.clear();fileSize=-1;filePos=0;}
int CFileHandler::Read(void* o,int c){ifs.read(static_cast<char*>(o),c);int n=int(ifs.gcount());filePos+=n;return n;}
bool CFileHandler::TryReadFromPWD(const std::string&){return false;}
bool CFileHandler::TryReadFromRawFS(const std::string&){return false;}
bool CFileHandler::TryReadFromVFS(const std::string&,int){return false;}
std::string FileSystem::GetExtensionLowerCase(const std::string& p){auto n=std::filesystem::path(p).extension().string();if(!n.empty())n.erase(n.begin());std::transform(n.begin(),n.end(),n.begin(),[](unsigned char c){return char(std::tolower(c));});return n;}
namespace {uint64_t checks=0;void Check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
uint8_t Gray(uint8_t r,uint8_t g,uint8_t b){const float luma=(float(r)/255.f*.299f)+(float(g)/255.f*.587f)+(float(b)/255.f*.114f);return uint8_t(std::min(uint32_t((256.f/255.f)*luma),255u));}
}
int main(int argc,char** argv){if(argc!=5)return 3;try{CBitmap::InitPool(unsigned(std::stoul(argv[1])));for(bool buffered:{false,true})for(int i=2;i<4;++i){fixtureBuffered=buffered;CBitmap image;Check(image.Load(argv[i]),"original CBitmap could not load actual Seths fog PNG");Check(image.xsize==512&&image.ysize==512&&image.channels==4&&!image.compressed,"original fog layout differs");int x,y,c;auto raw=stbi_load(argv[i],&x,&y,&c,4);Check(raw&&x==512&&y==512,"independent PNG decoder failed");const size_t n=image.GetMemSize();Check(n==512*512*4,"original payload extent differs");for(size_t j=0;j<n;++j)Check(image.GetRawMem()[j]==raw[j],"original loader channel/order mismatch");if(buffered){std::ofstream out(std::string(argv[4])+"/fog"+std::to_string(i-2)+".rgba",std::ios::binary);out.write(reinterpret_cast<const char*>(image.GetRawMem()),n);Check(bool(out),"GPU fixture source payload write failed");}image.InvertColors();uint64_t changed=0;for(size_t j=0;j<n;++j){const auto expect=j%4==3?raw[j]:255-raw[j];Check(image.GetRawMem()[j]==expect,"actual InvertColors differs from original RGB inversion/alpha preservation");changed+=image.GetRawMem()[j]!=raw[j];}Check(changed>n/4,"omitted inversion negative is not distinguishable");stbi_image_free(raw);}
// Original transform ordering after resize: invert -> grayscale -> tint.
{std::array<uint8_t,16> p{13,71,219,9,71,144,212,99,1,2,3,177,255,0,127,231};CBitmap image(p.data(),2,2,4);image=image.CreateRescaled(1,1);const std::array<uint8_t,4> resized{image.GetRawMem()[0],image.GetRawMem()[1],image.GetRawMem()[2],image.GetRawMem()[3]};image.InvertColors();image.MakeGrayScale();const float t[3]{.3f,.63f,1.4f};const auto grey=Gray(255-resized[0],255-resized[1],255-resized[2]);image.Tint(t);for(unsigned k=0;k<3;++k)Check(image.GetRawMem()[k]==uint8_t(std::clamp(float(grey)*t[k],0.f,255.f)),"resize/invert/gray/tint order or original quantization differs");Check(image.GetRawMem()[3]==resized[3],"original transform chain changed alpha");}
CBitmap::KillPool();std::cout<<"PASS checks="<<checks<<" actualFogPNG=2 poolMiB="<<argv[1]<<" buffered+unbuffered originalInvert=1 order=resize/invert/gray/tint\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL checks="<<checks<<" "<<e.what()<<'\n';return 1;}}
