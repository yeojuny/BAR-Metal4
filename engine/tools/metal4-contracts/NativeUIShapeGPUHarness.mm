/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Test-only OpenGL reference. The native engine does not link this harness.
#define GL_SILENCE_DEPRECATION
#include "Rendering/Metal4/Metal4ScreenRenderer.h"
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace rendering;
namespace {
void Require(bool value,const char* reason) {if(!value)throw std::runtime_error(reason);}

void PNG(const std::vector<std::uint8_t>& rgba,unsigned width,unsigned height,NSString* path)
{
	CGColorSpaceRef color=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGDataProviderRef provider=CGDataProviderCreateWithData(nullptr,rgba.data(),rgba.size(),nullptr);
	CGImageRef bitmap=CGImageCreate(width,height,8,32,std::size_t(width)*4,color,
		CGBitmapInfo(kCGBitmapByteOrder32Big)|CGBitmapInfo(kCGImageAlphaLast),provider,nullptr,false,kCGRenderingIntentDefault);
	CGImageDestinationRef output=CGImageDestinationCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:path],CFSTR("public.png"),1,nullptr);
	bool saved=false;
	if(bitmap && output){CGImageDestinationAddImage(output,bitmap,nullptr);saved=CGImageDestinationFinalize(output);}
	if(output)CFRelease(output);if(bitmap)CGImageRelease(bitmap);if(provider)CFRelease(provider);if(color)CGColorSpaceRelease(color);
	Require(saved,"Cannot save comparison image");
}

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
	std::vector<std::uint8_t> Render(NSArray* fans,ScreenColor clear)
	{
		CGLSetCurrentContext(context);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);
		glViewport(0,0,GLsizei(width),GLsizei(height));glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
		glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_2D);glDisable(GL_FOG);glDisable(GL_SCISSOR_TEST);
		glDisable(GL_ALPHA_TEST);glDisable(GL_DITHER);glShadeModel(GL_SMOOTH);
		glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glClearColor(clear[0],clear[1],clear[2],clear[3]);glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,0,height,-1,1);
		glMatrixMode(GL_MODELVIEW);glLoadIdentity();
		for(NSDictionary* fan in fans){
			Require([fan[@"primitive"] unsignedIntValue]==GL_TRIANGLE_FAN,"Oracle contains a non-fan primitive");
			glBegin(GL_TRIANGLE_FAN);
			for(NSDictionary* vertex in fan[@"vertices"]){
				NSArray* p=vertex[@"position"];NSArray* c=vertex[@"color"];NSArray* uv=vertex[@"uv"];NSArray* n=vertex[@"normal"];
				glColor4f([c[0] floatValue],[c[1] floatValue],[c[2] floatValue],[c[3] floatValue]);
				glTexCoord2f([uv[0] floatValue],[uv[1] floatValue]);glNormal3f([n[0] floatValue],[n[1] floatValue],[n[2] floatValue]);
				glVertex3f([p[0] floatValue],[p[1] floatValue],[p[2] floatValue]);
			}
			glEnd();
		}
		std::vector<std::uint8_t> bottom(std::size_t(width)*height*4),top(bottom.size());
		glReadPixels(0,0,GLsizei(width),GLsizei(height),GL_RGBA,GL_UNSIGNED_BYTE,bottom.data());
		Require(glGetError()==GL_NO_ERROR,"Reference OpenGL reported an error");
		for(unsigned y=0;y<height;++y)std::copy_n(bottom.data()+std::size_t(y)*width*4,std::size_t(width)*4,top.data()+std::size_t(height-1-y)*width*4);
		return top;
	}
};

ScreenDrawList NativeGeometry(NSDictionary* source,bool omit)
{
	ScreenDrawList list;
	if(omit)return list;
	std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<std::uint32_t> indices;
	for(NSDictionary* v in source[@"vertices"]){
		NSArray* p=v[@"position"];NSArray* c=v[@"color"];NSArray* t=v[@"uv"];
		vertices.push_back({[p[0] floatValue],[p[1] floatValue],[p[2] floatValue],{[c[0] floatValue],[c[1] floatValue],[c[2] floatValue],[c[3] floatValue]}});
		uv.push_back({[t[0] floatValue],[t[1] floatValue]});
	}
	for(NSNumber* value in source[@"indices"])indices.push_back(value.unsignedIntValue);
	for(NSDictionary* command in source[@"commands"]){
		const auto first=[command[@"firstIndex"] unsignedIntValue],count=[command[@"indexCount"] unsignedIntValue];
		Require(first<=indices.size() && count<=indices.size()-first,"Native oracle indices outside bounds");
		NSDictionary* b=command[@"blend"];ScreenDrawState state;
		state.blend={bool([b[@"enabled"] boolValue]),ScreenBlendFactor([b[@"sourceColor"] unsignedIntValue]),ScreenBlendFactor([b[@"destinationColor"] unsignedIntValue]),
			ScreenBlendFactor([b[@"sourceAlpha"] unsignedIntValue]),ScreenBlendFactor([b[@"destinationAlpha"] unsignedIntValue])};
		Require(state.blend==ScreenBlendState{},"FlowUI changed original default blend state");
		list.AppendTriangles(vertices,uv,std::span<const std::uint32_t>(indices).subspan(first,count),state);
	}
	return list;
}
}

int main(int argc,char** argv)
{
	@autoreleasepool {
		try {
			Require(argc==3 || argc==4,"Usage: shape-gpu <oracle.json> <output-directory> [--negative-omit-shape]");
			const bool negative=argc==4;
			Require(!negative || std::string(argv[3])=="--negative-omit-shape","Unknown negative control");
			NSString* input=[NSString stringWithUTF8String:argv[1]];NSString* directory=[NSString stringWithUTF8String:argv[2]];
			std::filesystem::create_directories(argv[2]);NSError* error=nil;
			NSDictionary* oracle=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfFile:input] options:0 error:&error];
			Require(oracle && !error && [oracle[@"schemaVersion"] intValue]==1,"Invalid Shape oracle");
			const auto width=[oracle[@"width"] unsignedIntValue],height=[oracle[@"height"] unsignedIntValue];
			Require(width>0 && height>0 && width<=1024 && height<=1024,"Oracle extent outside bounds");
			NSArray* clearValues=oracle[@"clearRGBA"];
			const ScreenColor clear{[clearValues[0] floatValue],[clearValues[1] floatValue],[clearValues[2] floatValue],[clearValues[3] floatValue]};
			ReferenceGL gl(width,height);
			NSString* glRenderer=[NSString stringWithUTF8String:reinterpret_cast<const char*>(glGetString(GL_RENDERER))];
			NSString* glVersion=[NSString stringWithUTF8String:reinterpret_cast<const char*>(glGetString(GL_VERSION))];
			NSMutableArray* reports=[NSMutableArray array];std::size_t mismatches=0;unsigned maxError=0;
			NSString* nativeEncoder=@"";
			for(NSDictionary* test in oracle[@"cases"]){
				NSString* label=test[@"label"];
				const auto reference=gl.Render(test[@"referenceFans"],clear);
				auto list=NativeGeometry(test[@"native"],negative);
				ScreenRenderRequest request;request.drawList=&list;request.width=width;request.height=height;request.clearColor=clear;
				const auto native=RenderMetal4Screen(request);nativeEncoder=[NSString stringWithUTF8String:native.renderEncoderClass.c_str()];
				Require(native.rgba.size()==reference.size(),"GPU image extents differ");
				std::size_t caseMismatch=0,exactPixels=0;unsigned caseMax=0;
				for(std::size_t p=0;p<reference.size();p+=4){
					unsigned pixelError=0;
					for(unsigned c=0;c<4;++c)pixelError=std::max(pixelError,unsigned(std::abs(int(reference[p+c])-int(native.rgba[p+c]))));
					if(pixelError>2)++caseMismatch;if(pixelError==0)++exactPixels;caseMax=std::max(caseMax,pixelError);
				}
				mismatches+=caseMismatch;maxError=std::max(maxError,caseMax);
				PNG(reference,width,height,[directory stringByAppendingPathComponent:[label stringByAppendingString:@"-opengl.png"]]);
				PNG(native.rgba,width,height,[directory stringByAppendingPathComponent:[label stringByAppendingString:@"-metal4.png"]]);
				[reports addObject:@{@"label":label,@"exactPixels":@(exactPixels),@"pixelsOutsideTolerance":@(caseMismatch),@"maxChannelError":@(caseMax)}];
			}
			NSDictionary* result=@{@"status":mismatches?@"failed":@"passed",@"negativeOmitShape":@(negative),@"width":@(width),@"height":@(height),
				@"rgba8ChannelTolerance":@2,@"allPixelsCompared":@YES,@"excludedEdgesOrRegions":@NO,@"pixelsOutsideTolerance":@(mismatches),@"maxChannelError":@(maxError),
				@"referenceRenderer":glRenderer,@"referenceVersion":glVersion,@"nativeEncoder":nativeEncoder,@"cases":reports,
				@"scope":@"Exact original FlowUI/parser command oracle on an actual compatibility OpenGL context versus actual production Metal4 screen GPU; not whole-game Windows/Linux parity."};
			NSData* json=[NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:&error];
			Require(json && !error && [json writeToFile:[directory stringByAppendingPathComponent:@"report.json"] atomically:YES],"Cannot write comparison report");
			std::cout<<"cases="<<reports.count<<" pixelsOutsideTolerance="<<mismatches<<" maxChannelError="<<maxError<<" negative="<<negative<<"\n";
			return mismatches?2:0;
		} catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
	}
}
