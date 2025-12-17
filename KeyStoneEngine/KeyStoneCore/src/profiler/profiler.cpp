#include "../../include/profiler/profiler.h"
#include "../../include/core/log.h"

#include <fstream>
#include <string>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

class Instrumentor {
private:
    std::string m_SessionName;
    std::ofstream m_OutputStream;
    int m_ProfileCount;
    std::mutex m_Lock;
    bool m_Active_Session;

    Instrumentor() : m_SessionName("None"), m_ProfileCount(0), m_Active_Session(false) {}

public:
    static Instrumentor& Get() {
        static Instrumentor instance;
        return instance;
    }

    void BeginSession(const std::string& name, const std::string& filepath) {
#ifndef KS_ENABLE_PROFILING
        return;
#endif
        std::lock_guard<std::mutex> lock(m_Lock);
        if (m_Active_Session) InternalEndSession();

        fs::path p(filepath);
        if (p.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
            if (ec) {
                KS_LOG_ERROR("[Profiler] Failed to create directory: %s", p.parent_path().string().c_str());
                return;
            }
        }

        m_OutputStream.open(filepath);
        if (m_OutputStream.is_open()) {
            m_Active_Session = true;
            WriteHeader();
            m_SessionName = name;
            KS_LOG_INFO("[Profiler] Session '%s' started.", name.c_str());
        }
        else {
            KS_LOG_ERROR("[Profiler] Failed to open '%s'", filepath.c_str());
        }
    }

    void EndSession() {
        std::lock_guard<std::mutex> lock(m_Lock);
        InternalEndSession();
    }

    void WriteProfile(const char* name, long long start, long long end, uint32_t thread_id) {
        std::lock_guard<std::mutex> lock(m_Lock);
        if (!m_Active_Session) return;

        if (m_ProfileCount++ > 0) m_OutputStream << ",";

        std::string s_name = name;
        std::replace(s_name.begin(), s_name.end(), '"', '\'');

        m_OutputStream << "{";
        m_OutputStream << "\"cat\":\"function\",";
        m_OutputStream << "\"dur\":" << (end - start) << ",";
        m_OutputStream << "\"name\":\"" << s_name << "\",";
        m_OutputStream << "\"ph\":\"X\",";
        m_OutputStream << "\"pid\":0,";
        m_OutputStream << "\"tid\":" << thread_id << ",";
        m_OutputStream << "\"ts\":" << start;
        m_OutputStream << "}";
    }

private:
    void WriteHeader() {
        m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
    }

    void WriteFooter() {
        m_OutputStream << "]}";
    }

    void InternalEndSession() {
        if (!m_Active_Session) return;
        WriteFooter();
        m_OutputStream.close();
        m_Active_Session = false;
        m_ProfileCount = 0;
        KS_LOG_INFO("[Profiler] Session ended.");
    }
};

KS_API void ks_profile_begin_session(ks_str name, ks_str filepath) {
    Instrumentor::Get().BeginSession(name ? name : "Session", filepath ? filepath : "results.json");
}

KS_API void ks_profile_end_session() {
    Instrumentor::Get().EndSession();
}

KS_API void ks_profile_write_profile(ks_str name, ks_int64 start, ks_int64 end, uint32_t thread_id) {
    Instrumentor::Get().WriteProfile(name, (long long)start, (long long)end, thread_id);
}

KS_API ks_int64 ks_profile_get_microtime() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::time_point_cast<std::chrono::microseconds>(now).time_since_epoch().count();
}