// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "view/game_cell.hpp"

GameCell::GameCell(bool with_icon)
{
    if (!with_icon) return;
    this->icon = new brls::Image();
    this->icon->setWidth(44);
    this->icon->setHeight(44);
    this->icon->setCornerRadius(8);
    this->icon->setMarginRight(16);
    this->icon->setScalingType(brls::ImageScalingType::FIT);
    this->addView(this->icon, 0);   // before the title
}

void GameCell::set_icon(const std::vector<unsigned char>& image)
{
    if (!this->icon || image.empty()) return;
    this->icon->setImageFromMem(image.data(), (int)image.size());
}
