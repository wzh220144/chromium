// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

#include <algorithm>

#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

bool IsBackgroundAbort(const PageLoadExtraInfo& info) {
  if (!info.started_in_foreground || !info.first_background_time)
    return false;

  if (!info.page_end_time)
    return true;

  return info.first_background_time <= info.page_end_time;
}

PageAbortReason GetAbortReasonForEndReason(PageEndReason end_reason) {
  switch (end_reason) {
    case END_RELOAD:
      return ABORT_RELOAD;
    case END_FORWARD_BACK:
      return ABORT_FORWARD_BACK;
    case END_NEW_NAVIGATION:
      return ABORT_NEW_NAVIGATION;
    case END_STOP:
      return ABORT_STOP;
    case END_CLOSE:
      return ABORT_CLOSE;
    case END_OTHER:
      return ABORT_OTHER;
    default:
      return ABORT_NONE;
  }
}

}  // namespace

base::Optional<std::string> GetGoogleHostnamePrefix(const GURL& url) {
  const size_t registry_length =
      net::registry_controlled_domains::GetRegistryLength(
          url,

          // Do not include unknown registries (registries that don't have any
          // matches in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,

          // Do not include private registries, such as appspot.com. We don't
          // want to match URLs like www.google.appspot.com.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  const base::StringPiece hostname = url.host_piece();
  if (registry_length == 0 || registry_length == std::string::npos ||
      registry_length >= hostname.length()) {
    return base::Optional<std::string>();
  }

  // Removes the tld and the preceding dot.
  const base::StringPiece hostname_minus_registry =
      hostname.substr(0, hostname.length() - (registry_length + 1));

  if (hostname_minus_registry == "google")
    return std::string("");

  if (!base::EndsWith(hostname_minus_registry, ".google",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return base::Optional<std::string>();
  }

  return std::string(hostname_minus_registry.substr(
      0, hostname_minus_registry.length() - strlen(".google")));
}

bool IsGoogleHostname(const GURL& url) {
  return GetGoogleHostnamePrefix(url).has_value();
}

bool WasStartedInForegroundOptionalEventInForeground(
    const base::Optional<base::TimeDelta>& event,
    const PageLoadExtraInfo& info) {
  return info.started_in_foreground && event &&
         (!info.first_background_time ||
          event.value() <= info.first_background_time.value());
}

PageAbortInfo GetPageAbortInfo(const PageLoadExtraInfo& info) {
  if (IsBackgroundAbort(info)) {
    // Though most cases where a tab is backgrounded are user initiated, we
    // can't be certain that we were backgrounded due to a user action. For
    // example, on Android, the screen times out after a period of inactivity,
    // resulting in a non-user-initiated backgrounding.
    return {ABORT_BACKGROUND, UserInitiatedInfo::NotUserInitiated(),
            info.first_background_time.value()};
  }

  PageAbortReason abort_reason =
      GetAbortReasonForEndReason(info.page_end_reason);
  if (abort_reason == ABORT_NONE)
    return PageAbortInfo();

  return {abort_reason, info.page_end_user_initiated_info,
          info.page_end_time.value()};
}

base::Optional<base::TimeDelta> GetInitialForegroundDuration(
    const PageLoadExtraInfo& info,
    base::TimeTicks app_background_time) {
  if (!info.started_in_foreground)
    return base::Optional<base::TimeDelta>();

  base::Optional<base::TimeDelta> time_on_page =
      OptionalMin(info.first_background_time, info.page_end_time);

  // If we don't have a time_on_page value yet, and we have an app background
  // time, use the app background time as our end time. This addresses cases
  // where the Chrome app is backgrounded before the page load is complete, on
  // platforms where Chrome may be killed once it goes into the background
  // (Android). In these cases, we use the app background time as the 'end
  // time'.
  if (!time_on_page && !app_background_time.is_null()) {
    time_on_page = app_background_time - info.navigation_start;
  }
  return time_on_page;
}

base::Optional<base::TimeDelta> OptionalMin(
    const base::Optional<base::TimeDelta>& a,
    const base::Optional<base::TimeDelta>& b) {
  if (a && !b)
    return a;
  if (b && !a)
    return b;
  if (!a && !b)
    return a;  // doesn't matter which
  return base::Optional<base::TimeDelta>(std::min(a.value(), b.value()));
}

bool DidObserveLoadingBehaviorInAnyFrame(
    const page_load_metrics::PageLoadExtraInfo& info,
    blink::WebLoadingBehaviorFlag behavior) {
  const int all_frame_loading_behavior_flags =
      info.main_frame_metadata.behavior_flags |
      info.subframe_metadata.behavior_flags;

  return (all_frame_loading_behavior_flags & behavior) != 0;
}

}  // namespace page_load_metrics
