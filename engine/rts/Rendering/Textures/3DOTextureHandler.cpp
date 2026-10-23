/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include <cctype>
#include <sstream>

#include "3DOTextureHandler.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeS3OTextureSource.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include <bit>
#include <limits>
#include <stdexcept>
#endif
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Textures/IAtlasAllocator.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "TAPalette.h"
#include "System/Exceptions.h"
#include "System/UnorderedSet.hpp"
#include "System/StringUtil.h"
#include "System/type2.h"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/FileSystem/SimpleParser.h"
#include "System/Log/ILog.h"

#include <fmt/format.h>

#include "System/Misc/TracyDefs.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

C3DOTextureHandler textureHandler3DO;

struct TexFile {
	CBitmap tex;  ///< same format as s3o's
	CBitmap tex2; ///< same format as s3o's
	std::string name;
};

static constexpr auto DEFAULT_NUM_OF_TEXTURE_LEVELS = 4;
void C3DOTextureHandler::Init()
{
	RECOIL_DETAILED_TRACY_ZONE;
	std::vector<TexFile> texFiles = LoadTexFiles();

	// TODO: make this use TextureAtlas directly
	CTextureAtlas atlas;
	atlas.SetName("3DOModelTextureAtlas");

	IAtlasAllocator* atlasAlloc = atlas.GetAllocator();

	// NOTE: most Intels report maxTextureSize=2048, some even 1024 (!)
	atlasAlloc->SetMaxSize(std::min(globalRendering->maxTextureSize, 4096), std::min(globalRendering->maxTextureSize, 4096));
	atlasAlloc->SetMaxTexLevel(DEFAULT_NUM_OF_TEXTURE_LEVELS);

	// default for 3DO primitives that point to non-existing textures
	textures["___dummy___"] = UnitTexture(0.0f, 0.0f, 1.0f, 1.0f);

	for (const TexFile& texFile: texFiles) {
		atlasAlloc->AddEntry(texFile.name, int2(texFile.tex.xsize, texFile.tex.ysize));
	}

	const bool allocated = atlasAlloc->Allocate();
	const auto& curAtlasSize = atlasAlloc->GetAtlasSize();
	const auto& maxAtlasSize = atlasAlloc->GetMaxSize();

	if (!allocated) {
		// either the algorithm simply failed to fit all
		// textures or the textures would really not fit
		LOG_L(L_WARNING,
			"[%s] failed to allocate 3DO texture-atlas memory (size=%ux%u max=%ux%u)",
			__func__,
			curAtlasSize.x, curAtlasSize.y,
			maxAtlasSize.x, maxAtlasSize.y
		);

		return;
	}

	bigTexX = curAtlasSize.x;
	bigTexY = curAtlasSize.y;

	assert(curAtlasSize.x <= maxAtlasSize.x);
	assert(curAtlasSize.y <= maxAtlasSize.y);

	std::vector<uint8_t> bigtex1(curAtlasSize.x * curAtlasSize.y * 4, 0);
	std::vector<uint8_t> bigtex2(curAtlasSize.x * curAtlasSize.y * 4, 0);

	for (int a = 0; a < (curAtlasSize.x * curAtlasSize.y); ++a) {
		bigtex1[a*4 + 0] = 128;
		bigtex1[a*4 + 1] = 128;
		bigtex1[a*4 + 2] = 128;
		bigtex1[a*4 + 3] =   0;

		bigtex2[a*4 + 0] =   0;
		bigtex2[a*4 + 1] = 128;
		bigtex2[a*4 + 2] =   0;
		bigtex2[a*4 + 3] = 255;
	}

	for (const TexFile& texFile: texFiles) {
		const CBitmap& curtex1 = texFile.tex;
		const CBitmap& curtex2 = texFile.tex2;

		const size_t foundx = atlasAlloc->GetEntry(texFile.name).x;
		const size_t foundy = atlasAlloc->GetEntry(texFile.name).y;
		const auto texCoords = atlasAlloc->GetTexCoordsCntr(texFile.name);

		const uint8_t* rmem1 = curtex1.GetRawMem();
		const uint8_t* rmem2 = curtex2.GetRawMem();

		for (int y = 0; y < curtex1.ysize; ++y) {
			for (int x = 0; x < curtex1.xsize; ++x) {
				for (int col = 0; col < 4; ++col) {
					bigtex1[(((foundy + y) * curAtlasSize.x + (foundx + x)) * 4) + col] = rmem1[(((y * curtex1.xsize) + x) * 4) + col];
					bigtex2[(((foundy + y) * curAtlasSize.x + (foundx + x)) * 4) + col] = rmem2[(((y * curtex1.xsize) + x) * 4) + col];
				}
			}
		}

		textures[texFile.name] = UnitTexture(texCoords);
	}

	numLevels = atlasAlloc->GetNumTexLevels();

#ifdef RECOIL_METAL4_LIVE_SCENE
	// The original atlas allocator and CPU texel writer above are unchanged.
	// Publish those exact pixels through the existing original model image
	// consumer, preserving each atlas's distinct MAG/MIN and mip-generation call.
	auto& context=rendering::OriginalNativeTextureContext();
	if(!context.Initialized()||!context.UIAttached())throw std::logic_error("Original 3DO atlas upload has no actual native texture context");
	auto& registry=context.Registry();auto& history=context.History();
	const auto active=history.ActiveUnit();
	if(!active)throw std::logic_error("Original 3DO atlas upload has no observed active texture unit");
	const auto mirror=[&]{if(*active<2)rendering::ui::GetRecorder().SyncObservedTextureBinding(*active);};
	const auto upload=[&](unsigned atlasNumber,const std::vector<uint8_t>& bytes,bool mipmaps,unsigned mag,unsigned min)->GLuint{
		const auto object=registry.Create(rendering::NativeCUSTextureTarget::Image2D,"Original C3DOTextureHandler atlas "+std::to_string(atlasNumber));
		if(object->name>std::numeric_limits<GLuint>::max())throw std::runtime_error("Original 3DO atlas name exceeds GLuint");
		try{
			registry.Bind(*active,object->name,rendering::NativeCUSTextureTarget::Image2D);mirror();
			auto sampling=object->sampling;sampling.magFilter=mag;sampling.minFilter=min;
			sampling.wrapS=sampling.wrapT=GL_CLAMP_TO_EDGE;sampling.maxLevel=numLevels-1;
			registry.SetSampling(object->name,sampling);mirror();
			const unsigned levels=mipmaps?std::bit_width(static_cast<unsigned>(std::max(curAtlasSize.x,curAtlasSize.y))):1;
			auto image=std::make_shared<rendering::WorldTexture>();image->sourcePath=object->source;
			image->originalInternalFormat=GL_RGBA8;image->format=rendering::WorldTextureFormat::RGBA8Unorm;image->generateMipmaps=mipmaps;
			image->mips.push_back({uint32_t(curAtlasSize.x),uint32_t(curAtlasSize.y),size_t(curAtlasSize.x)*4,bytes});
			registry.Allocate(object->name,{uint32_t(curAtlasSize.x),uint32_t(curAtlasSize.y),1,levels,1,GL_RGBA8});mirror();
			// RecoilBuildMipmaps overrides the earlier atlas maxLevel with the full
			// chain. Atlas2 calls it even when MIN is non-mipmapped (numLevels==1).
			if(mipmaps){sampling.baseLevel=0;sampling.maxLevel=levels-1;registry.SetSampling(object->name,sampling);mirror();}
			const auto allocated=registry.Find(object->name);auto backing=std::make_shared<rendering::NativeModelTextureBacking>();
			backing->storageGeneration=allocated->identity;backing->contentVersion=allocated->version+1;
			backing->sourceDrawFrame=globalRendering->drawFrame;backing->content=rendering::NativeTextureContent::ProducerVersion;backing->pixels=image;
			registry.PublishContent(object->name,image,std::move(backing));mirror();
			if(mipmaps&&globalRendering->amdHacks){history.Enable(*active,rendering::NativeCUSTextureTarget::Image2D,true);mirror();history.Enable(*active,rendering::NativeCUSTextureTarget::Image2D,false);mirror();}
			return GLuint(object->name);
		}catch(...){registry.Delete(object->name);mirror();throw;}
	};
	try{
		atlas3do1=upload(1,bigtex1,numLevels>1,GL_LINEAR,numLevels>1?GL_LINEAR_MIPMAP_NEAREST:GL_LINEAR);
		atlas3do2=upload(2,bigtex2,numLevels>0,GL_LINEAR,numLevels>1?GL_NEAREST_MIPMAP_NEAREST:GL_NEAREST);
	}catch(...){if(atlas3do1)registry.Delete(atlas3do1);atlas3do1=atlas3do2=0;mirror();throw;}
#else

	{
		glGenTextures(1, &atlas3do1);
		glBindTexture(GL_TEXTURE_2D, atlas3do1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (numLevels > 1) ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  numLevels - 1);

		if (numLevels > 1) {
			RecoilBuildMipmaps(GL_TEXTURE_2D, GL_RGBA8, curAtlasSize.x, curAtlasSize.y, GL_RGBA, GL_UNSIGNED_BYTE, bigtex1.data()); //FIXME disable texcompression
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, curAtlasSize.x, curAtlasSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, bigtex1.data());
		}
	}
	{
		glGenTextures(1, &atlas3do2);
		glBindTexture(GL_TEXTURE_2D, atlas3do2);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (numLevels > 1) ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  numLevels - 1);

		if (numLevels > 0) {
			RecoilBuildMipmaps(GL_TEXTURE_2D, GL_RGBA8, curAtlasSize.x, curAtlasSize.y, GL_RGBA, GL_UNSIGNED_BYTE, bigtex2.data()); //FIXME disable texcompression
		} else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, curAtlasSize.x, curAtlasSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, bigtex2.data());
		}
	}
#endif

	if (CTextureAtlas::GetDebug()) {
		CBitmap tex1(bigtex1.data(), curAtlasSize.x, curAtlasSize.y);
		CBitmap tex2(bigtex2.data(), curAtlasSize.x, curAtlasSize.y);

		tex1.Save(atlas.GetName() + "-1-" + IntToString(curAtlasSize.x) + "x" + IntToString(curAtlasSize.y) + ".png", true);
		tex2.Save(atlas.GetName() + "-2-" + IntToString(curAtlasSize.x) + "x" + IntToString(curAtlasSize.y) + ".png", true);
	}
}

void C3DOTextureHandler::Kill()
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	if(atlas3do1||atlas3do2){
		auto& context=rendering::OriginalNativeTextureContext();
		if(!context.Initialized()||!context.UIAttached())throw std::logic_error("Original 3DO atlas deletion outlived its actual native context");
		context.Registry().Delete(atlas3do1);context.Registry().Delete(atlas3do2);
		rendering::ui::GetRecorder().SyncObservedTextureBinding(0);rendering::ui::GetRecorder().SyncObservedTextureBinding(1);
	}
#else
	glDeleteTextures(1, &atlas3do1);
	glDeleteTextures(1, &atlas3do2);
#endif

	atlas3do1 = 0;
	atlas3do2 = 0;

	textures.clear();
}

void C3DOTextureHandler::DumpAtlasTextures(const std::string& fileExt) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (atlas3do1) {
		for (int level = 0; level < numLevels; ++level) {
			glSaveTexture(atlas3do1, fmt::format("3DOAtlas1-{}.{}", level, fileExt).c_str(), level);
		}
	}
	if (atlas3do2) {
		for (int level = 0; level < numLevels; ++level) {
			glSaveTexture(atlas3do2, fmt::format("3DOAtlas2-{}.{}", level, fileExt).c_str(), level);
		}
	}
}


std::vector<TexFile> C3DOTextureHandler::LoadTexFiles()
{
	RECOIL_DETAILED_TRACY_ZONE;
	CFileHandler teamTexFile("unittextures/tatex/teamtex.txt");
	CFileHandler paletteFile("unittextures/tatex/palette.pal");

	CSimpleParser parser(teamTexFile);

	spring::unordered_set<std::string> teamTexes;
	spring::unordered_set<string> usedNames;

	while (!parser.Eof()) {
		teamTexes.insert(StringToLower(parser.GetCleanLine()));
	}

	std::vector<TexFile> texFiles;

	const std::vector<std::string>& bmpFiles = CFileHandler::FindFiles("unittextures/tatex/", "*.bmp");
	      std::vector<std::string>  tgaFiles = CFileHandler::FindFiles("unittextures/tatex/", "*.tga");

	tgaFiles.insert(tgaFiles.end(), bmpFiles.begin(), bmpFiles.end());
	texFiles.reserve(tgaFiles.size() + CTAPalette::NUM_PALETTE_ENTRIES);

	for (const std::string& s: tgaFiles) {
		const std::string s2 = StringToLower(FileSystem::GetBasename(s));

		// avoid duplicate names and give tga images priority
		if (usedNames.find(s2) != usedNames.end())
			continue;

		usedNames.insert(s2);

		if (teamTexes.find(s2) == teamTexes.end()) {
			texFiles.push_back(CreateTex(s, s2, false));
		} else {
			texFiles.push_back(CreateTex(s, s2, true));
		}
	}

	palette.Init(paletteFile);

	for (unsigned a = 0; a < CTAPalette::NUM_PALETTE_ENTRIES; ++a) {
		TexFile texFile;
		texFile.name = "ta_color" + IntToString(a, "%i");

		texFile.tex.AllocDummy(SColor{palette[a][0], palette[a][1], palette[a][2], uint8_t(0)}); // A=team-color
		texFile.tex2.AllocDummy(SColor{0, 30, 0, 255}); // R=self-illum, G=reflectivity

		texFiles.push_back(texFile);
	}

	return texFiles;
}


C3DOTextureHandler::UnitTexture* C3DOTextureHandler::Get3DOTexture(const std::string& name)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const auto tti = textures.find(name);

	if (tti != textures.end())
		return &tti->second;

	// unknown texture
	return nullptr;
}

TexFile C3DOTextureHandler::CreateTex(const std::string& name, const std::string& name2, bool teamcolor)
{
	RECOIL_DETAILED_TRACY_ZONE;
	TexFile texFile;
	static constexpr float defaultAlpha = 30.0f / 255.0f;
	texFile.tex.Load(name, defaultAlpha, 4, GL_UNSIGNED_BYTE, true);
	texFile.name = name2;

	texFile.tex2.Alloc(texFile.tex.xsize, texFile.tex.ysize);

	CBitmap* tex1 = &texFile.tex;
	CBitmap* tex2 = &texFile.tex2;

	unsigned char* wmem1 = tex1->GetRawMem();
	unsigned char* wmem2 = tex2->GetRawMem();

	for (int a = 0; a < (tex1->ysize * tex1->xsize); ++a) {
		wmem2[a*4 + 0] = 0;
		wmem2[a*4 + 1] = wmem1[a*4 + 3]; // move reflectivity to texture2
		wmem2[a*4 + 2] = 0;
		wmem2[a*4 + 3] = 255;

		wmem1[a*4 + 3] = 0;

		if (teamcolor) {
			//purple = teamcolor
			if ((wmem1[a*4] == wmem1[a*4 + 2]) && (wmem1[a*4+1] == 0)) {
				const unsigned char lum = wmem1[a*4];
				wmem1[a*4 + 0] = 0;
				wmem1[a*4 + 1] = 0;
				wmem1[a*4 + 2] = 0;
				wmem1[a*4 + 3] = (unsigned char)(std::min(255.0f, lum * 1.5f));
			}
		}
	}

	return texFile;
}

