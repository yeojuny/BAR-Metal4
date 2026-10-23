/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Test-only compatibility OpenGL oracle; never linked by the native engine.
#define GL_SILENCE_DEPRECATION
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Metal4/Metal4ScreenRenderer.h"
#import <Foundation/Foundation.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace rendering;
using namespace rendering::ui;
static void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char**)
{
	@autoreleasepool {
		try {
			constexpr unsigned width=64,height=48;
			// GL 2.1 section3.3.1 defines odd-width centers at floor(xw)+.5.
			// Apple CGL's exact-integer tie behavior differs (retained separate
			// differential receipt); do not replace the standard with that bias.
			{
				NativeUIRecorder canonical;canonical.BeginFrame(width,height);canonical.BeginScreenStage();
				canonical.DrawPixelPoints({{0,0,0,{1,1,1,1}},{1,1,0,{1,1,1,1}},{10,10,0,{1,1,1,1}},{63,47,0,{1,1,1,1}}});
				const auto frame=canonical.EndFrame();const auto vertices=frame.drawList->Vertices();
				Require(vertices.size()==16,"Canonical point coverage count");
				const std::array<std::array<float,2>,4> cells{{{0,0},{1,1},{10,10},{63,47}}};
				for(unsigned n=0;n<4;++n)Require(vertices[n*4].x==cells[n][0] && vertices[n*4].y==cells[n][1],"Canonical GL point center mismatch");
			}
			const bool negative=argc>1;
			CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAColorSize,CGLPixelFormatAttribute(24),kCGLPFAAlphaSize,CGLPixelFormatAttribute(8),CGLPixelFormatAttribute(0)};
			CGLPixelFormatObj format=nullptr;CGLContextObj context=nullptr;GLint count=0;
			Require(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError && format,"No reference pixel format");
			Require(CGLCreateContext(format,nullptr,&context)==kCGLNoError && context,"No reference context");
			CGLDestroyPixelFormat(format);CGLSetCurrentContext(context);
			GLuint texture=0,fbo=0;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
			glGenFramebuffersEXT(1,&fbo);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,texture,0);
			Require(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT,"Reference FBO incomplete");
			glViewport(0,0,width,height);glDisable(GL_POINT_SMOOTH);glPointSize(1);
			glDisable(GL_TEXTURE_2D);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);glDisable(GL_ALPHA_TEST);
			glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,0,height,-1,1);
			glMatrixMode(GL_MODELVIEW);glLoadIdentity();
			std::size_t checks=0,mismatches=0;unsigned maxError=0;
			for (unsigned mode=0;mode<4;++mode) {
				std::vector<ScreenVertex> points;
				for (unsigned y=0;y<6;++y) for (unsigned x=0;x<7;++x) {
					const float dx=float(x)*8+1+(.125f+float(mode)*.1875f),dy=float(y)*7+1+(.125f+float(mode)*.1875f);
					points.push_back({dx,dy,0,{float(x)/7,float(y)/6,.7f,.65f}});
				}
				for (const auto& p:std::array<std::array<float,3>,10>{{{-.1f,8.25f,0},{.125f,10.25f,0},{63.875f,12.25f,0},{64.125f,14.25f,0},{8.25f,-.1f,0},{10.25f,.125f,0},{12.25f,47.875f,0},{14.25f,48.125f,0},{20.25f,20.25f,1.01f},{22.25f,22.25f,-1.01f}}})
					points.push_back({p[0],p[1],p[2],{1,0,0,1}});
				const ScreenColor clear{.08f,.12f,.18f,.4f};glClearColor(clear[0],clear[1],clear[2],clear[3]);glClear(GL_COLOR_BUFFER_BIT);
				if(mode==3){glEnable(GL_SCISSOR_TEST);glScissor(10,8,30,22);}else glDisable(GL_SCISSOR_TEST);
				glBegin(GL_POINTS);for(const auto& p:points){glColor4fv(p.color.data());glVertex3f(p.x,p.y,p.z);}glEnd();
				std::vector<std::uint8_t> reference(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,reference.data());
				Require(glGetError()==GL_NO_ERROR,"Reference GL error");
				NativeUIRecorder recorder;recorder.BeginFrame(width,height);recorder.BeginScreenStage();
				if(mode==3)recorder.SetScissor(ScreenScissor{10,8,30,22});
				if(negative)for(auto& p:points)p.x+=1;
				if(mode==2){recorder.BeginList();recorder.DrawPixelPoints(points);auto list=recorder.EndList();recorder.CallList(list);}
				else recorder.DrawPixelPoints(points);
				auto frame=recorder.EndFrame();ScreenRenderRequest request;request.width=width;request.height=height;request.drawList=frame.drawList;request.clearColor=clear;
				const auto result=RenderMetal4Screen(request);
				unsigned printed=0;const auto oldMismatch=mismatches;
				for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)for(unsigned c=0;c<4;++c){
					const auto error=unsigned(std::abs(int(reference[(y*width+x)*4+c])-int(result.rgba[((height-1-y)*width+x)*4+c])));
					++checks;maxError=std::max(maxError,error);if(error>2)++mismatches;
					if(error>2 && printed++<12)std::cerr<<"mode="<<mode<<" x="<<x<<" y="<<y<<" c="<<c<<" GL="<<int(reference[(y*width+x)*4+c])<<" native="<<int(result.rgba[((height-1-y)*width+x)*4+c])<<"\n";
				}
				std::cerr<<"mode="<<mode<<" mismatches="<<(mismatches-oldMismatch)<<"\n";
			}
			glDeleteFramebuffersEXT(1,&fbo);glDeleteTextures(1,&texture);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);
			std::cout<<"checks="<<checks<<" mismatches="<<mismatches<<" maxError="<<maxError<<" negative="<<negative<<"\n";
			return mismatches?2:0;
		}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
	}
}
