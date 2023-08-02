// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTE_QUOTE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTE_QUOTE_UI_H_

#include "build/build_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

// The WebUI handler for chrome://blank-tab-page.
class QuoteUI : public content::WebUIController {
 public:
  explicit QuoteUI(content::WebUI* web_ui);

  QuoteUI(const QuoteUI&) = delete;
  QuoteUI& operator=(const QuoteUI&) = delete;

  ~QuoteUI() override;

  // Returns the IDS_* string id for the variation of the processor.
  static int VersionProcessorVariation();

  // Loads a data source with many named details comprising version info.
  // The keys are from version_ui_constants.
  static void AddVersionDetailStrings(content::WebUIDataSource* html_source);

#if !BUILDFLAG(IS_ANDROID)
  // Returns a localized version string suitable for displaying in UI.
  static std::u16string GetAnnotatedVersionStringForUi();
#endif  // !BUILDFLAG(IS_ANDROID)
};

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTE_QUOTE_UI_H_
