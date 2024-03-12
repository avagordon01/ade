#include <pangomm-2.48/pangomm.h>
#include <pangomm/context.h>
#include <pango/pangocairo.h>
#include <fmt/core.h>
#include <memory>

#include <cassert>

#include "pango/pango-layout.h"
#include "render.hh"

struct notifications_t {
    connection_t& connection;
    screen_t& screen;
    window_t window;
    surface_t surface;
    notifications_t(connection_t& connection, screen_t& screen, aabb_t aabb, size_t notification_lines, size_t notification_columns):
        connection(connection),
        screen(screen),
        window(connection, screen, aabb),
        surface(connection, screen, window)
    {}
    void redraw() {
        // PangoContext* p = pango_cairo_create_context(surface.c->cobj());
        PangoLayout* l0 = pango_cairo_create_layout(surface.c->cobj());
        // pango_cairo_context_set_resolution(context, screen->dpi_y);
    
        // PangoLayout* l1 = pango_layout_new(p);
        (void)l0;
        // (void)l1;
        // auto l = Pango::Layout::create(context);
        std::string s;
        {
            size_t volume_pct = 90;
            s = fmt::format("volume {volume_pct:3}%", fmt::arg("volume_pct", volume_pct));
            std::string title = "title";
            std::string body = "body";
            s = fmt::format("<b>{title}</b>\n{body}", fmt::arg("title", title), fmt::arg("body", body));
        }
        PangoFontDescription* desc = pango_font_description_from_string("Hack 8");
        pango_layout_set_font_description(l0, desc);
        pango_font_description_free(desc);
        // pango_layout_set_width(l0, 200);
        // pango_layout_set_height(l0, 50);
        // pango_layout_set_ellipsize(l0, PangoEllipsizeMode::PANGO_ELLIPSIZE_END);
        pango_layout_set_text(l0, "text!", -1);
        // pango_layout_set_markup(l0, s.c_str(), s.length());
        cairo_move_to(surface.c->cobj(), 0, 0);
        cairo_set_source_rgb(surface.c->cobj(), 1.0, 1.0, 1.0);
        pango_cairo_show_layout(surface.c->cobj(), l0);
        // l->set_width(200);
        // l->set_height(50);
        // l->set_ellipsize(Pango::EllipsizeMode::END);
        // l->set_markup(Glib::ustring(s));
        // l->add_to_cairo_context(surface.c);
        // int notif_width = 200;
        // aabb_t notifications_aabb = screen.aabb.chop(aabb_t::direction::right, notif_width);
        // (void)notifications_aabb;
    }
};
