// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/release_notes.hpp"

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/paths.hpp"

namespace release_notes
{
namespace
{
brls::Label* text_label(const std::string& text, int font_size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setSingleLine(false);
    label->setFontSize(font_size);
    label->setTextColor(color);
    label->setText(text);
    return label;
}
}   // namespace

std::vector<changelog::Line> current(std::string* date)
{
    std::string md;
    paths::read_file(BRLS_ASSET("CHANGELOG.md"), md);
    return changelog::release_notes(md, app::version(), date);
}

void fill(brls::Box* box, const std::vector<changelog::Line>& lines)
{
    bool first = true;
    for (const changelog::Line& l : lines) {
        brls::View* view = nullptr;
        switch (l.kind) {
            case changelog::Line::Release:   // not under one version's heading
            case changelog::Line::Section:
                view = text_label(l.text, 22, ui::color_text());
                view->setMarginTop(first ? 0 : 12);
                view->setMarginBottom(4);
                break;
            case changelog::Line::Item: {
                auto* row = new brls::Box(brls::Axis::ROW);
                row->setMarginLeft(8 + 24 * l.depth);
                row->setMarginTop(4);
                brls::Label* bullet = text_label("•", 20, ui::color_text());
                bullet->setWidth(20);
                brls::Label* text = text_label(l.text, 20, ui::color_text());
                text->setGrow(1.0f);
                row->addView(bullet);
                row->addView(text);
                view = row;
                break;
            }
            case changelog::Line::Text:
                view = text_label(l.text, 20, ui::color_note());
                view->setMarginTop(6);
                break;
        }
        box->addView(view);
        first = false;
    }
}

}   // namespace release_notes
