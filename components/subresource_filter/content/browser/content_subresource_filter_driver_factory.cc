// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver_factory.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/subresource_filter/content/browser/content_activation_list_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_client.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    subresource_filter::ContentSubresourceFilterDriverFactory);

namespace subresource_filter {

namespace {

std::string DistillURLToHostAndPath(const GURL& url) {
  return url.host() + url.path();
}

// Returns true with a probability given by |performance_measurement_rate| if
// ThreadTicks is supported, otherwise returns false.
bool ShouldMeasurePerformanceForPageLoad(double performance_measurement_rate) {
  if (!base::ThreadTicks::IsSupported())
    return false;
  return performance_measurement_rate == 1 ||
         (performance_measurement_rate > 0 &&
          base::RandDouble() < performance_measurement_rate);
}

}  // namespace

// static
void ContentSubresourceFilterDriverFactory::CreateForWebContents(
    content::WebContents* web_contents,
    SubresourceFilterClient* client) {
  if (FromWebContents(web_contents))
    return;
  web_contents->SetUserData(
      UserDataKey(), base::MakeUnique<ContentSubresourceFilterDriverFactory>(
                         web_contents, client));
}

// static
bool ContentSubresourceFilterDriverFactory::NavigationIsPageReload(
    const GURL& url,
    const content::Referrer& referrer,
    ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD) ||
         // Some pages 'reload' from JavaScript by navigating to themselves.
         url == referrer.url;
}

ContentSubresourceFilterDriverFactory::ContentSubresourceFilterDriverFactory(
    content::WebContents* web_contents,
    SubresourceFilterClient* client)
    : content::WebContentsObserver(web_contents),
      client_(client),
      throttle_manager_(
          base::MakeUnique<ContentSubresourceFilterThrottleManager>(
              this,
              client_->GetRulesetDealer(),
              web_contents)) {}

ContentSubresourceFilterDriverFactory::
    ~ContentSubresourceFilterDriverFactory() {}

void ContentSubresourceFilterDriverFactory::
    OnMainResourceMatchedSafeBrowsingBlacklist(
        const GURL& url,
        safe_browsing::SBThreatType threat_type,
        safe_browsing::ThreatPatternType threat_type_metadata) {
  AddActivationListMatch(
      url, GetListForThreatTypeAndMetadata(threat_type, threat_type_metadata));
}

void ContentSubresourceFilterDriverFactory::
    ComputeActivationForMainFrameNavigation(
        content::NavigationHandle* navigation_handle) {
  const GURL& url(navigation_handle->GetURL());

  if (!url.SchemeIsHTTPOrHTTPS()) {
    activation_decision_ = ActivationDecision::UNSUPPORTED_SCHEME;
    activation_options_ = Configuration::ActivationOptions();
    return;
  }

  const auto config_list = GetEnabledConfigurations();
  const auto highest_priority_activated_config =
      std::find_if(config_list->configs_by_decreasing_priority().begin(),
                   config_list->configs_by_decreasing_priority().end(),
                   [&url, this](const Configuration& config) {
                     return DoesMainFrameURLSatisfyActivationConditions(
                         url, config.activation_conditions);
                   });

  bool has_activated_config =
      highest_priority_activated_config !=
      config_list->configs_by_decreasing_priority().end();
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "ContentSubresourceFilterDriverFactory::"
               "ComputeActivationForMainFrameNavigation",
               "highest_priority_activated_config",
               has_activated_config
                   ? highest_priority_activated_config->ToTracedValue()
                   : base::MakeUnique<base::trace_event::TracedValue>());
  if (!has_activated_config) {
    activation_decision_ = ActivationDecision::ACTIVATION_CONDITIONS_NOT_MET;
    activation_options_ = Configuration::ActivationOptions();
    return;
  }

  activation_options_ = highest_priority_activated_config->activation_options;
  activation_decision_ =
      activation_options_.activation_level == ActivationLevel::DISABLED
          ? ActivationDecision::ACTIVATION_DISABLED
          : ActivationDecision::ACTIVATED;
}

bool ContentSubresourceFilterDriverFactory::
    DoesMainFrameURLSatisfyActivationConditions(
        const GURL& url,
        const Configuration::ActivationConditions& conditions) const {
  switch (conditions.activation_scope) {
    case ActivationScope::ALL_SITES:
      return true;
    case ActivationScope::ACTIVATION_LIST:
      if (DidURLMatchActivationList(url, conditions.activation_list))
        return true;
      if (conditions.activation_list == ActivationList::PHISHING_INTERSTITIAL &&
          DidURLMatchActivationList(
              url, ActivationList::SOCIAL_ENG_ADS_INTERSTITIAL)) {
        // Handling special case, where activation on the phishing sites also
        // mean the activation on the sites with social engineering metadata.
        return true;
      }
      return false;
    case ActivationScope::NO_SITES:
      return false;
  }
  NOTREACHED();
  return false;
}

void ContentSubresourceFilterDriverFactory::OnReloadRequested() {
  UMA_HISTOGRAM_BOOLEAN("SubresourceFilter.Prompt.NumReloads", true);
  const GURL& whitelist_url = web_contents()->GetLastCommittedURL();

  // Only whitelist via content settings when using the experimental UI,
  // otherwise could get into a situation where content settings cannot be
  // adjusted.
  if (base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilterExperimentalUI)) {
    client_->WhitelistByContentSettings(whitelist_url);
  } else {
    client_->WhitelistInCurrentWebContents(whitelist_url);
  }
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ContentSubresourceFilterDriverFactory::WillProcessResponse(
    content::NavigationHandle* navigation_handle) {
  DCHECK(!navigation_handle->IsSameDocument());
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->GetNetErrorCode() != net::OK) {
    return;
  }

  const GURL& url = navigation_handle->GetURL();
  const content::Referrer& referrer = navigation_handle->GetReferrer();
  ui::PageTransition transition = navigation_handle->GetPageTransition();

  if (activation_options_.should_whitelist_site_on_reload &&
      NavigationIsPageReload(url, referrer, transition)) {
    // Whitelist this host for the current as well as subsequent navigations.
    client_->WhitelistInCurrentWebContents(url);
  }

  ComputeActivationForMainFrameNavigation(navigation_handle);
  DCHECK_NE(activation_decision_, ActivationDecision::UNKNOWN);

  // Check for whitelisted status last, so that the client gets an accurate
  // indication of whether there would be activation otherwise.
  bool whitelisted = client_->OnPageActivationComputed(
      navigation_handle,
      activation_options_.activation_level == ActivationLevel::ENABLED);

  // Only reset the activation decision reason if we would have activated.
  if (whitelisted && activation_decision_ == ActivationDecision::ACTIVATED) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ActivationWhitelisted");
    activation_decision_ = ActivationDecision::URL_WHITELISTED;
    activation_options_ = Configuration::ActivationOptions();
  }

  if (activation_decision_ != ActivationDecision::ACTIVATED) {
    DCHECK_EQ(activation_options_.activation_level, ActivationLevel::DISABLED);
    return;
  }

  DCHECK_NE(activation_options_.activation_level, ActivationLevel::DISABLED);
  ActivationState state = ActivationState(activation_options_.activation_level);
  state.measure_performance = ShouldMeasurePerformanceForPageLoad(
      activation_options_.performance_measurement_rate);
  // TODO(csharrison): Set state.enable_logging based on metadata returns from
  // the safe browsing filter, when it is available. Add tests for this
  // behavior.
  throttle_manager_->NotifyPageActivationComputed(navigation_handle, state);
}

void ContentSubresourceFilterDriverFactory::OnFirstSubresourceLoadDisallowed() {
  if (activation_options_.should_suppress_notifications)
    return;
  client_->ToggleNotificationVisibility(activation_options_.activation_level ==
                                        ActivationLevel::ENABLED);
}

void ContentSubresourceFilterDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument()) {
    activation_decision_ = ActivationDecision::UNKNOWN;
    activation_list_matches_.clear();
    client_->ToggleNotificationVisibility(false);
  }
}

void ContentSubresourceFilterDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame() &&
      !navigation_handle->IsSameDocument() &&
      activation_decision_ == ActivationDecision::UNKNOWN &&
      navigation_handle->HasCommitted()) {
    activation_decision_ = ActivationDecision::ACTIVATION_DISABLED;
    activation_options_ = Configuration::ActivationOptions();
  }
}

bool ContentSubresourceFilterDriverFactory::DidURLMatchActivationList(
    const GURL& url,
    ActivationList activation_list) const {
  auto match_types =
      activation_list_matches_.find(DistillURLToHostAndPath(url));
  return match_types != activation_list_matches_.end() &&
         match_types->second.find(activation_list) != match_types->second.end();
}

void ContentSubresourceFilterDriverFactory::AddActivationListMatch(
    const GURL& url,
    ActivationList match_type) {
  if (match_type == ActivationList::NONE)
    return;
  if (url.has_host() && url.SchemeIsHTTPOrHTTPS())
    activation_list_matches_[DistillURLToHostAndPath(url)].insert(match_type);
}

}  // namespace subresource_filter
