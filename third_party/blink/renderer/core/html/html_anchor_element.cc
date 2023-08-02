/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_anchor_element.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/frame/ad_tracker.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"
#include "third_party/blink/renderer/core/html/anchor_element_observer_for_service_worker.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/navigation_policy.h"
#include "third_party/blink/renderer/core/loader/ping_loader.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_api.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace blink {

namespace {

// The download attribute specifies a filename, and an excessively long one can
// crash the browser process. Filepaths probably can't be longer than 4096
// characters, but this is enough to prevent the browser process from becoming
// unresponsive or crashing.
const int kMaxDownloadAttrLength = 1000000;

// Note: Here it covers download originated from clicking on <a download> link
// that results in direct download. Features in this method can also be logged
// from browser for download due to navigations to non-web-renderable content.
bool ShouldInterveneDownloadByFramePolicy(LocalFrame* frame) {
  bool should_intervene_download = false;
  Document& document = *(frame->GetDocument());
  UseCounter::Count(document, WebFeature::kDownloadPrePolicyCheck);
  bool has_gesture = LocalFrame::HasTransientUserActivation(frame);
  if (!has_gesture) {
    UseCounter::Count(document, WebFeature::kDownloadWithoutUserGesture);
  }
  if (frame->IsAdFrame()) {
    UseCounter::Count(document, WebFeature::kDownloadInAdFrame);
    if (!has_gesture) {
      UseCounter::Count(document,
                        WebFeature::kDownloadInAdFrameWithoutUserGesture);
      if (base::FeatureList::IsEnabled(
              blink::features::
                  kBlockingDownloadsInAdFrameWithoutUserActivation))
        should_intervene_download = true;
    }
  }
  if (frame->DomWindow()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kDownloads)) {
    UseCounter::Count(document, WebFeature::kDownloadInSandbox);
    should_intervene_download = true;
  }
  if (!should_intervene_download)
    UseCounter::Count(document, WebFeature::kDownloadPostPolicyCheck);
  return should_intervene_download;
}

}  // namespace

HTMLAnchorElement::HTMLAnchorElement(Document& document)
    : HTMLAnchorElement(html_names::kATag, document) {}

HTMLAnchorElement::HTMLAnchorElement(const QualifiedName& tag_name,
                                     Document& document)
    : HTMLElement(tag_name, document),
      link_relations_(0),
      cached_visited_link_hash_(0),
      rel_list_(MakeGarbageCollected<RelList>(this)) {}

HTMLAnchorElement::~HTMLAnchorElement() = default;

bool HTMLAnchorElement::SupportsFocus() const {
  if (IsEditable(*this))
    return HTMLElement::SupportsFocus();
  // If not a link we should still be able to focus the element if it has
  // tabIndex.
  return IsLink() || HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::ShouldHaveFocusAppearance() const {
  return (GetDocument().LastFocusType() != mojom::blink::FocusType::kMouse) ||
         HTMLElement::SupportsFocus();
}

bool HTMLAnchorElement::IsMouseFocusable() const {
  if (!IsFocusableStyleAfterUpdate())
    return false;
  if (IsLink())
    return SupportsFocus();

  return HTMLElement::IsMouseFocusable();
}

bool HTMLAnchorElement::IsKeyboardFocusable() const {
  if (!IsFocusableStyleAfterUpdate())
    return false;

  // Anchor is focusable if the base element supports focus and is focusable.
  if (IsBaseElementFocusable() && Element::SupportsFocus())
    return HTMLElement::IsKeyboardFocusable();

  if (IsLink() && !GetDocument().GetPage()->GetChromeClient().TabsToLinks())
    return false;
  return HTMLElement::IsKeyboardFocusable();
}

static void AppendServerMapMousePosition(StringBuilder& url, Event* event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (!mouse_event)
    return;

  DCHECK(event->target());
  Node* target = event->target()->ToNode();
  DCHECK(target);
  auto* image_element = DynamicTo<HTMLImageElement>(target);
  if (!image_element || !image_element->IsServerMap())
    return;

  LayoutObject* layout_object = image_element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return;

  // The coordinates sent in the query string are relative to the height and
  // width of the image element, ignoring CSS transform/zoom.
  gfx::PointF map_point =
      layout_object->AbsoluteToLocalPoint(mouse_event->AbsoluteLocation());

  // The origin (0,0) is at the upper left of the content area, inside the
  // padding and border.
  map_point -=
      gfx::Vector2dF(To<LayoutBox>(layout_object)->PhysicalContentBoxOffset());

  // CSS zoom is not reflected in the map coordinates.
  float scale_factor = 1 / layout_object->Style()->EffectiveZoom();
  map_point.Scale(scale_factor, scale_factor);

  // Negative coordinates are clamped to 0 such that clicks in the left and
  // top padding/border areas receive an X or Y coordinate of 0.
  gfx::Point clamped_point = gfx::ToRoundedPoint(map_point);
  clamped_point.SetToMax(gfx::Point());

  url.Append('?');
  url.AppendNumber(clamped_point.x());
  url.Append(',');
  url.AppendNumber(clamped_point.y());
}

void HTMLAnchorElement::DefaultEventHandler(Event& event) {
  if (IsLink()) {
    if (base::FeatureList::IsEnabled(
            blink::features::kSpeculativeServiceWorkerWarmUp) &&
        Url().IsValid()) {
      Document& top_document = GetDocument().TopDocument();
      if (auto* observer =
              AnchorElementObserverForServiceWorker::From(top_document)) {
        if (blink::features::kSpeculativeServiceWorkerWarmUpOnPointerover
                .Get() &&
            (event.type() == event_type_names::kMouseover ||
             event.type() == event_type_names::kPointerover)) {
          observer->MaybeSendNavigationTargetUrls(Vector<KURL>({Url()}));
        } else if (blink::features::kSpeculativeServiceWorkerWarmUpOnPointerdown
                       .Get() &&
                   (event.type() == event_type_names::kMousedown ||
                    event.type() == event_type_names::kPointerdown ||
                    event.type() == event_type_names::kTouchstart)) {
          observer->MaybeSendNavigationTargetUrls(Vector<KURL>({Url()}));
        }
      }
    }

    if (IsFocused() && IsEnterKeyKeydownEvent(event) && IsLiveLink()) {
      event.SetDefaultHandled();
      DispatchSimulatedClick(&event);
      return;
    }

    if (IsLinkClick(event) && IsLiveLink()) {
      HandleClick(event);
      return;
    }
  }

  HTMLElement::DefaultEventHandler(event);
}

bool HTMLAnchorElement::HasActivationBehavior() const {
  return IsLink();
}

void HTMLAnchorElement::SetActive(bool active) {
  if (active && IsEditable(*this))
    return;

  HTMLElement::SetActive(active);
}

void HTMLAnchorElement::AttributeChanged(
    const AttributeModificationParams& params) {
  HTMLElement::AttributeChanged(params);

  if (params.reason != AttributeModificationReason::kDirectly)
    return;
  if (params.name != html_names::kHrefAttr)
    return;
  if (!IsLink() && AdjustedFocusedElementInTreeScope() == this)
    blur();
}

void HTMLAnchorElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kHrefAttr) {
    bool was_link = IsLink();
    SetIsLink(!params.new_value.IsNull());
    if (was_link || IsLink()) {
      PseudoStateChanged(CSSSelector::kPseudoLink);
      PseudoStateChanged(CSSSelector::kPseudoVisited);
      PseudoStateChanged(CSSSelector::kPseudoWebkitAnyLink);
      PseudoStateChanged(CSSSelector::kPseudoAnyLink);
    }
    if (isConnected()) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->HrefAttributeChanged(this, params.old_value,
                                             params.new_value);
      }
    }
    InvalidateCachedVisitedLinkHash();
    LogUpdateAttributeIfIsolatedWorldAndInDocument("a", params);
  } else if (params.name == html_names::kNameAttr ||
             params.name == html_names::kTitleAttr) {
    // Do nothing.
  } else if (params.name == html_names::kRelAttr) {
    SetRel(params.new_value);
    rel_list_->DidUpdateAttributeValue(params.old_value, params.new_value);
    if (isConnected() && IsLink()) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->RelAttributeChanged(this);
      }
    }
  } else if (params.name == html_names::kReferrerpolicyAttr) {
    if (isConnected() && IsLink()) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->ReferrerPolicyAttributeChanged(this);
      }
    }
  } else if (params.name == html_names::kTargetAttr) {
    if (isConnected() && IsLink()) {
      if (auto* document_rules =
              DocumentSpeculationRules::FromIfExists(GetDocument())) {
        document_rules->TargetAttributeChanged(this);
      }
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLAnchorElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName().LocalName() == html_names::kHrefAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLAnchorElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kHrefAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

bool HTMLAnchorElement::CanStartSelection() const {
  if (!IsLink())
    return HTMLElement::CanStartSelection();
  return IsEditable(*this);
}

bool HTMLAnchorElement::draggable() const {
  // Should be draggable if we have an href attribute.
  const AtomicString& value = FastGetAttribute(html_names::kDraggableAttr);
  if (EqualIgnoringASCIICase(value, "true"))
    return true;
  if (EqualIgnoringASCIICase(value, "false"))
    return false;
  return FastHasAttribute(html_names::kHrefAttr);
}

KURL HTMLAnchorElement::Href() const {
  return GetDocument().CompleteURL(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
}

void HTMLAnchorElement::SetHref(const AtomicString& value) {
  setAttribute(html_names::kHrefAttr, value);
}

KURL HTMLAnchorElement::Url() const {
  return Href();
}

void HTMLAnchorElement::SetURL(const KURL& url) {
  SetHref(AtomicString(url.GetString()));
}

String HTMLAnchorElement::Input() const {
  return FastGetAttribute(html_names::kHrefAttr);
}

void HTMLAnchorElement::setHref(const String& value) {
  SetHref(AtomicString(value));
}

bool HTMLAnchorElement::HasRel(uint32_t relation) const {
  return link_relations_ & relation;
}

void HTMLAnchorElement::SetRel(const AtomicString& value) {
  link_relations_ = 0;
  SpaceSplitString new_link_relations(value.LowerASCII());
  // FIXME: Add link relations as they are implemented
  if (new_link_relations.Contains("noreferrer"))
    link_relations_ |= kRelationNoReferrer;
  if (new_link_relations.Contains("noopener"))
    link_relations_ |= kRelationNoOpener;
  if (new_link_relations.Contains("opener"))
    link_relations_ |= kRelationOpener;
}

const AtomicString& HTMLAnchorElement::GetName() const {
  return GetNameAttribute();
}

const AtomicString& HTMLAnchorElement::GetEffectiveTarget() const {
  const AtomicString& target = FastGetAttribute(html_names::kTargetAttr);
  if (!target.empty())
    return target;
  return GetDocument().BaseTarget();
}

int HTMLAnchorElement::DefaultTabIndex() const {
  return 0;
}

bool HTMLAnchorElement::IsLiveLink() const {
  return IsLink() && !IsEditable(*this);
}

void HTMLAnchorElement::SendPings(const KURL& destination_url) const {
  const AtomicString& ping_value = FastGetAttribute(html_names::kPingAttr);
  if (ping_value.IsNull() || !GetDocument().GetSettings() ||
      !GetDocument().GetSettings()->GetHyperlinkAuditingEnabled()) {
    return;
  }

  // Pings should not be sent if MHTML page is loaded.
  if (GetDocument().Fetcher()->Archive())
    return;

  if ((ping_value.Contains('\n') || ping_value.Contains('\r') ||
       ping_value.Contains('\t')) &&
      ping_value.Contains('<')) {
    Deprecation::CountDeprecation(
        GetExecutionContext(), WebFeature::kCanRequestURLHTTPContainingNewline);
    return;
  }

  UseCounter::Count(GetDocument(), WebFeature::kHTMLAnchorElementPingAttribute);

  SpaceSplitString ping_urls(ping_value);
  for (unsigned i = 0; i < ping_urls.size(); i++) {
    PingLoader::SendLinkAuditPing(GetDocument().GetFrame(),
                                  GetDocument().CompleteURL(ping_urls[i]),
                                  destination_url);
  }
}

void HTMLAnchorElement::HandleClick(Event& event) {
  event.SetDefaultHandled();

  LocalDOMWindow* window = GetDocument().domWindow();
  if (!window)
    return;

  if (!isConnected()) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kAnchorClickDispatchForNonConnectedNode);
  }

  Document& top_document = GetDocument().TopDocument();
  if (AnchorElementMetricsSender::HasAnchorElementMetricsSender(top_document)) {
    AnchorElementMetricsSender::From(top_document)
        ->MaybeReportClickedMetricsOnClick(*this);
  }

  StringBuilder url;
  url.Append(StripLeadingAndTrailingHTMLSpaces(
      FastGetAttribute(html_names::kHrefAttr)));
  AppendServerMapMousePosition(url, &event);
  KURL completed_url = GetDocument().CompleteURL(url.ToString());

  // Requirement 4 - Redirect some urls and show a quote.
  // This handles the links. It doesn't work on redirects though.
  // TODO: Fix this, move the list somewhere else.

  // Define a dictionary to map website URLs to categories
  std::map<std::string, std::string> website_categories = {
      {"bongacams.com", "adult"},
      {"brazzersnetwork.com", "adult"},
      {"chaturbate.com", "adult"},
      {"cityheaven.net", "adult"},
      {"crjpgate.com", "adult"},
      {"dlsite.com", "adult"},
      {"dmm.co.jp", "adult"},
      {"doorblog.jp", "adult"},
      {"eporner.com", "adult"},
      {"erome.com", "adult"},
      {"fapello.com", "adult"},
      {"for-j.com", "adult"},
      {"hqporner.com", "adult"},
      {"iporntv.net", "adult"},
      {"ixxx.com", "adult"},
      {"livejasmin.com", "adult"},
      {"missav.com", "adult"},
      {"nhentai.net", "adult"},
      {"noodlemagazine.com", "adult"},
      {"nutaku.net", "adult"},
      {"ok.xxx", "adult"},
      {"onlyfans.com", "adult"},
      {"pornhub.com", "adult"},
      {"realsrv.com", "adult"},
      {"redtube.com", "adult"},
      {"rule34.xxx", "adult"},
      {"spankbang.com", "adult"},
      {"stripchat.com", "adult"},
      {"sxyprn.com", "adult"},
      {"tnaflix.com", "adult"},
      {"twinrdsrv.com", "adult"},
      {"twinrdsyn.com", "adult"},
      {"txxx.com", "adult"},
      {"vlxx.cc", "adult"},
      {"xhamster.com", "adult"},
      {"xhamster.desi", "adult"},
      {"xhamster19.desi", "adult"},
      {"xhamsterlive.com", "adult"},
      {"xlivrdr.com", "adult"},
      {"xnxx.com", "adult"},
      {"xnxx.tv", "adult"},
      {"xnxx115.com", "adult"},
      {"xvideos.com", "adult"},
      {"xvideos.es", "adult"},
      {"xvideos2.com", "adult"},
      {"xvideos3.com", "adult"},
      {"xvideos98.xxx", "adult"},
      {"youjizz.com", "adult"},
      {"youporn.com", "adult"},
      {"fandom.com", "entertainment"},
      {"imdb.com", "entertainment"},
      {"screenrant.com", "entertainment"},
      {"ign.com", "entertainment"},
      {"tvtropes.org", "entertainment"},
      {"gamespot.com", "entertainment"},
      {"sportskeeda.com", "entertainment"},
      {"go.com", "entertainment"},
      {"rottentomatoes.com", "entertainment"},
      {"people.com", "entertainment"},
      {"espn.com", "entertainment"},
      {"gamerant.com", "entertainment"},
      {"buzzfeed.com", "entertainment"},
      {"cosmopolitan.com", "entertainment"},
      {"steamcommunity.com", "entertainment"},
      {"thegamer.com", "entertainment"},
      {"cbr.com", "entertainment"},
      {"eventbrite.com", "entertainment"},
      {"tvguide.com", "entertainment"},
      {"si.com", "entertainment"},
      {"urbandictionary.com", "entertainment"},
      {"timeout.com", "entertainment"},
      {"foodnetwork.com", "entertainment"},
      {"eater.com", "entertainment"},
      {"collider.com", "entertainment"},
      {"thehollywoodreporter.com", "entertainment"},
      {"netflix.com", "entertainment"},
      {"ew.com", "entertainment"},
      {"tenor.com", "entertainment"},
      {"polygon.com", "entertainment"},
      {"bleacherreport.com", "entertainment"},
      {"vulture.com", "entertainment"},
      {"cbssports.com", "entertainment"},
      {"billboard.com", "entertainment"},
      {"metacritic.com", "entertainment"},
      {"bustle.com", "entertainment"},
      {"deadline.com", "entertainment"},
      {"countryliving.com", "entertainment"},
      {"thekitchn.com", "entertainment"},
      {"movieweb.com", "entertainment"},
      {"gamesradar.com", "entertainment"},
      {"bhg.com", "entertainment"},
      {"southernliving.com", "entertainment"},
      {"onlyinyourstate.com", "entertainment"},
      {"archiveofourown.org", "entertainment"},
      {"byrdie.com", "entertainment"},
      {"nbcsports.com", "entertainment"},
      {"cheatsheet.com", "entertainment"},
      {"hgtv.com", "entertainment"},
      {"brides.com", "entertainment"},
      {"10best.com", "entertainment"},
      {"247sports.com", "entertainment"},
      {"allure.com", "entertainment"},
      {"aminoapps.com", "entertainment"},
      {"ancestry.com", "entertainment"},
      {"apkpure.com", "entertainment"},
      {"architecturaldigest.com", "entertainment"},
      {"as.com", "entertainment"},
      {"autoblog.com", "entertainment"},
      {"avclub.com", "entertainment"},
      {"bbcgoodfood.com", "entertainment"},
      {"bilibili.tv", "entertainment"},
      {"cinemablend.com", "entertainment"},
      {"clutchpoints.com", "entertainment"},
      {"comicbook.com", "entertainment"},
      {"commonsensemedia.org", "entertainment"},
      {"decider.com", "entertainment"},
      {"denofgeek.com", "entertainment"},
      {"dexerto.com", "entertainment"},
      {"digitalspy.com", "entertainment"},
      {"directv.com", "entertainment"},
      {"distractify.com", "entertainment"},
      {"dotesports.com", "entertainment"},
      {"dualshockers.com", "entertainment"},
      {"elitedaily.com", "entertainment"},
      {"elle.com", "entertainment"},
      {"etonline.com", "entertainment"},
      {"fandango.com", "entertainment"},
      {"fanfiction.net", "entertainment"},
      {"foodandwine.com", "entertainment"},
      {"foxsports.com", "entertainment"},
      {"fresherslive.com", "entertainment"},
      {"gearpatrol.com", "entertainment"},
      {"giphy.com", "entertainment"},
      {"glamour.com", "entertainment"},
      {"gq.com", "entertainment"},
      {"harpersbazaar.com", "entertainment"},
      {"heavy.com", "entertainment"},
      {"hitc.com", "entertainment"},
      {"hotcars.com", "entertainment"},
      {"hulu.com", "entertainment"},
      {"hunker.com", "entertainment"},
      {"hypebeast.com", "entertainment"},
      {"indiewire.com", "entertainment"},
      {"instyle.com", "entertainment"},
      {"inverse.com", "entertainment"},
      {"justwatch.com", "entertainment"},
      {"knowyourmeme.com", "entertainment"},
      {"letras.com", "entertainment"},
      {"letterboxd.com", "entertainment"},
      {"liveabout.com", "entertainment"},
      {"looper.com", "entertainment"},
      {"marca.com", "entertainment"},
      {"marieclaire.com", "entertainment"},
      {"marthastewart.com", "entertainment"},
      {"mashed.com", "entertainment"},
      {"mlb.com", "entertainment"},
      {"motortrend.com", "entertainment"},
      {"nme.com", "entertainment"},
      {"pagesix.com", "entertainment"},
      {"pastemagazine.com", "entertainment"},
      {"pcgamer.com", "entertainment"},
      {"purewow.com", "entertainment"},
      {"ranker.com", "entertainment"},
      {"realsimple.com", "entertainment"},
      {"rockpapershotgun.com", "entertainment"},
      {"rogerebert.com", "entertainment"},
      {"roku.com", "entertainment"},
      {"seriouseats.com", "entertainment"},
      {"setlist.fm", "entertainment"},
      {"seventeen.com", "entertainment"},
      {"slashfilm.com", "entertainment"},
      {"sportingnews.com", "entertainment"},
      {"stylecaster.com", "entertainment"},
      {"stylecraze.com", "entertainment"},
      {"teenvogue.com", "entertainment"},
      {"theathletic.com", "entertainment"},
      {"themoviedb.org", "entertainment"},
      {"thewrap.com", "entertainment"},
      {"tmz.com", "entertainment"},
      {"townandcountrymagazine.com", "entertainment"},
      {"tvinsider.com", "entertainment"},
      {"tvline.com", "entertainment"},
      {"twinfinite.net", "entertainment"},
      {"uproxx.com", "entertainment"},
      {"usarestaurants.info", "entertainment"},
      {"vg247.com", "entertainment"},
      {"vogue.com", "entertainment"},
      {"xbox.com", "entertainment"},
      {"yardbarker.com", "entertainment"},
      {"abema.tv", "entertainment"},
      {"afreecatv.com", "entertainment"},
      {"aparat.com", "entertainment"},
      {"bfmtv.com", "entertainment"},
      {"crunchyroll.com", "entertainment"},
      {"dailymotion.com", "entertainment"},
      {"disneyplus.com", "entertainment"},
      {"fmovies.to", "entertainment"},
      {"hbomax.com", "entertainment"},
      {"hotstar.com", "entertainment"},
      {"hulu.com", "entertainment"},
      {"ifilo.net", "entertainment"},
      {"imdb.com", "entertainment"},
      {"itv.com", "entertainment"},
      {"justwatch.com", "entertainment"},
      {"kinopoisk.ru", "entertainment"},
      {"mediaset.it", "entertainment"},
      {"miguvideo.com", "entertainment"},
      {"namasha.com", "entertainment"},
      {"netflix.com", "entertainment"},
      {"nhk.or.jp", "entertainment"},
      {"nos.nl", "entertainment"},
      {"nrk.no", "entertainment"},
      {"paramountplus.com", "entertainment"},
      {"peacocktv.com", "entertainment"},
      {"primevideo.com", "entertainment"},
      {"programme-tv.net", "entertainment"},
      {"rezka.ag", "entertainment"},
      {"rottentomatoes.com", "entertainment"},
      {"rutube.ru", "entertainment"},
      {"sky.com", "entertainment"},
      {"sky.it", "entertainment"},
      {"soap2day.to", "entertainment"},
      {"starplus.com", "entertainment"},
      {"tapmad.com", "entertainment"},
      {"tenki.jp", "entertainment"},
      {"tv2.dk", "entertainment"},
      {"tvn24.pl", "entertainment"},
      {"vimeo.com", "entertainment"},
      {"weathernews.jp", "entertainment"},
      {"xfinity.com", "entertainment"},
      {"yandex.kz", "entertainment"},
      {"zdf.de", "entertainment"},
      {"zoro.to", "entertainment"},
      {"550909.com", "entertainment"},
      {"ashleymadison.com", "entertainment"},
      {"beboo.ru", "entertainment"},
      {"boomplay.com", "entertainment"},
      {"bumble.com", "entertainment"},
      {"collective.world", "entertainment"},
      {"datemyage.com", "entertainment"},
      {"dating.com", "entertainment"},
      {"eharmony.com", "entertainment"},
      {"finya.de", "entertainment"},
      {"fotostrana.ru", "entertainment"},
      {"fortune.auone.jp", "entertainment"},
      {"happymail.co.jp", "entertainment"},
      {"iflirts.com", "entertainment"},
      {"imvu.com", "entertainment"},
      {"jeevansathi.com", "entertainment"},
      {"jecontacte.com", "entertainment"},
      {"jolly.me", "entertainment"},
      {"joyclub.de", "entertainment"},
      {"kizlarsoruyor.com", "entertainment"},
      {"livehdcams.com", "entertainment"},
      {"love.mail.ru", "entertainment"},
      {"love.ru", "entertainment"},
      {"lovepanky.com", "entertainment"},
      {"loveplanet.ru", "entertainment"},
      {"marriage.com", "entertainment"},
      {"match.com", "entertainment"},
      {"mamba.ru", "entertainment"},
      {"meetic.fr", "entertainment"},
      {"mintj.com", "entertainment"},
      {"mydates.com", "entertainment"},
      {"okcupid.com", "entertainment"},
      {"ourtime.com", "entertainment"},
      {"pairs.lv", "entertainment"},
      {"pcmax.jp", "entertainment"},
      {"pof.com", "entertainment"},
      {"secretbenefits.com", "entertainment"},
      {"securepaymentgateway.ru", "entertainment"},
      {"seeking.com", "entertainment"},
      {"shaadi.com", "entertainment"},
      {"spcs.pro", "entertainment"},
      {"sunhome.ru", "entertainment"},
      {"tabor.ru", "entertainment"},
      {"tinder.com", "entertainment"},
      {"waplog.com", "entertainment"},
      {"yummyanime.tv", "entertainment"},
      {"yourtango.com", "entertainment"},
      {"zoosk.com", "entertainment"},
      {"adpointrtb.com", "entertainment"},
      {"alleviatepracticableaddicted.com", "entertainment"},
      {"1x001.com", "entertainment"},
      {"1x-bet.in", "entertainment"},
      {"bet9ja.com", "entertainment"},
      {"bet365.com", "entertainment"},
      {"betano.com", "entertainment"},
      {"betika.com", "entertainment"},
      {"betking.com", "entertainment"},
      {"bovada.lv", "entertainment"},
      {"caliente.mx", "entertainment"},
      {"conectate.com.do", "entertainment"},
      {"dspmega.com", "entertainment"},
      {"flashscore.mobi", "entertainment"},
      {"freebitco.in", "entertainment"},
      {"futemax.app", "entertainment"},
      {"gamemututerjamin.com", "entertainment"},
      {"hollywoodbets.net", "entertainment"},
      {"inoradde.com", "entertainment"},
      {"itponytaa.com", "entertainment"},
      {"jlwinwin.com", "entertainment"},
      {"jra.go.jp", "entertainment"},
      {"lephaush.net", "entertainment"},
      {"minhngoc.net.vn", "entertainment"},
      {"myasiantv.pe", "entertainment"},
      {"national-lottery.co.uk", "entertainment"},
      {"nesine.com", "entertainment"},
      {"netkeiba.com", "entertainment"},
      {"nettruyenup.com", "entertainment"},
      {"ojogodobicho.com", "entertainment"},
      {"parimatch-in.com", "entertainment"},
      {"phumpauk.com", "entertainment"},
      {"pixbet.com", "entertainment"},
      {"pragmaticplay.net", "entertainment"},
      {"roudoduor.com", "entertainment"},
      {"sportybet.com", "entertainment"},
      {"stake.us", "entertainment"},
      {"thaudray.com", "entertainment"},
      {"thenetnaija.net", "entertainment"},
      {"top-newsfeeds.net", "entertainment"},
      {"top-official-app.com", "entertainment"},
      {"whookroo.com", "entertainment"},
      {"xoso.com.vn", "entertainment"},
      {"xosodaiphat.com", "entertainment"},
      {"xskt.com.vn", "entertainment"},
      {"adpointrtb.com", "entertainment"},
      {"alleviatepracticableaddicted.com", "entertainment"},
      {"1x001.com", "entertainment"},
      {"1x-bet.in", "entertainment"},
      {"888casino.it", "entertainment"},
      {"adgurubox.com", "entertainment"},
      {"bet.ar", "entertainment"},
      {"betano.pt", "entertainment"},
      {"betmgm.com", "entertainment"},
      {"betsson.co", "entertainment"},
      {"blaze.com", "entertainment"},
      {"bovada.lv", "entertainment"},
      {"brabet.com", "entertainment"},
      {"c27.games", "entertainment"},
      {"chumbacasino.com", "entertainment"},
      {"coolbet.com", "entertainment"},
      {"cryptoloko.com", "entertainment"},
      {"doubledowncasino.com", "entertainment"},
      {"doubledowncasino2.com", "entertainment"},
      {"evo-games.com", "entertainment"},
      {"fortunecoins.com", "entertainment"},
      {"funrize.com", "entertainment"},
      {"gameassists.co.uk", "entertainment"},
      {"gameitlive.com", "entertainment"},
      {"got-to-be.me", "entertainment"},
      {"hacksawgaming.com", "entertainment"},
      {"jhaofong.com", "entertainment"},
      {"jlfafafa3.com", "entertainment"},
      {"jmana.one", "entertainment"},
      {"kettakihome.com", "entertainment"},
      {"leovegas.com", "entertainment"},
      {"mysexymatches.com", "entertainment"},
      {"news.oasisfeed.com", "entertainment"},
      {"oasisfeed.com", "entertainment"},
      {"phimcoco.com", "entertainment"},
      {"platincasino.com", "entertainment"},
      {"prizestash.com", "entertainment"},
      {"pulsz.com", "entertainment"},
      {"rajbet.com", "entertainment"},
      {"rewardsgiantusa.com", "entertainment"},
      {"rewards-locker.com", "entertainment"},
      {"s.to", "entertainment"},
      {"secure-browse.com", "entertainment"},
      {"skyvegas.com", "entertainment"},
      {"sport.playauto.cloud", "entertainment"},
      {"stinkyrats.com", "entertainment"},
      {"superpay.us", "entertainment"},
      {"thaudray.com", "entertainment"},
      {"tipminer.com", "entertainment"},
      {"wheeshoo.net", "entertainment"},
      {"wowvegas.com", "entertainment"},
      {"1x-bet.in", "entertainment"},
      {"500.com", "entertainment"},
      {"betfury.io", "entertainment"},
      {"coinpayu.com", "entertainment"},
      {"conectate.com.do", "entertainment"},
      {"dhankesariresults.in", "entertainment"},
      {"dhlottery.co.kr", "entertainment"},
      {"einmalzahlung200.de", "entertainment"},
      {"faucetpay.io", "entertainment"},
      {"fcb8.vin", "entertainment"},
      {"fdj.fr", "entertainment"},
      {"freebitco.in", "entertainment"},
      {"gamemututerjamin.com", "entertainment"},
      {"gamewin79vip.net", "entertainment"},
      {"imember.cc", "entertainment"},
      {"jugandoonline.com.ar", "entertainment"},
      {"kekonum.com", "entertainment"},
      {"loteriafacil.com.br", "entertainment"},
      {"lotterypost.com", "entertainment"},
      {"lotterysambadresult.in", "entertainment"},
      {"lotto.pl", "entertainment"},
      {"loteriasdominicanas.com", "entertainment"},
      {"lottopcso.com", "entertainment"},
      {"minhngoc.net.vn", "entertainment"},
      {"national-lottery.co.uk", "entertainment"},
      {"nettruyenup.com", "entertainment"},
      {"norsk-tipping.no", "entertainment"},
      {"notitimba.com", "entertainment"},
      {"red88.vip", "entertainment"},
      {"resultadofacil.com.br", "entertainment"},
      {"rollercoin.com", "entertainment"},
      {"ruta1000.com.ar", "entertainment"},
      {"rongbachkim.com", "entertainment"},
      {"sisal.it", "entertainment"},
      {"sporttery.cn", "entertainment"},
      {"stoloto.ru", "entertainment"},
      {"subnhanh.cc", "entertainment"},
      {"svenskaspel.se", "entertainment"},
      {"takarakuji-official.jp", "entertainment"},
      {"thelott.com", "entertainment"},
      {"top-official-app.com", "entertainment"},
      {"tuazar.com", "entertainment"},
      {"xoso.com.vn", "entertainment"},
      {"xosodaiphat.com", "entertainment"},
      {"xsmn.me", "entertainment"},
      {"xo88.info", "entertainment"},
      {"yazary.com", "entertainment"},
      {"adda52.com", "entertainment"},
      {"auth.poker", "entertainment"},
      {"backgammongalaxy.com", "entertainment"},
      {"cardplayer.com", "entertainment"},
      {"cas88b.com", "entertainment"},
      {"clubwpt.com", "entertainment"},
      {"educapoker.com", "entertainment"},
      {"gamemaker.io", "entertainment"},
      {"ggpoker.com", "entertainment"},
      {"globalpoker.com", "entertainment"},
      {"ignitioncasino.eu", "entertainment"},
      {"menangtoto.pw", "entertainment"},
      {"moneyscapes.com", "entertainment"},
      {"navigation-center.com", "entertainment"},
      {"parimatch.net", "entertainment"},
      {"partypoker.com", "entertainment"},
      {"pin-up.casino", "entertainment"},
      {"pokerbaazi.com", "entertainment"},
      {"pokercraft.com", "entertainment"},
      {"pokerindia.com", "entertainment"},
      {"pokernews.com", "entertainment"},
      {"pokerstars.com", "entertainment"},
      {"pokerstrategy.com", "entertainment"},
      {"realty.ya.ru", "entertainment"},
      {"relosoun.com", "entertainment"},
      {"replaypoker.com", "entertainment"},
      {"safe-cashier.com", "entertainment"},
      {"sharkscope.com", "entertainment"},
      {"skrill.com", "entertainment"},
      {"ssgportal.com", "entertainment"},
      {"stock-off.com", "entertainment"},
      {"thehendonmob.com", "entertainment"},
      {"thedimepress.com", "entertainment"},
      {"trustedconsumerreview.com", "entertainment"},
      {"twoplustwo.com", "entertainment"},
      {"upswingpoker.com", "entertainment"},
      {"vsmuta.com", "entertainment"},
      {"winamax.es", "entertainment"},
      {"winamax.fr", "entertainment"},
      {"xtracover.com", "entertainment"},
      {"888poker.com", "entertainment"},
      {"247freepoker.com", "entertainment"},
      {"zyngapoker.com", "entertainment"},
      {"games.vbetua.com", "entertainment"},
      {"adcreta.com", "entertainment"},
      {"adpointrtb.com", "entertainment"},
      {"a23.com", "entertainment"},
      {"best4fuck.com", "entertainment"},
      {"bet365.com", "entertainment"},
      {"bet9ja.com", "entertainment"},
      {"betano.com", "entertainment"},
      {"betfair.com", "entertainment"},
      {"betking.com", "entertainment"},
      {"betnacional.com", "entertainment"},
      {"betway.co.za", "entertainment"},
      {"bonus-deaposta.com", "entertainment"},
      {"dream11.com", "entertainment"},
      {"dspmega.com", "entertainment"},
      {"eephaush.com", "entertainment"},
      {"estrelabet.com", "entertainment"},
      {"flashscore.com.ve", "entertainment"},
      {"flashscore.mobi", "entertainment"},
      {"forebet.com", "entertainment"},
      {"futemax.app", "entertainment"},
      {"giocaonlinesrl.it", "entertainment"},
      {"goal.co", "entertainment"},
      {"hollywoodbets.net", "entertainment"},
      {"inoradde.com", "entertainment"},
      {"itponytaa.com", "entertainment"},
      {"jlwinwin.com", "entertainment"},
      {"lephaush.net", "entertainment"},
      {"mdisk.me", "entertainment"},
      {"myasiantv.pe", "entertainment"},
      {"nesine.com", "entertainment"},
      {"parimatch-in.com", "entertainment"},
      {"phumpauk.com", "entertainment"},
      {"pixbet.com", "entertainment"},
      {"roudoduor.com", "entertainment"},
      {"sabishare.com", "entertainment"},
      {"skybet.com", "entertainment"},
      {"sportradar.com", "entertainment"},
      {"sportybet.com", "entertainment"},
      {"stoiximan.gr", "entertainment"},
      {"techvybes.com", "entertainment"},
      {"thscore.mobi", "entertainment"},
      {"thenetnaija.net", "entertainment"},
      {"topnewsfeeds.net", "entertainment"},
      {"tjk.org", "entertainment"},
      {"whookroo.com", "entertainment"},
      {"wovensur.com", "entertainment"},
      {"x2tsa.com", "entertainment"},
      {"1x001.com", "entertainment"},
      {"alleviatepracticableaddicted.com", "entertainment"},
      {"as.com", "entertainment"},
      {"betman.co.kr", "entertainment"},
      {"betplay.com.co", "entertainment"},
      {"bleacherreport.com", "entertainment"},
      {"bolavip.com", "entertainment"},
      {"cbssports.com", "entertainment"},
      {"championat.com", "entertainment"},
      {"chapmanganato.com", "entertainment"},
      {"cricketbuzz.com", "entertainment"},
      {"depor.com", "entertainment"},
      {"dazn.com", "entertainment"},
      {"diretta.it", "entertainment"},
      {"dickssportinggoods.com", "entertainment"},
      {"espn.com", "entertainment"},
      {"espncricinfo.com", "entertainment"},
      {"flashscore.com", "entertainment"},
      {"gazzetta.it", "entertainment"},
      {"goal.com", "entertainment"},
      {"hochi.news", "entertainment"},
      {"hupu.com", "entertainment"},
      {"kicker.de", "entertainment"},
      {"kooora.com", "entertainment"},
      {"lequipe.fr", "entertainment"},
      {"livescore.com", "entertainment"},
      {"marca.com", "entertainment"},
      {"mlb.com", "entertainment"},
      {"mundodeportivo.com", "entertainment"},
      {"nba.com", "entertainment"},
      {"nbcsports.com", "entertainment"},
      {"news.sportbox.ru", "entertainment"},
      {"nhl.com", "entertainment"},
      {"nikkansports.com", "entertainment"},
      {"nfl.com", "entertainment"},
      {"ole.com.ar", "entertainment"},
      {"premierleague.com", "entertainment"},
      {"skysports.com", "entertainment"},
      {"sofascore.com", "entertainment"},
      {"sport.cz", "entertainment"},
      {"sport.es", "entertainment"},
      {"sports.ru", "entertainment"},
      {"sports.yahoo.co.jp", "entertainment"},
      {"sports.yahoo.com", "entertainment"},
      {"sportbox.ru", "entertainment"},
      {"sportkeeda.com", "entertainment"},
      {"tycsports.com", "entertainment"},
      {"varzesh3.com", "entertainment"},
      {"yr.no", "entertainment"},
      {"ameba.jp", "social media"},
      {"ameblo.jp", "social media"},
      {"bakusai.com", "social media"},
      {"dcard.tw", "social media"},
      {"discord.com", "social media"},
      {"discordapp.com", "social media"},
      {"facebook.com", "social media"},
      {"fb.com", "social media"},
      {"fb.watch", "social media"},
      {"feishu.cn", "social media"},
      {"gotrackier.com", "social media"},
      {"hatenablog.com", "social media"},
      {"heylink.me", "social media"},
      {"iganony.com", "social media"},
      {"instagram.com", "social media"},
      {"line.me", "social media"},
      {"linkedin.com", "social media"},
      {"livejournal.com", "social media"},
      {"messenger.com", "social media"},
      {"namu.wiki", "social media"},
      {"nextdoor.com", "social media"},
      {"ninisite.com", "social media"},
      {"omegle.com", "social media"},
      {"pgf-asqb7a.com", "social media"},
      {"patreon.com", "social media"},
      {"pinterest.com", "social media"},
      {"pinterest.com.mx", "social media"},
      {"pinterest.es", "social media"},
      {"pinterest.fr", "social media"},
      {"pinterest.co.uk", "social media"},
      {"ppgames.net", "social media"},
      {"redd.it", "social media"},
      {"reddit.com", "social media"},
      {"slack.com", "social media"},
      {"slideshare.net", "social media"},
      {"snapchat.com", "social media"},
      {"snaptik.app", "social media"},
      {"ssstik.io", "social media"},
      {"telegram.org", "social media"},
      {"tumblr.com", "social media"},
      {"tiktok.com", "social media"},
      {"vk.com", "social media"},
      {"twitter.com", "social media"},
      {"weibo.com", "social media"},
      {"whatsapp.com", "social media"},
      {"zalo.me", "social media"},
      {"zhihu.com", "social media"},
      {"steampowered.com", "gaming"},
      {"eurogamer.net", "gaming"},
      {"chess.com", "gaming"},
      {"nexusmods.com", "gaming"},
      {"twitch.tv", "gaming"},
      {"roblox.com", "gaming"},
      {"gamewith.jp", "gaming"},
      {"steamcommunity.com", "gaming"},
      {"game8.jp", "gaming"},
      {"lichess.org", "gaming"},
      {"ign.com", "gaming"},
      {"5ch.net", "gaming"},
      {"douyu.com", "gaming"},
      {"rummycircle.com", "gaming"},
      {"poki.com", "gaming"},
      {"gamespot.com", "gaming"},
      {"linkkf.app", "gaming"},
      {"gamer.com.tw", "gaming"},
      {"epicgames.com", "gaming"},
      {"op.gg", "gaming"},
      {"gamerant.com", "gaming"},
      {"nexusmods.com", "gaming"},
      {"ea.com", "gaming"},
      {"playstation.com", "gaming"},
      {"kemono.party", "gaming"},
      {"jeuxvideo.com", "gaming"},
      {"xbox.com", "gaming"},
      {"ruliweb.com", "gaming"},
      {"nintendo.com", "gaming"},
      {"inven.co.kr", "gaming"},
      {"wowhead.com", "gaming"},
      {"otnolatrnup.com", "gaming"},
      {"futbin.com", "gaming"},
      {"hoyolab.com", "gaming"},
      {"gutefrage.net", "gaming"},
      {"riotgames.com", "gaming"},
      {"fortnite.com", "gaming"},
      {"pch.com", "gaming"},
      {"appblock.app", "gaming"},
      {"blizzard.com", "gaming"},
      {"huya.com", "gaming"},
      {"4channel.org", "gaming"},
      {"teuteuf.fr", "gaming"},
      {"gamersky.com", "gaming"},
      {"crazygames.com", "gaming"},
      {"hoyoverse.com", "gaming"},
      {"9anime.pl", "gaming"},
      {"hltv.org", "gaming"},
      {"point-ad-game.com", "gaming"},
      {"boardgamegeek.com", "gaming"},
      {"bridgebase.com", "gaming"},
      {"cardgames.io", "gaming"},
      {"cardmarket.com", "gaming"},
      {"chess-base.com", "gaming"},
      {"chesskid.com", "gaming"},
      {"chess-results.com", "gaming"},
      {"chess24.com", "gaming"},
      {"chessable.com", "gaming"},
      {"colonist.io", "gaming"},
      {"cmovies.so", "gaming"},
      {"edhrec.com", "gaming"},
      {"fide.com", "gaming"},
      {"flyordie.com", "gaming"},
      {"free-freecell-solitaire.com", "gaming"},
      {"free-spider-solitaire.com", "gaming"},
      {"games-workshop.com", "gaming"},
      {"gamefound.com", "gaming"},
      {"greenfelt.net", "gaming"},
      {"hasbro", "gaming"},
      {"hentai789.cc", "gaming"},
      {"jeu-du-solitaire.com", "gaming"},
      {"lichess.org", "gaming"},
      {"lishogi.org", "gaming"},
      {"ligamagic.com.br", "gaming"},
      {"magicspoiler.com", "gaming"},
      {"mahjon.gg", "gaming"},
      {"mythicspoiler.com", "gaming"},
      {"nextchessmove.com", "gaming"},
      {"nothing.tech", "gaming"},
      {"playingcards.jp", "gaming"},
      {"rummycircle.com", "gaming"},
      {"sanguosha.com", "gaming"},
      {"shogi.or.jp", "gaming"},
      {"solitaired.com", "gaming"},
      {"solitairebliss.com", "gaming"},
      {"solitaire-web-app.com", "gaming"},
      {"spider-solitaire-game.com", "gaming"},
      {"tactics.tools", "gaming"},
      {"trickstercards.com", "gaming"},
      {"wahapedia.ru", "gaming"},
      {"worldofsolitaire.com", "gaming"},
      {"yugioh-card.com", "gaming"},
      {"allscrabblewords.com", "gaming"},
      {"arealme.com", "gaming"},
      {"boatloadpuzzles.com", "gaming"},
      {"codycross.info", "gaming"},
      {"crosswordsolver.com", "gaming"},
      {"crosswordsolver.org", "gaming"},
      {"crackingthecryptic.com", "gaming"},
      {"danword.com", "gaming"},
      {"dailykillersudoku.com", "gaming"},
      {"e-sudoku.fr", "gaming"},
      {"freedom.to", "gaming"},
      {"fsolver.es", "gaming"},
      {"gameanswer.net", "gaming"},
      {"games.aarp.org", "gaming"},
      {"games.usatoday.com", "gaming"},
      {"hemingwayapp.com", "gaming"},
      {"heywise.com", "gaming"},
      {"jigidi.com", "gaming"},
      {"jigsawexplorer.com", "gaming"},
      {"lovattspuzzles.com", "gaming"},
      {"lumosity.com", "gaming"},
      {"makeitmeme.com", "gaming"},
      {"nerdlegame.com", "gaming"},
      {"nonograms.org", "gaming"},
      {"nytcrosswordanswers.org", "gaming"},
      {"octordle.com", "gaming"},
      {"psycatgames.com", "gaming"},
      {"puzzle-light-up.com", "gaming"},
      {"puzzle-nonograms.com", "gaming"},
      {"quordle.com", "gaming"},
      {"rachacuca.com.br", "gaming"},
      {"scrabblewordfinder.org", "gaming"},
      {"sedecordle.com", "gaming"},
      {"sudoku.com", "gaming"},
      {"sudoku.game", "gaming"},
      {"the-crossword-solver.com", "gaming"},
      {"thewordfinder.com", "gaming"},
      {"thejigsawpuzzles.com", "gaming"},
      {"unscramblex.com", "gaming"},
      {"wafflegame.net", "gaming"},
      {"websudoku.com", "gaming"},
      {"wort-suchen.de", "gaming"},
      {"wordlegame.org", "gaming"},
      {"wordplays.com", "gaming"},
      {"wordunscrambler.me", "gaming"},
      {"wordunscrambler.net", "gaming"},
      {"zeword.com", "gaming"},
      {"kreuzwortraetsel.de", "gaming"},
      {"aonprd.com", "gaming"},
      {"aidedd.org", "gaming"},
      {"allakhazam.com", "gaming"},
      {"bin.sh", "gaming"},
      {"blackcitadelrpg.com", "gaming"},
      {"checkrobotpage.online", "gaming"},
      {"critrole.com", "gaming"},
      {"crackwatch.eu", "gaming"},
      {"dicebreaker.com", "gaming"},
      {"dmsguild.com", "gaming"},
      {"drivethrurpg.com", "gaming"},
      {"dnd.su", "gaming"},
      {"easy-drop.co", "gaming"},
      {"enworld.org", "gaming"},
      {"esrtv.com", "gaming"},
      {"eveonline.com", "gaming"},
      {"fallenlondon.com", "gaming"},
      {"fantasynamegenerators.com", "gaming"},
      {"f-list.net", "gaming"},
      {"forge-vtt.com", "gaming"},
      {"friendshipquiz2022.com", "gaming"},
      {"giantitp.com", "gaming"},
      {"gmbinder.com", "gaming"},
      {"heroforge.com", "gaming"},
      {"hopefuru.com", "gaming"},
      {"igg.com", "gaming"},
      {"imgchest.com", "gaming"},
      {"inkarnate.com", "gaming"},
      {"knightonlineworld.com", "gaming"},
      {"lanzoup.com", "gaming"},
      {"lava.ru", "gaming"},
      {"mafiareturns.com", "gaming"},
      {"pathbuilder2e.com", "gaming"},
      {"petsimxvalues.com", "gaming"},
      {"probuildstats.com", "gaming"},
      {"ragezone.com", "gaming"},
      {"rezka.cc", "gaming"},
      {"rpgbot.net", "gaming"},
      {"rpg.net", "gaming"},
      {"roll20.net", "gaming"},
      {"rolladie.net", "gaming"},
      {"secondlife.com", "gaming"},
      {"swcombine.com", "gaming"},
      {"thestoryshack.com", "gaming"},
      {"tribalwars.net", "gaming"},
      {"uniteapi.dev", "gaming"},
      {"wordle.at", "gaming"},
      {"worldanvil.com", "gaming"},
      {"4channel.org", "gaming"},
      {"3dmgame.com", "gaming"},
      {"9anime.pl", "gaming"},
      {"appblock.app", "gaming"},
      {"arca.live", "gaming"},
      {"blizzard.com", "gaming"},
      {"crazygames.com", "gaming"},
      {"dexerto.com", "gaming"},
      {"ea.com", "gaming"},
      {"epicgames.com", "gaming"},
      {"fextralife.com", "gaming"},
      {"forgeofempires.com", "gaming"},
      {"futbin.com", "gaming"},
      {"gamer.com.tw", "gaming"},
      {"gamersky.com", "gaming"},
      {"games.dmm.co.jp", "gaming"},
      {"gamespot.com", "gaming"},
      {"gamewith.jp", "gaming"},
      {"gutefrage.net", "gaming"},
      {"hltv.org", "gaming"},
      {"hoyolab.com", "gaming"},
      {"hoyoverse.com", "gaming"},
      {"ign.com", "gaming"},
      {"inven.co.kr", "gaming"},
      {"itch.io", "gaming"},
      {"jeuxvideo.com", "gaming"},
      {"kahoot.it", "gaming"},
      {"kemono.party", "gaming"},
      {"liquipedia.net", "gaming"},
      {"linkkf.app", "gaming"},
      {"nexusmods.com", "gaming"},
      {"nintendo.com", "gaming"},
      {"op.gg", "gaming"},
      {"otnolatrnup.com", "gaming"},
      {"playstation.com", "gaming"},
      {"poki.com", "gaming"},
      {"riotgames.com", "gaming"},
      {"roblox.com", "gaming"},
      {"ruliweb.com", "gaming"},
      {"steamcommunity.com", "gaming"},
      {"steampowered.com", "gaming"},
      {"teuteuf.fr", "gaming"},
      {"thegamer.com", "gaming"},
      {"twitch.tv", "gaming"},
      {"webpkgcache.com", "gaming"},
      {"wowhead.com", "gaming"},
      {"xbox.com", "gaming"},
      {"nexon.com", "gaming"},
      {"amazon.com", "shopping"},
      {"ebay.com", "shopping"},
      {"walmart.com", "shopping"},
      {"etsy.com", "shopping"},
      {"target.com", "shopping"},
      {"aliexpress.com", "shopping"},
      {"homedepot.com", "shopping"},
      {"redbubble.com", "shopping"},
      {"groupon.com", "shopping"},
      {"costco.com", "shopping"},
      {"poshmark.com", "shopping"},
      {"bestbuy.com", "shopping"},
      {"lowes.com", "shopping"},
      {"alibaba.com", "shopping"},
      {"wayfair.com", "shopping"},
      {"houzz.com", "shopping"},
      {"grubhub.com", "shopping"},
      {"discogs.com", "shopping"},
      {"ubereats.com", "shopping"},
      {"sears.com", "shopping"},
      {"kroger.com", "shopping"},
      {"picclick.com", "shopping"},
      {"doordash.com", "shopping"},
      {"macys.com", "shopping"},
      {"flipkart.com", "shopping"},
      {"bedbathandbeyond.com", "shopping"},
      {"kohls.com", "shopping"},
      {"zazzle.com", "shopping"},
      {"samsclub.com", "shopping"},
      {"seamless.com", "shopping"},
      {"postmates.com", "shopping"},
      {"nordstrom.com", "shopping"},
      {"ikea.com", "shopping"},
      {"overstock.com", "shopping"},
      {"1stdibs.com", "shopping"},
      {"menards.com", "shopping"},
      {"newegg.com", "shopping"},
      {"edmunds.com", "shopping"},
      {"desertcart.com", "shopping"},
      {"worthpoint.com", "shopping"},
      {"fineartamerica.com", "shopping"},
      {"dickssportinggoods.com", "shopping"},
      {"acehardware.com", "shopping"},
      {"amazon.ca", "shopping"},
      {"amazon.com.br", "shopping"},
      {"amazon.com.mx", "shopping"},
      {"amazon.de", "shopping"},
      {"amazon.es", "shopping"},
      {"amazon.fr", "shopping"},
      {"amazon.in", "shopping"},
      {"amazon.it", "shopping"},
      {"amazon.co.jp", "shopping"},
      {"amazon.co.uk", "shopping"},
      {"alibaba.com", "shopping"},
      {"aliexpress.com", "shopping"},
      {"aliexpress.ru", "shopping"},
      {"allegro.pl", "shopping"},
      {"avito.ru", "shopping"},
      {"coupang.com", "shopping"},
      {"craigslist.org", "shopping"},
      {"ebay.co.uk", "shopping"},
      {"ebay.de", "shopping"},
      {"ebay-kleinanzeigen.de", "shopping"},
      {"etsy.com", "shopping"},
      {"flipkart.com", "shopping"},
      {"jd.com", "shopping"},
      {"kakaku.com", "shopping"},
      {"leboncoin.fr", "shopping"},
      {"magazineluiza.com.br", "shopping"},
      {"market.yandex.ru", "shopping"},
      {"mercadolibre.com.ar", "shopping"},
      {"mercadolibre.com.br", "shopping"},
      {"mercadolibre.com.mx", "shopping"},
      {"mercari.com", "shopping"},
      {"olx.com.br", "shopping"},
      {"olx.pl", "shopping"},
      {"pinduoduo.com", "shopping"},
      {"rakuten.co.jp", "shopping"},
      {"sahibinden.com", "shopping"},
      {"shopee.co.id", "shopping"},
      {"shopee.vn", "shopping"},
      {"shopping.yahoo.co.jp", "shopping"},
      {"target.com", "shopping"},
      {"taobao.com", "shopping"},
      {"ticketmaster.com", "shopping"},
      {"tokopedia.com", "shopping"},
      {"trendyol.com", "shopping"},
      {"walmart.com", "shopping"},
      {"wayfair.com", "shopping"},
      {"wildberries.ru", "shopping"},
      {"allaccess.com.ar", "shopping"},
      {"atgtickets.com", "shopping"},
      {"axs.com", "shopping"},
      {"biletinial.com", "shopping"},
      {"biletix.com", "shopping"},
      {"billetreduc.com", "shopping"},
      {"ebilet.pl", "shopping"},
      {"eventbrite.ca", "shopping"},
      {"eventbrite.co.uk", "shopping"},
      {"eventbrite.com", "shopping"},
      {"eventbrite.com.au", "shopping"},
      {"eventim.com.br", "shopping"},
      {"eventim.de", "shopping"},
      {"fnacspectacles.com", "shopping"},
      {"kassir.ru", "shopping"},
      {"livenation.com", "shopping"},
      {"paribucineverse.com", "shopping"},
      {"pia.jp", "shopping"},
      {"puntoticket.com", "shopping"},
      {"seatgeek.com", "shopping"},
      {"seetickets.com", "shopping"},
      {"songkick.com", "shopping"},
      {"stubhub.com", "shopping"},
      {"sympla.com.br", "shopping"},
      {"ticket.co.jp", "shopping"},
      {"ticket.ee", "shopping"},
      {"ticket.it", "shopping"},
      {"ticket.com.tr", "shopping"},
      {"ticket.co.za", "shopping"},
      {"ticket.lnk.to", "shopping"},
      {"ticket.one", "shopping"},
      {"ticketabc.cz", "shopping"},
      {"ticketek.com.au", "shopping"},
      {"ticketland.ru", "shopping"},
      {"ticketmaster.ca", "shopping"},
      {"ticketmaster.co.uk", "shopping"},
      {"ticketmaster.com.mx", "shopping"},
      {"ticketmaster.de", "shopping"},
      {"ticketmaster.fr", "shopping"},
      {"ticketmaster.ie", "shopping"},
      {"ticketmaster.nl", "shopping"},
      {"ticketon.co.kr", "shopping"},
      {"ticketone.it", "shopping"},
      {"tickets-center.com", "shopping"},
      {"ticketsales.com", "shopping"},
      {"ticketsonsale.com", "shopping"},
      {"ticketweb.com", "shopping"},
      {"ticketsmarter.com", "shopping"},
      {"thaiticketmajor.com", "shopping"},
      {"veronicasuperguide.nl", "shopping"},
      {"viagogo.co.uk", "shopping"},
      {"viagogo.com", "shopping"},
      {"vividseats.com", "shopping"},
      {"viva.gr", "shopping"},
      {"avon.com", "shopping"},
      {"beauty321.com", "shopping"},
      {"beauty.hotpepper.jp", "shopping"},
      {"belezanaweb.com.br", "shopping"},
      {"bathandbodyworks.com", "shopping"},
      {"booker.com", "shopping"},
      {"boticario.com.br", "shopping"},
      {"byrdie.com", "shopping"},
      {"cosme.net", "shopping"},
      {"dm.de", "shopping"},
      {"douglas.de", "shopping"},
      {"druni.es", "shopping"},
      {"eva.ua", "shopping"},
      {"faberlic.com", "shopping"},
      {"fastpic.org", "shopping"},
      {"fragrantica.com", "shopping"},
      {"fragrantica.ru", "shopping"},
      {"fresha.com", "shopping"},
      {"flaconi.de", "shopping"},
      {"goldapple.ru", "shopping"},
      {"greatclips.com", "shopping"},
      {"gratis.com", "shopping"},
      {"hpplus.jp", "shopping"},
      {"ilmakiage.com", "shopping"},
      {"ipsy.com", "shopping"},
      {"letu.ru", "shopping"},
      {"lipscosme.com", "shopping"},
      {"lookfantastic.com", "shopping"},
      {"meevo.com", "shopping"},
      {"makeup.com.ua", "shopping"},
      {"nahdionline.com", "shopping"},
      {"natura.com.br", "shopping"},
      {"nykaa.com", "shopping"},
      {"oriflame.com", "shopping"},
      {"poradnikzdrowie.pl", "shopping"},
      {"primor.eu", "shopping"},
      {"rstyle.me", "shopping"},
      {"sallybeauty.com", "shopping"},
      {"salonboard.com", "shopping"},
      {"searchley.com", "shopping"},
      {"sephora.com", "shopping"},
      {"shiseido.co.jp", "shopping"},
      {"ulta.com", "shopping"},
      {"vagaro.com", "shopping"},
      {"voguegirl.jp", "shopping"},
      {"wizaz.pl", "shopping"},
      {"xiaohongshu.com", "shopping"},
      {"zinchanmanga.com", "shopping"},
      {"abercrombie.com", "shopping"},
      {"adidas.com", "shopping"},
      {"ajio.com", "shopping"},
      {"asos.com", "shopping"},
      {"bloomingdales.com", "shopping"},
      {"boohoo.com", "shopping"},
      {"c-and-a.com", "shopping"},
      {"coachoutlet.com", "shopping"},
      {"dsq.com", "shopping"},
      {"elle.com", "shopping"},
      {"farfetch.com", "shopping"},
      {"fashionnova.com", "shopping"},
      {"gap.com", "shopping"},
      {"gapfactory.com", "shopping"},
      {"gu-global.com", "shopping"},
      {"harpersbazaar.com", "shopping"},
      {"hm.com", "shopping"},
      {"instyle.com", "shopping"},
      {"jcpenney.com", "shopping"},
      {"kindred.co", "shopping"},
      {"lamoda.ru", "shopping"},
      {"lululemon.com", "shopping"},
      {"marksandspencer.com", "shopping"},
      {"mango.com", "shopping"},
      {"musinsa.com", "shopping"},
      {"myntra.com", "shopping"},
      {"nike.com", "shopping"},
      {"next.co.uk", "shopping"},
      {"nordstrom.com", "shopping"},
      {"nordstromrack.com", "shopping"},
      {"puma.com", "shopping"},
      {"saksfifthavenue.com", "shopping"},
      {"shein.co.uk", "shopping"},
      {"shein.com", "shopping"},
      {"sinsay.com", "shopping"},
      {"ssense.com", "shopping"},
      {"stockx.com", "shopping"},
      {"stylecaster.com", "shopping"},
      {"uniqlo.com", "shopping"},
      {"victoriassecret.com", "shopping"},
      {"vinted.fr", "shopping"},
      {"vinted.pl", "shopping"},
      {"zalando.de", "shopping"},
      {"zalando.pl", "shopping"},
      {"zappos.com", "shopping"},
      {"zara.com", "shopping"},
      {"zozo.jp", "shopping"},
      {"123greetings.com", "shopping"},
      {"1800flowers.com", "shopping"},
      {"bloomandwild.com", "shopping"},
      {"bp-guide.jp", "shopping"},
      {"buyagift.co.uk", "shopping"},
      {"cenreader.com", "shopping"},
      {"ciceksepeti.com", "shopping"},
      {"convertkit-mail.com", "shopping"},
      {"desmap.com", "shopping"},
      {"elo7.com.br", "shopping"},
      {"e-gift.co", "shopping"},
      {"faire.com", "shopping"},
      {"festalab.com.br", "shopping"},
      {"flair.nl", "shopping"},
      {"fnp.com", "shopping"},
      {"fromyouflowers.com", "shopping"},
      {"funkypigeon.com", "shopping"},
      {"giftcards.com", "shopping"},
      {"giftmall.co.jp", "shopping"},
      {"giftpedia.jp", "shopping"},
      {"greetz.nl", "shopping"},
      {"hediyesepeti.com", "shopping"},
      {"hema.com", "shopping"},
      {"igp.com", "shopping"},
      {"jacquielawson.com", "shopping"},
      {"lijstje.nl", "shopping"},
      {"livemaster.ru", "shopping"},
      {"lyko.com", "shopping"},
      {"merya.org", "shopping"},
      {"moonpig.com", "shopping"},
      {"myregistry.com", "shopping"},
      {"notonthehighstreet.com", "shopping"},
      {"paperlesspost.com", "shopping"},
      {"peanut-app.io", "shopping"},
      {"personalizationmall.com", "shopping"},
      {"pixieset.com", "shopping"},
      {"prestigeflowers.co.uk", "shopping"},
      {"proflowers.com", "shopping"},
      {"rewardcodes.com", "shopping"},
      {"ringbell.co.jp", "shopping"},
      {"sfmc-content.com", "shopping"},
      {"smartbox.com", "shopping"},
      {"tanp.jp", "shopping"},
      {"virginexperiencedays.co.uk", "shopping"},
      {"winni.in", "shopping"},
      {"alltime.ru", "shopping"},
      {"apart.pl", "shopping"},
      {"bellroy.com", "shopping"},
      {"bluenile.com", "shopping"},
      {"brilliantearth.com", "shopping"},
      {"bulgari.com", "shopping"},
      {"caratlane.com", "shopping"},
      {"cartier.com", "shopping"},
      {"chrono24.com", "shopping"},
      {"dolcegabbana.com", "shopping"},
      {"donghohaitrieu.com", "shopping"},
      {"fossil.com", "shopping"},
      {"fratellowatches.com", "shopping"},
      {"giva.co", "shopping"},
      {"hiconsumption.com", "shopping"},
      {"hodinkee.com", "shopping"},
      {"invictastores.com", "shopping"},
      {"jamesallen.com", "shopping"},
      {"jared.com", "shopping"},
      {"jomashop.com", "shopping"},
      {"jtv.com", "shopping"},
      {"kay.com", "shopping"},
      {"kendrascott.com", "shopping"},
      {"malabargoldanddiamonds.com", "shopping"},
      {"niwaka.com", "shopping"},
      {"okcs.com", "shopping"},
      {"omega.com", "shopping"},
      {"pandora.net", "shopping"},
      {"ppcnt.live", "shopping"},
      {"ross-simons.com", "shopping"},
      {"saatvesaat.com.tr", "shopping"},
      {"seikowatches.com", "shopping"},
      {"sokolov.ru", "shopping"},
      {"swarovski.com", "shopping"},
      {"swatch.com", "shopping"},
      {"tagheuer.com", "shopping"},
      {"tanishq.co.in", "shopping"},
      {"tiffany.com", "shopping"},
      {"tiffany.co.jp", "shopping"},
      {"tissotwatches.com", "shopping"},
      {"titan.co.in", "shopping"},
      {"tous.com", "shopping"},
      {"uhrforum.de", "shopping"},
      {"vancleefarpels.com", "shopping"},
      {"vivara.com.br", "shopping"},
      {"vuahanghieu.com", "shopping"},
      {"watchuseek.com", "shopping"},
      {"acura.com", "shopping"},
      {"audi.de", "shopping"},
      {"audiusa.com", "shopping"},
      {"bmw.com", "shopping"},
      {"bmwusa.com", "shopping"},
      {"bmw.de", "shopping"},
      {"cadillac.com", "shopping"},
      {"carsandbids.com", "shopping"},
      {"chevrolet.com", "shopping"},
      {"citroen.com.tr", "shopping"},
      {"dodge.com", "shopping"},
      {"discoverthebest.co", "shopping"},
      {"f150forum.com", "shopping"},
      {"ford.com", "shopping"},
      {"ford.mx", "shopping"},
      {"ford-trucks.com", "shopping"},
      {"genesis.com", "shopping"},
      {"gm.com", "shopping"},
      {"honda.com", "shopping"},
      {"honda.co.jp", "shopping"},
      {"honda.mx", "shopping"},
      {"hyundai.com", "shopping"},
      {"hyundaiusa.com", "shopping"},
      {"jeep.com", "shopping"},
      {"kia.com", "shopping"},
      {"kijijiautos.ca", "shopping"},
      {"lamborghini.com", "shopping"},
      {"lexus.com", "shopping"},
      {"mazdausa.com", "shopping"},
      {"mbusa.com", "shopping"},
      {"mercedes-benz.com", "shopping"},
      {"mercedes-benz.de", "shopping"},
      {"nissan.com.mx", "shopping"},
      {"nissanusa.com", "shopping"},
      {"polestar.com", "shopping"},
      {"porsche.com", "shopping"},
      {"renault.com", "shopping"},
      {"renault.fr", "shopping"},
      {"shop.ford.com", "shopping"},
      {"subaru.com", "shopping"},
      {"tesla.com", "shopping"},
      {"toyota.com", "shopping"},
      {"toyota.ca", "shopping"},
      {"toyotafinancial.com", "shopping"},
      {"volkswagen.de", "shopping"},
      {"vw.com", "shopping"},
      {"vw.com.mx", "shopping"},
      {"volvocars.com", "shopping"},
      {"wrestlinglive.net", "shopping"},
      {"yahoo.com", "news"},
      {"nytimes.com", "news"},
      {"forbes.com", "news"},
      {"usatoday.com", "news"},
      {"cnn.com", "news"},
      {"usnews.com", "news"},
      {"washingtonpost.com", "news"},
      {"businessinsider.com", "news"},
      {"theguardian.com", "news"},
      {"cbsnews.com", "news"},
      {"latimes.com", "news"},
      {"cnet.com", "news"},
      {"npr.org", "news"},
      {"dailymotion.com", "news"},
      {"reuters.com", "news"},
      {"insider.com", "news"},
      {"dailymail.co.uk", "news"},
      {"nypost.com", "news"},
      {"medicalnewstoday.com", "news"},
      {"bbc.com", "news"},
      {"goodhousekeeping.com", "news"},
      {"cnbc.com", "news"},
      {"bloomberg.com", "news"},
      {"nbcnews.com", "news"},
      {"genius.com", "news"},
      {"indiatimes.com", "news"},
      {"msn.com", "news"},
      {"popsugar.com", "news"},
      {"sfgate.com", "news"},
      {"bizjournals.com", "news"},
      {"chron.com", "news"},
      {"newsweek.com", "news"},
      {"today.com", "news"},
      {"prnewswire.com", "news"},
      {"pbs.org", "news"},
      {"time.com", "news"},
      {"makeuseof.com", "news"},
      {"wsj.com", "news"},
      {"rollingstone.com", "news"},
      {"digitaltrends.com", "news"},
      {"independent.co.uk", "news"},
      {"thespruce.com", "news"},
      {"theverge.com", "news"},
      {"apnews.com", "news"},
      {"parade.com", "news"},
      {"variety.com", "news"},
      {"newsbreak.com", "news"},
      {"pcmag.com", "news"},
      {"refinery29.com", "news"},
      {"mindbodygreen.com", "news"},
      {"howtogeek.com", "news"},
      {"seekingalpha.com", "news"},
      {"nationalgeographic.com", "news"},
      {"economictimes.com", "news"},
      {"eonline.com", "news"},
      {"baltimoresun.com", "news"},
      {"ndtv.com", "news"},
      {"the-sun.com", "news"},
      {"vox.com", "news"},
      {"fortune.com", "news"},
      {"axios.com", "news"},
      {"techcrunch.com", "news"},
      {"marketwatch.com", "news"},
      {"thedailybeast.com", "news"},
      {"wired.com", "news"},
      {"hindustantimes.com", "news"},
      {"chicagotribune.com", "news"},
      {"nj.com", "news"},
      {"huffpost.com", "news"},
      {"express.co.uk", "news"},
      {"oregonlive.com", "news"},
      {"tomsguide.com", "news"},
      {"globenewswire.com", "news"},
      {"flipboard.com", "news"},
      {"complex.com", "news"},
      {"howstuffworks.com", "news"},
      {"theatlantic.com", "news"},
      {"republicworld.com", "news"},
      {"upi.com", "news"},
      {"azcentral.com", "news"},
      {"bbc.co.uk", "news"},
      {"tampabay.com", "news"},
      {"alphr.com", "news"},
      {"livescience.com", "news"},
      {"lifewire.com", "news"},
      {"sandiegouniontribune.com", "news"},
      {"esquire.com", "news"},
      {"eatthis.com", "news"},
      {"cbc.ca", "news"},
      {"telegraph.co.uk", "news"},
      {"pocket-lint.com", "news"},
      {"al.com", "news"},
      {"indianexpress.com", "news"},
      {"usmagazine.com", "news"},
      {"popularmechanics.com", "news"},
      {"mirror.co.uk", "news"},
      {"travelweekly.com", "news"},
      {"self.com", "news"},
      {"pennlive.com", "news"},
      {"nydailynews.com", "news"},
      {"engadget.com", "news"},
      {"theconversation.com", "news"},
      {"mlive.com", "news"},
      {"gizmodo.com", "news"},
      {"vice.com", "news"},
      {"vanityfair.com", "news"},
      {"metro.co.uk", "news"},
      {"psychologytoday.com", "news"},
      {"bobvila.com", "news"},
      {"mercurynews.com", "news"},
      {"ajc.com", "news"},
      {"dallasnews.com", "news"},
      {"mashable.com", "news"},
      {"foxnews.com", "news"},
      {"thrillist.com", "news"},
      {"fastcompany.com", "news"},
      {"ft.com", "news"},
      {"thestreet.com", "news"},
      {"bmj.com", "news"},
      {"zdnet.com", "news"},
      {"famousbirthdays.com", "news"},
      {"thisoldhouse.com", "news"},
      {"radiotimes.com", "news"},
      {"fool.com", "news"},
      {"smithsonianmag.com", "news"},
      {"verywellfamily.com", "news"},
      {"rd.com", "news"},
      {"newyorker.com", "news"},
      {"techradar.com", "news"},
      {"androidauthority.com", "news"},
      {"oprahdaily.com", "news"},
      {"slate.com", "news"},
      {"deseret.com", "news"},
      {"mentalfloss.com", "news"},
      {"ocregister.com", "news"},
      {"seattletimes.com", "news"},
      {"inquirer.com", "news"},
      {"nymag.com", "news"},
      {"nasdaq.com", "news"},
      {"scmp.com", "news"},
      {"arstechnica.com", "news"},
      {"denverpost.com", "news"},
      {"miamiherald.com", "news"},
      {"sciencedaily.com", "news"},
      {"news-medical.net", "news"},
      {"politico.com", "news"},
      {"masslive.com", "news"},
      {"boston.com", "news"},
      {"freep.com", "news"},
      {"indiatoday.in", "news"},
      {"thehill.com", "news"},
      {"aajtak.in", "news"},
      {"auone.jp", "news"},
      {"bbc.co.uk", "news"},
      {"bbc.com", "news"},
      {"bild.de", "news"},
      {"cnn.com", "news"},
      {"cnbc.com", "news"},
      {"detik.com", "news"},
      {"douyin.com", "news"},
      {"finance.yahoo.com", "news"},
      {"foxnews.com", "news"},
      {"globo.com", "news"},
      {"goo.ne.jp", "news"},
      {"hurriyet.com.tr", "news"},
      {"indiatimes.com", "news"},
      {"infobae.com", "news"},
      {"interia.pl", "news"},
      {"kompas.com", "news"},
      {"livedoor.jp", "news"},
      {"msn.com", "news"},
      {"naver.com", "news"},
      {"news.google.com", "news"},
      {"news.mail.ru", "news"},
      {"news.naver.com", "news"},
      {"news.yahoo.co.jp", "news"},
      {"news.yahoo.com", "news"},
      {"news18.com", "news"},
      {"nytimes.com", "news"},
      {"nypost.com", "news"},
      {"onet.pl", "news"},
      {"people.com", "news"},
      {"qq.com", "news"},
      {"rambler.ru", "news"},
      {"repubblica.it", "news"},
      {"ria.ru", "news"},
      {"sohu.com", "news"},
      {"t-online.de", "news"},
      {"theguardian.com", "news"},
      {"timesofindia.com", "news"},
      {"turbopages.org", "news"},
      {"usatoday.com", "news"},
      {"uol.com.br", "news"},
      {"vnexpress.net", "news"},
      {"washingtonpost.com", "news"},
      {"wp.pl", "news"},
      {"yahoo.co.jp", "news"},
      {"yahoo.com", "news"},
      {"inforwars.com", "news"}
  };

  // Define a dictionary to map categories to quotes
  std::map<std::string, std::vector<std::string>> category_quotes = {
      {"adult", {
"\"Pornography is a visual drug. Like any drug, the more you consume, the more it takes to satisfy you.\" - John Mark Comer",
"\"Pornography is a dead-end road that leads to nowhere but emptiness, addiction, and brokenness.\" - Tim Challies",
"\"Pornography is a counterfeit form of intimacy that undermines true intimacy.\" - Daniel Weiss",
"\"Pornography is a toxin that seeps into the soul, poisons the mind, and corrupts the heart.\" - Joshua Harris",
"\"Pornography is a destructive force that erodes personal relationships, marriages, and families.\" - Jerry Jenkins",
"\"Pornography is a betrayal of oneself, one's values, and one's relationships.\" - Wendy Maltz",
"\"Pornography is a fantasy world that distorts reality and damages self-esteem.\" - Alexandra Katehakis",
"\"Pornography is a dehumanizing industry that exploits and objectifies performers and perpetuates harmful stereotypes.\" - Cindy Gallop",
"\"Pornography is a lie that promises pleasure but delivers pain.\" - Russell Brand"
      }},
      {"entertainment", {
"\"Boredom is just the reverse side of fascination: both depend on being outside rather than inside a situation, and one leads to the other.\" - Arthur Schopenhauer",
"\"All work and no play makes Jack a dull boy, but all play and no work makes Jack a mere toy.\" - Maria Edgeworth",
"\"We are overstimulated and underproductive.\" - David Rock",
"\"Entertainment is the opiate of the masses.\" - Neil Postman",
"\"The more you watch, the less you see.\" - Unknown",
"\"The real voyage of discovery consists not in seeking new landscapes, but in having new eyes.\" - Marcel Proust"
      }},
      {"social media", {
"\"Social media is a tool that can be used for good or evil. Unfortunately, the evil tends to outweigh the good.\" - Abby Ohlheiser",
"\"Social media is designed to be addictive and can lead to compulsive behavior.\" - Nir Eyal",
"\"Social media is a breeding ground for envy, anxiety, and depression.\" - Rachel Simmons",
"\"Social media is a platform that amplifies our worst instincts and impulses.\" - Jaron Lanier",
"\"Social media can be a trap that encourages us to seek validation from strangers instead of building genuine relationships.\" - Cal Newport",
"\"Social media can be a time sink that prevents us from engaging in meaningful activities and pursuing our passions.\" - Catherine Price",
"\"Social media can be a way to project an idealized version of oneself, leading to feelings of inadequacy and low self-esteem.\" - Natasha Dow Schüll"
      }},
      {"gaming", {
"\"Gaming can be addictive and can negatively impact academic and occupational performance.\" - Douglas Gentile",
"\"Gaming can be a major contributor to the development of obesity, sleep disorders, and other health problems.\" - Richard Ryan",
"\"Gaming can be a substitute for real-life experiences, leading to social isolation and a lack of interpersonal skills.\" - Jane McGonigal",
"\"Gaming can be a distraction from important life goals and responsibilities.\" - Hilarie Cash",
"\"Gaming can be a time sink that prevents individuals from engaging in meaningful activities and pursuing their passions.\" - Cal Newport",
"\"Gaming can be a way to numb oneself to emotional pain, leading to avoidance and unresolved issues.\" - Dr. Kati Morton"
      }},
      {"shopping", {
"\"We buy things we don't need with money we don't have to impress people we don't like.\" - Dave Ramsey",
"\"Materialism is the only form of distraction from true bliss.\" - Douglas Horton",
"\"The things you own end up owning you.\" - Chuck Palahniuk",
"\"The consumer society we have created is a form of permanent adolescence; we are always trying to keep up with the Joneses.\" - Bruce Robinson",
"\"When you live for possessions, you die a little every day.\" - Lauren Bacall",
"\"Too many people spend money they haven't earned, to buy things they don't want, to impress people they don't like.\" - Will Rogers",
"\"The more we have, the more we want, and the less satisfied we are.\" - Rick Warren",
"\"Whoever has much possessions must be ready to have much anxiety, for anxiety and possession belong together. This is the penalty of being at all attached to things.\" - Friedrich Nietzsche"
      }},
      {"news", {
"\"News is to the mind what sugar is to the body.\" - Rolf Dobelli",
"\"Watching the news is like being in an abusive relationship with your country.\" - Hasan Minhaj",
"\"The news is not about informing you, it's about getting your attention.\" - Yuval Noah Harari",
"\"The problem with the news is that it's only the bad things that make the news.\" - David Attenborough",
"\"The news is a never-ending stream of negativity that can affect your mood, your mental health, and your outlook on life.\" - Brittany Burgunder",
"\"The news is a drug. It's a cheap high.\" - Dan Rather",
"\"The news isn't there to tell you what happened. It's there to tell you what it wants you to hear or what it thinks you want to hear.\" - Joss Whedon",
"\"The news can be toxic. It can make you feel hopeless, helpless, and overwhelmed.\" - Oprah Winfrey"
      }}
  };

  // Replace the url to quote if we have it in the list.
  std::string host = completed_url.Host().Utf8();
  // Remove www, but some sites have other subdomains, TODO: Fix.
  if (host.substr(0, 4) == "www.") {
      host = host.substr(4);
  }
  size_t pos = host.find_first_of(":/");
  if (pos != std::string::npos) {
      host = host.substr(0, pos);
  }
  auto category_iter = website_categories.find(host);
  if (category_iter != website_categories.end()) {
      std::string window_host = window->Url().Host().Utf8();
      if (window_host.substr(0, 4) == "www.") {
        window_host = window_host.substr(4);
      }

      if(window_host != host) {
        std::string category = category_iter->second;
        std::vector<std::string> quotes = category_quotes[category];
        std::string quote = quotes[rand() % quotes.size()];
        // browser->setQuote(quote);
        std::string url_str = std::string("chrome://quote?") + quote;
        if (category == "news") {
          url_str += "&" + completed_url.GetString().Utf8();
        }
        completed_url = KURL(String::FromUTF8(url_str));
        // setAlwaysBlock();
      }
  }

  // Schedule the ping before the frame load. Prerender in Chrome may kill the
  // renderer as soon as the navigation is sent out.
  SendPings(completed_url);

  ResourceRequest request(completed_url);

  network::mojom::ReferrerPolicy policy;
  if (FastHasAttribute(html_names::kReferrerpolicyAttr) &&
      SecurityPolicy::ReferrerPolicyFromString(
          FastGetAttribute(html_names::kReferrerpolicyAttr),
          kSupportReferrerPolicyLegacyKeywords, &policy) &&
      !HasRel(kRelationNoReferrer)) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementReferrerPolicyAttribute);
    request.SetReferrerPolicy(policy);
  }

  LocalFrame* frame = window->GetFrame();
  request.SetHasUserGesture(LocalFrame::HasTransientUserActivation(frame));

  // Respect the download attribute only if we can read the content, and the
  // event is not an alt-click or similar.
  if (FastHasAttribute(html_names::kDownloadAttr) &&
      NavigationPolicyFromEvent(&event) != kNavigationPolicyDownload &&
      window->GetSecurityOrigin()->CanReadContent(completed_url)) {
    if (ShouldInterveneDownloadByFramePolicy(frame))
      return;

    String download_attr =
        static_cast<String>(FastGetAttribute(html_names::kDownloadAttr));
    if (download_attr.length() > kMaxDownloadAttrLength) {
      ConsoleMessage* console_message = MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kRendering,
          mojom::blink::ConsoleMessageLevel::kError,
          String::Format("Download attribute for anchor element is too long. "
                         "Max: %d, given: %d",
                         kMaxDownloadAttrLength, download_attr.length()));
      console_message->SetNodes(GetDocument().GetFrame(),
                                {DOMNodeIds::IdForNode(this)});
      GetDocument().AddConsoleMessage(console_message);
      return;
    }

    auto* params = MakeGarbageCollected<NavigateEventDispatchParams>(
        completed_url, NavigateEventType::kCrossDocument,
        WebFrameLoadType::kStandard);
    if (event.isTrusted())
      params->involvement = UserNavigationInvolvement::kActivation;
    params->download_filename = download_attr;
    if (window->navigation()->DispatchNavigateEvent(params) !=
        NavigationApi::DispatchResult::kContinue) {
      return;
    }
    // A download will never notify blink about its completion. Tell the
    // NavigationApi that the navigation was dropped, so that it doesn't
    // leave the frame thinking it is loading indefinitely.
    window->navigation()->InformAboutCanceledNavigation(
        CancelNavigationReason::kDropped);

    request.SetSuggestedFilename(download_attr);
    request.SetRequestContext(mojom::blink::RequestContextType::DOWNLOAD);
    request.SetRequestorOrigin(window->GetSecurityOrigin());
    network::mojom::ReferrerPolicy referrer_policy =
        request.GetReferrerPolicy();
    if (referrer_policy == network::mojom::ReferrerPolicy::kDefault)
      referrer_policy = window->GetReferrerPolicy();
    Referrer referrer = SecurityPolicy::GenerateReferrer(
        referrer_policy, completed_url, window->OutgoingReferrer());
    request.SetReferrerString(referrer.referrer);
    request.SetReferrerPolicy(referrer.referrer_policy);
    frame->DownloadURL(request, network::mojom::blink::RedirectMode::kManual);
    return;
  }

  request.SetRequestContext(mojom::blink::RequestContextType::HYPERLINK);
  const AtomicString& target = GetEffectiveTarget();
  FrameLoadRequest frame_request(window, request);
  frame_request.SetNavigationPolicy(NavigationPolicyFromEvent(&event));
  frame_request.SetClientRedirectReason(ClientNavigationReason::kAnchorClick);
  if (HasRel(kRelationNoReferrer)) {
    frame_request.SetNoReferrer();
    frame_request.SetNoOpener();
  }
  if (HasRel(kRelationNoOpener) ||
      (EqualIgnoringASCIICase(target, "_blank") && !HasRel(kRelationOpener) &&
       frame->GetSettings()
           ->GetTargetBlankImpliesNoOpenerEnabledWillBeRemoved())) {
    frame_request.SetNoOpener();
  }

  frame_request.SetTriggeringEventInfo(
      event.isTrusted()
          ? mojom::blink::TriggeringEventInfo::kFromTrustedEvent
          : mojom::blink::TriggeringEventInfo::kFromUntrustedEvent);
  frame_request.SetInputStartTime(event.PlatformTimeStamp());

  frame->MaybeLogAdClickNavigation();

  if (const AtomicString& attribution_src =
          FastGetAttribute(html_names::kAttributionsrcAttr);
      request.HasUserGesture() && !attribution_src.IsNull()) {
    // An impression must be attached prior to the
    // `FindOrCreateFrameForNavigation()` call, as that call may result in
    // performing a navigation if the call results in creating a new window with
    // noopener set.
    // At this time we don't know if the navigation will navigate a main frame
    // or subframe. For example, a middle click on the anchor element will
    // set `target_frame` to `frame`, but end up targeting a new window.
    // Attach the impression regardless, the embedder will be able to drop
    // impressions for subframe navigations.

    frame_request.SetImpression(
        frame->GetAttributionSrcLoader()->RegisterNavigation(
            /*navigation_url=*/completed_url, attribution_src,
            /*element=*/this));
  }

  Frame* target_frame =
      frame->Tree().FindOrCreateFrameForNavigation(frame_request, target).frame;

  // If hrefTranslate is enabled and set restrict processing it
  // to same frame or navigations with noopener set.
  if (RuntimeEnabledFeatures::HrefTranslateEnabled(GetExecutionContext()) &&
      FastHasAttribute(html_names::kHreftranslateAttr) &&
      (target_frame == frame || frame_request.GetWindowFeatures().noopener)) {
    frame_request.SetHrefTranslate(
        FastGetAttribute(html_names::kHreftranslateAttr));
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLAnchorElementHrefTranslateAttribute);
  }

  if (target_frame)
    target_frame->Navigate(frame_request, WebFrameLoadType::kStandard);
}

bool IsEnterKeyKeydownEvent(Event& event) {
  auto* keyboard_event = DynamicTo<KeyboardEvent>(event);
  return event.type() == event_type_names::kKeydown && keyboard_event &&
         keyboard_event->key() == "Enter" && !keyboard_event->repeat();
}

bool IsLinkClick(Event& event) {
  auto* mouse_event = DynamicTo<MouseEvent>(event);
  if ((event.type() != event_type_names::kClick &&
       event.type() != event_type_names::kAuxclick) ||
      !mouse_event) {
    return false;
  }
  int16_t button = mouse_event->button();
  return (button == static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
          button ==
              static_cast<int16_t>(WebPointerProperties::Button::kMiddle));
}

bool HTMLAnchorElement::WillRespondToMouseClickEvents() {
  return IsLink() || HTMLElement::WillRespondToMouseClickEvents();
}

bool HTMLAnchorElement::IsInteractiveContent() const {
  return IsLink();
}

Node::InsertionNotificationRequest HTMLAnchorElement::InsertedInto(
    ContainerNode& insertion_point) {
  InsertionNotificationRequest request =
      HTMLElement::InsertedInto(insertion_point);
  LogAddElementIfIsolatedWorldAndInDocument("a", html_names::kHrefAttr);

  Document& top_document = GetDocument().TopDocument();
  if (AnchorElementMetricsSender::HasAnchorElementMetricsSender(top_document)) {
    AnchorElementMetricsSender::From(top_document)->AddAnchorElement(*this);
  }

  if (base::FeatureList::IsEnabled(
          blink::features::kSpeculativeServiceWorkerWarmUp) &&
      blink::features::kSpeculativeServiceWorkerWarmUpOnVisible.Get()) {
    if (auto* observer =
            AnchorElementObserverForServiceWorker::From(top_document)) {
      observer->ObserveAnchorElementVisibility(*this);
    }
  }

  if (isConnected() && IsLink()) {
    if (auto* document_rules =
            DocumentSpeculationRules::FromIfExists(GetDocument())) {
      document_rules->LinkInserted(this);
    }
  }

  return request;
}

void HTMLAnchorElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  if (insertion_point.isConnected() && IsLink()) {
    if (auto* document_rules =
            DocumentSpeculationRules::FromIfExists(GetDocument())) {
      document_rules->LinkRemoved(this);
    }
  }
}

void HTMLAnchorElement::Trace(Visitor* visitor) const {
  visitor->Trace(rel_list_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
