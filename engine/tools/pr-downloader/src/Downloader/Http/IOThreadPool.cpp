#include <cassert>
#include <random>
#include <thread>
#include <variant>

#include "IOThreadPool.h"

IOThreadPool::IOThreadPool(unsigned poolSize, unsigned workQueueSlots)
	: randomGen(std::random_device{}())
	, threadDist(0, poolSize - 1)
{
	assert(poolSize > 0);
	assert(workQueueSlots > 0);
	threads.reserve(poolSize);
	// sendQ and recvQ are not accessed under mutex, the vector can't resize.
	sendQ.reserve(poolSize);
	recvQ.reserve(poolSize);
	for (unsigned id = 0; id < poolSize; ++id) {
		sendQ.emplace_back(workQueueSlots);
		recvQ.emplace_back(workQueueSlots);
		threads.emplace_back(&IOThreadPool::worker, this, id);
	}
}

IOThreadPool::~IOThreadPool()
{
	if (!threads.empty()) {
		finish();
	}
}

void IOThreadPool::pullResults()
{
	for (auto& r : recvQ) {
		std::variant<Close, RetF> res;
		while (r.try_dequeue(res)) {
			assert(std::holds_alternative<RetF>(res));
			std::get<RetF>(res)();
		}
	}
}

void IOThreadPool::finish()
{
	assert(!threads.empty());
	for (auto& s : sendQ) {
		s.wait_enqueue(Close{});
	}
	for (auto& r : recvQ) {
		while (true) {
			std::variant<Close, RetF> res;
			r.wait_dequeue(res);
			if (auto* f = std::get_if<RetF>(&res)) {
				(*f)();
			} else {
				break;
			}
		}
	}
	for (auto& t : threads) {
		t.join();
	}
	threads.clear();
}

void IOThreadPool::worker(unsigned id)
{
	auto& send = sendQ[id];
	auto& recv = recvQ[id];
	while (true) {
		std::variant<Close, WorkF> work;
		send.wait_dequeue(work);
		if (auto* f = std::get_if<WorkF>(&work)) {
			if (auto res = (*f)()) {
				recv.enqueue(std::move(res.value()));
			}
		} else {
			recv.enqueue(Close{});
			return;
		}
	}
}
