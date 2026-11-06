//
// A synthetic font for headless use: exact, repeatable metrics of half
// an em per character, so layout tests never depend on installed fonts.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "gfx/graphics.h"

#include <cmath>

namespace gfx {
namespace {

// Layout units per point.
constexpr double lu_per_point = 20.0;

// Measures every character as half an em.
class fixed_font final : public font {
public:
    explicit fixed_font(font_request request)
        : request_(std::move(request)),
          em_(static_cast<lu>(
              std::lround(request_.size_points * lu_per_point))) {
    }

    const font_request &request() const override {
        return request_;
    }

    lu ascent() const override {
        return em_ * 4 / 5;
    }

    lu descent() const override {
        return em_ / 5;
    }

    lu advance(char32_t) const override {
        return em_ / 2;
    }

private:
    font_request request_;
    lu em_;
};

} // namespace

std::shared_ptr<const font>
fixed_font_provider::find(const font_request &request) {
    return std::make_shared<fixed_font>(request);
}

} // namespace gfx
