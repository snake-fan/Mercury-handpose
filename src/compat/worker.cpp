// Copyright 2022, Collabora, Ltd.
// Copyright 2026 Zhang
// SPDX-License-Identifier: BSL-1.0

#include "util/u_worker.h"

#include "util/u_logging.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace {
struct worker_group_impl
{
	std::mutex mutex;
	std::condition_variable completed;
	std::size_t pending = 0;
};

struct queued_task
{
	worker_group_impl *group = nullptr;
	u_worker_group_func_t function = nullptr;
	void *data = nullptr;
};

struct worker_pool_impl
{
	std::mutex mutex;
	std::condition_variable available;
	std::deque<queued_task> queue;
	std::vector<std::thread> threads;
	bool stopping = false;
};

void
worker_loop(worker_pool_impl *pool)
{
	while (true) {
		queued_task task{};
		{
			std::unique_lock<std::mutex> lock(pool->mutex);
			pool->available.wait(lock, [&] { return pool->stopping || !pool->queue.empty(); });
			if (pool->stopping && pool->queue.empty()) {
				return;
			}
			task = pool->queue.front();
			pool->queue.pop_front();
		}

		try {
			task.function(task.data);
		} catch (const std::exception &error) {
			U_LOG_E("Unhandled exception in Mercury worker task: %s", error.what());
		} catch (...) {
			U_LOG_E("Unhandled non-standard exception in Mercury worker task");
		}

		{
			std::lock_guard<std::mutex> lock(task.group->mutex);
			--task.group->pending;
		}
		task.group->completed.notify_all();
	}
}
} // namespace

extern "C" struct u_worker_thread_pool *
u_worker_thread_pool_create(uint32_t, uint32_t thread_count, const char *)
{
	if (thread_count == 0) {
		return nullptr;
	}
	auto *pool = new u_worker_thread_pool{};
	pool->reference.count = 1;
	pool->thread_count = thread_count;
	auto *impl = new worker_pool_impl{};
	pool->implementation = impl;
	try {
		impl->threads.reserve(thread_count);
		for (std::uint32_t index = 0; index < thread_count; ++index) {
			impl->threads.emplace_back(worker_loop, impl);
		}
	} catch (...) {
		{
			std::lock_guard<std::mutex> lock(impl->mutex);
			impl->stopping = true;
		}
		impl->available.notify_all();
		for (auto &thread : impl->threads) {
			if (thread.joinable()) thread.join();
		}
		delete impl;
		delete pool;
		return nullptr;
	}
	return pool;
}

extern "C" void
u_worker_thread_pool_destroy(struct u_worker_thread_pool *pool)
{
	if (pool == nullptr) return;
	auto *impl = static_cast<worker_pool_impl *>(pool->implementation);
	{
		std::lock_guard<std::mutex> lock(impl->mutex);
		impl->stopping = true;
	}
	impl->available.notify_all();
	for (auto &thread : impl->threads) {
		if (thread.joinable()) thread.join();
	}
	delete impl;
	delete pool;
}

extern "C" struct u_worker_group *
u_worker_group_create(struct u_worker_thread_pool *pool)
{
	if (pool == nullptr) {
		return nullptr;
	}
	auto *group = new u_worker_group{};
	group->reference.count = 1;
	group->implementation = new worker_group_impl{};
	group->pool = nullptr;
	u_worker_thread_pool_reference(&group->pool, pool);
	return group;
}

extern "C" void
u_worker_group_push(struct u_worker_group *group, u_worker_group_func_t function, void *data)
{
	auto &group_impl = *static_cast<worker_group_impl *>(group->implementation);
	auto &pool_impl = *static_cast<worker_pool_impl *>(group->pool->implementation);
	{
		std::lock_guard<std::mutex> lock(group_impl.mutex);
		++group_impl.pending;
	}
	{
		std::lock_guard<std::mutex> lock(pool_impl.mutex);
		pool_impl.queue.push_back({&group_impl, function, data});
	}
	pool_impl.available.notify_one();
}

extern "C" void
u_worker_group_wait_all(struct u_worker_group *group)
{
	auto &impl = *static_cast<worker_group_impl *>(group->implementation);
	std::unique_lock<std::mutex> lock(impl.mutex);
	impl.completed.wait(lock, [&] { return impl.pending == 0; });
}

extern "C" void
u_worker_group_destroy(struct u_worker_group *group)
{
	u_worker_group_wait_all(group);
	delete static_cast<worker_group_impl *>(group->implementation);
	u_worker_thread_pool_reference(&group->pool, nullptr);
	delete group;
}
