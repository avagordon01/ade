#include <format>
#include <iostream>
#include <string>
#include <fmt/core.h>
int main() {
    size_t volume_pct = 90;
    std::string s = fmt::format("volume {volume_pct:3}%", fmt::arg("volume_pct", volume_pct));
    std::cout << s << std::endl;
    s = fmt::format("<b>{title}</b>\n{body}", fmt::arg("title", "title"), fmt::arg("body", "body"));
    std::cout << s << std::endl;
}
