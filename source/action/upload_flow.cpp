// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/upload_flow.hpp"

#include <borealis.hpp>
#include <fmt/format.h>
#include <vector>

#include "app.hpp"
#include "ui/ui.hpp"
#include "util/diagnostics.hpp"
#include "util/log_upload.hpp"
#include "util/paths.hpp"
#include "util/pctl_ops_c.hpp"
#include "view/qr_view.hpp"

using namespace brls::literals;

namespace upload_flow
{

namespace
{
bool s_sending = false;   // one upload at a time

brls::Label* centered_label(const std::string& text, int font_size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setFontSize(font_size);
    label->setTextColor(color);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    label->setText(text);
    return label;
}

brls::Box* qr_card(const std::string& title, const std::string& url, const std::string& caption)
{
    auto* card = new brls::Box(brls::Axis::COLUMN);
    card->setAlignItems(brls::AlignItems::CENTER);
    card->setWidth(300);
    card->addView(new QrView(url, 220));
    brls::Label* name = centered_label(title, 20, ui::color_text());
    name->setMarginTop(6);
    card->addView(name);
    card->addView(centered_label(caption, 17, ui::color_note()));
    return card;
}

// Keeps the links in logs/uploads.txt, newest last: one can be found again
// after the dialog is closed.
void remember(const std::string& url)
{
    const std::string path = paths::logs_dir() + "/uploads.txt";
    std::string list;
    paths::read_file(path, list);
    list += ui::now_stamp() + "  " + url + "\n";
    if (paths::ensure_dir(paths::logs_dir())) paths::atomic_write(path, list);
}

void show_link(const log_upload::Result& r)
{
    SysInfo si;
    sysinfo_get(&si);
    char fw[16];
    sysinfo_version_string(si.hos_version, fw, sizeof(fw));
    const std::string ams =
        si.ams_valid ? fmt::format("{}.{}.{}", si.ams_major, si.ams_minor, si.ams_micro) : std::string();
    const std::string issue = log_upload::issue_url(app::repo_url(), r.url, app::version(), fw, ams);

    std::string text = brls::getStr("playguard/upload/done", log_upload::short_url(r.url));

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setPadding(24, 24, 12, 24);
    box->addView(centered_label(text, 20, ui::color_text()));
    auto* row = new brls::Box(brls::Axis::ROW);
    row->setJustifyContent(brls::JustifyContent::SPACE_EVENLY);
    row->setMarginTop(14);
    row->addView(qr_card("playguard/upload/qr_report"_i18n, r.url, log_upload::short_url(r.url)));
    row->addView(qr_card("playguard/upload/qr_issue"_i18n, issue, "playguard/upload/qr_issue_note"_i18n));
    box->addView(row);
    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/ok"_i18n, []() {});
    dialog->open();
}

void send(const std::vector<log_upload::Part>& parts)
{
    bool cut = false;
    const std::string text = log_upload::bundle(parts, log_upload::MAX_BYTES, &cut);
    // Too large (a long history): the start, with the report, is what matters.
    std::string list;
    for (const auto& p : parts) list += "\n• " + p.name;
    if (cut) list += "\n" + "playguard/upload/partial"_i18n;
    const int kb = (int)((text.size() + 1023) / 1024);
    const std::string body = brls::getStr("playguard/upload/confirm", kb) + "\n" + list + "\n\n" +
                             "playguard/upload/confirm_public"_i18n;
    ui::confirm(body, "playguard/upload/send"_i18n, [text]() {
        ui::notify("playguard/upload/sending"_i18n);
        if (s_sending) return;
        s_sending = true;
        brls::async([text]() {
            const log_upload::Result r = log_upload::upload(text);
            brls::sync([r]() {
                s_sending = false;
                if (!r.ok) {
                    ui::info(brls::getStr("playguard/upload/failed", r.error));
                    return;
                }
                remember(r.url);
                show_link(r);
            });
        });
    });
}

std::vector<log_upload::Part> with_debug_files(const std::string& report)
{
    std::vector<log_upload::Part> parts{ { "playguard/upload/part_report"_i18n, report } };
    for (auto& f : log_upload::debug_files()) parts.push_back(std::move(f));
    return parts;
}
}   // namespace

void send_report(const std::string& report)
{
    send(with_debug_files(report));
}

void choose()
{
    const std::vector<std::string> saved = log_upload::saved_reports();
    if (saved.empty()) {
        send_report(diagnostic::current_report());
        return;
    }
    std::vector<std::string> labels{ "playguard/upload/current"_i18n };
    for (const auto& name : saved) labels.push_back(brls::getStr("playguard/upload/saved", name));
    ui::pick("playguard/upload/pick"_i18n, labels, 0, [saved](int i) {
        if (i == 0) {
            send_report(diagnostic::current_report());
            return;
        }
        const std::string name = saved[(size_t)i - 1];
        std::string content;
        if (!paths::read_file(paths::logs_dir() + "/" + name, content)) {
            ui::info(brls::getStr("playguard/upload/failed", "cannot read " + name));
            return;
        }
        send({ { "logs/" + name, content } });
    });
}

}   // namespace upload_flow
