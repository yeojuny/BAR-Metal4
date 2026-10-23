/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSUniformData.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rendering {

void NativeCUSUniformStorage::NewContext()
{
	if (generation == std::numeric_limits<uint64_t>::max())
		throw std::runtime_error("CUS uniform publication generation overflows");
	++generation;
	sequence = 0;
	current.reset();
}

void NativeCUSUniformStorage::PublishMatrices(uint64_t frame, std::span<const uint8_t> bytes)
{
	if (!generation || bytes.size() != NativeCUSMatrixBytes || sequence == std::numeric_limits<uint64_t>::max())
		throw std::runtime_error("CUS original matrix UBO publication is incomplete");
	auto matrix = std::make_shared<NativeCUSMatrixPublication>();
	matrix->contextGeneration = generation;
	matrix->sequence = sequence + 1;
	matrix->sourceDrawFrame = frame;
	std::copy(bytes.begin(), bytes.end(), matrix->bytes.begin());
	auto next = std::make_shared<NativeCUSUniformPublication>();
	if (current) *next = *current;
	next->matrices = std::move(matrix);
	current = std::move(next);
	++sequence;
}

void NativeCUSUniformStorage::PublishParams(uint64_t frame, std::span<const uint8_t> bytes,
	std::span<const uint16_t> writtenRows)
{
	if (!generation || bytes.size() != NativeCUSParamsBytes || sequence == std::numeric_limits<uint64_t>::max())
		throw std::runtime_error("CUS original parameter UBO publication is incomplete");
	for (const uint16_t row: writtenRows) if (row >= NativeCUSTeamRows)
		throw std::runtime_error("CUS original team-color writer is outside its UBO");
	auto params = std::make_shared<NativeCUSParamsPublication>();
	if (current && current->params) *params = *current->params;
	params->contextGeneration = generation;
	params->sequence = sequence + 1;
	params->sourceDrawFrame = frame;
	std::copy_n(bytes.begin(), NativeCUSParamsPrefixBytes, params->bytes.begin());
	for (const uint16_t row: writtenRows) {
		const size_t offset = NativeCUSParamsPrefixBytes + row * 16;
		std::copy_n(bytes.begin() + offset, 16, params->bytes.begin() + offset);
		params->writtenTeamRows.set(row);
	}
	auto next = std::make_shared<NativeCUSUniformPublication>();
	if (current) *next = *current;
	next->params = std::move(params);
	current = std::move(next);
	++sequence;
}

void ValidateNativeCUSUniformPublication(const NativeCUSUniformPublication& uniforms)
{
	if (!uniforms.matrices || !uniforms.params || !uniforms.matrices->contextGeneration ||
		uniforms.matrices->contextGeneration != uniforms.params->contextGeneration ||
		!uniforms.matrices->sequence || !uniforms.params->sequence)
		throw std::runtime_error("CUS original uniform blocks have no paired actual publication");
}

std::array<uint8_t, 16> ReadNativeCUSTeamColor(const NativeCUSUniformPublication& uniforms, uint32_t row)
{
	ValidateNativeCUSUniformPublication(uniforms);
	if (row >= NativeCUSTeamRows || !uniforms.params->writtenTeamRows.test(row))
		throw std::runtime_error("CUS original team-color row has no observed writer");
	std::array<uint8_t, 16> result;
	std::copy_n(uniforms.params->bytes.begin() + NativeCUSParamsPrefixBytes + row * 16, 16, result.begin());
	return result;
}

} // namespace rendering
