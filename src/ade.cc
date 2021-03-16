#include <thread>
#include <string>

#include <toml.hpp>

#include "area.hh"
#include "module.hh"
#include "render.hh"
#include "bar.hh"

int main() {
    content_t content;
    const auto data = toml::parse("config.toml");
    const toml::array& modules_config = toml::find(data, "modules").as_array();
    const auto separator = toml::find_or<std::string>(data, "separator", "");
    for (const auto& module_config: modules_config) {
        const auto gravity = toml::find_or<std::string>(module_config, "gravity", "left");
        aabb_t::direction dir;
        if (gravity == "left") {
            dir = aabb_t::direction::left;
        } else if (gravity == "right") {
            dir = aabb_t::direction::right;
        } else {
            abort();
        }
        content.modules.emplace_back(module_t{
            toml::find_or<std::string>(module_config, "exec", ""),
            toml::find_or<std::string>(module_config, "text", ""),
            {},
            dir,
            toml::find_or<std::string>(module_config, "left_click", ""),
            toml::find_or<std::string>(module_config, "middle_click", ""),
            toml::find_or<std::string>(module_config, "right_click", ""),
            toml::find_or<std::string>(module_config, "wheel_up", ""),
            toml::find_or<std::string>(module_config, "wheel_down", ""),
        });
        content.modules.emplace_back(module_t{
            {}, separator, {}, dir, "", "", "", "", ""
        });
    }
    connection_t connection;
    screen_t screen{connection};
    float font_size = toml::find<float>(data, "font_size");
    int bar_height = font_size;
    aabb_t bar_aabb = screen.aabb.chop(aabb_t::direction::top, bar_height);
    bar_t bar{connection, screen, content, bar_aabb};
    bar.font = toml::find<std::string>(data, "font");
    bar.font_size = font_size;
    bar.foreground = toml::find<std::array<float, 3>>(data, "foreground");
    bar.background = toml::find<std::array<float, 3>>(data, "background");

    std::thread xcb_update_thread(
        [](bar_t& bar) {
            bar.redraw();
            while (true) {
                bar.handle_events();
                bar.redraw();
            }
        },
        std::ref(bar)
    );

    std::thread content_update_thread(
        [](content_t& content, bar_t& bar) {
            while (true) {
                content.lock.lock();
                for (auto& module: content.modules) {
                    if (!module.exec.empty()) {
                        module.content = exec(module.exec);
                    }
                }
                content.lock.unlock();
                bar.redraw();
                using namespace std::literals::chrono_literals;
                std::this_thread::sleep_for(1s);
            }
        },
        std::ref(content),
        std::ref(bar)
    );

    xcb_update_thread.join();
    content_update_thread.join();

    int notif_width = 200;
    aabb_t notifs = screen.aabb.chop(aabb_t::direction::right, notif_width);
    (void)notifs;

    return 0;
}
