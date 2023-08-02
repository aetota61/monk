// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: The handle* functions below are called internally on promise
// resolution, unlike the other return* functions, which are called
// asynchronously by the host.

// clang-format off
// <if expr="is_ios">
import 'chrome://resources/js/ios/web_ui.js';
// </if>
