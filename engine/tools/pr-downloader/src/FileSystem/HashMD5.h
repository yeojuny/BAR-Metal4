/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#pragma once

#include "IHash.h"
#include "lib/md5/md5.h"

class HashMD5 : public IHash
{
public:
	void Init() override;
	void Final() override;
	void Update(const char* data, const int size) override;
	bool Set(const unsigned char* data, int size) override;
	unsigned char get(int pos) const override;
	const unsigned char* Data() const
	{
		return &mdContext.digest[0];
	}

	int getSize() const override
	{
		return sizeof(mdContext.digest);
	}

	std::string toString() const override
	{
		return IHash::toString(&mdContext.digest[0], sizeof(mdContext.digest));
	}

private:
	MD5_CTX mdContext = {};
};
