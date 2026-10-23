#pragma once

#include <chrono>

// Throttler is a token bucket rate limiter.
class Throttler
{
public:
	Throttler(unsigned req_per_sec_, unsigned burst_size_);
	void refill_bucket();
	bool get_token();

private:
	double req_per_msec;
	unsigned burst_size, bucket;
	unsigned generated = 0;
	std::chrono::steady_clock::time_point start_time;
};
