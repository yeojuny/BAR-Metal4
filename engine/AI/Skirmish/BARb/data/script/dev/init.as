#include "../common.as"
#include "../unit.as"


namespace Init {

SInitInfo AiInit()
{
	AiLog("dev AngelScript Rules!");

	dictionary@ mo = aiSetupMgr.GetModOptions();
	aiSetupMgr.SetWaterHarmful(string(mo["map_waterislava"]) == "1");

	SInitInfo data;
	data.armor = InitArmordef();
	data.category = InitCategories();
	@data.profile = @(array<string> = {"behaviour", "block_map", "build_chain", "commander", "economy", "factory", "response"});
	return data;
}

}
