#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_atom.h>
#include "atoms.hh"

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

struct window_t {
    aabb_t aabb;
    xcb_drawable_t window;
    xcb_visualtype_t *visual_type;
    xcb_screen_t *screen;

    window_t(connection_t& connection) {
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection.connection));
        screen = iter.data;
        aabb = {0, 0, screen->width_in_pixels, 16};
        window = xcb_generate_id(connection.connection);
        xcb_create_window(
            connection.connection, XCB_COPY_FROM_PARENT,
            window, screen->root,
            aabb.xpos(), aabb.ypos(),
            aabb.width(), aabb.height(),
            0,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            screen->root_visual,
            0, NULL
        );

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

struct surface_t {
    cairo_surface_t *surface;
    cairo_t *cr;

    surface_t(connection_t& connection, window_t& window) {
        surface = cairo_xcb_surface_create(connection.connection, window.window, window.visual_type, window.aabb.width(), window.aabb.height());
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

struct bar_t {
    connection_t& connection;
    window_t window;
    surface_t surface;
    content_t &content;
    bar_t(connection_t& _connection, content_t& _content):
        connection(_connection),
        window(connection),
        surface(connection, window),
        content(_content)
    {
        uint32_t events =
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_BUTTON_PRESS |
            XCB_EVENT_MASK_BUTTON_RELEASE;
        xcb_change_window_attributes(connection.connection, window.window, XCB_CW_EVENT_MASK, &events);

        const auto& c = connection.connection;
        const auto& w = window.window;

        std::string wmname = "ade";
        xcb_icccm_set_wm_name(c, w, XCB_ATOM_STRING, 8, wmname.length(), wmname.c_str());
        std::string wmclass =wmname;
        xcb_icccm_set_wm_class(c, w, wmclass.length(), wmclass.c_str());
        std::vector<xcb_atom_t> flags = {WM_DELETE_WINDOW, WM_TAKE_FOCUS};
        xcb_icccm_set_wm_protocols(c, w, WM_PROTOCOLS, flags.size(), flags.data());

        const unsigned int visual {window.screen->root_visual};
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
        xcb_ewmh_set_wm_desktop(&ewmh, w, 0xFFFFFFFF);
        xcb_ewmh_set_wm_pid(&ewmh, w, getpid());
        xcb_flush(connection.connection);
        xcb_map_window(connection.connection, window.window);
    }

    void redraw() {
        cairo_set_source_rgb(surface.cr, 0.125, 0.125, 0.125);
        cairo_paint(surface.cr);
        cairo_select_font_face(surface.cr, "Misc Tamsyn", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        float font_size = 12.0;
        cairo_set_font_size(surface.cr, font_size);
        cairo_set_source_rgb(surface.cr, 0.875, 0.875, 0.875);

        cairo_font_extents_t font_extents;
        cairo_font_extents(surface.cr, &font_extents);

        aabb_t screen {
            0, 0, window.screen->width_in_pixels, window.screen->height_in_pixels
        };
        int padding = 0;
        int bar_height = 2 * padding + font_extents.ascent + font_extents.descent;
        aabb_t bar = screen.chop_to(aabb_t::direction::top, bar_height);
        int notif_width = 200;
        aabb_t notifs =
            screen
            .chop_off(aabb_t::direction::top, bar_height)
            .chop_to(aabb_t::direction::right, notif_width);
        (void)notifs;
        aabb_t padded_bar = bar.chop_off(aabb_t::direction::all, padding);

        cairo_text_extents_t text_extents;

        for (auto& section: content.modules) {
            cairo_text_extents(surface.cr, section.content.c_str(), &text_extents);
            section.aabb = padded_bar.chop_to(section.gravity, text_extents.x_advance);
            padded_bar = padded_bar.chop_off(section.gravity, text_extents.x_advance);
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
                case XCB_EVENT_MASK_BUTTON_RELEASE:
                    {
                        xcb_button_press_event_t &button_press = *reinterpret_cast<xcb_button_press_event_t*>(event);
                        aabb_t mouse_aabb {button_press.event_x, button_press.event_y, 0, 0};

                        module_t* clicked_section = nullptr;
                        for (auto& section: content.modules) {
                            if (section.aabb.contains(mouse_aabb)) {
                                clicked_section = &section;
                                break;
                            }
                        }
                        module_t::event_t event {button_press.detail};
                        if (clicked_section && clicked_section->event) {
                            clicked_section->event(event);
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

std::string exec(std::string cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    std::string result;
    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    return result;
};

void content_update(content_t& content) {
    for (size_t i = 0; ; i++) {
        content.lock.lock();
        content.modules[0].content = "menu";
        content.modules[2].content = "browser terminal";
        content.modules[4].content = "shutdown";
        content.modules[5].content = "battery 100%";
        content.modules[7].content = "volume  12%";
        content.modules[9].content = "wifi kiera";
        content.lock.unlock();
        std::this_thread::sleep_for(100ms);
    }
}

int main() {
    content_t content;
    std::function<bool(module_t::event_t)> event = [](module_t::event_t event) -> bool {
        switch (event) {
            case module_t::event_t::left_click:
                printf("left_click\n");
                break;
            case module_t::event_t::middle_click:
                printf("middle_click\n");
                break;
            case module_t::event_t::right_click:
                printf("right_click\n");
                break;
            case module_t::event_t::wheel_up:
                printf("wheel_up\n");
                break;
            case module_t::event_t::wheel_down:
                printf("wheel_down\n");
                break;
            default:
                break;
        }
        return true;
    };
    using namespace std::string_literals;
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::left, event});
    content.modules.emplace_back(module_t{"   ", {}, aabb_t::direction::left, nullptr});
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::left, event});
    content.modules.emplace_back(module_t{"   ", {}, aabb_t::direction::left, nullptr});
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::left, event});
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::right, event});
    content.modules.emplace_back(module_t{"   ", {}, aabb_t::direction::right, nullptr});
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::right, event});
    content.modules.emplace_back(module_t{"   ", {}, aabb_t::direction::right, nullptr});
    content.modules.emplace_back(module_t{"", {}, aabb_t::direction::right, event});
    connection_t connection;
    bar_t bar(connection, content);
    std::thread content_update_thread(content_update, std::ref(content));

    bar.redraw();
    while (true) {
        content.lock.lock();
        bar.handle_events();
        bar.redraw();
        std::this_thread::sleep_for(10ms);
        content.lock.unlock();
    }
}
