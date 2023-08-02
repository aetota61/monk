// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/blank_tab_page/blank_tab_page_handler.h"

#include <stddef.h>

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/active_field_trials.h"
#include "components/blank_tab_page_ui/blank_tab_page_handler_helper.h"
#include "components/blank_tab_page_ui/blank_tab_page_ui_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

BlankTabPageHandler::BlankTabPageHandler() {}

BlankTabPageHandler::~BlankTabPageHandler() {}

void BlankTabPageHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void BlankTabPageHandler::RegisterMessages() {
  // web_ui()->RegisterMessageCallback(
  //     blank_tab_page_ui::kRequestVersionInfo,
  //     base::BindRepeating(&BlankTabPageHandler::HandleRequestVersionInfo,
  //                         base::Unretained(this)));
  // web_ui()->RegisterMessageCallback(
  //     blank_tab_page_ui::kRequestVariationInfo,
  //     base::BindRepeating(&BlankTabPageHandler::HandleRequestVariationInfo,
  //                         base::Unretained(this)));
  // web_ui()->RegisterMessageCallback(
  //     blank_tab_page_ui::kRequestPathInfo,
  //     base::BindRepeating(&BlankTabPageHandler::HandleRequestPathInfo,
  //                         base::Unretained(this)));
}
