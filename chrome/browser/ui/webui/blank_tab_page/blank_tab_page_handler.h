// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BLANK_TAB_PAGE_BLANK_TAB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BLANK_TAB_PAGE_BLANK_TAB_PAGE_HANDLER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

// Handler class for Version page operations.
class BlankTabPageHandler : public content::WebUIMessageHandler {
 public:
  BlankTabPageHandler();

  BlankTabPageHandler(const BlankTabPageHandler&) = delete;
  BlankTabPageHandler& operator=(const BlankTabPageHandler&) = delete;

  ~BlankTabPageHandler() override;

  // content::WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

 private:
  // Factory for the creating refs in callbacks.
  base::WeakPtrFactory<BlankTabPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_BLANK_TAB_PAGE_BLANK_TAB_PAGE_HANDLER_H_
