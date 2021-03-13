#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_atom.h>
#include "atoms.hh"

#include <toml.hpp>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cassert>
#include <unistd.h>

#include <iostream>
#include <algorithm>
#include <thread>
#include <mutex>

#include "area.hh"
#include "module.hh"

using namespace std::literals::chrono_literals;

struct connection_t {
    xcb_connection_t *connection;
    connection_t() {
        connection = xcb_connect(NULL, NULL);
        cache_atoms(connection);
    }
    ~connection_t() {
        xcb_disconnect(connection);
    }
};

struct screen_t {
    aabb_t aabb;
    xcb_screen_t *screen;
    xcb_visualtype_t *visual_type;
    screen_t(connection_t& connection) {
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection.connection));
        screen = iter.data;
        aabb = {0, 0, screen->width_in_pixels, screen->height_in_pixels};

        xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
        for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
            xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
            for (; visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
                if (screen->root_visual == visual_iter.data->visual_id) {
                    visual_type = visual_iter.data;
                    return;
                }
            }
        }
    }
};

struct window_t {
    connection_t connection;
    xcb_drawable_t window;
    aabb_t aabb;

    window_t(connection_t& connection_, screen_t& screen, aabb_t aabb_): connection(connection_), aabb(aabb_) {
        window = xcb_generate_id(connection.connection);
        xcb_create_window(
            connection.connection, XCB_COPY_FROM_PARENT,
            window, screen.screen->root,
            aabb.xpos(), aabb.ypos(),
            aabb.width(), aabb.height(),
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            screen.screen->root_visual,
            0, NULL
        );
    }
    ~window_t() {
        xcb_destroy_window(connection.connection, window);
    }
};

struct surface_t {
    cairo_surface_t *surface;
    cairo_t *cr;

    surface_t(connection_t& connection, screen_t& screen, window_t& window) {
        surface = cairo_xcb_surface_create(connection.connection, window.window, screen.visual_type, window.aabb.width(), window.aabb.height());
        cr = cairo_create(surface);
    }
    ~surface_t() {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
};

struct content_t {
    std::mutex lock;
    std::vector<module_t> modules;
};

void exec_nocapture(std::string cmd) {
    system(cmd.c_str());
}
std::string exec(std::string cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cout << "popen() failed!" << std::endl;
        exit(1);
    }
    std::string result;
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    std::replace(result.begin(), result.end(), '\n', ' ');
    if (result.back() == ' ') {
        result.pop_back();
    }
    return result;
};

struct bar_t {
    connection_t& connection;
    screen_t screen;
    window_t window;
    surface_t surface;
    content_t &content;
    std::string font;
    float font_size;
    std::array<float, 3> foreground;
    std::array<float, 3> background;
    bar_t(connection_t& _connection, screen_t& _screen, content_t& _content, aabb_t aabb):
        connection(_connection),
        screen(_screen),
        window(connection, screen, aabb),
        surface(connection, screen, window),
        content(_content)
    {
        uint32_t events =
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS;
        xcb_change_window_attributes(connection.connection, window.window, XCB_CW_EVENT_MASK, &events);

        const auto& c = connection.connection;
        const auto& w = window.window;

        std::string wmname = "ade";
        xcb_icccm_set_wm_name(c, w, XCB_ATOM_STRING, 8, wmname.length(), wmname.c_str());
        std::string wmclass = wmname;
        xcb_icccm_set_wm_class(c, w, wmclass.length(), wmclass.c_str());
        std::vector<xcb_atom_t> flags = {WM_DELETE_WINDOW, WM_TAKE_FOCUS};
        xcb_icccm_set_wm_protocols(c, w, WM_PROTOCOLS, flags.size(), flags.data());

        const unsigned int visual {screen.screen->root_visual};
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, _NET_SYSTEM_TRAY_VISUAL, XCB_ATOM_VISUALID, 32, 1, &visual);
        #define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
        #define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1
        const unsigned int orientation {_NET_SYSTEM_TRAY_ORIENTATION_HORZ};
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, w, _NET_SYSTEM_TRAY_ORIENTATION, _NET_SYSTEM_TRAY_ORIENTATION, 32, 1, &orientation);

        xcb_ewmh_connection_t ewmh;
        xcb_intern_atom_cookie_t *ewmh_cookie = xcb_ewmh_init_atoms(c, &ewmh);
        xcb_ewmh_init_atoms_replies(&ewmh, ewmh_cookie, nullptr);

        std::vector<xcb_atom_t> types = {_NET_WM_WINDOW_TYPE_DOCK, _NET_WM_WINDOW_TYPE_NORMAL};
        xcb_ewmh_set_wm_window_type(&ewmh, w, types.size(), types.data());
        std::vector<xcb_atom_t> states = {_NET_WM_STATE_STICKY, _NET_WM_STATE_ABOVE, _NET_WM_STATE_SKIP_TASKBAR};
        xcb_ewmh_set_wm_state(&ewmh, w, states.size(), states.data());

        xcb_ewmh_wm_strut_partial_t strut {0};
        strut.top = window.aabb.y1;
        strut.top_start_x = window.aabb.x0;
        strut.top_end_x = window.aabb.x1;
        xcb_ewmh_set_wm_strut_partial(&ewmh, w, strut);

        xcb_ewmh_set_wm_desktop(&ewmh, w, 0xFFFFFFFF);
        xcb_ewmh_set_wm_pid(&ewmh, w, getpid());

        xcb_flush(connection.connection);
        xcb_map_window(connection.connection, window.window);
    }

    void redraw() {
        cairo_set_source_rgb(surface.cr, background[0], background[1], background[2]);
        cairo_paint(surface.cr);

        cairo_select_font_face(surface.cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(surface.cr, font_size);
        cairo_set_source_rgb(surface.cr, foreground[0], foreground[1], foreground[2]);

        cairo_font_extents_t font_extents;
        cairo_font_extents(surface.cr, &font_extents);

        aabb_t bar = window.aabb;
        for (auto& section: content.modules) {
            cairo_text_extents_t text_extents;
            cairo_text_extents(surface.cr, section.content.c_str(), &text_extents);
            section.aabb = bar.chop(section.gravity, text_extents.x_advance);
            cairo_move_to(surface.cr, section.aabb.x0, section.aabb.y0 + font_extents.ascent);
            cairo_show_text(surface.cr, section.content.c_str());
        }

        cairo_surface_flush(surface.surface);
        xcb_flush(connection.connection);
    }
    void handle_events() {
        xcb_generic_event_t *event;
        while ((event = xcb_poll_for_event(connection.connection))) {
            switch (event->response_type & ~0x80) {
                case XCB_EXPOSE:
                    break;
                case XCB_EVENT_MASK_BUTTON_PRESS:
                    {
                        xcb_button_press_event_t &button_press = *reinterpret_cast<xcb_button_press_event_t*>(event);
                        aabb_t mouse_aabb {button_press.event_x, button_press.event_y, 0, 0};

                        for (auto& section: content.modules) {
                            if (section.aabb.contains(mouse_aabb)) {
                                switch (button_press.detail) {
                                    case 1:
                                        if (!section.left_click.empty()) {
                                            exec_nocapture(section.left_click);
                                        }
                                        break;
                                    case 2:
                                        if (!section.middle_click.empty()) {
                                            exec_nocapture(section.middle_click);
                                        }
                                        break;
                                    case 3:
                                        if (!section.right_click.empty()) {
                                            exec_nocapture(section.right_click);
                                        }
                                        break;
                                    case 4:
                                        if (!section.wheel_up.empty()) {
                                            exec_nocapture(section.wheel_up);
                                        }
                                        break;
                                    case 5:
                                        if (!section.wheel_down.empty()) {
                                            exec_nocapture(section.wheel_down);
                                        }
                                        break;
                                    default:
                                        break;
                                }
                                if (!section.exec.empty()) {
                                    section.content = exec(section.exec);
                                }
                                break;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
        }
        free(event);
    }
};

void content_update(content_t& content) {
    while (true) {
        content.lock.lock();
        for (auto& module: content.modules) {
            if (!module.exec.empty()) {
                module.content = exec(module.exec);
            }
        }
        content.lock.unlock();
        std::this_thread::sleep_for(250ms);
    }
}

cairo_font_extents_t calculate_font_extents(std::string font, float font_size) {
    cairo_surface_t* tmp_surface = cairo_image_surface_create(cairo_format_t{}, 0, 0);
    cairo_t* tmp = cairo_create(tmp_surface);
    cairo_select_font_face(tmp, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(tmp, font_size);
    cairo_font_extents_t font_extents;
    cairo_font_extents(tmp, &font_extents);
    cairo_destroy(tmp);
    cairo_surface_destroy(tmp_surface);
    return font_extents;
}

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
    std::thread content_update_thread(content_update, std::ref(content));

    int notif_width = 200;
    aabb_t notifs = screen.aabb.chop(aabb_t::direction::right, notif_width);
    (void)notifs;

    bar.redraw();
    while (true) {
        content.lock.lock();
        bar.handle_events();
        bar.redraw();
        content.lock.unlock();
        std::this_thread::sleep_for(10ms);
    }
}
