// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tab_base.hpp"

#include "app.hpp"
#include "ui/ui.hpp"

using namespace brls::literals;

TabBase::TabBase(const std::string& xml_res)
{
    this->inflateFromXMLRes(xml_res);
    this->registerAction("playguard/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        this->refresh();
        return true;
    });
    if (auto* note = dynamic_cast<brls::Label*>(this->getView("tab_ro_note"))) {
        note->setSingleLine(false);
        ui::set_visible(note, app::read_only_build());
    }
}

void TabBase::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    this->refresh();
}

void TabBase::enable_auto_refresh(int period_ms)
{
    this->auto_timer.setPeriod(period_ms);
    this->auto_timer.setCallback([this]() {
        if (!app::in_focus()) return;
        auto stack = brls::Application::getActivitiesStack();
        if (stack.empty() || stack.back() != this->getParentActivity()) return;
        this->refresh();
    });
    this->auto_timer.start();
}
