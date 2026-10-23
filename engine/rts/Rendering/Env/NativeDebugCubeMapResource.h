/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWorldEnvironmentData.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/GL/myGL.h"
#include "System/Exceptions.h"
#include "System/FileSystem/FileHandler.h"
#include "System/Sync/SHA512.hpp"
#include "System/Log/ILog.h"

namespace rendering {
inline std::shared_ptr<const NativeWorldSkyResource> LoadOriginalDebugCubeMapResource()
{
	auto out=std::make_shared<NativeWorldSkyResource>();
	out->kind=NativeSkyKind::CubeMap;out->sourcePath="bitmaps/testsky.dds";
	out->generateMipmaps=false;out->mipmappedSampling=false; // Original final GL_LINEAR override.
	CFileHandler file(out->sourcePath,SPRING_VFS_RAW_FIRST);
	if(file.FileSize()>384*1024*1024)throw content_error("Original debug cube exceeds bounded capture storage");
	CBitmap bitmap;
	if(!bitmap.Load(out->sourcePath)||bitmap.textype!=GL_TEXTURE_CUBE_MAP){
		LOG_L(L_WARNING,"[DebugCubeMapTexture] could not load debug skybox texture from file %s, using fallback colors",out->sourcePath.c_str());
		constexpr std::array<std::array<uint8_t,4>,6> colors{{
			{{255,255,0,255}},{{0,255,0,255}},{{255,255,255,255}},
			{{0,0,0,255}},{{0,255,255,255}},{{255,0,0,255}}
		}};
		out->sourceIdentity="original DebugCubeMapTexture 16px six-face fallback";
		out->diagnostic="Original missing/invalid cube asset fallback";
		for(size_t face=0;face<6;++face){
			WorldTextureMip mip{16,16,64,{}};mip.bytes.reserve(16*16*4);
			for(size_t pixel=0;pixel<16*16;++pixel)mip.bytes.insert(mip.bytes.end(),colors[face].begin(),colors[face].end());
			out->cubeFaces[face].push_back(std::move(mip));
		}
		return out;
	}
	std::string source;if(!file.LoadStringData(source))throw content_error("Original debug cube source identity could not be read");
	sha512::raw_digest digest;sha512::calc_digest(reinterpret_cast<const uint8_t*>(source.data()),source.size(),digest.data());
	out->sourceIdentity=sha512::dump_digest(digest);
	const auto& image=bitmap.ddsimage;size_t block=16;unsigned channels=0;
	switch(image.get_format()){
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:out->format=WorldTextureFormat::BC1RGBA;block=8;break;
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:out->format=WorldTextureFormat::BC2RGBA;break;
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:out->format=WorldTextureFormat::BC3RGBA;break;
		case GL_COMPRESSED_RED_RGTC1:out->format=WorldTextureFormat::BC4Unorm;block=8;break;
		case GL_COMPRESSED_RG_RGTC2:out->format=WorldTextureFormat::BC5Unorm;break;
		case GL_COMPRESSED_RGBA_BPTC_UNORM:out->format=WorldTextureFormat::BC7RGBA;break;
		case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:out->format=WorldTextureFormat::BC7RGBASRGB;break;
		case GL_BGRA:channels=4;break;
		case GL_BGR:channels=3;break;
		case GL_LUMINANCE:channels=1;break;
		default:throw content_error("Original debug cube format has no native representation");
	}
	size_t total=0;
	for(unsigned face=0;face<6;++face){
		const auto& input=image.get_cubemap_face(face);
		auto append=[&](const nv_dds::CSurface& surface){
			const uint32_t width=surface.get_width(),height=surface.get_height();
			if(!width||width!=height||width>16384)throw content_error("Original debug cube face extent is invalid");
			const size_t row=channels?size_t(width)*channels:size_t((width+3)/4)*block;
			const size_t bytes=row*(channels?height:(height+3)/4);
			const size_t retainedBytes=channels?size_t(width)*height*4:bytes;
			if(bytes!=surface.get_size()||retainedBytes>384u*1024u*1024u-total)throw content_error("Original debug cube face storage is invalid");
			total+=retainedBytes;const uint8_t* data=surface;WorldTextureMip mip{width,height,row,{}};
			if(channels){
				mip.bytesPerRow=size_t(width)*4;mip.bytes.resize(mip.bytesPerRow*height);
				for(size_t i=0;i<size_t(width)*height;++i){auto* p=mip.bytes.data()+i*4;const auto* q=data+i*channels;
					p[0]=channels==1?q[0]:q[2];p[1]=channels==1?q[0]:q[1];p[2]=q[0];p[3]=channels==4?q[3]:255;}
			}else mip.bytes.assign(data,data+bytes);
			out->cubeFaces[face].push_back(std::move(mip));
		};
		append(input);for(unsigned level=0;level<input.get_num_mipmaps();++level)append(input.get_mipmap(level));
	}
	// CBitmap owns the original nv_dds flip and +/-Y swap; do not apply either twice.
	return out;
}
} // namespace rendering
