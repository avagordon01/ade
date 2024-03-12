#pragma once

#include <cairomm-1.16/cairomm/cairomm.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_atom.h>
#include "atoms.hh"
#include "cairomm/fontface.h"
#include "cairomm/types.h"

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
    double dpi_x, dpi_y;
    screen_t(connection_t& connection) {
        xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection.connection));
        screen = iter.data;
        aabb = {0, 0, screen->width_in_pixels, screen->height_in_pixels};

        dpi_x = (static_cast<double>(screen->width_in_pixels) * 25.4 / static_cast<double>(screen->width_in_millimeters));
        dpi_y = (static_cast<double>(screen->height_in_pixels) * 25.4 / static_cast<double>(screen->height_in_millimeters));

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
    Cairo::Surface s;
    std::shared_ptr<Cairo::Context> c;

    surface_t(connection_t& connection, screen_t& screen, window_t& window):
        s(cairo_xcb_surface_create(connection.connection, window.window, screen.visual_type, window.aabb.width(), window.aabb.height())),
        c(std::make_shared<Cairo::Context>(cairo_create(s.cobj())))
    {}
};

Cairo::FontExtents calculate_font_extents(std::string font, double font_size) {
    Cairo::Surface s(cairo_image_surface_create(cairo_format_t{}, 0, 0));
    Cairo::Context c(cairo_create(s.cobj()));
    c.select_font_face(font, Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::NORMAL);
    c.set_font_size(font_size);
    Cairo::FontExtents font_extents;
    c.get_font_extents(font_extents);
    return font_extents;
}
