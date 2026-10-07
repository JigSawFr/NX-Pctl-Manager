// Copyright (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#include "tab/tab_base.hpp"

#include "ui/ui.hpp"

using namespace brls::literals;

TabBase::TabBase(const std::string& xml_res)
{
    this->inflateFromXMLRes(xml_res);
    this->registerAction("nx_pctl/hints/refresh"_i18n, brls::BUTTON_X, [this](brls::View*) {
        this->refresh();
        brls::Application::notify("nx_pctl/toast/refreshed"_i18n);
        return true;
    });
}

void TabBase::willAppear(bool resetState)
{
    brls::Box::willAppear(resetState);
    this->refresh();
}
