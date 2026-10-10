// GameCell — a game in the Activity list: its icon (when the control data has
// one and the app runs with full memory), its name and a play time. The icon
// slot is kept even before the icon arrives, so the names line up.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <vector>

class GameCell : public brls::DetailCell
{
  public:
    explicit GameCell(bool with_icon);

    // A JPEG or PNG as the control data holds it; nothing happens without an
    // icon slot or with an empty image.
    void set_icon(const std::vector<unsigned char>& image);
    // An NVG image someone else owns and keeps (the Activity tab's icon
    // cache): shown without decoding again, never freed by this cell.
    void set_icon_texture(int texture);

  private:
    brls::Image* icon = nullptr;
};
