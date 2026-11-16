--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- mapinfo.lua
--

--TODO:
-- fix water with ices settings
-- blur height noise more
-- less specular variation in grass
-- remove black spots 
-- water more sandy
-- another feature set
-- 

		
local mapinfo = {
	name        = "All That Simmers v1.1.1",
	shortname   = "simmers",
	description = "Warning! Enemy scouts will peek through the steam clouds.",
	author      = "Nikuksis",
	version     = "1.1.1",
	--mutator   = "deployment";
	mapfile     = "maps/ATQ.smf", --// location of smf/sm3 file (optional)
	modtype     = 3, --// 1=primary, 0=hidden, 3=map
	depend      = {"Map Helper v1"},
	replace     = {},

	--startpic   = "", --// deprecated
	--StartMusic = "", --// deprecated

	maphardness     = 450,
	notDeformable   = false,
	gravity         = 100,
	tidalStrength   = 15,
	maxMetal        = 2.05,
	extractorRadius = 50.0,
	voidWater       = false,
	autoShowMetal   = true,


	smf = {
		minheight = -82,	
		maxheight = 700,
		smtFileName0 = "maps/ATQ.smt",
	},

	sound = {
		--// Sets the _reverb_ preset (= echo parameters),
		--// passfilter (the direct sound) is unchanged.
		--//
		--// To get a list of all possible presets check:
		--//   https://github.com/spring/spring/blob/master/rts/System/Sound/OpenAL/EFXPresets.cpp
		--//
		--// Hint:
		--// You can change the preset at runtime via:
		--//   /tset UseEFX [1|0]
		--//   /tset snd_eaxpreset preset_name   (may change to a real cmd in the future)
		--//   /tset snd_filter %gainlf %gainhf  (may    "   "  "  "    "  "   "    "   )
		preset = "default",

		passfilter = {
			--// Note, you likely want to set these
			--// tags due to the fact that they are
			--// _not_ set by `preset`!
			--// So if you want to create a muffled
			--// sound you need to use them.
			gainlf = 1.0,
			gainhf = 1.0,
		},

		reverb = {
			--// Normally you just want use the `preset` tag
			--// but you can use handtweak a preset if wanted
			--// with the following tags.
			--// To know their function & ranges check the
			--// official OpenAL1.1 SDK document.
			
			--density
			--diffusion
			--gain
			--gainhf
			--gainlf
			--decaytime
			--decayhflimit
			--decayhfratio
			--decaylfratio
			--reflectionsgain
			--reflectionsdelay
			--reflectionspan
			--latereverbgain
			--latereverbdelay
			--latereverbpan
			--echotime
			--echodepth
			--modtime
			--moddepth
			--airabsorptiongainhf
			--hfreference
			--lfreference
			--roomrollofffactor
		},
	},

	resources = {
		--grassBladeTex = "grass_blade_tex.tga", --blade texture
		--grassShadingTex = "grass_shading_tex.tga", --defaults to minimap
		detailTex = "detailtexblurred.bmp",
		--specularTex = "laststand_v17_speculartex.dds",
		--specularTex = "spec_black_zeroexponent.tga",
		-- specularTex = "spec_black_16exponent.tga",
		specularTex = "ATQ_speculartex.dds",
		splatDetailTex = "iwantDNTS.tga",
		splatDistrTex = "ATQ_splat_distribution.dds", --sand, rock, pebbles, cracks
		--splatDistrTex = "throne_remake_v6_splats.tga", --sand, rock, pebbles, cracks
		--skyReflectModTex = "rrsky.dds",
		splatDetailNormalDiffuseAlpha = 1,
		--splatDetailNormalTex1 = "Ground_MossSolid_1k_dnts.tga";
		--the order is cliffs, pebbles, grass, metalspots
		splatDetailNormalTex1 = "Rock_Metal_faf_dnts.tga";
		splatDetailNormalTex2 = "Gravel_2k_dnts.tga";
		splatDetailNormalTex3 = "Ground06_1k_dnts.tga";
		splatDetailNormalTex4 = "Ground_GrassThickGreen_1k_dnts.tga";
		detailNormalTex = "ATQ_normals.dds", --holy crap we can do 8K?
		--lightEmissionTex = "",
	},

	splats = {
		texScales = {0.010, 0.016, 0.008, 0.0045},
		texMults  = {1.2, 0.7, 1.0, 1.2},
	},

	atmosphere = {
    minWind = 0,
	maxWind = 16.0,
    fogEnd = 1.0,
    fogStart = 0.5,
    skyBox = "GoldSunriseSkybox_TBv1.dds",
    cloudColor = {
      0.89999998,
      0.89999998,
      0.89999998,
    },
    fogColor = {
      0.48,
      0.37,
      0.32,
    },
    skyColor = {
      0.42879999,
      0.58016002,
      0.63999999,
    },
    sunColor = {
      1,
      0.92,
      0.78,
    },
  },


  lighting = {
    groundShadowDensity = 0.7,
    unitShadowDensity = 0.7,
    groundAmbientColor = {
      0.45,
      0.45,
      0.45,
    },
    groundDiffuseColor = {
      1.1,
      1.1,
      1.1,
    },
    groundSpecularColor = {
      0.6,
      0.5,
      0.5,
    },
    sunDir = {
      -0.64,
      0.66,
      -0.57,
    },
    --specularsuncolor = { 1.1, 0.95, 0.9  },
    unitAmbientColor = {
      0.56999999,
      0.56942999,
      0.56942999,
    },
    unitDiffuseColor = {
      1,
      0.98533332,
      0.92000002,
    },
    unitSpecularColor = {
      0.8,
      0.60000001,
      0.60000001,
    },
  },
	
	water = {
		absorb    = {0.0075, 0.0020, 0.0061},
		baseColor = {0.20, 0.78, 0.50},
		minColor  = {0.005, 0.0078, 0.002},
    ambientFactor = 0.3,
    blurBase      = 2.0,
	blurExponent = 1.5,
    -- diffuseFactor = 1,
    foamTexture = "bitmaps/foam.jpg",
    forceRendering = false,
    fresnelMax = 1.4,
    fresnelMin = 0.2,
    fresnelPower = 8,
   -- planeColor = {0.13, 0.22, 0.25},
	hasWaterPlane = false,
	forceRendering = true,
    --normalTexture = "bitmaps/waterbump.png",
    --numTiles = 3,
    perlinStartFreq  =  8.0,
	perlinLacunarity = 3.0,
	perlinAmplitude  =  0.9,
    reflectionDistortion = 1.1,
    repeatX = 0,
    repeatY = 0,
    shoreWaves = true,
    specularFactor = 0.4,
	specularPower  = 20.0,
    texture = "bitmaps/ocean.jpg",
    diffuseFactor  = 1.0,
    surfaceColor  = {0.15, 0.50, 0.25},
	surfaceAlpha  = 0.1,
    specularColor = {0.65, 0.70, 0.65},
	waveoffsetfactor  = 1.35,
	waveLength  = 0.70,
	waveFoamDistortion  = 0.35,
	waveFoamIntensity  = 0.50,
	causticsStrength  = 0.03,
	ambientFactor  = 0.005,
	normalTexture = "maps/waterbump_4tiles.png",
  },

	teams = {
		[0] = {startPos = {x = 4600, z = 2000}},
		[1] = {startPos = {x = 2600, z = 8220}},
		[2] = {startPos = {x = 3200, z = 2000}},
		[3] = {startPos = {x = 4000, z = 8220}},
		
		[4] = {startPos = {x = 1800, z = 2000}},
		[5] = {startPos = {x = 5300, z = 8070}},
		[6] = {startPos = {x = 6050, z = 2000}},
		[7] = {startPos = {x = 1100, z = 8300}},
		
		[8] = {startPos = {x = 850, z = 850}},
		[9] = {startPos = {x = 6300, z = 9400}},
		[10] = {startPos = {x = 6900, z = 450}},
		[11] = {startPos = {x = 300, z = 9770}},
		
		[12] = {startPos = {x = 3322, z = 750}},
		[13] = {startPos = {x = 3850, z = 9500}},
		[14] = {startPos = {x = 5060, z = 620}},
		[15] = {startPos = {x = 2100, z = 9600}},
	},
	
	terrainTypes = {
		[0] = {
			name = "Default",
			hardness = 1.0,
			receiveTracks = true,
			moveSpeeds = {
				tank  = 1.0,
				kbot  = 1.0,
				hover = 1.0,
				ship  = 1.0,
			},
		},
	},

	custom = {
		fog = {
			color    = {0.48, 0.37, 0.32},
			height   = "40%", --// allows either absolue sizes or in percent of map's MaxHeight
			fogatten = 0.0035,
		},
		clouds = {
			speed = 0.1, -- multiplier for speed of scrolling with wind
			--color    = {0.49,0.37,0.25}, -- diffuse color of the fog OLD
			--color       = {0.23,0.17,0.11},
			--color    	= {0.68,0.60,0.79},
			-- all altitude values can be either absolute, in percent, or "auto"
			-- High Clouds
			height   = 4500, 
			bottom = 4200, 
			fade_alt = 4300, 
			-- Low Clouds
			--height   = 1000, -- opacity of fog above and at this altitude will be zero
			--bottom = 600, -- no fog below this altitude
			--fade_alt = 950, -- fog will linearly fade away between this and "height", should be between height and bottom

			scale = 650, -- how large will the clouds be
			opacity = 0.4, -- for low altitude
			--opacity = 0.55,
			clamp_to_map = false, -- whether fog volume is sliced to fit map, or spreads to horizon
			sun_penetration = 40, -- how much does the sun penetrate the fog
		},	
         grassConfig= {
            grassDistTGA = "maps/ATQ_grassmap.tga",
            grassMaxSize = 1.5,
           -- grassBladeColorTex = "maps/grass_field_dry.dds.cached.dds", -- rgb + alpha transp
            grassShaderParams = { -- allcaps because thats how i know
                MAPCOLORFACTOR = 0.6, -- how much effect the minimapcolor has
                MAPCOLORBASE = 0.3,     --how much more to blend the bottom of the grass patches into map color
             },
			},
	},
}

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- Helper

local function lowerkeys(ta)
	local fix = {}
	for i,v in pairs(ta) do
		if (type(i) == "string") then
			if (i ~= i:lower()) then
				fix[#fix+1] = i
			end
		end
		if (type(v) == "table") then
			lowerkeys(v)
		end
	end
	
	for i=1,#fix do
		local idx = fix[i]
		ta[idx:lower()] = ta[idx]
		ta[idx] = nil
	end
end

lowerkeys(mapinfo)

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
-- Map Options

do
	local function tmerge(t1, t2)
		for i,v in pairs(t2) do
			if (type(v) == "table") then
				t1[i] = t1[i] or {}
				tmerge(t1[i], v)
			else
				t1[i] = v
			end
		end
	end

	getfenv()["mapinfo"] = mapinfo
		local files = VFS.DirList("mapconfig/mapinfo/", "*.lua")
		table.sort(files)
		for i=1,#files do
			local newcfg = VFS.Include(files[i])
			if newcfg then
				lowerkeys(newcfg)
				tmerge(mapinfo, newcfg)
			end
		end
	getfenv()["mapinfo"] = nil
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

return mapinfo

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
