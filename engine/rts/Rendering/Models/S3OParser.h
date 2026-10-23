/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include "3DModelPiece.hpp"
#include "IModelParser.h"
#include "S3OData.h"

#include "System/type2.h"
#include "System/Threading/SpringThreading.h"

enum {
	S3O_PRIMTYPE_TRIANGLES      = s3o::PRIMTYPE_TRIANGLES,
	S3O_PRIMTYPE_TRIANGLE_STRIP = s3o::PRIMTYPE_TRIANGLE_STRIP,
	S3O_PRIMTYPE_QUADS          = s3o::PRIMTYPE_QUADS,
};


struct SS3OPiece: public S3DModelPiece {
public:
	SS3OPiece() = default;
	SS3OPiece(const SS3OPiece&) = delete;
	SS3OPiece(SS3OPiece&& p) { *this = std::move(p); }

	SS3OPiece& operator = (const SS3OPiece& p) = delete;
	SS3OPiece& operator = (SS3OPiece&& p) {
		#if 0
		// piece is never actually moved, just need the operator for pool
		vertices = std::move(p.vertices);
		indices = std::move(p.indices);

		primType = p.primType;
		#endif
		return *this;
	}

	void Clear() override {
		S3DModelPiece::Clear();
		primType = S3O_PRIMTYPE_TRIANGLES;
	}
public:
	void SetVertexCount(unsigned int n) { vertices.resize(n); }
	void SetIndexCount(unsigned int n) { indices.resize(n); }
	void SetVertex(int idx, const SVertexData& v) { vertices[idx] = v; }
	void SetIndex(int idx, const unsigned int drawIdx) { indices[idx] = drawIdx; }

	void Trianglize();
	void SetMinMaxExtends();
	void SetVertexTangents();

public:
	int primType = S3O_PRIMTYPE_TRIANGLES;
};



class CS3OParser: public IModelParser
{
public:
	void Init() override;
	void Kill() override;

	void Load(S3DModel& model, const std::string& name) override;

private:
	SS3OPiece* AllocPiece();
	SS3OPiece* LoadPiece(S3DModel*, SS3OPiece*, const s3o::DecodedModel&, uint32_t pieceIndex);

private:
	std::vector<SS3OPiece> piecePool;
	spring::mutex poolMutex;

	unsigned int numPoolPieces = 0;
};
