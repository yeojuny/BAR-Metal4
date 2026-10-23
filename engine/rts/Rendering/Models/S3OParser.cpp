/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include <cctype>
#include <stdexcept>

#include "S3OParser.h"
#include "3DModel.hpp"
#include "ModelUtils.h"
#include "Game/GlobalUnsynced.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Textures/S3OTextureHandler.h"
#include "Sim/Misc/CollisionVolume.h"
#include "System/Exceptions.h"
#include "System/SpringMath.h"
#include "System/StringUtil.h"
#include "System/Log/ILog.h"
#include "System/FileSystem/FileHandler.h"

#include "System/Misc/TracyDefs.h"



void CS3OParser::Init() { numPoolPieces = 0; }
void CS3OParser::Kill() {
	RECOIL_DETAILED_TRACY_ZONE;
	LOG_L(L_INFO, "[S3OParser::%s] allocated %u pieces", __func__, numPoolPieces);

	// reuse piece innards when reloading
	// piecePool.clear();
	for (unsigned int i = 0; i < numPoolPieces; i++) {
		piecePool[i].Clear();
	}

	numPoolPieces = 0;
}

void CS3OParser::Load(S3DModel& model, const std::string& name)
{
	RECOIL_DETAILED_TRACY_ZONE;
	CFileHandler file(name);
	std::vector<uint8_t> fileBuf;

	if (!file.FileExists())
		throw content_error("[S3OParser] could not find model-file " + name);

	if (!file.IsBuffered()) {
		fileBuf.resize(file.FileSize(), 0);
		file.Read(fileBuf.data(), fileBuf.size());
	} else {
		fileBuf = std::move(file.GetBuffer());
	}

	s3o::DecodedModel data;
	try {
		data = s3o::Decode(fileBuf);
	} catch (const std::runtime_error& error) {
		throw content_error("[S3OParser] " + std::string(error.what()) + " for model-file " + name);
	}

	model.name = name;
	model.type = MODELTYPE_S3O;
	model.numPieces = 0;
	model.texs[0] = data.textures[0];
	model.texs[1] = data.textures[1];
	model.mins = DEF_MIN_SIZE;
	model.maxs = DEF_MAX_SIZE;

	textureHandlerS3O.PreloadTexture(
		&model,
		false,
		false
	);

	model.FlattenPieceTree(LoadPiece(&model, nullptr, data, 0));

	// set after the extrema are known
	model.radius = (data.radius <= 0.01f)? model.CalcDrawRadius(): data.radius;
	model.height = (data.height <= 0.01f)? model.CalcDrawHeight(): data.height;
	model.relMidPos = float3(data.midpoint[0], data.midpoint[1], data.midpoint[2]);
}


SS3OPiece* CS3OParser::AllocPiece()
{
	RECOIL_DETAILED_TRACY_ZONE;
	std::lock_guard<spring::mutex> lock(poolMutex);

	// lazily reserve pool here instead of during Init
	// this way games using only one model-type do not
	// cause redundant allocation
	if (piecePool.empty())
		piecePool.resize(MAX_MODEL_OBJECTS * AVG_MODEL_PIECES);

	if (numPoolPieces >= piecePool.size()) {
		throw std::bad_alloc();
		return nullptr;
	}

	return &piecePool[numPoolPieces++];
}

SS3OPiece* CS3OParser::LoadPiece(S3DModel* model, SS3OPiece* parent, const s3o::DecodedModel& data, uint32_t pieceIndex)
{
	RECOIL_DETAILED_TRACY_ZONE;
	model->numPieces++;
	const s3o::DecodedPiece& source = data.pieces[pieceIndex];

	// create piece
	SS3OPiece* piece = AllocPiece();

	piece->offset.x = source.offset[0];
	piece->offset.y = source.offset[1];
	piece->offset.z = source.offset[2];
	piece->primType = static_cast<int>(source.primitiveType);
	piece->name = source.name;
	piece->parent = parent;
	piece->SetParentModel(model);

	// retrieve vertices
	piece->SetVertexCount(static_cast<unsigned int>(source.vertices.size()));
	for (size_t a = 0; a < source.vertices.size(); ++a) {
		const s3o::DecodedVertex& v = source.vertices[a];

		SVertexData sv;
		sv.pos = float3(v.position[0], v.position[1], v.position[2]);
		sv.normal = float3(v.normal[0], v.normal[1], v.normal[2]);

		if (sv.normal.CheckNaNs()) {
			sv.normal.SafeANormalize();
		} else {
			sv.normal = ZeroVector;
		}

		sv.texCoords[0] = float2(v.uv[0], v.uv[1]);
		sv.texCoords[1] = float2(v.uv[0], v.uv[1]);

		piece->SetVertex(static_cast<int>(a), sv);
	}

	// retrieve draw indices
	piece->SetIndexCount(static_cast<unsigned int>(source.indices.size()));
	for (size_t a = 0; a < source.indices.size(); ++a) {
		piece->SetIndex(static_cast<int>(a), source.indices[a]);
	}

	// post process the piece
	{
		piece->goffset = piece->offset + ((parent != NULL)? parent->goffset: ZeroVector);

		piece->Trianglize();
		piece->SetVertexTangents();
		piece->SetMinMaxExtends();

		model->mins = float3::min(piece->goffset + piece->mins, model->mins);
		model->maxs = float3::max(piece->goffset + piece->maxs, model->maxs);

		piece->SetCollisionVolume(CollisionVolume('b', 'z', piece->maxs - piece->mins, (piece->maxs + piece->mins) * 0.5f));
	}

	// load children pieces
	piece->children.reserve(source.children.size());

	for (const uint32_t childIndex : source.children) {
		SS3OPiece* childPiece = LoadPiece(model, piece, data, childIndex);
		piece->children.push_back(childPiece);
	}

	return piece;
}

void SS3OPiece::SetMinMaxExtends()
{
	RECOIL_DETAILED_TRACY_ZONE;
	for (const SVertexData& v: vertices) {
		mins = float3::min(mins, v.pos);
		maxs = float3::max(maxs, v.pos);
	}
}


void SS3OPiece::Trianglize()
{
	RECOIL_DETAILED_TRACY_ZONE;
	uint32_t primitiveType = static_cast<uint32_t>(primType);
	s3o::Triangulate(primitiveType, indices);
	primType = static_cast<int>(primitiveType);
}

void SS3OPiece::SetVertexTangents()
{
	ModelUtils::CalculateTangents(vertices, indices);
}
