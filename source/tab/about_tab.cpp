// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/about_tab.hpp"

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/changelog.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

namespace
{
// The latest releases only: the full history is on the releases page.
constexpr int CHANGELOG_RELEASES = 3;

brls::Label* note_label(const std::string& text, int font_size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setSingleLine(false);
    label->setFontSize(font_size);
    label->setTextColor(color);
    label->setText(text);
    return label;
}
}   // namespace

AboutTab::AboutTab()
    : TabBase("xml/tab/about.xml")
{
    credits->setSingleLine(false);
    changelog_note->setSingleLine(false);
    version->registerClickAction([this](brls::View*) {
        this->count_version_press();
        return true;
    });
    this->fill_changelog();
}

void AboutTab::fill_changelog()
{
    std::string md;
    paths::read_file(BRLS_ASSET("CHANGELOG.md"), md);
    const auto lines = changelog::parse(md, CHANGELOG_RELEASES);
    changelog_note->setText(brls::getStr(lines.empty() ? "playguard/about/changelog_none" : "playguard/about/changelog_note",
                                         std::string(app::repo_url()) + "/releases"));
    bool first = true;
    for (const changelog::Line& l : lines) {
        brls::View* view = nullptr;
        switch (l.kind) {
            case changelog::Line::Release: {
                auto* header = new brls::Header();
                header->setTitle(l.text);
                header->setMarginTop(first ? 8 : 24);
                header->setMarginBottom(4);
                view = header;
                break;
            }
            case changelog::Line::Section:
                view = note_label(l.text, 22, ui::color_text());
                view->setMarginTop(12);
                view->setMarginBottom(4);
                break;
            case changelog::Line::Item: {
                // The bullet in its own column: wrapped lines stay under the text.
                auto* row = new brls::Box(brls::Axis::ROW);
                row->setMarginLeft(8 + 24 * l.depth);
                row->setMarginTop(4);
                brls::Label* bullet = note_label("•", 20, ui::color_text());
                bullet->setWidth(20);
                brls::Label* text = note_label(l.text, 20, ui::color_text());
                text->setGrow(1.0f);
                row->addView(bullet);
                row->addView(text);
                view = row;
                break;
            }
            case changelog::Line::Text:
                view = note_label(l.text, 20, ui::color_note());
                view->setMarginTop(6);
                break;
        }
        changelog->addView(view);
        first = false;
    }
}

void AboutTab::count_version_press()
{
    if (app::dev_mode()) {
        ui::notify("playguard/dev/already_on"_i18n);
        return;
    }
    // Seven presses, at most 3 s apart.
    const brls::Time now = brls::getCPUTimeUsec();
    if (now - this->last_version_press > 3000000) this->version_presses = 0;
    this->last_version_press = now;
    const int left = 7 - ++this->version_presses;
    if (left > 0) {
        if (left == 1) ui::notify("playguard/dev/presses_left_one"_i18n);
        else if (left <= 3) ui::notify(brls::getStr("playguard/dev/presses_left", left));
        return;
    }
    this->version_presses = 0;
    ui::notify(app::set_dev_mode(true, true) ? "playguard/dev/enabled"_i18n
                                             : "playguard/dev/enabled"_i18n + " " + "playguard/toast/config_err"_i18n);
    ui::on_mode_changed();
}

void AboutTab::refresh()
{
    SysInfo si;
    sysinfo_get(&si);
    std::string flags;
    if (app::read_only()) flags += " · " + "playguard/tools/flag_read_only"_i18n;
    if (app::dev_mode()) flags += " · " + "playguard/tools/flag_dev"_i18n;
    version->setDetailText(app::version() + flags);
    mode->setDetailText(si.applet_mode ? "playguard/tools/mode_applet"_i18n : "playguard/tools/mode_app"_i18n);
    data->setDetailText(paths::data_dir());
    license->setDetailText("playguard/tools/license_value"_i18n);
    source->setDetailText(app::repo_url());
    // A note rather than a cell: the credits are longer than a cell's value.
    credits->setText(brls::getStr("playguard/tools/credits_line", "playguard/tools/credits_value"_i18n));
}

brls::View* AboutTab::create()
{
    return new AboutTab();
}
