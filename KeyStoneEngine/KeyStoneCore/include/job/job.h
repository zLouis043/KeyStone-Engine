/**
 * @file job_system.h
 * @brief Multithreaded Task Scheduler.
 * Handles background execution of tasks using a worker thread pool.
 * @ingroup Core
 */
#pragma once

#include "../core/defines.h"
#include "../core/types.h"
#include "../core/cb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ks_ptr Ks_JobManager;

/** 
 * @brief Opaque handle to a synchronization counter.
 * Used to wait for a specific job or a group of jobs to finish.
 */
typedef ks_ptr Ks_JobCounter;

// --- Lifecycle ---

/**
 * @brief Initializes the Job Manager.
 * Spawns worker threads (default: logical cores - 1).
 */
KS_API Ks_JobManager ks_job_manager_create();

/**
 * @brief Shuts down the Job Manager.
 * Waits for active jobs to finish and destroys worker threads.
 */
KS_API ks_no_ret ks_job_manager_destroy(Ks_JobManager js);

// --- Job Submission ---

#define ks_job_run(js, func, ...) \
    ks_job_run_impl(js, func, KS_PAYLOAD(__VA_ARGS__))

/**
 * @brief Submits a single job to the queue.
 * * @param js The job system.
 * @param func The function to execute.
 * @param data User data to pass to the function.
 * @return A counter handle to wait on. IMPORTANT: You must free this counter eventually or wait on it.
 * (Internal implementation details determine if explicit free is needed, see below).
 */
KS_API Ks_JobCounter ks_job_run_impl(Ks_JobManager js, ks_callback func, Ks_Payload payload);

#define ks_job_dispatch(js, func, ...) \
    ks_job_dispatch_impl(js, func, KS_PAYLOAD(__VA_ARGS__))


KS_API ks_no_ret ks_job_dispatch_impl(Ks_JobManager js, ks_callback func, Ks_Payload payload);

// --- Synchronization ---

/**
 * @brief Waits for a counter to reach zero (job completion).
 * @note While waiting, the calling thread will help execute other jobs
 * to prevent deadlocks and CPU waste.
 */
KS_API ks_no_ret ks_job_wait(Ks_JobManager js, Ks_JobCounter counter);

/**
 * @brief Returns true if the job associated with the counter is finished.
 */
KS_API ks_bool ks_job_is_busy(Ks_JobManager js, Ks_JobCounter counter);

/**
 * @brief Helper to get the number of worker threads.
 */
KS_API uint32_t ks_job_system_get_thread_count(Ks_JobManager js);

#ifdef __cplusplus
}
#endif