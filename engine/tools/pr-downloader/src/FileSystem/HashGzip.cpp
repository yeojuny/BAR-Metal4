/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "HashGzip.h"
#include "FileSystem.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <zlib.h>

HashGzip::HashGzip(std::unique_ptr<IHash> hash)
	: subhash(std::move(hash))
{
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	[[maybe_unused]] int ret = inflateInit2(&strm, 32);
	assert(ret == Z_OK);
}

HashGzip::~HashGzip()
{
	[[maybe_unused]] int ret = inflateEnd(&strm);
	assert(ret == Z_OK);
}

void HashGzip::Init()
{
	isset = false;
	error = false;
	stream_done = false;
	[[maybe_unused]] int ret = inflateReset(&strm);
	assert(ret == Z_OK);
	subhash->Init();
}

void HashGzip::Update(const char* data, const int size)
{
	if (error || stream_done)
		return;

	constexpr int out_size = IO_BUF_SIZE * 2;  // * 2 because decompressing.
	unsigned char out[out_size];
	strm.avail_in = size;
	strm.next_in =
		reinterpret_cast<Bytef*>(const_cast<char*>(data));  // inflate doesn't modify data.
	do {
		strm.avail_out = out_size;
		strm.next_out = out;
		int ret = inflate(&strm, Z_SYNC_FLUSH);
		assert(ret != Z_STREAM_ERROR);
		switch (ret) {
			case Z_NEED_DICT:
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				error = true;
				return;
			case Z_STREAM_END:
				stream_done = true;
				[[fallthrough]];
			case Z_OK:
				subhash->Update(reinterpret_cast<char*>(out), out_size - strm.avail_out);
				break;
			case Z_BUF_ERROR:
				break;
		}
	} while (strm.avail_out == 0 && !stream_done);
	assert(stream_done || strm.avail_in == 0);
}

void HashGzip::Final()
{
	isset = true;
	if (!stream_done)
		error = true;
	subhash->Final();
}

bool HashGzip::Set(const unsigned char* data, int size)
{
	bool ret = subhash->Set(data, size);
	if (ret) {
		isset = true;
		error = false;
	}
	return ret;
}
