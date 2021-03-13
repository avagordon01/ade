struct module_t {
    std::string exec;
    std::string content;
    aabb_t aabb;
    aabb_t::direction gravity;
    std::string left_click;
    std::string middle_click;
    std::string right_click;
    std::string wheel_up;
    std::string wheel_down;
};
