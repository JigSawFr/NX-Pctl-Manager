// ui — dialogs, confirmations, the dropdown, keyboard prompts and the PIN dialog.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "ui/ui.hpp"

#include "core/platform.h"
#include "util/duration.hpp"

#include <memory>

using namespace brls::literals;

namespace ui
{

void on_cancel(brls::Dialog* dialog, std::function<void()> on_cancel)
{
    dialog->setCancelable(true);
    dialog->getAppletFrame()->registerAction(
        "hints/back"_i18n, brls::BUTTON_B,
        [dialog, on_cancel](brls::View*) {
            dialog->close([on_cancel]() { if (on_cancel) on_cancel(); });
            return true;
        },
        false, false, brls::SOUND_BACK);
}

// Lines `text` takes at `per_line` characters a line (a rough count: the
// font is proportional, but the paragraphs are what makes a text tall).
static int estimated_lines(const std::string& text, size_t per_line)
{
    int lines = 0;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        // UTF-8 continuation bytes are not characters.
        size_t chars = 0;
        for (size_t i = start; i < end; i++) chars += ((unsigned char)text[i] & 0xC0) != 0x80;
        lines += chars == 0 ? 1 : (int)((chars + per_line - 1) / per_line);
        start = end + 1;
    }
    return lines;
}

brls::Dialog* dialog(const std::string& text)
{
    // borealis' layout (720 px wide, 115 px side margins, 24 px font) holds
    // about 40 characters a line and 13 lines above the buttons.
    if (estimated_lines(text, 40) <= 13) return new brls::Dialog(text);
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(19);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    auto* box = new brls::Box();
    box->addView(label);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(28, 40, 28, 40);
    return new brls::Dialog(box);
}

// The text (slightly smaller, so a chart fits under a long one) and `extra`.
static brls::Dialog* dialog_with(const std::string& text, brls::View* extra)
{
    auto* label = new brls::Label();
    label->setText(text);
    label->setFontSize(20);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::STRETCH);
    box->setPadding(26, 40, 18, 40);
    box->addView(label);
    extra->setMarginTop(18);
    box->addView(extra);
    return new brls::Dialog(box);
}

static void open_confirm(const std::string& body, const std::string& confirm_label,
                         std::function<void()> on_yes, std::function<void()> on_no, bool danger,
                         brls::View* extra = nullptr)
{
    auto* dialog = extra ? dialog_with(body, extra) : ui::dialog(body);
    dialog->addButton("hints/cancel"_i18n, [on_no]() { if (on_no) on_no(); });
    dialog->addButton(confirm_label, [on_yes]() { if (on_yes) on_yes(); });
    on_cancel(dialog, on_no);
    if (danger)
        if (auto* button = dynamic_cast<brls::Button*>(dialog->getView("brls/dialog/button2")))
            button->setTextColor(color_bad());
    dialog->open();
}

void confirm(const std::string& body, const std::string& confirm_label,
             std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), danger);
}

void confirm_with(const std::string& body, brls::View* extra, const std::string& confirm_label,
                  std::function<void()> on_yes, std::function<void()> on_no, bool danger)
{
    open_confirm(body, confirm_label, std::move(on_yes), std::move(on_no), danger, extra);
}

void confirm_danger(const std::string& body, const std::string& confirm_label, std::function<void()> on_yes)
{
    open_confirm(body, confirm_label, std::move(on_yes), nullptr, true);
}

void info(const std::string& body)
{
    auto* dialog = ui::dialog(body);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

void pick(const std::string& title, const std::vector<std::string>& values, int selected,
          std::function<void(int)> on_pick)
{
    if (values.empty()) return;
    auto chosen = std::make_shared<int>(-1);
    auto* dropdown = new brls::Dropdown(
        title, values, [chosen](int index) { *chosen = index; },
        selected < 0 ? 0 : selected,
        [chosen, on_pick](int) {
            if (*chosen >= 0 && on_pick) {
                int index = *chosen;
                brls::sync([on_pick, index]() { on_pick(index); });
            }
        });
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void prompt_minutes(const std::string& header, uint16_t current, std::function<void(uint16_t)> on_value)
{
    const std::string guide   = "playguard/numpad/guide"_i18n;
    const std::string initial = duration::format_hm(current == PT_DAY_NOLIMIT ? 60 : current);
    auto handle = [on_value](const std::string& text) {
        uint16_t minutes = 0;
        if (!duration::parse(text, &minutes)) {
            notify(rc_text(NXM_RC_INVALID_ARGUMENT));
            return;
        }
        on_value(minutes);
    };
    // The system number pad, with a ":" key (core/platform.h); borealis' own
    // text input where there is none.
    char out[16] = {};
    switch (platform_numpad(header.c_str(), guide.c_str(), initial.c_str(), 5, out, sizeof(out))) {
        case PLATFORM_INPUT_OK:        handle(out); return;
        case PLATFORM_INPUT_CANCELLED: return;
        case PLATFORM_INPUT_NONE:      break;
    }
    brls::Application::getImeManager()->openForText([handle](std::string text) { handle(text); },
                                                   header, guide, 5, initial);
}

void prompt_text(const std::string& header, const std::string& initial, int max_len,
                 std::function<void(std::string)> on_value)
{
    brls::Application::getImeManager()->openForText(
        [on_value](std::string text) { if (!text.empty()) on_value(text); },
        header, "", max_len, initial);
}

void offer_restart()
{
    auto* dialog = new brls::Dialog("playguard/common/restart_body"_i18n);
    dialog->addButton("playguard/common/later"_i18n, []() {});
    dialog->addButton("playguard/common/quit"_i18n, []() { brls::Application::quit(); });
    dialog->setCancelable(true);
    dialog->open();
}

namespace
{
void wipe(char* p, size_t n)
{
    volatile char* b = p;
    while (n--) *b++ = 0;
}
}   // namespace

void show_pin_dialog()
{
    char pin[16];
    Result rc = pctl_get_pin(pin, sizeof(pin));
    brls::Logger::info("pctl_get_pin returned 0x{:08X}", (unsigned)rc);
    if (R_FAILED(rc)) {
        notify_result(rc, "", "playguard/security/show_pin_err"_i18n);
        return;
    }
    std::string spaced;   // "1 2 3 4": easier to read out and to type
    spaced.reserve(2 * sizeof(pin));   // no reallocation, so no stray copy
    for (const char* c = pin; *c; c++) {
        if (!spaced.empty()) spaced += ' ';
        spaced += *c;
    }
    wipe(pin, sizeof(pin));

    auto* title = new brls::Label();
    title->setText("playguard/security/show_pin_title"_i18n);
    title->setFontSize(22);
    title->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    title->setTextColor(color_note());
    auto* digits = new brls::Label();
    digits->setText(spaced);
    digits->setFontSize(56);
    digits->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    digits->setMarginTop(16);
    wipe(&spaced[0], spaced.size());

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setJustifyContent(brls::JustifyContent::CENTER);
    box->setPadding(40, 40, 40, 40);
    box->addView(title);
    box->addView(digits);

    auto* d = new brls::Dialog(box);
    d->addButton("hints/ok"_i18n, []() {});
    d->setCancelable(true);
    d->open();
}

}   // namespace ui
