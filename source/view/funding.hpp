// The funding links (GitHub Sponsors, Ko-fi) as QR codes to scan with a
// phone — a console cannot open a link — with their name and short address.
// Shown in About, the first steps, "What's new" and the monthly reminder.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>
#include <functional>

namespace funding
{

// The two cards side by side. Focusable cards (in a tab or a screen) let the
// D-pad reach them and show their code larger on Ⓐ; in a dialog, whose
// buttons take the focus, they are not.
brls::Box* cards(float qr_side, bool focusable);

// A dialog with the support note and both codes; `then` runs once it closes.
void open_dialog(std::function<void()> then = nullptr);

}   // namespace funding
