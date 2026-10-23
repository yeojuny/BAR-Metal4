/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#define GL_SILENCE_DEPRECATION
#include "Rendering/Backend/NativeScreenLine.h"
#include "Rendering/Metal4/Metal4ScreenRenderer.h"
#import <Foundation/Foundation.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cmath>
#include <algorithm>
using namespace rendering;
void Require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct ReferenceGL {
	CGLContextObj context=nullptr;
	GLuint framebuffer=0,texture=0;
	unsigned width,height;
	ReferenceGL(unsigned w,unsigned h):width(w),height(h)
	{
		CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAColorSize,CGLPixelFormatAttribute(24),
			kCGLPFAAlphaSize,CGLPixelFormatAttribute(8),CGLPixelFormatAttribute(0)};
		CGLPixelFormatObj format=nullptr;GLint count=0;
		Require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,"Reference GL pixel format unavailable");
		const auto error=CGLCreateContext(format,nullptr,&context);CGLDestroyPixelFormat(format);
		Require(error==kCGLNoError && context,"Reference GL context unavailable");
		Require(CGLSetCurrentContext(context)==kCGLNoError,"Cannot activate reference GL context");
		glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(w),GLsizei(h),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
		glGenFramebuffersEXT(1,&framebuffer);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,texture,0);
		Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT,"Reference GL framebuffer incomplete");
	}
	~ReferenceGL()
	{
		if(context){CGLSetCurrentContext(context);glDeleteFramebuffersEXT(1,&framebuffer);glDeleteTextures(1,&texture);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);}
	}
	std::vector<uint8_t> Render(const ScreenLine& l) {
 glViewport(0,0,32,32);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_FOG);glDisable(GL_SCISSOR_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_DITHER);glDisable(GL_BLEND);glDisable(GL_LINE_SMOOTH);glShadeModel(GL_SMOOTH);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,32,0,32,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();if(l.stippled){glEnable(GL_LINE_STIPPLE);glLineStipple(l.factor,l.pattern);}else glDisable(GL_LINE_STIPPLE);glLineWidth(l.width);glBegin(GL_LINES);glColor4fv(l.start.color.data());glVertex3f(l.start.x,l.start.y,l.start.z);glColor4fv(l.end.color.data());glVertex3f(l.end.x,l.end.y,l.end.z);glEnd();std::vector<uint8_t> bytes(32*32*4);glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,bytes.data());return bytes;
 }
};

std::vector<uint8_t> CPU(const ScreenDrawList& list){std::vector<uint8_t> a(32*32*4);for(size_t n=0;n<list.Vertices().size();n+=4){const auto& v=list.Vertices()[n];for(size_t c=0;c<4;++c)a[(size_t(v.y)*32+size_t(v.x))*4+c]=uint8_t(std::lround(std::clamp(v.color[c],0.f,1.f)*255));}return a;}
bool Tie(const ScreenLine& l,int x,int y){double dx=l.end.x-l.start.x,dy=l.end.y-l.start.y;bool xm=std::abs(dx)>=std::abs(dy);int width=std::max(1,int(std::floor(l.width+.5f)));double shift=-double(width-1)*.5,ax=l.start.x+(xm?0:shift),ay=l.start.y+(xm?shift:0);for(int r=0;r<width;++r){double px=x+.5-(xm?0:r),py=y+.5-(xm?r:0);std::array<double,4> times{0,1,dx==0?0:(px-ax)/dx,dy==0?0:(py-ay)/dy};double distance=1e100;for(double t:times){t=std::clamp(t,0.,1.);distance=std::min(distance,std::abs(ax+dx*t-px)+std::abs(ay+dy*t-py));}if(std::abs(distance-.5)<1e-5)return true;}double major=xm?x:y,start=xm?l.start.x:l.start.y,end=xm?l.end.x:l.end.y;return std::abs(major-start)<1.01||std::abs(major-end)<1.01;}
int main(int argc,char** argv){@autoreleasepool{try{
 Require(argc==2||argc==3,"line-gpu receipt.json [--negative-ignore-stipple]");bool negative=argc==3;ReferenceGL gl(32,32);float range[2];glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE,range);NSMutableArray* exceptions=[NSMutableArray array];uint32_t cases=0,gradientFailures=0,interiorFailures=0,metalFailures=0,metalRenders=0,checks=0;
 for(float width:{1.f,1.5f,2.5f,3.f})for(float ax:{2.f,2.25f,2.5f})for(float ay:{3.f,3.25f,3.5f})for(float bx:{3.f,15.f,27.f})for(float by:{2.f,16.f,28.f}){
 ScreenLine line{{ax,ay,0,{.15f,.3f,.6f,.4f}},{bx,by,0,{.85f,.7f,.2f,.9f}},width};auto expected=gl.Render(line);ScreenDrawList list;ScreenDrawState state;state.blend.enabled=false;AppendNativeScreenLines(list,{&line,1},state,32,32);auto actual=CPU(list);++cases;
 for(size_t i=0;i<actual.size();i+=4){bool a=actual[i+3]!=0,e=expected[i+3]!=0;int x=int(i/4)%32,y=int(i/4)/32;if(a!=e){if(!Tie(line,x,y))++interiorFailures;else {NSMutableArray* old=[NSMutableArray array],*now=[NSMutableArray array];for(size_t c=0;c<4;++c){[old addObject:@(expected[i+c])];[now addObject:@(actual[i+c])];}[exceptions addObject:@{@"case":@(cases),@"start":@[@(ax),@(ay)],@"end":@[@(bx),@(by)],@"width":@(width),@"pixel":@[@(x),@(y)],@"OpenGL":old,@"canonical":now,@"reason":@"GL2.1 permitted endpoint/diamond-boundary tie"}];}}else if(a)for(size_t c=0;c<4;++c){++checks;if(std::abs(int(actual[i+c])-int(expected[i+c]))>2)++gradientFailures;}}
 if(cases%32==0){ScreenRenderRequest request;request.drawList=&list;request.width=request.height=32;request.clearColor={0,0,0,0};auto gpu=RenderMetal4Screen(request);++metalRenders;for(uint32_t y=0;y<32;++y)for(uint32_t x=0;x<32;++x)for(size_t c=0;c<4;++c){++checks;if(std::abs(int(gpu.rgba[(size_t(31-y)*32+x)*4+c])-int(actual[(size_t(y)*32+x)*4+c]))>2)++metalFailures;}}
 }
 for(bool reverse:{false,true})for(int factor:{1,2,3})for(uint16_t pattern:{uint16_t(0xaaaa),uint16_t(0x00ff),uint16_t(0xf001)}){
 ScreenLine line{{reverse?27.5f:2.5f,8.5f,0,{1,1,1,1}},{reverse?2.5f:27.5f,8.5f,0,{1,1,1,1}},2.5f,true,pattern,factor,true};auto expected=gl.Render(line);if(negative)line.stippled=false;ScreenDrawList list;ScreenDrawState state;state.blend.enabled=false;AppendNativeScreenLines(list,{&line,1},state,32,32);auto actual=CPU(list);++cases;for(size_t q=0;q<actual.size();q+=4){const bool a=actual[q+3]!=0,e=expected[q+3]!=0;const int x=int(q/4)%32,y=int(q/4)/32;for(size_t c=0;c<4;++c)++checks;
 if(a!=e && Tie(line,x,y)){NSMutableArray* old=[NSMutableArray array],*now=[NSMutableArray array];for(size_t c=0;c<4;++c){[old addObject:@(expected[q+c])];[now addObject:@(actual[q+c])];}[exceptions addObject:@{@"case":@(cases),@"pixel":@[@(x),@(y)],@"OpenGL":old,@"canonical":now,@"reason":@"GL2.1 permitted half-open endpoint; Apple CGL software stipple endpoint choice"}];}
 else for(size_t c=0;c<4;++c)if(actual[q+c]!=expected[q+c])++interiorFailures;
 }}
 // Clip, negative extent, degenerate endpoint, strip continuation and reset.
 ScreenDrawState state;state.blend.enabled=false;
 for(auto line:std::array<ScreenLine,4>{{{{-10.5f,4.5f,0,{1,1,1,1}},{15.5f,4.5f,0,{1,1,1,1}},1},{{4.5f,4.5f,-2,{1,1,1,1}},{24.5f,4.5f,2,{1,1,1,1}},1},{{4.5f,4.5f,0,{1,1,1,1}},{4.5f,4.5f,0,{1,1,1,1}},1},{{80,80,0,{1,1,1,1}},{90,90,0,{1,1,1,1}},1}}}){auto expected=gl.Render(line);ScreenDrawList list;AppendNativeScreenLines(list,{&line,1},state,32,32);auto actual=CPU(list);++cases;uint32_t bad=0;for(size_t i=0;i<actual.size();++i){++checks;if(expected[i]!=actual[i]){++interiorFailures;++bad;}}if(bad){std::cout<<"case "<<cases<<" bad="<<bad<<" pixels";for(size_t q=3;q<actual.size();q+=4)if(actual[q]!=expected[q])std::cout<<" "<<(q/4)%32<<","<<(q/4)/32<<":"<<int(actual[q])<<"/"<<int(expected[q]);std::cout<<"\n";}}
 std::array<ScreenLine,2> strip{{{{2.5f,6.5f,0,{1,1,1,1}},{10.5f,6.5f,0,{1,1,1,1}},1,true,0x00ff,1,true},{{10.5f,6.5f,0,{1,1,1,1}},{18.5f,6.5f,0,{1,1,1,1}},1,true,0x00ff,1,false}}};ScreenDrawList continuous;AppendNativeScreenLines(continuous,strip,state,32,32);auto a=CPU(continuous);for(int x=2;x<18;++x){++checks;Require((a[(6*32+x)*4+3]!=0)==(x<10),"stipple strip continuation/reset oracle failed");}
 strip[1].resetStipple=true;ScreenDrawList separate;AppendNativeScreenLines(separate,strip,state,32,32);a=CPU(separate);for(int x=2;x<18;++x){++checks;Require(a[(6*32+x)*4+3]==255,"independent line reset oracle failed");}
 NSDictionary* report=@{@"status":gradientFailures||interiorFailures||metalFailures?@"failed":@"passed",@"cases":@(cases),@"checks":@(checks),@"gradientFailures":@(gradientFailures),@"interiorOrWidthFailures":@(interiorFailures),@"MetalPixelFailures":@(metalFailures),@"MetalRenders":@(metalRenders),@"CGLRenderer":[NSString stringWithUTF8String:reinterpret_cast<const char*>(glGetString(GL_RENDERER))],@"GLAliasedWidthRange":@[@(range[0]),@(range[1])],@"canonicalTieExceptions":exceptions,@"negativeIgnoreStipple":@(negative),@"scope":@"Canonical GL2.1 diamond/wide-line coverage; actual ordinary OpenGL interpolation comparison, width/clip/stipple, actual Metal per-pixel triangle consumer; no whole-game parity claim."};NSError* error=nil;NSData* json=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:&error];Require(json&&!error&&[json writeToFile:[NSString stringWithUTF8String:argv[1]] atomically:YES],"cannot save receipt");std::cout<<"cases="<<cases<<" gradient="<<gradientFailures<<" interior="<<interiorFailures<<" metal="<<metalFailures<<" ties="<<exceptions.count<<"\n";return gradientFailures||interiorFailures||metalFailures?2:0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}}
