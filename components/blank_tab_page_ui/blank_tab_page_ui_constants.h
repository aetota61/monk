// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLANK_TAB_PAGE_UI_BLANK_TAB_PAGE_UI_CONSTANTS_H_
#define COMPONENTS_BLANK_TAB_PAGE_UI_BLANK_TAB_PAGE_UI_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace blank_tab_page_ui {

// Resource paths.
// Must match the resource file names.
extern const char kBlankTabPageCSS[];
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
extern const char kBlankTabPageMobileCSS[];
#endif
extern const char kBlankTabPageJS[];

}  // namespace blank_tab_page_ui

#endif  // COMPONENTS_BLANK_TAB_PAGE_UI_BLANK_TAB_PAGE_UI_CONSTANTS_H_
