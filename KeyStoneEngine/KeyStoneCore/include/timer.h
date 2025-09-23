#pragma once

#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <queue>

#include "memory.h"

struct lua_State;
class TypeManager;

class Timer {
public:
    Timer();
    ~Timer();

    void start();
    void stop();
    void reset();
    void set_limit(double seconds);
    double get_limit_seconds() const;
    void set_loop(bool loop);
    void set_name(const std::string& name);

    double get_elapsed_seconds() const;
    double get_elapsed_milliseconds() const;
    double get_elapsed_nanoseconds() const;
    double get_elapsed_minutes() const;
    double get_remaining_seconds() const;
    double get_remaining_milliseconds() const;
    double get_remaining_nanoseconds() const;
    double get_remaining_minutes() const;
    bool is_running() const;
    bool is_loop() const;
    bool has_limit() const;
    std::string get_name() const;
    
    void set_callback(const std::function<void()>& callback);
    const std::function<void()>& get_callback() const;
    void clear_callback();

    std::chrono::nanoseconds get_expiration_time() const;

public:

    std::string name;
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point pause_time;
    std::chrono::nanoseconds elapsed_before_pause;
    std::chrono::nanoseconds time_limit;
    
    std::atomic<bool> running;
    std::atomic<bool> looping;
    std::atomic<bool> has_time_limit;
    
    std::function<void()> callback;
    mutable std::mutex mutex;
};

struct TimerDeleter {
    void operator()(Timer* timer) const {
        if (timer) {
            MemoryManager::get_instance().dealloc(timer);
        }
    }
};

using TimerUniquePtr = std::unique_ptr<Timer, TimerDeleter>;

class TimeManager {
public:
    TimeManager();
    ~TimeManager();

    void cleanup();

    Timer& get_main_timer();

    Timer* create_timer();

    void set_target_fps(int fps);
    int get_target_fps() const;
    double get_current_fps() const;
    double get_delta_time() const;
    bool is_limiting_enabled() const;

    void begin_frame();
    void end_frame();
    void init_lua_bindings(lua_State* lua, TypeManager* tm);

private:

    struct TimerComparator {
        bool operator()(const TimerUniquePtr& a, const TimerUniquePtr& b) const {
            if (!a) return true;
            if (!b) return false;
            return a->get_expiration_time() > b->get_expiration_time();
        }
    };

private:
    Timer main_timer;
    std::priority_queue<TimerUniquePtr, std::vector<TimerUniquePtr>, TimerComparator> child_timers_queue;
    int target_fps;
    bool limiting_enabled;
    double frame_start_time = 0;
    double last_frame_time = 0; 
    int cleanup_counter = 0;
};