#include "timer.h"
#include <sol/sol.hpp>
#include <iostream>
#include "typemanager.h"

Timer::Timer() 
    : 
      elapsed_before_pause(0),
      time_limit(0),
      running(false),
      looping(false),
      has_time_limit(false) {
}

Timer::~Timer() {
}

void Timer::start() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running) {
        if (elapsed_before_pause.count() > 0) {
            start_time = std::chrono::high_resolution_clock::now() - elapsed_before_pause;
        } else {
            start_time = std::chrono::high_resolution_clock::now();
        }
        running = true;
    }
}

void Timer::stop() {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        pause_time = std::chrono::high_resolution_clock::now();
        elapsed_before_pause = pause_time - start_time;
        running = false;
    }
}

void Timer::reset() {
    std::lock_guard<std::mutex> lock(mutex);
    elapsed_before_pause = std::chrono::nanoseconds(0);
    if (running) {
        start_time = std::chrono::high_resolution_clock::now();
    }
}

void Timer::set_limit(double seconds) {
    std::lock_guard<std::mutex> lock(mutex);
    time_limit = std::chrono::nanoseconds(static_cast<int64_t>(seconds * 1e9));
    has_time_limit = (time_limit.count() > 0);
}

void Timer::set_loop(bool loop) {
    std::lock_guard<std::mutex> lock(mutex);
    looping = loop;
}

void Timer::set_name(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    this->name = name;
}

double Timer::get_elapsed_seconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(now - start_time).count();
    } else {
        return std::chrono::duration<double>(elapsed_before_pause).count();
    }
}

double Timer::get_elapsed_milliseconds() const {    
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_time).count();
    } else {
        return std::chrono::duration<double, std::milli>(elapsed_before_pause).count();
    }
}

double Timer::get_elapsed_nanoseconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::nano>(now - start_time).count();
    } else {
        return std::chrono::duration<double, std::nano>(elapsed_before_pause).count();
    }
}

double Timer::get_elapsed_minutes() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (running) {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::ratio<60>>(now - start_time).count();
    } else {
        return std::chrono::duration<double, std::ratio<60>>(elapsed_before_pause).count();
    }
}

double Timer::get_remaining_seconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_time_limit) return 0.0;
    
    double elapsed = get_elapsed_seconds();
     return std::max(0.0, std::chrono::duration<double>(time_limit).count() - elapsed);
}

double Timer::get_remaining_milliseconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_limit()) {
        return std::numeric_limits<double>::infinity();
    }
    double elapsed = get_elapsed_milliseconds();
    return std::max(0.0, std::chrono::duration<double, std::milli>(time_limit).count() - elapsed);
}

double Timer::get_remaining_nanoseconds() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_limit()) {
        return std::numeric_limits<double>::infinity();
    }
    double elapsed = get_elapsed_nanoseconds();
    return std::max(0.0, std::chrono::duration<double, std::nano>(time_limit).count() - elapsed);
}

double Timer::get_remaining_minutes() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_limit()) {
        return std::numeric_limits<double>::infinity();
    }
    double elapsed = get_elapsed_minutes();
    return std::max(0.0, std::chrono::duration<double, std::ratio<60>>(time_limit).count() - elapsed);
}

bool Timer::is_running() const {
    return running;
}

bool Timer::is_loop() const {
    return looping;
}

bool Timer::has_limit() const {
    return has_time_limit;
}

std::string Timer::get_name() const {
    std::lock_guard<std::mutex> lock(mutex);
    return name;
}

void Timer::set_callback(const std::function<void()>& callback) {
    std::lock_guard<std::mutex> lock(mutex);
    this->callback = callback;
}

void Timer::clear_callback() {
    std::lock_guard<std::mutex> lock(mutex);
    callback = nullptr;
}

std::chrono::nanoseconds Timer::get_expiration_time() const {
    std::lock_guard<std::mutex> lock(mutex);
    if (!has_limit()) {
        return std::chrono::nanoseconds::max(); 
    }
    return start_time.time_since_epoch() + time_limit;
}

double Timer::get_limit_seconds() const {
    return time_limit.count() / 1e9;
}

const std::function<void()>& Timer::get_callback() const {
    std::lock_guard<std::mutex> lock(mutex);
    return callback;
}

TimeManager::TimeManager() 
    : 
      target_fps(0), limiting_enabled(true) {
    main_timer.set_name("MainTimer");
}

Timer& TimeManager::get_main_timer() {
    return main_timer;
}

TimeManager::~TimeManager() {
    main_timer.stop();
    cleanup();
}

void TimeManager::cleanup(){
    main_timer.stop();
    main_timer.clear_callback();

    while (!child_timers_queue.empty()) {
        child_timers_queue.pop();
    }
}

void TimeManager::set_target_fps(int fps) {
    target_fps = fps;
    limiting_enabled = (fps > 0);

    if (!main_timer.is_running()) {
        main_timer.start();
    }
    
    main_timer.set_loop(false); 
}

int TimeManager::get_target_fps() const {
    return target_fps;
}

double TimeManager::get_delta_time() const{
    if (last_frame_time == 0) {
        return frame_start_time;
    }
    return frame_start_time - last_frame_time;
}

double TimeManager::get_current_fps() const{
    double delta = get_delta_time();
    return (delta > 0) ? (1.0 / delta) : 0;
}


bool TimeManager::is_limiting_enabled() const {
    return limiting_enabled;
}

Timer* TimeManager::create_timer() {
    Timer* timer = MemoryManager::get_instance().alloc_t<Timer>(
        MemoryManager::SMART_MANAGED, 
        MemoryManager::SYSTEM_DATA, 
        "TimeManager_Timer"
    );
    
    if (timer) {

        TimerUniquePtr timer_ptr(timer, TimerDeleter());

        timer = timer_ptr.get();;
        child_timers_queue.push(std::move(timer_ptr));
    }
    
    return timer;
}

void TimeManager::begin_frame() {
    frame_start_time = main_timer.get_elapsed_seconds();
}

void TimeManager::end_frame(){
    double current_time = main_timer.get_elapsed_seconds();
    
    if (is_limiting_enabled()) {
        double target_frame_time = 1.0 / target_fps;
        double elapsed_this_frame = current_time - frame_start_time;
        
        if (elapsed_this_frame < target_frame_time) {
            double sleep_time = (target_frame_time - elapsed_this_frame) * 1000.0;
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(sleep_time)));
            current_time = main_timer.get_elapsed_seconds();
        }
    }

    std::chrono::high_resolution_clock::time_point current_time_point = std::chrono::high_resolution_clock::now();

    while (!child_timers_queue.empty()) {
        Timer* top_timer = child_timers_queue.top().get();

        if (!top_timer || !top_timer->is_running() || !top_timer->has_limit()) {
            if (top_timer && !top_timer->is_running()) {
                child_timers_queue.pop(); 
            } else {
                break;
            }
        }
        
        std::chrono::high_resolution_clock::time_point current_time_point = std::chrono::high_resolution_clock::now();
        if (top_timer->get_expiration_time() > current_time_point.time_since_epoch()) {
            break;
        }

        if (top_timer->get_callback()) {
            top_timer->get_callback()();
        }

        if (top_timer->is_loop()) {
            TimerUniquePtr looped_timer = std::move(const_cast<TimerUniquePtr&>(child_timers_queue.top()));
            child_timers_queue.pop();

            looped_timer->reset();
            child_timers_queue.push(std::move(looped_timer));
        } else {
            child_timers_queue.pop();
        }
    }

    last_frame_time = frame_start_time;
}

void TimeManager::init_lua_bindings(lua_State* lua, TypeManager* tm){
    sol::state_view view(lua);
    
    tm->register_type<Timer>("Timer",
        sol::constructors<Timer()>(),
        "start", &Timer::start,
        "stop", &Timer::stop,
        "reset", &Timer::reset,
        "set_limit", &Timer::set_limit,
        "set_loop", &Timer::set_loop,
        "set_name", &Timer::set_name,
        "get_elapsed_seconds", &Timer::get_elapsed_seconds,
        "get_elapsed_milliseconds", &Timer::get_elapsed_milliseconds,
        "get_elapsed_nanoseconds", &Timer::get_elapsed_nanoseconds,
        "get_elapsed_minutes", &Timer::get_elapsed_minutes,
        "get_remaining_seconds", &Timer::get_remaining_seconds,
        "get_remaining_milliseconds", &Timer::get_remaining_milliseconds,
        "get_remaining_nanoseconds", &Timer::get_remaining_nanoseconds,
        "get_remaining_minutes", &Timer::get_remaining_minutes,
        "is_running", &Timer::is_running,
        "is_loop", &Timer::is_loop,
        "has_limit", &Timer::has_limit,
        "get_name", &Timer::get_name,
        "set_callback", &Timer::set_callback
    );

    sol::table ftm = view.create_named_table("time_manager");

    ftm["get_delta_time"] = [this]() -> double {
        return this->get_delta_time();
    };
    
    ftm["get_current_fps"] = [this]() -> int {
        return this->get_current_fps();
    };
    
    ftm["set_target_fps"] = [this](int fps) {
        this->set_target_fps(fps);
    };
    
    ftm["get_target_fps"] = [this]() -> int {
        return this->get_target_fps();
    };
    
    ftm["is_limiting_enabled"] = [this]() -> bool {
        return this->is_limiting_enabled();
    };
    
    ftm["create_timer"] = [this]() -> Timer* {
        return this->create_timer();
    };
}