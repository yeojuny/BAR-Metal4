/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBlueprintPrograms.h"
#include "NativeBlueprintProgramIdentity.h"

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace rendering {
namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::uint64_t Hash(std::string_view text)
{
	std::uint64_t result = 14695981039346656037ULL;
	for (unsigned char byte: text) result = (result^byte)*1099511628211ULL;
	return result;
}
void ReplaceOne(std::string& source, std::string_view expanded, std::string_view marker)
{
	Check(!expanded.empty(),"Original Blueprint helper expansion is missing");
	const auto first = source.find(expanded);
	Check(first != std::string::npos && source.find(expanded,first+expanded.size()) == std::string::npos,
		"Original Blueprint helper expansion identity/count differs");
	source.replace(first,expanded.size(),marker);
}
void Defines(NativeBlueprintProgram program, std::string_view text)
{
	std::istringstream input{std::string(text)};
	std::map<std::string,std::string> values;
	std::string line;
	while (std::getline(input,line)) {
		std::istringstream parts(line); std::string directive,name,value,extra;
		Check(bool(parts>>directive>>name>>value) && !(parts>>extra) && directive == "#define" && !values.contains(name),
			"Invalid original Blueprint shader definitions");
		values.emplace(std::move(name),std::move(value));
	}
	if(program==NativeBlueprintProgram::FeatureShape){Check(values.size()==1&&values["USEQUATERNIONS"]=="1","Original FeatureShape active engine transform variant differs");return;}
	Check(values.size() == 3 && values["STATICMODEL"] == (program == NativeBlueprintProgram::UnitShape ? "1" : "0") &&
		values["TRANSPARENCY"] == "0.5" && values["USEQUATERNIONS"] == "1", "Original Blueprint shader variant differs");
}
}

void ValidateNativeBlueprintProgram(NativeBlueprintProgram program,
	std::string_view vertex, std::string_view fragment, std::string_view engineDefinitions,
	std::string_view quaternionDefinitions, std::string_view shaderDefines, bool game31657)
{
	if(game31657) Check(Hash(engineDefinitions)==NativeBlueprintEngineDefinitionsHash31657,"Pinned31657 Blueprint engine definitions differ");
	std::string vs(vertex), fs(fragment);
	ReplaceOne(vs,engineDefinitions,"//__ENGINEUNIFORMBUFFERDEFS__");
	if (program == NativeBlueprintProgram::Outline) {
		Check(quaternionDefinitions.empty() && shaderDefines.empty(),"Unexpected original Blueprint outline helper/variant");
		Check(Hash(vs) == NativeBlueprintOutlineVertexSourceHash && Hash(fs) == NativeBlueprintOutlineFragmentSourceHash,
			"Original Blueprint outline shader source differs from the paired program");
		return;
	}
	Check(program == NativeBlueprintProgram::Unit || program == NativeBlueprintProgram::UnitShape || program == NativeBlueprintProgram::FeatureShape,"Unknown original Blueprint shader program");
	Defines(program,shaderDefines);
	if(game31657) Check(Hash(quaternionDefinitions)==NativeBlueprintQuaternionHash31657,"Pinned31657 Blueprint quaternion definitions differ");
	ReplaceOne(fs,engineDefinitions,"//__ENGINEUNIFORMBUFFERDEFS__");
	ReplaceOne(vs,quaternionDefinitions,"//__QUATERNIONDEFS__");
	ReplaceOne(vs,shaderDefines,"//__DEFINES__");
	ReplaceOne(fs,shaderDefines,"//__DEFINES__");
	if(program==NativeBlueprintProgram::FeatureShape){Check(game31657&&Hash(vs)==NativeFeatureShapeVertexHash&&Hash(fs)==NativeFeatureShapeFragmentHash,"Original FeatureShape shader source differs");return;}
	Check(Hash(vs) == (game31657?NativeBlueprintModelVertexSourceHash31657:NativeBlueprintModelVertexSourceHash) && Hash(fs) == (game31657?NativeBlueprintModelFragmentSourceHash31657:NativeBlueprintModelFragmentSourceHash),
		"Original Blueprint model shader source differs from the paired program");
}

} // namespace rendering
