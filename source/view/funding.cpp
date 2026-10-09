// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/funding.hpp"

#include "app.hpp"
#include "ui/ui.hpp"
#include "view/qr_view.hpp"

using namespace brls::literals;

namespace funding
{
namespace
{
brls::Label* centered_label(const std::string& text, int font_size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setFontSize(font_size);
    label->setTextColor(color);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setText(text);
    return label;
}

// "https://ko-fi.com/jigsawfr" → "ko-fi.com/jigsawfr": what to type by hand.
std::string short_url(const std::string& url)
{
    const size_t scheme = url.find("://");
    return scheme == std::string::npos ? url : url.substr(scheme + 3);
}

void open_large(const std::string& name, const std::string& url)
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setPadding(24, 24, 16, 24);
    box->addView(new QrView(url, 340));
    brls::Label* caption = centered_label(name + " — " + short_url(url), 22, ui::color_text());
    caption->setMarginTop(12);
    box->addView(caption);
    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

brls::Box* card(const std::string& name, const std::string& url, float qr_side, bool focusable)
{
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setAlignItems(brls::AlignItems::CENTER);
    card->setWidth(qr_side + 160);   // both cards alike, whatever their address
    card->setPadding(12, 16, 12, 16);
    card->addView(new QrView(url, qr_side));
    brls::Label* title = centered_label(name, 22, ui::color_text());
    title->setMarginTop(8);
    card->addView(title);
    card->addView(centered_label(short_url(url), 18, ui::color_note()));
    if (focusable) {
        card->setFocusable(true);
        card->registerClickAction([name, url](brls::View*) {
            open_large(name, url);
            return true;
        });
    }
    return card;
}
}   // namespace

brls::Box* cards(float qr_side, bool focusable)
{
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setJustifyContent(brls::JustifyContent::SPACE_EVENLY);
    row->addView(card("GitHub Sponsors", app::SPONSORS_URL, qr_side, focusable));
    row->addView(card("Ko-fi", app::KOFI_URL, qr_side, focusable));
    return row;
}

void open_dialog(std::function<void()> then)
{
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->setPadding(28, 32, 12, 32);
    auto* note = new brls::Label();
    note->setSingleLine(false);
    note->setFontSize(20);
    note->setText("playguard/about/support_body"_i18n);
    box->addView(note);
    brls::Box* row = cards(180, false);
    row->setMarginTop(12);
    box->addView(row);
    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/ok"_i18n, [then]() {
        if (then) then();
    });
    dialog->open();
}

}   // namespace funding
