#include "Rendering/Backend/NativeUIBindingResolverOwner.h"
#include <cassert>
#include <iostream>
using namespace rendering;
class RuntimeLease {
 ui::NativeUIBindingResolverOwner uiResolver;
public:
 #include "lease-ui-resolver.inc"
};
template<class F> bool Throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){
 NativeCUSTextureBinding binding;
 for(int i=0;i<1000;++i){
  auto a=std::make_shared<ScreenTexture>(),b=std::make_shared<ScreenTexture>();int live=0;
  auto fallback=[&]{++live;return ui::NativeUITextureObserver::ObservedImage{b,{}};};
  RuntimeLease A,B,standalone,empty;
  A.BindUIResolver([a](const auto&){return ui::NativeUITextureObserver::ObservedImage{a,{}};});
  B.BindUIResolver([b](const auto&){return ui::NativeUITextureObserver::ObservedImage{b,{}};});
  assert(A.UIResolver().Describe(binding,false,fallback).image==a);
  assert(B.UIResolver().Describe(binding,false,fallback).image==b&&!live);
  assert(standalone.UIResolver().Describe(binding,true,fallback).image==b&&live==1);
  assert(Throws([&]{standalone.UIResolver().Describe(binding,false,fallback);}));assert(live==1);
  empty.BindUIResolver({});assert(empty.UIResolver().Describe(binding,true,fallback).image==b);
  assert(Throws([&]{empty.UIResolver().Describe(binding,false,fallback);}));
  assert(Throws([&]{A.BindUIResolver({});}));assert(Throws([&]{empty.BindUIResolver({});}));
  assert(A.UIResolver().Describe(binding,false,fallback).image==a);
  // Screen continuation receives its own frozen resolver, not another lease slot.
  ui::NativeUICapturedBindingResolver captured=[a](const auto&){return ui::NativeUITextureObserver::ObservedImage{a,{}};};
  RuntimeLease continuation;continuation.BindUIResolver(std::move(captured));
  assert(continuation.UIResolver().Describe(binding,false,fallback).image==a);
  assert(B.UIResolver().Describe(binding,false,fallback).image==b);
  RuntimeLease failed;failed.BindUIResolver([](const auto&)->ui::NativeUITextureObserver::ObservedImage{throw std::runtime_error("descriptor failure");});
  const int before=live;assert(Throws([&]{failed.UIResolver().Describe(binding,true,fallback);}));assert(live==before);
  assert(A.UIResolver().Describe(binding,false,fallback).image==a);
 }
 std::weak_ptr<ScreenTexture> retained;
 {RuntimeLease lease;auto owner=std::make_shared<ScreenTexture>();retained=owner;lease.BindUIResolver([owner](const auto&){return ui::NativeUITextureObserver::ObservedImage{owner,{}};});owner.reset();assert(!retained.expired());}
 assert(retained.expired());
 std::cout<<"1000 independent/continuation/empty/worker/failure cases and exact resolver capture lifetime PASS\n";
}
