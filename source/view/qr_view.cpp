// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/qr_view.hpp"

#include <cmath>
#include <qrcodegen.hpp>

QrView::QrView(const std::string& text, float side)
{
    this->setWidth(side);
    this->setHeight(side);
    this->setFocusable(false);
    try {
        // Medium correction: a URL stays small (version 3 or 4) and still reads
        // off a screen with some glare.
        const auto qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
        this->size = qr.getSize();
        this->modules.resize((size_t)size * size);
        for (int y = 0; y < size; y++)
            for (int x = 0; x < size; x++) this->modules[(size_t)y * size + x] = qr.getModule(x, y);
    } catch (const std::exception& e) {
        brls::Logger::error("QR code of \"{}\": {}", text, e.what());
        this->size = 0;
    }
}

void QrView::draw(NVGcontext* vg, float x, float y, float width, float height,
                  brls::Style, brls::FrameContext*)
{
    if (this->size == 0) return;
    const float side   = std::fmin(width, height);
    const int   quiet  = 4;
    const float module = std::floor(side / (this->size + 2 * quiet));
    if (module < 1) return;
    const float code = module * (this->size + 2 * quiet);
    const float left = std::round(x + (width - code) / 2), top = std::round(y + (height - code) / 2);

    nvgBeginPath(vg);
    nvgRect(vg, left, top, code, code);
    nvgFillColor(vg, nvgRGB(0xFF, 0xFF, 0xFF));
    nvgFill(vg);

    // One path for every dark module: a single fill call.
    nvgBeginPath(vg);
    for (int r = 0; r < this->size; r++)
        for (int c = 0; c < this->size; c++)
            if (this->modules[(size_t)r * this->size + c])
                nvgRect(vg, left + (quiet + c) * module, top + (quiet + r) * module, module, module);
    nvgFillColor(vg, nvgRGB(0x00, 0x00, 0x00));
    nvgFill(vg);
}
