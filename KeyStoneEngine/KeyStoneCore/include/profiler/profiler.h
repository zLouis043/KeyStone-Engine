/**
 * @file profiler.h
 * @brief Integrated Performance Profiler (Chrome Tracing format).
 * @ingroup Debug
 */
#pragma once

#include "../core/defines.h"
#include "../core/types.h"

// #define KS_ENABLE_PROFILING

#ifdef __cplusplus
extern "C" {
#endif

KS_API void ks_profile_begin_session(ks_str name, ks_str filepath);

KS_API void ks_profile_end_session();

KS_API void ks_profile_write_profile(ks_str name, ks_int64 start_time, ks_int64 end_time, uint32_t thread_id);

KS_API ks_int64 ks_profile_get_microtime();

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <chrono>
#include <thread>
#include <string>

namespace KeyStone {

    class InstrumentationTimer {
    public:
        InstrumentationTimer(const char* name)
            : m_Name(name), m_Stopped(false)
        {
            m_StartTime = ks_profile_get_microtime();
        }

        ~InstrumentationTimer() {
            if (!m_Stopped) Stop();
        }

        void Stop() {
            ks_int64 endTime = ks_profile_get_microtime();
            size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            ks_profile_write_profile(m_Name, m_StartTime, endTime, (uint32_t)thread_id);

            m_Stopped = true;
        }

    private:
        const char* m_Name;
        ks_int64 m_StartTime;
        bool m_Stopped;
    };
}

#if defined(KS_ENABLE_PROFILING)

#define KS_PROFILE_BEGIN_SESSION(name, filepath) ::ks_profile_begin_session(name, filepath)
#define KS_PROFILE_END_SESSION() ::ks_profile_end_session()
#define KS_PROFILE_SCOPE(name) ::KeyStone::InstrumentationTimer timer##__LINE__(name)

#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
#define KS_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__DMC__) && (__DMC__ >= 0x810)
#define KS_FUNC_SIG __PRETTY_FUNCTION__
#elif defined(__FUNCSIG__)
#define KS_FUNC_SIG __FUNCSIG__
#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
#define KS_FUNC_SIG __FUNCTION__
#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
#define KS_FUNC_SIG __FUNC__
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#define KS_FUNC_SIG __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#define KS_FUNC_SIG __func__
#else
#define KS_FUNC_SIG "KS_FUNC_SIG unknown!"
#endif

#define KS_PROFILE_FUNCTION() KS_PROFILE_SCOPE(KS_FUNC_SIG)

#else

#define KS_PROFILE_BEGIN_SESSION(name, filepath)
#define KS_PROFILE_END_SESSION()
#define KS_PROFILE_SCOPE(name)
#define KS_PROFILE_FUNCTION()

#endif

#endif