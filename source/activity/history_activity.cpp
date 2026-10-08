// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#include "activity/history_activity.hpp"

#include "action/history_flow.hpp"
#include "ui/ui.hpp"
#include "util/history.hpp"
#include "util/paths.hpp"

using namespace brls::literals;

void HistoryActivity::onContentAvailable()
{
    note->setSingleLine(false);
    empty->setSingleLine(false);
    note->setText(brls::getStr("playguard/history/note", (int)history::MAX_ENTRIES, paths::history_file()));
    this->rebuild();
}

void HistoryActivity::rebuild()
{
    // The cells are reused in place: one of them has the focus when this runs
    // after a change, and borealis would keep a pointer to a deleted cell.
    // The list only grows (up to history::MAX_ENTRIES), so none is removed.
    const auto all = history::load();
    auto& cells = list->getChildren();
    for (size_t i = 0; i < all.size(); i++) {
        brls::DetailCell* cell;
        if (i < cells.size()) {
            cell = (brls::DetailCell*)cells[i];
        } else {
            cell = new brls::DetailCell();
            cell->setDetailTextColor(ui::color_neutral());
            list->addView(cell);
        }
        const history::Entry e = all[i];
        cell->setText(history_flow::title(e));
        cell->setDetailText(e.when);
        cell->registerClickAction([this, e](brls::View*) {
            history_flow::open(e, [this]() { this->rebuild(); });
            return true;
        });
    }
    // Fewer entries than cells: only when the file was emptied meanwhile.
    for (size_t i = 0; i < cells.size(); i++) ui::set_visible(cells[i], i < all.size());
    ui::set_visible(empty.getView(), all.empty());
}
