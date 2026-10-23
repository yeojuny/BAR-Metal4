/* This file is part of pr-downloader (GPL v2 or later), see the LICENSE file */

#include "IHash.h"
#include "Logger.h"

#include <stdio.h>

bool IHash::compare(const IHash* checksum) const
{
	assert(getSize() > 0 && checksum->getSize() > 0);
	if (checksum == nullptr)  // can't compare, so guess checksums are fine
		return true;
	if (checksum->getSize() != getSize())
		return false;
	for (int i = 0; i < getSize(); i++) {
		if (get(i) != checksum->get(i))
			return false;
	}
	return true;
}

bool IHash::compare(const unsigned char* data, int size) const
{
	assert(getSize() > 0 && size > 0);
	if (getSize() != size)
		return false;
	for (int i = 0; i < getSize(); i++) {
		unsigned char tmp = data[i];
		if (get(i) != tmp) {
			LOG_INFO("compare failed(): %s %s", toString().c_str(), toString(data, size).c_str());
			return false;
		}
	}
	return true;
}

constexpr char hexstr[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                             '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string IHash::toString(const unsigned char* data, int size)
{
	std::string str(size * 2, 0);
	for (int i = 0; i < size; i++) {
		str[i * 2] = hexstr[data[i] >> 4];
		str[i * 2 + 1] = hexstr[data[i] & 0x0f];
	}
	return str;
}

int IHash::getVal(char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;
	if ((c >= 'A') && (c <= 'F'))
		return c - 'A' + 10;
	return -1;
}

bool IHash::Set(const std::string& hash)
{
	unsigned char buf[256];
	if (hash.size() > sizeof(buf)) {
		LOG_ERROR("IHash::Set(): buffer to small");
		return false;
	}
	if (hash.size() % 2 != 0) {
		LOG_ERROR("IHash::Set(): buffer%2  != 0");
		return false;
	}
	for (unsigned i = 0; i < hash.size() / 2; i++) {
		int h = getVal(hash.at(i * 2)) * 16;
		int l = getVal(hash.at((i * 2) + 1));
		if (h < 0 || l < 0) {
			LOG_ERROR("IHash::Set(): invalid character");
			return false;
		}
		buf[i] = l + h;
	}
	if (!Set(buf, hash.size() / 2)) {
		LOG_ERROR("IHash:Set(): Error setting");
		return false;
	}
	isset = true;
	return true;
}

bool IHash::isSet() const
{
	return isset;
}
