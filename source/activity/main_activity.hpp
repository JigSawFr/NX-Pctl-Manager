// MainActivity — root screen: a TabFrame (sidebar of tabs) described in
// resources/xml/activity/main.xml. The tabs live in source/tab/.
// Copyright (C) 2026 JigSawFr, (C) 2026 Taylor.  GPLv3-or-later (see LICENSE).
#pragma once

#include <borealis.hpp>

class MainActivity : public brls::Activity
{
  public:
    CONTENT_FROM_XML_RES("activity/main.xml");

    void onContentAvailable() override;
};
