struct module_t {
    std::string content;
    aabb_t aabb;
    aabb_t::direction gravity;
    enum class event_t: uint8_t {
        left_click = 1,
        middle_click = 2,
        right_click = 3,
        wheel_up = 4,
        wheel_down = 5,
    };
    std::function<bool(event_t)> event;
};
