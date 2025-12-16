#include <doctest/doctest.h>
#include <keystone.h>
#include <atomic>
#include <thread>
#include <vector>

struct TestData {
    std::atomic<int>* counter;
    int add_value;
};

void job_increment(Ks_Payload p) {
    TestData* t = (TestData*)p.data;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    t->counter->fetch_add(t->add_value);
}

static std::atomic<int> g_free_calls{ 0 };
void custom_free(ks_ptr ptr) {
    ks_dealloc(ptr);
    g_free_calls++;
}

void job_ownership(Ks_Payload p) {
}

struct ArrayProcessData {
    float* start_ptr;
    ks_size count;
};

void job_process_array(Ks_Payload p) {
    ArrayProcessData* info = (ArrayProcessData*)p.data;
    for (ks_size i = 0; i < info->count; ++i) {
        info->start_ptr[i] *= 2.0f;
    }
}

TEST_CASE("Core: Job System & Payloads") {
    ks_memory_init();
    g_free_calls = 0;

    SUBCASE("Lifecycle") {
        Ks_JobManager js = ks_job_manager_create();
        CHECK(js != nullptr);
        CHECK(ks_job_system_get_thread_count(js) > 0);
        ks_job_manager_destroy(js);
    }

    SUBCASE("Run & Wait (Stack Payload)") {
        Ks_JobManager js = ks_job_manager_create();
        std::atomic<int> counter{ 0 };
        TestData data = { &counter, 10 };

        Ks_JobCounter jc = ks_job_run(js, job_increment, .data = &data);

        ks_job_wait(js, jc);

        CHECK(counter.load() == 10);
        ks_job_manager_destroy(js);
    }

    SUBCASE("Dispatch (Heap Payload + Ownership)") {
        Ks_JobManager js = ks_job_manager_create();

        int* heap_int = (int*)ks_alloc(sizeof(int), KS_LT_USER_MANAGED, KS_TAG_JOB_SYSTEM);
        *heap_int = 42;

        ks_job_dispatch(js, job_ownership, .data = heap_int, .owns_data = true, .free_fn = custom_free);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        CHECK(g_free_calls.load() == 1);
        ks_job_manager_destroy(js);
    }

    SUBCASE("Concurrency & Stress Test") {
        Ks_JobManager js = ks_job_manager_create();
        std::atomic<int> counter{ 0 };
        TestData data = { &counter, 1 };

        int count = 100;
        std::vector<Ks_JobCounter> handles;

        for (int i = 0; i < count; ++i) {
            handles.push_back(ks_job_run(js, job_increment, .data = &data));
        }

        for (auto h : handles) {
            ks_job_wait(js, h);
        }

        CHECK(counter.load() == count);
        ks_job_manager_destroy(js);
    }

    SUBCASE("Real World: Data Parallelism (No Atomics)") {
        Ks_JobManager js = ks_job_manager_create();

        const int SIZE = 10000;
        std::vector<float> data(SIZE);
        for (int i = 0; i < SIZE; ++i) data[i] = 1.0f;

        int chunks = 4;
        int chunk_size = SIZE / chunks;

        std::vector<Ks_JobCounter> jobs;
        std::vector<ArrayProcessData> payloads(chunks);

        for (int i = 0; i < chunks; ++i) {
            payloads[i].start_ptr = &data[i * chunk_size];
            payloads[i].count = chunk_size;
            jobs.push_back(ks_job_run(js, job_process_array, .data = &payloads[i]));
        }

        for (auto job : jobs) {
            ks_job_wait(js, job);
        }

        bool all_correct = true;
        for (int i = 0; i < SIZE; ++i) {
            if (data[i] != 2.0f) {
                all_correct = false;
                break;
            }
        }
        CHECK(all_correct == true);

        ks_job_manager_destroy(js);
    }

    ks_memory_shutdown();
}