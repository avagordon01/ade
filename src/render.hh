#pragma once

#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_atom.h>
#include "atoms.hh"

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
