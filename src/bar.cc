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
    int width;
    int height = 16;
    xcb_drawable_t window;
    xcb_visualtype_t *visual_type;
    xcb_screen_t *screen;

    window_t(connection_t& connection) {
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection.connection));
        screen = iter.data;
        width = screen->width_in_pixels;
        window = xcb_generate_id(connection.connection);
        xcb_create_window(
            connection.connection, XCB_COPY_FROM_PARENT,
            window, screen->root,
            0, 0,
            width, height,
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
        surface = cairo_xcb_surface_create(connection.connection, window.window, window.visual_type, window.width, window.height);
        cr = cairo_create(surface);
    }
    ~surface_t() {
        cairo_destroy(cr);
        cairo_surface_destroy(surface);
    }
};

struct content_t {
    std::mutex lock;
    std::string left = "empty";
    std::string middle = "";
    std::string right = "";
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
        cairo_set_source_rgb(surface.cr, 0.0, 0.0, 0.0);
        cairo_paint(surface.cr);
        cairo_select_font_face(surface.cr, "Misc Tamsyn", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        float font_size = 14.0;
        cairo_set_font_size(surface.cr, font_size);
        cairo_set_source_rgb(surface.cr, 1.0, 1.0, 1.0);
        content.lock.lock();

        cairo_font_extents_t font_extents;
        cairo_font_extents(surface.cr, &font_extents);

        double padding = 0;
        double width = 1920;
        double height = 2 * padding + font_extents.ascent + font_extents.descent;
        (void)height;

        cairo_move_to(surface.cr, padding, padding + font_extents.ascent);
        cairo_show_text(surface.cr, content.left.c_str());

        cairo_text_extents_t extents;
        cairo_text_extents(surface.cr, content.middle.c_str(), &extents);
        cairo_move_to(surface.cr, padding + width / 2 - extents.x_advance / 2, padding + font_extents.ascent);
        cairo_show_text(surface.cr, content.middle.c_str());

        cairo_text_extents(surface.cr, content.right.c_str(), &extents);
        cairo_move_to(surface.cr, width - padding - extents.x_advance, padding + font_extents.ascent);
        cairo_show_text(surface.cr, content.right.c_str());

        content.lock.unlock();

        cairo_surface_flush(surface.surface);
        xcb_flush(connection.connection);

        if (false) {
            cairo_t *cr = NULL;
            cairo_set_source_rgb(cr, 1, 1, 1);
            cairo_paint(cr);
            float scale = 16;
            cairo_save(cr);
            cairo_translate(cr, scale, scale);
            cairo_scale(cr, scale, scale);
            {
                uint32_t x, y;
                cairo_move_to(cr, x, y);
                cairo_line_to(cr, x, y);
            }
            cairo_restore(cr);
            cairo_set_line_width(cr, 1);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);

            cairo_save(cr);
            cairo_translate(cr, scale, scale);
            cairo_scale(cr, scale, scale);
            cairo_translate(cr, -0.5, -0.5);

            double mouse_x, mouse_y;
            /*
            mouse_x -= scale;
            mouse_y -= scale;
            */
            mouse_x /= scale;
            mouse_y /= scale;
            {
                uint32_t min_x, min_y, max_x, max_y;
                cairo_rectangle(cr, min_x, min_y, max_x - min_x, max_y - min_y);
                cairo_rectangle(cr, min_x, min_y, 1, 1);
                cairo_rectangle(cr, max_x, max_y, 1, 1);
            }
            cairo_restore(cr);
            cairo_set_source_rgba(cr, 0.8, 0.9, 0.95, 0.5);
            cairo_fill_preserve(cr);
            cairo_set_source_rgb(cr, 0, 0, 0);
            cairo_stroke(cr);
        }
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
        content.left = "hello world";
        content.middle = "wifi  brightness  15%  volume  14%  battery 100% " + exec("date +%H:%M");
        content.right = "right hand side";
        content.lock.unlock();
        std::this_thread::sleep_for(100ms);
    }
}

int main() {
    content_t content;
    connection_t connection;
    bar_t bar(connection, content);
    std::thread content_update_thread(content_update, std::ref(content));

    bar.redraw();

    while (true) {
        bar.redraw();
        xcb_generic_event_t *event = xcb_poll_for_event(connection.connection);
        if (!event) {
            continue;
        }
        switch (event->response_type & ~0x80) {
            case XCB_EXPOSE:
                break;
            case XCB_EVENT_MASK_BUTTON_PRESS:
            case XCB_EVENT_MASK_BUTTON_RELEASE:
                {
                    xcb_button_press_event_t &button_press = *reinterpret_cast<xcb_button_press_event_t*>(event);
                    printf("button press %u %u!\n",
                        button_press.event_x,
                        button_press.event_y
                    );
                    switch (button_press.detail) {
                    case 1:
                        printf("left click\n");
                        break;
                    case 2:
                        printf("middle click\n");
                        break;
                    case 3:
                        printf("right click\n");
                        break;
                    case 4:
                        printf("wheel up\n");
                        break;
                    case 5:
                        printf("wheel down\n");
                        break;
                    }
                }
                break;
            default:
                break;
        }
        free(event);
    }
}
