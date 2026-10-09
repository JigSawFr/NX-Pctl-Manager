// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "action/github_login_flow.hpp"

#include <atomic>
#include <borealis.hpp>
#include <chrono>
#include <memory>
#include <thread>

#include "ui/ui.hpp"
#include "util/github_auth.hpp"
#include "util/log_upload.hpp"
#include "view/qr_view.hpp"

using namespace brls::literals;

namespace github_login_flow
{

namespace
{
bool s_busy = false;   // one sign-in at a time

brls::Label* centered(const std::string& text, int size, NVGcolor color)
{
    auto* label = new brls::Label();
    label->setFontSize(size);
    label->setTextColor(color);
    label->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    label->setSingleLine(false);
    label->setText(text);
    return label;
}

// The code, the address and its QR code, until GitHub answers or the
// developer cancels.
void show_code(const github_auth::DeviceCode& code, std::function<void()> done)
{
    auto cancelled = std::make_shared<std::atomic<bool>>(false);

    auto* box = new brls::Box(brls::Axis::COLUMN);
    box->setAlignItems(brls::AlignItems::CENTER);
    box->setPadding(20, 32, 10, 32);
    box->addView(centered(brls::getStr("playguard/github/code_body", log_upload::short_url(code.verification_uri)), 20,
                          ui::color_text()));
    brls::Label* user_code = centered(code.user_code, 44, ui::color_text());
    user_code->setMarginTop(10);
    box->addView(user_code);
    auto* qr = new QrView(code.verification_uri, 200);
    qr->setMarginTop(8);
    box->addView(qr);
    box->addView(centered("playguard/github/code_note"_i18n, 17, ui::color_note()));
    auto* dialog = new brls::Dialog(box);
    dialog->addButton("hints/cancel"_i18n, [cancelled]() { *cancelled = true; });
    ui::on_cancel(dialog, [cancelled]() { *cancelled = true; });
    dialog->open();

    brls::async([code, cancelled, dialog, done]() {
        int interval = code.interval;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(code.expires_in);
        github_auth::Poll result = github_auth::Poll::Expired;
        std::string token, error;
        while (!*cancelled && std::chrono::steady_clock::now() < deadline) {
            for (int s = 0; s < interval * 10 && !*cancelled; s++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (*cancelled) break;
            result = github_auth::poll(code, &token, &error);
            if (result == github_auth::Poll::SlowDown) interval += 5;
            else if (result != github_auth::Poll::Pending) break;
        }
        brls::sync([result, token, error, cancelled, dialog, done]() {
            s_busy = false;
            if (*cancelled) return;   // the dialog is already closed
            *cancelled = true;
            dialog->close();
            std::string err = error;
            switch (result) {
                case github_auth::Poll::Token:
                    if (github_auth::save_token(token, &err)) {
                        ui::notify("playguard/github/signed_in"_i18n);
                        if (done) done();
                    } else {
                        ui::info(brls::getStr("playguard/github/failed", err));
                    }
                    break;
                case github_auth::Poll::Denied:
                    ui::info("playguard/github/denied"_i18n);
                    break;
                case github_auth::Poll::Failed:
                    ui::info(brls::getStr("playguard/github/failed", err));
                    break;
                default:   // expired (or still pending at the deadline)
                    ui::info("playguard/github/expired"_i18n);
                    break;
            }
        });
    });
}
}   // namespace

void sign_in(std::function<void()> done)
{
    if (s_busy) return;
    s_busy = true;
    ui::notify("playguard/github/starting"_i18n);
    brls::async([done]() {
        github_auth::DeviceCode code;
        std::string err;
        const bool ok = github_auth::start(&code, &err);
        brls::sync([ok, code, err, done]() {
            if (!ok) {
                s_busy = false;
                ui::info(brls::getStr("playguard/github/failed", err));
                return;
            }
            show_code(code, done);   // s_busy until it ends
        });
    });
}

void open(std::function<void()> done)
{
    if (github_auth::token().empty()) {
        sign_in(done);
        return;
    }
    ui::confirm("playguard/github/sign_out_body"_i18n, "playguard/github/sign_out"_i18n, [done]() {
        github_auth::forget_token();
        ui::notify("playguard/github/signed_out"_i18n);
        if (done) done();
    });
}

}   // namespace github_login_flow
