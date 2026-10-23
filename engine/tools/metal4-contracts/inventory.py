# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
"""Source-pinned static Lua graphics inventory; never executes or edits content."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import subprocess

BAR_COMMIT = "069f57319503b635698ebb855f11255443e4018b"
RECOIL_COMMIT = "de69361239d8c8b1012dba3f5aa3122954ea4da3"

# These mappings are migration obligations, not advertised backend capabilities.
GROUPS = {
	"capability-query": (
		"HasExtension GetNumber GetString GetViewSizes GetViewRange GetScreenViewTrans",
		"Describe actual semantic capabilities/device limits; retain the queried legacy names through an explicit adapter.",
	),
	"state-scope": (
		"ResetState ResetMatrices PushAttrib PopAttrib UnsafeState GetFixedState",
		"Record and restore Lua call-in state snapshots, including callback/error unwinding.",
	),
	"pipeline-raster-state": (
		"Scissor Viewport ColorMask DepthMask DepthTest DepthClamp Culling PolygonMode PolygonOffset StencilTest StencilMask StencilFunc StencilOp StencilMaskSeparate StencilFuncSeparate StencilOpSeparate AlphaToCoverage",
		"Render pipeline/depth-stencil/raster state plus encoder viewport/scissor; preserve per-face stencil and coverage.",
	),
	"blend-state": (
		"Blending BlendEquation BlendFunc BlendEquationSeparate BlendFuncSeparate",
		"Per-attachment native blend state; retain separate color/alpha factors and premultiplied RTT compositing.",
	),
	"compatibility-shader-state": (
		"Lighting ShadeModel LogicOp Fog AlphaTest LineStipple Material Color Light ClipPlane ClipDistance TexEnv MultiTexEnv TexGen MultiTexGen SecondaryColor FogCoord EdgeFlag Smoothing",
		"Explicit compatibility uniforms/variants and geometry where required; alpha test, stipple and logical operations need equivalent semantics, never ignored state.",
	),
	"expanded-lines-points": (
		"LineWidth PointSize PointSprite PointParameter",
		"Preserve size, clipping, point-coordinate/attenuation and coverage via native-supported state or geometry expansion.",
	),
	"matrix-stack": (
		"MatrixMode LoadIdentity LoadMatrix MultMatrix Translate Scale Rotate Ortho Frustum PushMatrix PopMatrix PushPopMatrix Billboard GetMatrixData",
		"CPU matrix stacks and uniform upload; preserve multiplication order and pass matrices, adapting clip depth at the backend boundary.",
	),
	"immediate-geometry": (
		"Shape BeginEnd Vertex Normal TexCoord MultiTexCoord Rect TexRect DrawGroundCircle DrawGroundQuad",
		"Record vertices/attributes, expand legacy topologies to indexed native draws, preserve TexRect UV defaults and ground projection.",
	),
	"recorded-display-list": (
		"CreateList CallList DeleteList DrawListAtUnit DrawFuncAtUnit",
		"Command recording and replay with matrix/state effects; CreateList executes construction Lua once, CallList must not re-run it.",
	),
	"texture-resource": (
		"Texture CreateTexture ChangeTextureParams DeleteTexture TextureInfo ActiveTexture CreateTextureAtlas FinalizeTextureAtlas DeleteTextureAtlas AddAtlasTexture GetAtlasTexture GetEngineAtlasTextures GetGlobalTexNames GetGlobalTexCoords",
		"Native texture/sampler handles, engine texture-name resolver and atlas metadata; preserve channel/mip/format/wrap/lifetime contracts.",
	),
	"copy-mipmap": (
		"CopyToTexture GenerateMipmap",
		"Native copy/blit or equivalent compute mip generation with explicit subresource dependencies.",
	),
	"offscreen-pass": (
		"CreateFBO DeleteFBO IsValidFBO ActiveFBO RawBindFBO ClearAttachmentFBO BlitFBO CreateRBO DeleteRBO DeleteTextureFBO RenderToTexture Clear",
		"Attachment load/store/clear/resolve/render/copy passes; preserve nested RTT restoration, scissored clears and read/write ordering.",
	),
	"shader-family-provisioning": (
		"CreateShader DeleteShader UseShader ActiveShader GetActiveUniforms GetUniformLocation GetSubroutineIndex Uniform UniformFloat UniformInt UniformArray UniformMatrix UniformSubroutine GetShaderLog SetGeometryShaderParameter SetTesselationShaderParameter SetShaderParameter",
		"Provide equivalent native family variants or an evaluated compiler route, preserve handle/log/reflection/uniform behavior; geometry/tessellation needs explicit family lowering.",
	),
	"engine-buffer-schema": (
		"GetEngineUniformBufferDef GetEngineModelUniformDataDef GetEngineModelUniformDataSize SetUnitBufferUniforms SetFeatureBufferUniforms",
		"Stable native buffer schemas with explicit offsets and bindings; GL UBO and SSBO indices are separate namespaces.",
	),
	"buffer-layout-and-draw": (
		"GetVBO GetVAO",
		"Native vertex/index/instance/storage buffers, upload/retirement and draw descriptors; preserve types, normalization, stride, divisor and base offsets.",
	),
	"compute-and-dependency": (
		"BindImageTexture DispatchCompute MemoryBarrier",
		"Native compute pipelines, storage textures and dispatch plus producer/consumer stage-access dependencies.",
	),
	"font-atlas-and-layout": (
		"BeginText Text EndText GetTextWidth GetTextHeight LoadFont DeleteFont AddFallbackFont ClearFallbackFonts",
		"Retain existing text layout/glyph metrics and font atlas draws/resources.",
	),
	"engine-model-draw": (
		"Unit UnitRaw UnitTextures UnitShape UnitShapeTextures UnitMultMatrix UnitPiece UnitPieceMatrix UnitPieceMultMatrix Feature FeatureRaw FeatureTextures FeatureShape FeatureShapeTextures FeatureMultMatrix FeaturePiece FeaturePieceMatrix FeaturePieceMultMatrix",
		"Use engine-owned geometry, piece transforms, textures and visibility; emit native indexed/instanced draws without changing synced Lua.",
	),
	"engine-pass-service": (
		"DrawMiniMap DrawMiniMapIcons SlaveMiniMap ConfigMiniMap SwapBuffers GetShadowMapParams GetAtmosphere GetSun GetWaterRendering GetMapRendering",
		"Retain engine pass/camera/lighting metadata and scheduling services; do not replace custom PIP with a reduced fallback.",
	),
	"query-and-readback": (
		"Flush Finish ReadPixels SaveImage CreateQuery DeleteQuery RunQuery GetQuery QueryCounter GetQueryDelta",
		"Native submission/completion, occlusion query and readback behavior including callback/result timing.",
	),
	"debug-markers": (
		"ObjectLabel PushDebugGroup PopDebugGroup",
		"Native resource labels and encoder debug scopes.",
	),
	"game-owned-helper": (
		"LuaShader InstanceVBOTable InstanceVBOIdTable R2tHelper",
		"Keep the pinned game module installed by modules/graphics/init.lua and supply its underlying graphics contracts.",
	),
}

API_GROUP = {name: group for group, (names, _) in GROUPS.items() for name in names.split()}
STAGE_KEYS = {"vertex": "vertex", "fragment": "fragment", "geometry": "geometry", "tcs": "tessellation-control", "tes": "tessellation-evaluation", "compute": "compute"}
LONG_OPEN = re.compile(r"\[(=*)\[")


def mask_lua(source: str) -> tuple[str, list[tuple[int, str]], list[str]]:
	"""Mask Lua 5.1 comments/strings while preserving positions/newlines.

	Returns string payloads for bounded shader/dependency inspection. This is a
	lexer, not a parser or evaluator; malformed strings are reported explicitly.
	"""
	masked = list(source)
	strings = []
	errors = []
	i = 0
	while i < len(source):
		comment = source.startswith("--", i)
		start = i
		probe = i + 2 if comment else i
		long_match = LONG_OPEN.match(source, probe)
		if long_match:
			body_start = long_match.end()
			close = "]" + long_match.group(1) + "]"
			end = source.find(close, body_start)
			if end < 0:
				errors.append(f"unterminated long token at line {line_at(source, i)}")
				end = len(source)
			if not comment:
				strings.append((body_start, source[body_start:end]))
			i = min(end + len(close), len(source))
		elif comment:
			i = source.find("\n", i)
			if i < 0:
				i = len(source)
		elif source[i] in "\"'":
			quote = source[i]
			i += 1
			body_start = i
			while i < len(source) and source[i] != quote:
				i += 2 if source[i] == "\\" else 1
			if i >= len(source):
				errors.append(f"unterminated quoted string at line {line_at(source, start)}")
			strings.append((body_start, source[body_start:i]))
			i = min(i + 1, len(source))
		else:
			i += 1
			continue
		for j in range(start, i):
			if masked[j] not in "\r\n":
				masked[j] = " "
	return "".join(masked), strings, errors


def line_at(source: str, offset: int) -> int:
	return source.count("\n", 0, offset) + 1


def git_tree(root: Path, commit: str) -> dict[str, str]:
	data = subprocess.check_output(["git", "-C", str(root), "ls-tree", "-rz", "--full-tree", commit])
	result = {}
	for item in data.split(b"\0"):
		if not item:
			continue
		metadata, path = item.split(b"\t", 1)
		_, kind, blob = metadata.split()
		if kind == b"blob":
			result[path.decode()] = blob.decode()
	return result


def source_record(repository: str, path: str, data: bytes, blob: str, location: str) -> dict:
	actual_blob = hashlib.sha1(b"blob " + str(len(data)).encode() + b"\0" + data).hexdigest()
	canonical = data.replace(b"\r\n", b"\n")
	canonical_blob = hashlib.sha1(b"blob " + str(len(canonical)).encode() + b"\0" + canonical).hexdigest()
	return {
		"id": repository + ":" + path,
		"repository": repository,
		"path": path,
		"bytes": len(data),
		"sha256": hashlib.sha256(data).hexdigest(),
		"pinned_git_blob": blob,
		"matches_pinned_git_blob": actual_blob == blob or canonical_blob == blob,
		"git_blob_match_mode": "exact-bytes" if actual_blob == blob else "checkout-crlf-to-lf" if canonical_blob == blob else "mismatch",
		"canonical_sha256": hashlib.sha256(data if actual_blob == blob else canonical).hexdigest(),
		"read_from": location,
	}


def mask_shader_comments(text: str) -> str:
	return re.sub(r"/\*[\s\S]*?\*/|//[^\r\n]*", lambda match: "".join(char if char in "\r\n" else " " for char in match.group()), text)


def shader_requirements(text: str, stages: set[str]) -> list[str]:
	requirements = set()
	if "__ENGINEUNIFORMBUFFERDEFS__" in text or "__QUATERNIONDEFS__" in text:
		requirements.add("engine-schema-injection-and-binding-remap")
	text = mask_shader_comments(text)
	if "geometry" in stages or re.search(r"\b(?:EmitVertex|EndPrimitive|gl_in|gl_InvocationID)\b", text):
		requirements.add("family-specific-equivalent-geometry-expansion")
	if "compute" in stages or "gl_GlobalInvocationID" in text:
		requirements.add("native-compute-and-consumer-dependency")
	if any(stage.startswith("tessellation") for stage in stages):
		requirements.add("native-tessellation-or-equivalent-family-expansion")
	if re.search(r"\b(?:gl_Vertex|gl_ModelViewMatrix|gl_ProjectionMatrix|gl_TexCoord|gl_FrontColor|ftransform|gl_NormalMatrix)\b", text):
		requirements.add("compatibility-builtins-and-matrix-uniforms")
	if re.search(r"\bbuffer\s+\w+\s*\{", text):
		requirements.add("native-storage-buffer-schema")
	if re.search(r"\b(?:sampler2DRect|TEXTURE_RECTANGLE)\b", text):
		requirements.add("pixel-coordinate-texture-sampling")
	if re.search(r"\b(?:gl_FragData|fragData|out\s+vec4\s+\w+\s*\[)\b", text):
		requirements.add("multiple-render-target-contract")
	if "gl_ClipDistance" in text:
		requirements.add("clip-plane-and-depth-convention")
	if any(stage in stages for stage in ("vertex", "fragment")):
		requirements.add("native-vertex-fragment-family-and-reflection")
	return sorted(requirements)


def infer_asset_stages(path: str) -> set[str]:
	name = Path(path).name.lower()
	result = set()
	for pattern, stage in ((r"(?:\.vert\.|\.vert$|\.vs$|\.vp$|vertprog)", "vertex"), (r"(?:\.frag\.|\.frag$|\.fs$|\.fp$|fragprog)", "fragment"), (r"(?:\.geom\.|\.geom$)", "geometry"), (r"(?:\.comp\.|\.comp$)", "compute"), (r"\.tcs", "tessellation-control"), (r"\.tes", "tessellation-evaluation")):
		if re.search(pattern, name):
			result.add(stage)
	return result


def main() -> None:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--bar-root", type=Path, required=True)
	parser.add_argument("--recoil-root", type=Path, required=True)
	parser.add_argument("--bar-supplement-root", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	trees = {"bar": git_tree(args.bar_root, BAR_COMMIT), "recoil": git_tree(args.recoil_root, RECOIL_COMMIT)}
	sources = {}
	api_uses = defaultdict(list)
	aliases = []
	shaders = []
	missing = []
	lex_errors = []
	registrations = defaultdict(list)
	usertype_methods = defaultdict(list)

	# Engine exports are scoped to their actual gl PushEntries functions. Font
	# userdata methods are deliberately separated from top-level gl exports.
	for filename in ("LuaOpenGL.cpp", "LuaShaders.cpp", "LuaFBOs.cpp", "LuaRBOs.cpp", "LuaFonts.cpp", "LuaVAO.cpp", "LuaVBO.cpp"):
		path = "rts/Lua/" + filename
		data = (args.recoil_root / path).read_bytes()
		text = data.decode("utf-8")
		sources["recoil:" + path] = source_record("recoil", path, data, trees["recoil"][path], "checkout")
		start = text.index("::PushEntries(")
		start = text.index("{", start)
		depth = 1
		end = start + 1
		while depth:
			depth += (text[end] == "{") - (text[end] == "}")
			end += 1
		for match in re.finditer(r"REGISTER_LUA_CFUNC\((\w+)\)", text[start:end]):
			registrations[match.group(1)].append({"source": "recoil:" + path, "line": line_at(text, start + match.start())})
		for match in re.finditer(r'REGISTER_NAMED_LUA_CFUNC\("(\w+)"\s*,\s*(\w+)\)', text[start:end]):
			registrations[match.group(1)].append({"source": "recoil:" + path, "line": line_at(text, start + match.start()), "implementation_alias": match.group(2)})
		if filename in ("LuaVAO.cpp", "LuaVBO.cpp"):
			kind = "VAO" if filename == "LuaVAO.cpp" else "VBO"
			for match in re.finditer(r'"(\w+)"\s*,\s*(?:&|sol::overload)', text[start:end]):
				usertype_methods[kind].append({"name": match.group(1), "source": "recoil:" + path, "line": line_at(text, start + match.start()), "required_equivalent": "native-buffer/layout/lifetime or indexed/instanced/submission operation; preserve exact method arguments"})

	# All pinned tracked Lua files in the game are considered; shaders are scoped
	# to official game/engine shader assets. Absent sparse files are explicit.
	for repository, root in (("bar", args.bar_root), ("recoil", args.recoil_root)):
		for path, blob in sorted(trees[repository].items()):
			is_lua = repository == "bar" and path.lower().endswith(".lua")
			is_shader = Path(path).suffix.lower() == ".glsl" or ("/shaders/" in ("/" + path.lower()) and Path(path).suffix.lower() in {".vert", ".frag", ".geom", ".comp", ".vs", ".fs", ".vp", ".fp"})
			if not (is_lua or is_shader):
				continue
			file = root / path
			location = "checkout"
			if not file.is_file() and repository == "bar" and args.bar_supplement_root:
				file = args.bar_supplement_root / path
				location = "pinned-official-raw-supplement"
			if not file.is_file():
				missing.append({"repository": repository, "path": path, "pinned_git_blob": blob})
				continue
			data = file.read_bytes()
			text = data.decode("utf-8", errors="replace")
			source_id = repository + ":" + path
			stages = infer_asset_stages(path)
			stage_declarations = []
			dependencies = []
			shader_text = text if is_shader else ""
			uses = []
			if is_lua:
				code, strings, errors = mask_lua(text)
				lex_errors.extend({"source": source_id, "error": error} for error in errors)
				matches = list(re.finditer(r"\bgl\s*\.\s*(\w+)\b", code))
				# Bracket access is matched only when the gl token is unmasked code.
				matches += [match for match in re.finditer(r'''\bgl\s*\[\s*["'](\w+)["']\s*\]''', text) if code[match.start():match.start() + 2] == "gl"]
				for match in matches:
					name = match.group(1)
					entry = {"source": source_id, "line": line_at(text, match.start()), "direct_call": code[match.end():].lstrip().startswith("(")}
					api_uses[name].append(entry)
					uses.append(name)
					alias = re.search(r"\b([A-Za-z_]\w*)\s*=\s*$", code[max(0, match.start() - 120):match.start()])
					if alias:
						alias_name = alias.group(1)
						calls = [line_at(text, call.start()) for call in re.finditer(r"\b" + re.escape(alias_name) + r"\s*\(", code)]
						aliases.append({"source": source_id, "line": entry["line"], "alias": alias_name, "api": name, "candidate_call_lines": calls, "scope_resolution": "file lexical candidate only"})
				for match in re.finditer(r"\b(vertex|fragment|geometry|tcs|tes|compute)\s*=", code):
					stages.add(STAGE_KEYS[match.group(1)])
					stage_declarations.append({"stage": STAGE_KEYS[match.group(1)], "line": line_at(text, match.start())})
				for offset, payload in strings:
					if re.search(r"(?:#version\s+\d+|void\s+main\s*\(|gl_Position|EmitVertex\s*\(|__ENGINEUNIFORMBUFFERDEFS__)", payload):
						shader_text += "\n" + payload
					if re.fullmatch(r"[A-Za-z0-9_./-]+\.(?:glsl|vs|fs|vert|frag|geom|comp)", payload):
						dependencies.append({"path_expression": payload, "line": line_at(text, offset), "resolution": "literal string only; runtime VFS/concatenation not evaluated"})
			shader_code = mask_shader_comments(shader_text)
			if re.search(r"\b(?:EmitVertex|EndPrimitive)\s*\(", shader_code):
				stages.add("geometry")
			if "gl_GlobalInvocationID" in shader_code or "gl_LocalInvocationID" in shader_code:
				stages.add("compute")
			if uses or stages or shader_text or dependencies or path in {"init.lua", "modules/graphics/init.lua"}:
				sources[source_id] = source_record(repository, path, data, blob, location)
			if stages or shader_text or dependencies:
				shaders.append({"source": source_id, "kind": "shader-asset" if is_shader else "lua-stage-or-source-candidate", "stage_candidates": sorted(stages), "stage_declarations": stage_declarations, "literal_dependencies": dependencies, "glsl_versions": sorted(set(re.findall(r"#version\s+(\d+)", shader_code))), "required_equivalents": shader_requirements(shader_text, stages), "status": "native-family-implementation-and-visual-validation-required"})

	# Schemas/material roots used by the reusable subset are explicitly included
	# even if they contain no Lua gl token or stage marker.
	for repository, paths in {
		"bar": ("modelmaterials_gl4/templates/defaultMaterialTemplate.lua", "modelmaterials_gl4/templates/cus_gl4.vert.glsl", "modelmaterials_gl4/templates/cus_gl4.frag.glsl", "modelmaterials_gl4/001_units_s3o_assimp.lua", "luarules/gadgets/cus_gl4.lua", "luarules/gadgets/api_pbr_enabler.lua", "units/armcom.lua"),
		"recoil": ("rts/Rendering/Models/VertexData.hpp", "rts/Rendering/GL/myGL.h", "rts/Lua/LuaVBOImpl.cpp", "rts/Lua/LuaVAOImpl.cpp", "rts/Rendering/UniformConstants.h", "rts/Rendering/Models/ModelsMemStorageDefs.h", "rts/Rendering/Models/S3OParser.cpp", "rts/Rendering/Textures/Bitmap.cpp"),
	}.items():
		root = args.bar_root if repository == "bar" else args.recoil_root
		for path in paths:
			data = (root / path).read_bytes()
			sources[repository + ":" + path] = source_record(repository, path, data, trees[repository][path], "checkout")

	apis = []
	for name in sorted(set(registrations) | set(api_uses)):
		group = API_GROUP.get(name, "manual-semantic-review-required")
		uses = api_uses.get(name, [])
		apis.append({"name": "gl." + name, "owner": "engine" if name in registrations else "game-helper" if group == "game-owned-helper" else "unresolved-static-reference", "registration": registrations.get(name, []), "code_reference_count": len(uses), "direct_call_count": sum(use["direct_call"] for use in uses), "sources": sorted({use["source"] for use in uses}), "references": sorted(uses, key=lambda use: (use["source"], use["line"])), "migration_group": group, "required_equivalent": GROUPS[group][1] if group in GROUPS else "Resolve owner, exact signature and effects before providing an equivalent; never disable or silently ignore.", "status": "migration-contract-only"})

	result = {
		"schema_version": 1,
		"repositories": {"bar": {"commit": BAR_COMMIT, "url": "https://github.com/beyond-all-reason/Beyond-All-Reason"}, "recoil": {"commit": RECOIL_COMMIT, "url": "https://github.com/beyond-all-reason/RecoilEngine"}},
		"scope": "All physically present pinned tracked BAR Lua plus supplied official module supplements; official game and engine shader assets. Immutable source records include SHA-256 and pinned Git blob checks.",
		"limitations": ["Static Lua lexer/reference inventory, not a complete call graph or runtime frequency measurement.", "Single-name local aliases have file-level candidate calls only; multi-assignment aliases, reassignment, lexical shadowing, dynamic dispatch, VFS includes and shader concatenation are not fully resolved.", "Stage keys and shader string markers are candidates; configurations/dead/optional code may be included. Shader source concatenation and CheckShaderUpdates variants require runtime tracing.", "Sparse absent tracked Lua/shader files are enumerated; external maps/widgets and untracked runtime content are outside scope.", "Operation classifications are implementation obligations, not Metal capabilities or evidence of visual equivalence."],
		"summary": {"engine_gl_exports": len(registrations), "observed_gl_names": len(api_uses), "lua_code_gl_references": sum(map(len, api_uses.values())), "immutable_sources": len(sources), "shader_source_candidates": len(shaders), "missing_sparse_files": len(missing), "alias_bindings": len(aliases), "lexer_errors": len(lex_errors), "unclassified_engine_exports": [name for name in sorted(registrations) if name not in API_GROUP], "unresolved_static_gl_names": [api["name"] for api in apis if api["owner"] == "unresolved-static-reference"], "shader_stage_candidate_counts": dict(sorted(Counter(stage for shader in shaders for stage in shader["stage_candidates"]).items()))},
		"apis": apis,
		"usertype_methods": dict(usertype_methods),
		"alias_bindings": aliases,
		"shader_sources": shaders,
		"immutable_sources": [sources[key] for key in sorted(sources)],
		"missing_sparse_files": missing,
		"lexer_errors": lex_errors,
	}
	args.output.parent.mkdir(parents=True, exist_ok=True)
	args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
	print(json.dumps(result["summary"], indent=2, sort_keys=True))
	if any(not source["matches_pinned_git_blob"] for source in sources.values()):
		raise SystemExit("Source hash mismatch: inventory records source edits; inspect before using pinned evidence")


if __name__ == "__main__":
	main()
