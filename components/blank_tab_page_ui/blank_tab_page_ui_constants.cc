// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blank_tab_page_ui/blank_tab_page_ui_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace blank_tab_page_ui {

// Resource paths.
const char kBlankTabPageCSS[] = "blank_tab_page.css";
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
const char kBlankTabPageMobileCSS[] = "blank_tab_page_mobile.css";
#endif
const char kBlankTabPageJS[] = "blank_tab_page.js";

}  // namespace blank_tab_page_ui
