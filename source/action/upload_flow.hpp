// upload_flow — Tools › Send a report online: pick a report, say where it
// goes, send it (util/log_upload.hpp), then show its link and two QR codes:
// the report itself, and the bug-report form with the link already in it.
// Copyright (C) 2026 JigSawFr.  GPLv3-or-later (see LICENSE).
#pragma once

#include <string>

namespace upload_flow
{

// Offers this report with the debug files, or one saved in logs/.
void choose();

// Sends `report` (as the diagnostic screen shows it) with the debug files,
// after the same confirmation.
void send_report(const std::string& report);

}   // namespace upload_flow
