#include "../../include/job/job.h"
#include "../../include/memory/memory.h"
#include "../../include/core/log.h"

#include <thread>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <cstring>

struct JobCounter_Impl {
    std::atomic<int> active_jobs;
    std::atomic<int> ref_count;
};

struct Job{
    ks_callback function;
    Ks_Payload payload;
    JobCounter_Impl* counter;
};

static const size_t COUNTER_CHUNK_SIZE = 256;

struct JobCounterChunk {
    JobCounter_Impl counters[COUNTER_CHUNK_SIZE];
};

class CounterPool {
public:
    std::vector<JobCounterChunk*> chunks;
    std::vector<JobCounter_Impl*> free_list;
    std::mutex mtx;

    CounterPool() {
        free_list.reserve(COUNTER_CHUNK_SIZE * 4);
    }

    ~CounterPool() {
        for (auto chunk : chunks) {
            for (size_t i = 0; i < COUNTER_CHUNK_SIZE; i++) {
                chunk->counters[i].~JobCounter_Impl();
            }
            ks_dealloc(chunk);
        }
        chunks.clear();
        free_list.clear();
    }

    void init(size_t initial_chunks) {
        for (size_t i = 0; i < initial_chunks; ++i) {
            expand_pool();
        }
    }

    JobCounter_Impl* allocate() {
        std::lock_guard<std::mutex> lock(mtx);

        if (free_list.empty()) {
            expand_pool();
        }

        JobCounter_Impl* c = free_list.back();
        free_list.pop_back();

        new (c) JobCounter_Impl();
        c->active_jobs.store(0);
        c->ref_count.store(0);

        return c;
    }

    void deallocate(JobCounter_Impl* c) {
        std::lock_guard<std::mutex> lock(mtx);
        free_list.push_back(c);
    }
private:
    void expand_pool() {
        void* mem = ks_alloc(sizeof(JobCounterChunk), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA);
        JobCounterChunk* new_chunk = new(mem) JobCounterChunk();

        chunks.push_back(new_chunk);

        for (int i = COUNTER_CHUNK_SIZE - 1; i >= 0; --i) {
            free_list.push_back(&new_chunk->counters[i]);
        }
    }
};

struct JobManager_Impl {
    std::vector<std::thread> workers;
    std::deque<Job> queue;
    std::mutex queue_mutex;
    std::condition_variable cv_worker;

    CounterPool counter_pool;

    std::atomic<bool> stop_flag;
    uint32_t num_threads;

    JobManager_Impl() : stop_flag(false) {

        counter_pool.init(4);

        unsigned int cores = std::thread::hardware_concurrency();
        num_threads = (cores > 1) ? (cores - 1) : 1;

        KS_LOG_INFO("[JobSystem] Spawning %d worker threads", num_threads);

        for (uint32_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] { this->worker_loop(); });
        }
    }

    ~JobManager_Impl() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop_flag = true;
        }
        cv_worker.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) worker.join();
        }
    }

    void release_counter(JobCounter_Impl* c) {
        if (!c) return;
        if (c->ref_count.fetch_sub(1) == 1) {
            counter_pool.deallocate(c);
        }
    }

    void execute_job(Job& job) {
        if (job.function) {
            job.function(job.payload);
            if (job.payload.owns_data && job.payload.data) {
                if (job.payload.free_fn) {
                    job.payload.free_fn(job.payload.data);
                }
                else {
                    ks_dealloc(job.payload.data);
                }
            }
        }

        if (job.counter) {
            job.counter->active_jobs.fetch_sub(1);
            release_counter(job.counter);
        }
    }


    void worker_loop() {
        while (true) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                cv_worker.wait(lock, [this] { return stop_flag || !queue.empty(); });

                if (stop_flag && queue.empty()) return;

                if (!queue.empty()) {
                    job = queue.front();
                    queue.pop_front();
                }
                else {
                    continue;
                }
            }
            execute_job(job);
        }
    }

    bool try_execute_work_stealing() {
        Job job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (queue.empty()) return false;
            job = queue.front();
            queue.pop_front();
        }
        execute_job(job);
        return true;
    }
};



static JobManager_Impl* impl(Ks_JobManager js) { return (JobManager_Impl*)js; }
static JobCounter_Impl* ctr(Ks_JobCounter c) { return (JobCounter_Impl*)c; }

KS_API Ks_JobManager ks_job_manager_create() {
    void* mem = ks_alloc(sizeof(JobManager_Impl), KS_LT_USER_MANAGED, KS_TAG_INTERNAL_DATA);
    return (Ks_JobManager) new(mem) JobManager_Impl();
}

KS_API ks_no_ret ks_job_manager_destroy(Ks_JobManager js) {
    if (js) {
        JobManager_Impl* s = impl(js);
        s->~JobManager_Impl();
        ks_dealloc(s);
    }
}

static Ks_JobCounter submit_job(JobManager_Impl* s, ks_callback func, Ks_Payload payload, bool return_handle) {

    JobCounter_Impl* c = nullptr;

    if (return_handle) {
        c = s->counter_pool.allocate();
        c->active_jobs.store(1);
        c->ref_count.store(2);
    }

    if (payload.owns_data && payload.size > 0 && payload.data) {
        void* deep_copy = ks_alloc(payload.size, KS_LT_USER_MANAGED, KS_TAG_JOB_SYSTEM);
        memcpy(deep_copy, payload.data, payload.size);
        payload.data = deep_copy;
    }

    {
        std::lock_guard<std::mutex> lock(s->queue_mutex);
        s->queue.push_back({ func, payload, c });
    }
    s->cv_worker.notify_one();

    return (Ks_JobCounter)c;
}

KS_API Ks_JobCounter ks_job_run_impl(Ks_JobManager js, ks_callback func, Ks_Payload payload) {
    if (!js) return nullptr;
    return submit_job(impl(js), func, payload, true);
}

KS_API ks_no_ret ks_job_dispatch_impl(Ks_JobManager js, ks_callback func, Ks_Payload payload) {
    if (!js) return;
    submit_job(impl(js), func, payload, false);
}

KS_API ks_no_ret ks_job_wait(Ks_JobManager js, Ks_JobCounter counter) {
    if (!js || !counter) return;
    JobManager_Impl* s = impl(js);
    JobCounter_Impl* c = ctr(counter);

    while (c->active_jobs.load(std::memory_order_acquire) > 0) {
        bool worked = s->try_execute_work_stealing();

        if (!worked) {
            std::this_thread::yield();
        }
    }

    s->release_counter(c);
}

KS_API ks_bool ks_job_is_busy(Ks_JobManager js, Ks_JobCounter counter) {
    if (!counter) return ks_false;
    return ctr(counter)->active_jobs.load(std::memory_order_relaxed) > 0;
}

KS_API uint32_t ks_job_system_get_thread_count(Ks_JobManager js) {
    return js ? impl(js)->num_threads : 0;
}