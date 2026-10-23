/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rankAtlas {
inline void Install(lua_State* L,sol::table& table)
{
 Bind(table,"CreateTextureAtlas",[L](int x,int y,int allocator){
  Check(x==2048 && y==2048 && allocator==1,"Original rank atlas creation differs");
  Check(x<=configHandler->GetInt("MaxTextureAtlasSizeX") && y<=configHandler->GetInt("MaxTextureAtlasSizeY"),"Original rank atlas exceeds actual configured bounds");
  return CLuaHandle::GetActiveAtlasTextures(L).Create(x,y,allocator);
 });
 Bind(table,"AddAtlasTexture",[L](const std::string& name,const std::string& source){
  auto* atlas=CLuaHandle::GetActiveAtlasTextures(L).GetAtlasById(name);Check(atlas && source.starts_with("LuaUI/Images/ranks/") && source.ends_with(".png"),"Original rank atlas source differs");
  auto binding=LuaNativeUI::ResolveTextureBinding(L,source);Check(bool(binding),"Original rank PNG source missing");const auto image=binding->Resolve();
  Check(image.image && image.image->format==rendering::ScreenTextureFormat::RGBA8Unorm && image.image->width && image.image->height && image.image->bytesPerRow==std::size_t(image.image->width)*4 && image.image->bytes.size()==image.image->bytesPerRow*image.image->height,"Original rank PNG CPU upload is not its actual RGBA base");
  auto& context=rendering::OriginalNativeTextureContext();auto& recorder=rendering::ui::GetRecorder();Check(!recorder.IsCompiling(),"Original rank atlas upload cannot be compiled into a display list");
  const auto unit=context.History().ActiveUnit();Check(unit.has_value(),"Original rank upload has no observed active texture unit");
  const auto target=rendering::NativeCUSTextureTarget::Image2D;const auto prior=context.History().Binding(*unit,target);
  const auto sampled=context.UI().CaptureBind({},image.image);Check(sampled && sampled->target==target,"Original rank PNG lacks its actual texture owner");
  auto mirror=[&]{if(*unit<2)recorder.SyncObservedTextureBinding(*unit);};
  auto restore=[&]{context.Registry().Bind(*unit,0,target);mirror();context.Registry().Bind(*unit,prior.object?prior.object->name:0,target);mirror();};
  context.Registry().Bind(*unit,sampled->name,target);mirror();
  try{atlas->AddTexFromMem(source,image.image->width,image.image->height,CTextureAtlas::TextureType::RGBA32,image.image->bytes.data());}catch(...){restore();throw;}
  restore();
 });
 Bind(table,"FinalizeTextureAtlas",[L](const std::string& name){auto* atlas=CLuaHandle::GetActiveAtlasTextures(L).GetAtlasById(name);Check(atlas!=nullptr,"Original rank atlas was deleted");return atlas->Finalize();});
 Bind(table,"GetAtlasTexture",[L](const std::string& name,const std::string& source){auto* atlas=CLuaHandle::GetActiveAtlasTextures(L).GetAtlasById(name);Check(atlas!=nullptr,"Original rank atlas was deleted");const auto uv=atlas->GetTexture(source);Check(uv!=AtlasedTexture::DefaultAtlasTexture,"Original rank subimage missing");return std::make_tuple(uv.x1,uv.x2,uv.y1,uv.y2,uv.pageNum);});
 Bind(table,"DeleteTextureAtlas",[L](const std::string& name){return CLuaHandle::GetActiveAtlasTextures(L).Delete(name);});
 Bind(table,"Texture",[L](unsigned requested,sol::object value){
  Check(requested==0,"Original rank shader samples texture unit0");auto& context=rendering::OriginalNativeTextureContext();auto& recorder=rendering::ui::GetRecorder();unsigned unit=0;
  if(value.is<bool>())unit=context.UI().LuaEnable(requested,value.as<bool>());
  else {const auto name=value.as<std::string>();unit=context.UI().LuaLookup(requested,[&]{auto* atlas=CLuaHandle::GetActiveAtlasTextures(L).GetAtlasById(name);Check(atlas && atlas->GetTexTarget()==GL_TEXTURE_2D,"Original rank atlas sampler2D source missing");auto object=context.Registry().Find(atlas->GetTexID());Check(bool(object),"Original rank atlas has no published native texture");return rendering::NativeTextureLookup{true,std::move(object),false};});}
  if(unit<2)recorder.SyncObservedTextureBinding(unit);return true;
 });
}
}
