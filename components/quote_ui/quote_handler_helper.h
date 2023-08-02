// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUOTE_UI_QUOTE_HANDLER_HELPER_H_
#define COMPONENTS_QUOTE_UI_QUOTE_HANDLER_HELPER_H_

#include "base/values.h"

namespace quote_ui {

// Returns the list of variations to be displayed on the chrome:://version page.
base::Value::List GetVariationsList();

// Returns the variations information in command line format to be displayed on
// the chrome:://version page.
base::Value GetVariationsCommandLineAsValue();

}  // namespace quote_ui

#endif  // COMPONENTS_QUOTE_UI_QUOTE_HANDLER_HELPER_H_
