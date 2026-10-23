/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// CPU-only production engine methods, with independent VFS/image identities.
// Does not exercise DDS decoding, Lua dispatch, recorder replay or any GPU.
// unitimage.inc/methods.inc contain verbatim production bodies extracted by the runner.
#include "Rendering/Backend/ScreenDrawList.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
struct lua_State;
using GLuint=unsigned;
#define CR_DECLARE_STRUCT(...)
#define RECOIL_DETAILED_TRACY_ZONE ((void)0)
static unsigned glDeletes=0;
void glDeleteTextures(int count,const GLuint*) { glDeletes+=count; }
#include "Rendering/Backend/NativeUIRecorder.h"
#include "unitimage.inc"
struct UnitDef {
    int id=1;
    std::string name="armcom",buildPicName;
    mutable UnitDefImage* buildPic=nullptr;
};
static std::map<std::string,bool> exists;
static std::vector<std::string> loaded,queried;
constexpr int SPRING_VFS_RAW_FIRST=0;
struct CFileHandler {
    static bool FileExists(const std::string& name,int) { queried.push_back(name);return exists[name]; }
};
static std::map<std::string,std::shared_ptr<rendering::ui::NativeUITextureBinding>> registry;
namespace LuaNativeUI {
std::shared_ptr<rendering::ui::NativeUITextureBinding> GetTextureBinding(lua_State*,const std::string& name) {
    const auto found=registry.find(name);return found==registry.end()?nullptr:found->second;
}
std::shared_ptr<const rendering::ScreenTexture> LoadBuildPicture(const std::string& name) {
    loaded.push_back(name);
    if (name=="unitpics/corrupt.dds") throw std::runtime_error("independent invalid DDS");
    auto image=std::make_shared<rendering::ScreenTexture>();
    image->sourceName=name;image->generation=loaded.size();
    if (name=="unitpics/missing.DDS") { image->incomplete=true;return image; }
    image->width=name.ends_with(".dds")?128:64;image->height=32;
    image->bytesPerRow=image->width*4;
    image->bytes={uint8_t(loaded.size()),17,33,255};
    if(name.ends_with(".dds")) image->mips.resize(8);
    return image;
}
}
class CUnitDrawerData {
public:
    std::vector<UnitDefImage> unitDefImages=std::vector<UnitDefImage>(3);
    void SetUnitDefImage(const UnitDef*,const std::string&);
    void SetUnitDefImage(const UnitDef*,unsigned,int,int);
    uint32_t GetUnitDefImage(const UnitDef*);
    std::shared_ptr<rendering::ui::NativeUITextureBinding> GetNativeUnitDefImage(const UnitDef*);
    void SetNativeUnitDefImage(const UnitDef*,std::shared_ptr<rendering::ui::NativeUITextureBinding>);
};
#include "methods.inc"
struct lua_State {
    enum Arg { Nil, String, Other } arg=Nil;
    int unitDefId=1;
    std::string text;
};
struct FixtureUnitDefHandler {
    const UnitDef* unit=nullptr;
    const UnitDef* GetUnitDefByID(int id) const {return unit&&unit->id==id?unit:nullptr;}
} fixtureHandler;
auto* unitDefHandler=&fixtureHandler;
CUnitDrawerData* fixtureData=nullptr;
struct CUnitDrawer {
    static void SetUnitDefImage(const UnitDef* ud,const std::string& name) {fixtureData->SetUnitDefImage(ud,name);}
    static void SetNativeUnitDefImage(const UnitDef* ud,std::shared_ptr<rendering::ui::NativeUITextureBinding> binding) {fixtureData->SetNativeUnitDefImage(ud,std::move(binding));}
};
struct LuaTextures {static constexpr char prefix='!';};
int luaL_checkint(lua_State* state,int) {return state->unitDefId;}
bool lua_isnoneornil(lua_State* state,int) {return state->arg==lua_State::Nil;}
bool lua_israwstring(lua_State* state,int) {return state->arg==lua_State::String;}
const char* lua_tostring(lua_State* state,int) {return state->text.c_str();}
static unsigned luaErrors=0;
int luaL_error(lua_State*,const char*,const char* error) {++luaErrors;throw std::runtime_error(error);}
struct LuaUnsyncedCtrl {static int SetUnitDefImage(lua_State*);};
#include "ctrl.inc"
unsigned checks=0;
void Check(bool result,const char* message) { ++checks;if(!result)throw std::runtime_error(message); }
template<class F> void Reject(F f,const char* message) { bool caught=false;try{f();}catch(const std::exception&){caught=true;}Check(caught,message); }
void Reset() { exists.clear();loaded.clear();queried.clear(); }
int main() {
    try {
        Reset(); CUnitDrawerData data;UnitDef ud;
        exists["unitpics/armcom.dds"]=true;exists["unitpics/armcom.png"]=true;
        const auto cell=data.GetNativeUnitDefImage(&ud);
        Check(loaded==std::vector<std::string>{"unitpics/armcom.dds"},"dds wins original existence order");
        Check(queried==loaded,"later formats not inspected after existing dds");
        Check(cell==data.unitDefImages[1].nativeBinding,"one stable engine cell");
        Check(ud.buildPic->imageSizeX==128&&ud.buildPic->imageSizeY==32,"decoded dimensions used");
        Check(cell->sampler.wrapS==rendering::ScreenWrap::Repeat&&cell->sampler.mipFilter==rendering::ScreenMipFilter::Nearest,"original DDS sampler boundary");
        const auto original=cell->current;
        data.SetUnitDefImage(&ud,"custom.png");
        Check(cell==data.GetNativeUnitDefImage(&ud)&&cell->current!=original,"file override changes same retained cell");
        Check(cell->current->sourceName=="unitpics/custom.png"&&cell->sampler.mipFilter==rendering::ScreenMipFilter::None,"explicit filename retained");
        Check(original->sourceName=="unitpics/armcom.dds","already captured image generation remains immutable");
        data.SetUnitDefImage(&ud,"custom.png");
        Check(loaded.size()==3&&cell->current->generation==3,"same filename Set requests real fresh load");
        ud.buildPicName="declared.dds";data.SetUnitDefImage(&ud,ud.buildPicName);
        Check(cell->current->sourceName=="unitpics/declared.dds","nil reset engine target uses declared default filename");
        auto rtt=std::make_shared<rendering::ui::NativeUITextureBinding>();
        auto version1=std::make_shared<rendering::ScreenTexture>();version1->width=37;version1->height=19;version1->generation=1;
        rtt->current=version1;data.SetNativeUnitDefImage(&ud,rtt);
        Check(cell==data.GetNativeUnitDefImage(&ud)&&cell->source==rtt&&!cell->current,"RTT override delegates same stable cell");
        Check(ud.buildPic->imageSizeX==37&&ud.buildPic->imageSizeY==19,"RTT dimensions retained");
        auto version2=std::make_shared<rendering::ScreenTexture>(*version1);version2->generation=2;rtt->current=version2;
        Check(cell->source->current==version2&&version1->generation==1,"new RTT generation available through retained delegation");
        rtt.reset();Check(cell->source->current==version2,"RTT binding lifetime does not borrow registry/state");
        Reject([&]{data.SetNativeUnitDefImage(&ud,{});},"invalid RTT rejected");
        Check(cell->source->current==version2,"invalid RTT does not partially clear current override");
        data.SetUnitDefImage(&ud,ud.buildPicName);
        Check(!cell->source&&cell->current->sourceName=="unitpics/declared.dds","default reset clears RTT delegation");
        Check(ud.buildPic->Free()&&cell==ud.buildPic->nativeBinding&&!cell->current&&!cell->source,"Free clears same retained cell");
        Check(!ud.buildPic->Free(),"empty repeated Free reports false");
        data.SetUnitDefImage(&ud,"");
        Reject([&]{data.SetUnitDefImage(&ud,"corrupt.dds");},"malformed DDS is not dummy success");
        Check(cell==ud.buildPic->nativeBinding&&!cell->current&&!cell->source,"malformed replacement retains cleared cell identity");
        Reset();CUnitDrawerData png;UnitDef p;exists["unitpics/armcom.png"]=true;
        Check(png.GetNativeUnitDefImage(&p)->current->sourceName=="unitpics/armcom.png","ordered default falls through missing dds");
        Check(queried==std::vector<std::string>{"unitpics/armcom.dds","unitpics/armcom.png"},"default format order is original");
        Reset();CUnitDrawerData missing;UnitDef m;auto fallback=missing.GetNativeUnitDefImage(&m)->current;
        Check(loaded.empty()&&queried.size()==4,"missing default checks four names without dummy DDS load");
        Check(fallback->width==1&&fallback->height==1&&fallback->bytes==std::vector<uint8_t>{255,0,0,255},"true missing default uses original exact red pixel");
        CUnitDrawerData missingExplicit;UnitDef explicitUnit;explicitUnit.buildPicName="missing.DDS";
        auto incomplete=missingExplicit.GetNativeUnitDefImage(&explicitUnit);
        Check(incomplete->current->incomplete,"valid unit missing explicit DDS retains incomplete texture");
        Check(explicitUnit.buildPic->imageSizeX==0&&explicitUnit.buildPic->imageSizeY==0,"original missing DDS dimensions remain zero");
        Check(incomplete->current->bytes.empty()&&incomplete->current->mips.empty(),"missing DDS is not a fabricated fallback image");
        Check(missingExplicit.GetNativeUnitDefImage(&explicitUnit)==incomplete,"missing DDS stable cell survives repeat cache lookup");
        auto absentVersion=incomplete->current;
        missingExplicit.SetUnitDefImage(&explicitUnit,"recovered.png");
        Check(incomplete->current->width==64&&!incomplete->current->incomplete,"real replacement recovers same retained unit cell");
        Check(absentVersion->incomplete&&absentVersion->width==0,"older incomplete snapshot remains immutable after replacement");
        Check(glDeletes==0,"native files/RTT never publish or delete fake GL texture IDs");
        Reset();CUnitDrawerData api;UnitDef a;a.buildPicName="declared.png";
        fixtureData=&api;fixtureHandler.unit=&a;lua_State state;
        LuaUnsyncedCtrl::SetUnitDefImage(&state);
        auto apiCell=api.GetNativeUnitDefImage(&a);
        Check(apiCell->current->sourceName=="unitpics/declared.png","actual Lua nil reset uses declared filename");
        state.arg=lua_State::String;state.text="replacement.png";
        LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->current->sourceName=="unitpics/replacement.png","actual Lua filename override uses same cell");
        auto apiRtt=std::make_shared<rendering::ui::NativeUITextureBinding>();apiRtt->current=version2;registry["!9"]=apiRtt;
        state.text="!9";LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->source==apiRtt&&!apiCell->current,"actual Lua !RTT override uses real registry binding");
        state.text="!missing";LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->source==apiRtt,"missing Lua RTT does not partially replace existing image");
        state.arg=lua_State::Other;LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->source==apiRtt,"nonstring Lua image ignored");
        state.arg=lua_State::String;state.text="replacement.png";state.unitDefId=0;
        LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->source==apiRtt,"invalid UnitDefID ignored");
        state.unitDefId=1;state.text="";exists["unitpics/armcom.dds"]=true;
        LuaUnsyncedCtrl::SetUnitDefImage(&state);
        Check(apiCell->current->sourceName=="unitpics/armcom.dds"&&!apiCell->source,"empty Lua filename selects ordered default without indexing empty string");
        state.text="corrupt.dds";
        Reject([&]{LuaUnsyncedCtrl::SetUnitDefImage(&state);},"malformed decode reported to Lua");
        Check(luaErrors==1,"actual Lua setter catches native decode exception at Lua boundary");
        std::cout<<"Native build-picture engine contract: "<<checks<<" independent CPU checks passed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
