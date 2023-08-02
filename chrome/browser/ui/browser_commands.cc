// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_commands.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chained_back_navigation_tracker.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/dom_distiller/tab_utils.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feed/web_feed_ui_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/autofill/payments/iban_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_bubble_controller_impl.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/commander/commander.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/sharing_hub/screenshot/screenshot_captured_bubble_controller.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/google/core/common/google_util.h"
#include "components/lens/buildflags.h"
#include "components/media_router/browser/media_router_dialog_controller.h"  // nogncheck
#include "components/media_router/browser/media_router_metrics.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sessions/core/live_tab_context.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/translate/core/browser/language_state.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/common/user_agent.h"
#include "extensions/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

#if BUILDFLAG(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // BUILDFLAG(ENABLE_PRINTING)

#if BUILDFLAG(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"
#include "chromeos/ui/wm/features.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#include "chrome/browser/lens/region_search/lens_region_search_helper.h"
#include "components/lens/lens_features.h"
#endif

namespace {

const char kOsOverrideForTabletSite[] = "Linux; Android 9; Chrome tablet";
const char kChPlatformOverrideForTabletSite[] = "Android";
const char kBackForwardNavigationIsTriggered[] =
    "back_forward_navigation_is_triggered";

// Creates a new tabbed browser window, with the same size, type and profile as
// |original_browser|'s window, inserts |contents| into it, and shows it.
void CreateAndShowNewWindowWithContents(
    std::unique_ptr<content::WebContents> contents,
    const Browser* original_browser) {
  Browser* new_browser = nullptr;
  DCHECK(!original_browser->is_type_app_popup());
  if (original_browser->is_type_app()) {
    new_browser = Browser::Create(Browser::CreateParams::CreateForApp(
        original_browser->app_name(), original_browser->is_trusted_source(),
        gfx::Rect(), original_browser->profile(), true));
  } else {
    new_browser = Browser::Create(Browser::CreateParams(
        original_browser->type(), original_browser->profile(), true));
  }
  // Preserve the size of the original window. The new window has already
  // been given an offset by the OS, so we shouldn't copy the old bounds.
  BrowserWindow* new_window = new_browser->window();
  new_window->SetBounds(
      gfx::Rect(new_window->GetRestoredBounds().origin(),
                original_browser->window()->GetRestoredBounds().size()));

  // We need to show the browser now.  Otherwise ContainerWin assumes the
  // WebContents is invisible and won't size it.
  new_browser->window()->Show();

  // The page transition below is only for the purpose of inserting the tab.
  new_browser->tab_strip_model()->AddWebContents(std::move(contents), -1,
                                                 ui::PAGE_TRANSITION_LINK,
                                                 AddTabTypes::ADD_ACTIVE);
}

bool GetTabURLAndTitleToSave(content::WebContents* web_contents,
                             GURL* url,
                             std::u16string* title) {
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/799668
  if (!web_contents)
    return false;
  return chrome::GetURLAndTitleToBookmark(web_contents, url, title);
}

ReadingListModel* GetReadingListModel(Browser* browser) {
  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser->profile());
  if (!model || !model->loaded())
    return nullptr;  // Ignore requests until model has loaded.
  return model;
}

bool CanMoveWebContentsToReadLater(Browser* browser,
                                   content::WebContents* web_contents,
                                   ReadingListModel* model,
                                   GURL* url,
                                   std::u16string* title) {
  return model && GetTabURLAndTitleToSave(web_contents, url, title) &&
         model->IsUrlSupported(*url) && !browser->profile()->IsGuestSession();
}

bool BookmarkCurrentTabHelper(Browser* browser,
                              bookmarks::BookmarkModel* model,
                              GURL* url,
                              std::u16string* title) {
  if (!model || !model->loaded())
    return false;  // Ignore requests until bookmarks are loaded.

  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // |web_contents| can be nullptr if the last tab in the browser was closed
  // but the browser wasn't closed yet. https://crbug.com/799668
  if (!web_contents)
    return false;
  if (!chrome::GetURLAndTitleToBookmark(web_contents, url, title))
    return false;
  bool is_bookmarked_by_any = model->IsBookmarked(*url);
  if (!is_bookmarked_by_any &&
      web_contents->GetBrowserContext()->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    favicon::SaveFaviconEvenIfInIncognito(web_contents);
  }
  return true;
}

}  // namespace

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace chrome {
namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtensionForBrowser(Browser* browser) {
  return extensions::ExtensionRegistry::Get(browser->profile())
      ->GetExtensionById(
          web_app::GetAppIdFromApplicationName(browser->app_name()),
          extensions::ExtensionRegistry::EVERYTHING);
}
#endif

// Based on |disposition|, creates a new tab as necessary, and returns the
// appropriate tab to navigate.  If that tab is the |current_tab|, reverts the
// location bar contents, since all browser-UI-triggered navigations should
// revert any omnibox edits in the |current_tab|.
WebContents* GetTabAndRevertIfNecessaryHelper(Browser* browser,
                                              WindowOpenDisposition disposition,
                                              WebContents* current_tab) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB)
        new_tab->WasHidden();
      const int index =
          browser->tab_strip_model()->GetIndexOfWebContents(current_tab);
      const auto group = browser->tab_strip_model()->GetTabGroupForTab(index);
      browser->tab_strip_model()->AddWebContents(
          std::move(new_tab), -1, ui::PAGE_TRANSITION_LINK,
          (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
              ? AddTabTypes::ADD_ACTIVE
              : AddTabTypes::ADD_NONE,
          group);
      return raw_new_tab;
    }
    case WindowOpenDisposition::NEW_WINDOW: {
      std::unique_ptr<WebContents> new_tab = current_tab->Clone();
      WebContents* raw_new_tab = new_tab.get();
      Browser* new_browser =
          Browser::Create(Browser::CreateParams(browser->profile(), true));
      new_browser->tab_strip_model()->AddWebContents(std::move(new_tab), -1,
                                                     ui::PAGE_TRANSITION_LINK,
                                                     AddTabTypes::ADD_ACTIVE);
      new_browser->window()->Show();
      return raw_new_tab;
    }
    default:
      browser->window()->GetLocationBar()->Revert();
      return current_tab;
  }
}

// Like the above, but auto-computes the current tab
WebContents* GetTabAndRevertIfNecessary(Browser* browser,
                                        WindowOpenDisposition disposition) {
  WebContents* activate_tab =
      browser->tab_strip_model()->GetActiveWebContents();
  return GetTabAndRevertIfNecessaryHelper(browser, disposition, activate_tab);
}

void ReloadInternal(Browser* browser,
                    WindowOpenDisposition disposition,
                    bool bypass_cache) {
  const WebContents* active_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  const auto& selected_indices =
      browser->tab_strip_model()->selection_model().selected_indices();
  for (int index : selected_indices) {
    WebContents* selected_tab =
        browser->tab_strip_model()->GetWebContentsAt(index);
    WebContents* new_tab =
        GetTabAndRevertIfNecessaryHelper(browser, disposition, selected_tab);

    // If the selected_tab is the activated page, give the focus to it, as this
    // is caused by a user action
    if (selected_tab == active_contents &&
        !new_tab->FocusLocationBarByDefault()) {
      new_tab->Focus();
    }

    DevToolsWindow* devtools =
        DevToolsWindow::GetInstanceForInspectedWebContents(new_tab);
    constexpr content::ReloadType kBypassingType =
        content::ReloadType::BYPASSING_CACHE;
    constexpr content::ReloadType kNormalType = content::ReloadType::NORMAL;
    if (!devtools || !devtools->ReloadInspectedWebContents(bypass_cache)) {
      new_tab->GetController().Reload(
          bypass_cache ? kBypassingType : kNormalType, true);
    }
  }
}

bool IsShowingWebContentsModalDialog(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  // TODO(gbillock): This is currently called in production by the CanPrint
  // method, and may be too restrictive if we allow print preview to overlap.
  // Re-assess how to queue print preview after we know more about popup
  // management policy.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  return manager && manager->IsDialogActive();
}

#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
bool PrintPreviewShowing(const Browser* browser) {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  auto* controller = printing::PrintPreviewDialogController::GetInstance();
  CHECK(controller);
  return controller->GetPrintPreviewForContents(contents) ||
         controller->is_creating_print_preview_dialog();
#else
  return false;
#endif
}
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)

}  // namespace

bool IsCommandEnabled(Browser* browser, int command) {
  return browser->command_controller()->IsCommandEnabled(command);
}

bool SupportsCommand(Browser* browser, int command) {
  return browser->command_controller()->SupportsCommand(command);
}

bool ExecuteCommand(Browser* browser, int command, base::TimeTicks time_stamp) {
  return browser->command_controller()->ExecuteCommand(command, time_stamp);
}

bool ExecuteCommandWithDisposition(Browser* browser,
                                   int command,
                                   WindowOpenDisposition disposition) {
  return browser->command_controller()->ExecuteCommandWithDisposition(
      command, disposition);
}

void UpdateCommandEnabled(Browser* browser, int command, bool enabled) {
  browser->command_controller()->UpdateCommandEnabled(command, enabled);
}

void AddCommandObserver(Browser* browser,
                        int command,
                        CommandObserver* observer) {
  browser->command_controller()->AddCommandObserver(command, observer);
}

void RemoveCommandObserver(Browser* browser,
                           int command,
                           CommandObserver* observer) {
  browser->command_controller()->RemoveCommandObserver(command, observer);
}

int GetContentRestrictions(const Browser* browser) {
  int content_restrictions = 0;
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (current_tab) {
    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(current_tab);
    content_restrictions = core_tab_helper->content_restrictions();
    NavigationEntry* last_committed_entry =
        current_tab->GetController().GetLastCommittedEntry();
    if (!content::IsSavableURL(last_committed_entry->GetURL()))
      content_restrictions |= CONTENT_RESTRICTION_SAVE;
  }
  return content_restrictions;
}

void NewEmptyWindow(Profile* profile, bool should_trigger_session_restore) {
  bool off_the_record = profile->IsOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  if (off_the_record) {
    if (IncognitoModePrefs::GetAvailability(prefs) ==
        policy::IncognitoModeAvailability::kDisabled) {
      off_the_record = false;
    }
  } else if (profile->IsGuestSession() ||
             IncognitoModePrefs::ShouldOpenSubsequentBrowsersInIncognito(
                 *base::CommandLine::ForCurrentProcess(), prefs)) {
    off_the_record = true;
  }

  if (off_the_record) {
    // This metric counts the Incognito and Off-The-Record Guest profiles
    // together.
    base::RecordAction(UserMetricsAction("NewIncognitoWindow"));
    if (profile->IsGuestSession())
      base::RecordAction(UserMetricsAction("NewGuestWindow"));
    else
      base::RecordAction(UserMetricsAction("NewIncognitoWindow2"));
    OpenEmptyWindow(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true),
                    should_trigger_session_restore);
  } else if (!should_trigger_session_restore) {
    base::RecordAction(UserMetricsAction("NewWindow"));
    OpenEmptyWindow(profile->GetOriginalProfile(),
                    /*should_trigger_session_restore=*/false);
  } else {
    base::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfileForSessionRestore(
            profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(StartupTabs(),
                                             /* restore_apps */ false)) {
      OpenEmptyWindow(profile->GetOriginalProfile());
    }
  }
}

Browser* OpenEmptyWindow(Profile* profile,
                         bool should_trigger_session_restore) {
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return nullptr;
  }
  Browser::CreateParams params =
      Browser::CreateParams(Browser::TYPE_NORMAL, profile, true);
  params.should_trigger_session_restore = should_trigger_session_restore;
  Browser* browser = Browser::Create(params);

  // Startup tabs could be created during browser creation. Add an empty tab
  // only if no tabs are created.
  if (browser->tab_strip_model()->empty()) {
    AddTabAt(browser, GURL(), -1, true);
  }

  browser->window()->Show();
  return browser;
}

void OpenWindowWithRestoredTabs(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (service)
    service->RestoreMostRecentEntry(nullptr);
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  ScopedTabbedBrowserDisplayer displayer(
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  AddSelectedTabWithURL(displayer.browser(), url, ui::PAGE_TRANSITION_LINK);
}

bool CanGoBack(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoBack();
}

bool CanGoBack(content::WebContents* web_contents) {
  return web_contents->GetController().CanGoBack();
}

enum class BackNavigationMenuIPHTrigger : int {
  kUserPerformsManyBackNavigation = 0,
  kUserPerformsChainedBackNavigation,
  kUserPerformsChainedBackNavigationWithBackButton
};

const char kBackNavigationMenuIPHExperimentParamName[] = "x_experiment";

void MaybeShowFeatureBackNavigationMenuPromo(Browser* browser,
                                             WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHBackNavigationMenuFeature)) {
    return;
  }

  bool should_show_feature_promo;
  const ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents);
  CHECK(tracker);
  switch (static_cast<BackNavigationMenuIPHTrigger>(
      base::GetFieldTrialParamByFeatureAsInt(
          feature_engagement::kIPHBackNavigationMenuFeature,
          kBackNavigationMenuIPHExperimentParamName, 0))) {
    case BackNavigationMenuIPHTrigger::kUserPerformsChainedBackNavigation:
      should_show_feature_promo =
          tracker->IsChainedBackNavigationRecentlyPerformed();
      break;

    case BackNavigationMenuIPHTrigger::
        kUserPerformsChainedBackNavigationWithBackButton:
      should_show_feature_promo =
          tracker->IsBackButtonChainedBackNavigationRecentlyPerformed();
      break;
    default:
      should_show_feature_promo = true;
      break;
  }

  if (should_show_feature_promo) {
    browser->window()->MaybeShowFeaturePromo(
        feature_engagement::kIPHBackNavigationMenuFeature);
  }
}

void GoBack(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Back"));

  if (CanGoBack(browser)) {
    WebContents* new_tab = GetTabAndRevertIfNecessary(browser, disposition);
    new_tab->GetController().GoBack();
    MaybeShowFeatureBackNavigationMenuPromo(browser, new_tab);
    browser->window()->NotifyFeatureEngagementEvent(
        kBackForwardNavigationIsTriggered);
  }
}

void GoBack(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Back"));

  if (CanGoBack(web_contents)) {
    web_contents->GetController().GoBack();
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (browser) {
      browser->window()->NotifyFeatureEngagementEvent(
          kBackForwardNavigationIsTriggered);
      MaybeShowFeatureBackNavigationMenuPromo(browser, web_contents);
    }
  }
}

bool CanGoForward(const Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetController()
      .CanGoForward();
}

bool CanGoForward(content::WebContents* web_contents) {
  return web_contents->GetController().CanGoForward();
}

void GoForward(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(browser)) {
    GetTabAndRevertIfNecessary(browser, disposition)
        ->GetController()
        .GoForward();
    browser->window()->NotifyFeatureEngagementEvent(
        kBackForwardNavigationIsTriggered);
  }
}

void GoForward(content::WebContents* web_contents) {
  base::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward(web_contents)) {
    web_contents->GetController().GoForward();
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    if (browser) {
      browser->window()->NotifyFeatureEngagementEvent(
          kBackForwardNavigationIsTriggered);
    }
  }
}

void NavigateToIndexWithDisposition(Browser* browser,
                                    int index,
                                    WindowOpenDisposition disposition) {
  NavigationController* controller =
      &GetTabAndRevertIfNecessary(browser, disposition)->GetController();
  DCHECK_GE(index, 0);
  DCHECK_LT(index, controller->GetEntryCount());
  controller->GoToIndex(index);
}

void Reload(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(browser, disposition, false);
}

void ReloadBypassingCache(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("ReloadBypassingCache"));
  ReloadInternal(browser, disposition, true);
}

bool CanReload(const Browser* browser) {
  return browser && !browser->is_type_devtools() &&
         !browser->is_type_picture_in_picture();
}

void Home(Browser* browser, WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("Home"));

  std::string extra_headers;
#if BUILDFLAG(ENABLE_RLZ)
  // If the home page is a Google home page, add the RLZ header to the request.
  PrefService* pref_service = browser->profile()->GetPrefs();
  if (pref_service) {
    if (google_util::IsGoogleHomePageUrl(
            GURL(pref_service->GetString(prefs::kHomePage)))) {
      extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
  }
#endif  // BUILDFLAG(ENABLE_RLZ)

  GURL url = browser->profile()->GetHomePage();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // With bookmark apps enabled, hosted apps should return to their launch page
  // when the home button is pressed.
  if (browser->is_type_app() || browser->is_type_app_popup()) {
    const extensions::Extension* extension = GetExtensionForBrowser(browser);
    if (!extension)
      return;
    url = extensions::AppLaunchInfo::GetLaunchWebURL(extension);
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
    extensions::MaybeShowExtensionControlledHomeNotification(browser);
#endif

  bool is_chrome_internal = url.SchemeIs(url::kAboutScheme) ||
                            url.SchemeIs(content::kChromeUIScheme) ||
                            url.SchemeIs(chrome::kChromeNativeScheme);
  base::UmaHistogramBoolean("Navigation.Home.IsChromeInternal",
                            is_chrome_internal);
  // Log a user action for the !is_chrome_internal case. This value is used as
  // part of a high-level guiding metric, which is being migrated to user
  // actions.
  if (!is_chrome_internal) {
    base::RecordAction(
        base::UserMetricsAction("Navigation.Home.NotChromeInternal"));
  }
  OpenURLParams params(
      url, Referrer(), disposition,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false);
  params.extra_headers = extra_headers;
  browser->OpenURL(params);
}

base::WeakPtr<content::NavigationHandle> OpenCurrentURL(Browser* browser) {
  base::RecordAction(UserMetricsAction("LoadURL"));
  // TODO(https://crbug.com/1294004): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment below.
  CHECK(browser);
  BrowserWindow* window = browser->window();
  CHECK(window);
  LocationBar* location_bar = window->GetLocationBar();
  if (!location_bar) {
    return nullptr;
  }

  // Requirement 4 - Redirect some urls and show a quote.
  // This handles the addresses entered directly to the location bar.
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

  GURL url(location_bar->GetDestinationURL());

  // Check if the URL is in the dictionary
  // std::string websiteUrl = url.scheme() + "://" + url.host() + "/";
  VLOG(0) << "host=" << url.host();
  std::string host = url.host();
  if (host.substr(0, 4) == "www.") {
      host = host.substr(4);
  }
  size_t pos = host.find_first_of(":/");
  if (pos != std::string::npos) {
      host = host.substr(0, pos);
  }
  auto category_iter = website_categories.find(host);
  if (category_iter != website_categories.end()) {
      std::string category = category_iter->second;
      std::vector<std::string> quotes = category_quotes[category];
      std::string quote = quotes[rand() % quotes.size()];
      browser->setQuote(quote);
      if (category == "news") {
        url = GURL("monk://quote?" + quote + "&" + url.spec());
      } else {
        url = GURL("monk://quote?" + quote);
      }
   // setAlwaysBlock();
  }
  TRACE_EVENT1("navigation", "chrome::OpenCurrentURL", "url", url);

  if (ShouldInterceptChromeURLNavigationInIncognito(browser, url)) {
    ProcessInterceptedChromeURLNavigationInIncognito(browser, url);
    return nullptr;
  }

  NavigateParams params(browser, url, location_bar->GetPageTransition());
  params.disposition = location_bar->GetWindowOpenDisposition();
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      AddTabTypes::ADD_FORCE_INDEX | AddTabTypes::ADD_INHERIT_OPENER;
  params.input_start = location_bar->GetMatchSelectionTimestamp();
  params.is_using_https_as_default_scheme =
      location_bar->IsInputTypedUrlWithoutScheme();
  params.url_typed_with_http_scheme =
      location_bar->IsInputTypedUrlWithHttpScheme();
  auto result = Navigate(&params);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DCHECK(extensions::ExtensionSystem::Get(browser->profile())
             ->extension_service());
  // TODO(https://crbug.com/1294004): Eliminate extra checks once source of
  //  bad pointer dereference is identified. See also TODO comment above.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser->profile());
  CHECK(extension_registry);
  const extensions::Extension* extension =
      extension_registry->enabled_extensions().GetAppByURL(url);
  if (extension) {
    extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_OMNIBOX_LOCATION,
                                    extension->GetType());
  }
#endif
  return result;
}

void Stop(Browser* browser) {
  base::RecordAction(UserMetricsAction("Stop"));
  browser->tab_strip_model()->GetActiveWebContents()->Stop();
}

void NewWindow(Browser* browser) {
  Profile* const profile = browser->profile();
#if BUILDFLAG(IS_MAC)
  // Web apps should open a window to their launch page.
  if (browser->app_controller()) {
    const web_app::AppId app_id = browser->app_controller()->app_id();

    auto launch_container = apps::LaunchContainer::kLaunchContainerWindow;

    auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
    if (provider && provider->registrar_unsafe().GetAppEffectiveDisplayMode(
                        app_id) == blink::mojom::DisplayMode::kBrowser) {
      launch_container = apps::LaunchContainer::kLaunchContainerTab;
    }
    apps::AppLaunchParams params = apps::AppLaunchParams(
        app_id, launch_container, WindowOpenDisposition::NEW_WINDOW,
        apps::LaunchSource::kFromKeyboard);
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params), base::DoNothing());
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Hosted apps should open a window to their launch page.
  const extensions::Extension* extension = GetExtensionForBrowser(browser);
  if (extension && extension->is_hosted_app()) {
    const auto app_launch_params = CreateAppLaunchParamsUserContainer(
        profile, extension, WindowOpenDisposition::NEW_WINDOW,
        apps::LaunchSource::kFromKeyboard);
    OpenApplicationWindow(
        profile, app_launch_params,
        extensions::AppLaunchInfo::GetLaunchWebURL(extension));
    return;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#endif  // BUILDFLAG(IS_MAC)
  NewEmptyWindow(profile->GetOriginalProfile());
}

void NewIncognitoWindow(Profile* profile) {
  NewEmptyWindow(profile->GetPrimaryOTRProfile(/*create_if_needed=*/true));
}

void CloseWindow(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseWindow"));
  browser->window()->Close();
}

void NewTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("NewTab"));
  // TODO(asvitkine): This is invoked programmatically from several places.
  // Audit the code and change it so that the histogram only gets collected for
  // user-initiated commands.
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab", NewTabTypes::NEW_TAB_COMMAND,
                            NewTabTypes::NEW_TAB_ENUM_COUNT);

  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)) {
    AddTabAt(browser, GURL(), -1, true);
  } else {
    ScopedTabbedBrowserDisplayer displayer(browser->profile());
    Browser* b = displayer.browser();
    AddTabAt(b, GURL(), -1, true);
    b->window()->Show();
    // The call to AddBlankTabAt above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    b->tab_strip_model()->GetActiveWebContents()->RestoreFocus();
  }
}

void NewTabToRight(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandNewTabToRight);
}

void CloseTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  browser->tab_strip_model()->CloseSelectedTabs();
}

bool CanZoomIn(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMaximumZoomPercent();
}

bool CanZoomOut(content::WebContents* contents) {
  return contents && !contents->IsCrashed() &&
         zoom::ZoomController::FromWebContents(contents)->GetZoomPercent() !=
             contents->GetMinimumZoomPercent();
}

bool CanResetZoom(content::WebContents* contents) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(contents);
  return !zoom_controller->IsAtDefaultZoom() ||
         !zoom_controller->PageScaleFactorIsOne();
}

void SelectNextTab(Browser* browser,
                   TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectNextTab"));
  browser->tab_strip_model()->SelectNextTab(gesture_detail);
}

void SelectPreviousTab(Browser* browser,
                       TabStripUserGestureDetails gesture_detail) {
  base::RecordAction(UserMetricsAction("SelectPrevTab"));
  browser->tab_strip_model()->SelectPreviousTab(gesture_detail);
}

void MoveTabNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabNext"));
  browser->tab_strip_model()->MoveTabNext();
}

void MoveTabPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("MoveTabPrevious"));
  browser->tab_strip_model()->MoveTabPrevious();
}

void SelectNumberedTab(Browser* browser,
                       int index,
                       TabStripUserGestureDetails gesture_detail) {
  int visible_count = 0;
  for (int i = 0; i < browser->tab_strip_model()->count(); i++) {
    if (browser->tab_strip_model()->IsTabCollapsed(i)) {
      continue;
    }
    if (visible_count == index) {
      base::RecordAction(UserMetricsAction("SelectNumberedTab"));
      browser->tab_strip_model()->ActivateTabAt(i, gesture_detail);
      break;
    }
    visible_count += 1;
  }
}

void SelectLastTab(Browser* browser,
                   TabStripUserGestureDetails gesture_detail) {
  for (int i = browser->tab_strip_model()->count() - 1; i >= 0; i--) {
    if (!browser->tab_strip_model()->IsTabCollapsed(i)) {
      base::RecordAction(UserMetricsAction("SelectLastTab"));
      browser->tab_strip_model()->ActivateTabAt(i, gesture_detail);
      break;
    }
  }
}

void DuplicateTab(Browser* browser) {
  base::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateTab(const Browser* browser) {
  return CanDuplicateTabAt(browser, browser->tab_strip_model()->active_index());
}

bool CanDuplicateKeyboardFocusedTab(const Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return false;
  return CanDuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
}

bool CanMoveActiveTabToNewWindow(Browser* browser) {
  const ui::ListSelectionModel::SelectedIndices& selection =
      browser->tab_strip_model()->selection_model().selected_indices();
  return CanMoveTabsToNewWindow(
      browser, std::vector<int>(selection.begin(), selection.end()));
}

void MoveActiveTabToNewWindow(Browser* browser) {
  const ui::ListSelectionModel::SelectedIndices& selection =
      browser->tab_strip_model()->selection_model().selected_indices();
  MoveTabsToNewWindow(browser,
                      std::vector<int>(selection.begin(), selection.end()));
}
bool CanMoveTabsToNewWindow(Browser* browser,
                            const std::vector<int>& tab_indices) {
  return browser->tab_strip_model()->count() >
         static_cast<int>(tab_indices.size());
}

void MoveTabsToNewWindow(Browser* browser,
                         const std::vector<int>& tab_indices,
                         absl::optional<tab_groups::TabGroupId> group) {
  if (tab_indices.empty())
    return;

  Browser* new_browser =
      Browser::Create(Browser::CreateParams(browser->profile(), true));

  if (group.has_value()) {
    SavedTabGroupKeyedService* const service =
        SavedTabGroupServiceFactory::GetForProfile(browser->profile());
    if (service && service->model()->Contains(group.value())) {
      // If the group we are looking to move is saved:
      // 1) Stop listening to changes on it
      // 2) Close the group in the browser
      // 3) Open the group in a new browser and link it to the saved guid.
      const base::Uuid& saved_guid =
          service->model()->Get(group.value())->saved_guid();

      service->DisconnectLocalTabGroup(group.value());
      browser->tab_strip_model()->CloseAllTabsInGroup(group.value());
      service->OpenSavedTabGroupInBrowser(new_browser, saved_guid);
      return;
    }

    const tab_groups::TabGroupVisualData* old_visual_data =
        browser->tab_strip_model()
            ->group_model()
            ->GetTabGroup(group.value())
            ->visual_data();
    tab_groups::TabGroupVisualData new_visual_data(old_visual_data->title(),
                                                   old_visual_data->color(),
                                                   false /* is_collapsed */);

    new_browser->tab_strip_model()->group_model()->AddTabGroup(group.value(),
                                                               new_visual_data);
  }

  int indices_size = tab_indices.size();
  int active_index = browser->tab_strip_model()->active_index();
  for (int i = 0; i < indices_size; i++) {
    // Adjust tab index to account for tabs already moved.
    int adjusted_index = tab_indices[i] - i;
    bool pinned = browser->tab_strip_model()->IsTabPinned(adjusted_index);
    std::unique_ptr<WebContents> contents_move =
        browser->tab_strip_model()->DetachWebContentsAtForInsertion(
            adjusted_index);

    int add_types = pinned ? AddTabTypes::ADD_PINNED : 0;
    // The last tab made active takes precedence, so activate the last active
    // tab, with a fallback for the first tab (i == 0) if the active tab isn’t
    // in the set of tabs being moved.
    if (i == 0 || tab_indices[i] == active_index)
      add_types = add_types | AddTabTypes::ADD_ACTIVE;

    new_browser->tab_strip_model()->AddWebContents(std::move(contents_move), -1,
                                                   ui::PAGE_TRANSITION_TYPED,
                                                   add_types, group);
  }
  new_browser->window()->Show();
}

bool CanCloseTabsToRight(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

bool CanCloseOtherTabs(const Browser* browser) {
  return browser->tab_strip_model()->IsContextMenuCommandEnabled(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

WebContents* DuplicateTabAt(Browser* browser, int index) {
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  CHECK(contents);
  std::unique_ptr<WebContents> contents_dupe = contents->Clone();
  WebContents* raw_contents_dupe = contents_dupe.get();

  bool pinned = false;
  if (browser->CanSupportWindowFeature(Browser::FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    const int contents_index = tab_strip_model->GetIndexOfWebContents(contents);
    pinned = tab_strip_model->IsTabPinned(contents_index);
    int add_types = AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER |
                    (pinned ? AddTabTypes::ADD_PINNED : 0);
    const auto old_group = tab_strip_model->GetTabGroupForTab(contents_index);
    tab_strip_model->InsertWebContentsAt(
        contents_index + 1, std::move(contents_dupe), add_types, old_group);
  } else {
    CreateAndShowNewWindowWithContents(std::move(contents_dupe), browser);
  }

  SessionServiceBase* session_service =
      GetAppropriateSessionServiceIfExisting(browser);
  if (session_service)
    session_service->TabRestored(raw_contents_dupe, pinned);
  return raw_contents_dupe;
}

bool CanDuplicateTabAt(const Browser* browser, int index) {
  if (browser->is_type_picture_in_picture()) {
    return false;
  }
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(index);
  return contents;
}

void MoveTabsToExistingWindow(Browser* source,
                              Browser* target,
                              const std::vector<int>& tab_indices) {
  if (tab_indices.empty())
    return;

  int indices_size = tab_indices.size();
  for (int i = 0; i < indices_size; i++) {
    // Adjust tab index to account for tabs already moved.
    int adjusted_index = tab_indices[i] - i;
    bool pinned = source->tab_strip_model()->IsTabPinned(adjusted_index);
    std::unique_ptr<WebContents> contents_move =
        source->tab_strip_model()->DetachWebContentsAtForInsertion(
            adjusted_index);
    int add_types =
        AddTabTypes::ADD_ACTIVE | (pinned ? AddTabTypes::ADD_PINNED : 0);
    target->tab_strip_model()->AddWebContents(
        std::move(contents_move), -1, ui::PAGE_TRANSITION_TYPED, add_types);
  }
  target->window()->Show();
}

void PinTab(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void GroupTab(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void MuteSite(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void MuteSiteForKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return;
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandToggleSiteMuted);
}

void PinKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return;
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandTogglePinned);
}

void GroupKeyboardFocusedTab(Browser* browser) {
  if (!HasKeyboardFocusedTab(browser))
    return;
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      *GetKeyboardFocusedTabIndex(browser),
      TabStripModel::ContextMenuCommand::CommandToggleGrouped);
}

void DuplicateKeyboardFocusedTab(Browser* browser) {
  if (HasKeyboardFocusedTab(browser)) {
    DuplicateTabAt(browser, *GetKeyboardFocusedTabIndex(browser));
  }
}

bool HasKeyboardFocusedTab(const Browser* browser) {
  return GetKeyboardFocusedTabIndex(browser).has_value();
}

void ConvertPopupToTabbedBrowser(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowAsTab"));
  TabStripModel* tab_strip = browser->tab_strip_model();
  std::unique_ptr<content::WebContents> contents =
      tab_strip->DetachWebContentsAtForInsertion(tab_strip->active_index());
  Browser* b = Browser::Create(Browser::CreateParams(browser->profile(), true));
  b->tab_strip_model()->AppendWebContents(std::move(contents), true);
  b->window()->Show();
}

void CloseTabsToRight(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseTabsToRight);
}

void CloseOtherTabs(Browser* browser) {
  browser->tab_strip_model()->ExecuteContextMenuCommand(
      browser->tab_strip_model()->active_index(),
      TabStripModel::CommandCloseOtherTabs);
}

void Exit() {
  base::RecordAction(UserMetricsAction("Exit"));
  chrome::AttemptUserExit();
}

void BookmarkCurrentTab(Browser* browser) {
  base::RecordAction(base::UserMetricsAction("Star"));
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser, model, &url, &title)) {
    return;
  }
  bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
#if !BUILDFLAG(IS_ANDROID)
  PrefService* prefs = browser->profile()->GetPrefs();
  if (base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel) &&
      !prefs->GetBoolean(
          bookmarks::prefs::kAddedBookmarkSincePowerBookmarksLaunch)) {
    bookmarks::AddIfNotBookmarked(model, url, title, model->other_node());
    prefs->SetBoolean(bookmarks::prefs::kAddedBookmarkSincePowerBookmarksLaunch,
                      true);
  }
#endif
  bookmarks::AddIfNotBookmarked(model, url, title);
  bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (browser->window()->IsActive() && is_bookmarked_by_user) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations where the bubble is deleted as soon as it is shown.
    browser->window()->ShowBookmarkBubble(url, was_bookmarked_by_user);
  }

  if (!was_bookmarked_by_user && is_bookmarked_by_user)
    RecordBookmarksAdded(browser->profile());
}

void BookmarkCurrentTabInFolder(Browser* browser, int64_t folder_id) {
  BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  GURL url;
  std::u16string title;
  if (!BookmarkCurrentTabHelper(browser, model, &url, &title)) {
    return;
  }
  const bookmarks::BookmarkNode* parent =
      bookmarks::GetBookmarkNodeByID(model, folder_id);
  if (parent) {
    bool was_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    model->AddNewURL(parent, 0, title, url);
    bool is_bookmarked_by_user = bookmarks::IsBookmarkedByUser(model, url);
    if (!was_bookmarked_by_user && is_bookmarked_by_user)
      RecordBookmarksAdded(browser->profile());
  }
}

bool CanBookmarkCurrentTab(const Browser* browser) {
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser->profile());
  return browser_defaults::bookmarks_enabled &&
         browser->profile()->GetPrefs()->GetBoolean(
             bookmarks::prefs::kEditBookmarksEnabled) &&
         model && model->loaded() && browser->is_type_normal();
}

void BookmarkAllTabs(Browser* browser) {
  base::RecordAction(UserMetricsAction("BookmarkAllTabs"));
  RecordBookmarkAllTabsWithTabsCount(browser->profile(),
                                     browser->tab_strip_model()->count());

  chrome::ShowBookmarkAllTabsDialog(browser);
}

bool CanBookmarkAllTabs(const Browser* browser) {
  return browser->tab_strip_model()->count() > 1 &&
         CanBookmarkCurrentTab(browser);
}

bool CanMoveActiveTabToReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ReadingListModel* model = GetReadingListModel(browser);
  return CanMoveWebContentsToReadLater(browser, web_contents, model, &url,
                                       &title);
}

bool MoveCurrentTabToReadLater(Browser* browser) {
  return MoveTabToReadLater(browser,
                            browser->tab_strip_model()->GetActiveWebContents());
}

bool MoveTabToReadLater(Browser* browser, content::WebContents* web_contents) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  if (!CanMoveWebContentsToReadLater(browser, web_contents, model, &url,
                                     &title)) {
    return false;
  }
  model->AddOrReplaceEntry(url, base::UTF16ToUTF8(title),
                           reading_list::EntrySource::ADDED_VIA_CURRENT_APP,
                           /*estimated_read_time=*/base::TimeDelta());
  browser->window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHReadingListDiscoveryFeature);
  base::UmaHistogramEnumeration(
      "ReadingList.BookmarkBarState.OnEveryAddToReadingList",
      browser->bookmark_bar_state());
  return true;
}

bool MarkCurrentTabAsReadInReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!model || !GetTabURLAndTitleToSave(web_contents, &url, &title))
    return false;
  scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
  // Mark current tab as read.
  if (entry && !entry->IsRead())
    model->SetReadStatusIfExists(url, true);
  return entry != nullptr;
}

bool IsCurrentTabUnreadInReadLater(Browser* browser) {
  GURL url;
  std::u16string title;
  ReadingListModel* model = GetReadingListModel(browser);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!model || !GetTabURLAndTitleToSave(web_contents, &url, &title))
    return false;
  scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
  return entry && !entry->IsRead();
}

void ShowOffersAndRewardsForPage(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::OfferNotificationBubbleControllerImpl* controller =
      autofill::OfferNotificationBubbleControllerImpl::FromWebContents(
          web_contents);
  DCHECK(controller);
  controller->ReshowBubble();
}

void SaveCreditCard(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveCardBubbleControllerImpl* controller =
      autofill::SaveCardBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble();
}

void SaveIBAN(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::IbanBubbleControllerImpl* controller =
      autofill::IbanBubbleControllerImpl::FromWebContents(web_contents);
  controller->ReshowBubble();
}

void ShowMandatoryReauthOptInPrompt(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::MandatoryReauthBubbleControllerImpl* controller =
      autofill::MandatoryReauthBubbleControllerImpl::FromWebContents(
          web_contents);
  controller->ReshowBubble();
}

void MigrateLocalCards(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::ManageMigrationUiController* controller =
      autofill::ManageMigrationUiController::FromWebContents(web_contents);
  // Show migration-related Ui when the user clicks the credit card icon.
  controller->OnUserClickedCreditCardIcon();
}

void SaveAutofillAddress(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::SaveUpdateAddressProfileBubbleControllerImpl* controller =
      autofill::SaveUpdateAddressProfileBubbleControllerImpl::FromWebContents(
          web_contents);
  controller->OnPageActionIconClicked();
}

void ShowVirtualCardManualFallbackBubble(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  auto* controller =
      autofill::VirtualCardManualFallbackBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller)
    controller->ReshowBubble();
}

void ShowVirtualCardEnrollBubble(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  autofill::VirtualCardEnrollBubbleControllerImpl* controller =
      autofill::VirtualCardEnrollBubbleControllerImpl::FromWebContents(
          web_contents);
  if (controller)
    controller->ReshowBubble();
}

void Translate(Browser* browser) {
  if (!browser->window()->IsActive())
    return;

  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents);

  std::string source_language;
  std::string target_language;
  chrome_translate_client->GetTranslateLanguages(web_contents, &source_language,
                                                 &target_language);

  translate::TranslateStep step = translate::TRANSLATE_STEP_BEFORE_TRANSLATE;
  if (chrome_translate_client) {
    if (chrome_translate_client->GetLanguageState().translation_pending())
      step = translate::TRANSLATE_STEP_TRANSLATING;
    else if (chrome_translate_client->GetLanguageState().translation_error())
      step = translate::TRANSLATE_STEP_TRANSLATE_ERROR;
    else if (chrome_translate_client->GetLanguageState().IsPageTranslated())
      step = translate::TRANSLATE_STEP_AFTER_TRANSLATE;
  }
  browser->window()->ShowTranslateBubble(
      web_contents, step, source_language, target_language,
      translate::TranslateErrors::NONE, true);
}

void ManagePasswordsForPage(Browser* browser) {
  browser->window()->CloseFeaturePromo(
      feature_engagement::kIPHPasswordsManagementBubbleAfterSaveFeature);
  browser->window()->CloseFeaturePromo(
      feature_engagement::kIPHPasswordsManagementBubbleDuringSigninFeature);
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  TabDialogs::FromWebContents(web_contents)
      ->ShowManagePasswordsBubble(!controller->IsAutomaticallyOpeningBubble());
}

void SendTabToSelfFromPageAction(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  send_tab_to_self::ShowBubble(web_contents);
}

void GenerateQRCodeFromPageAction(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  qrcode_generator::QRCodeGeneratorBubbleController* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  controller->ShowBubble(entry->GetURL());
}

void SharingHubFromPageAction(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  sharing_hub::SharingHubBubbleController* controller =
      sharing_hub::SharingHubBubbleController::CreateOrGetFromWebContents(
          web_contents);
  controller->ShowBubble(share::ShareAttempt(web_contents));
}

void ScreenshotCaptureFromPageAction(Browser* browser) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  sharing_hub::ScreenshotCapturedBubbleController* controller =
      sharing_hub::ScreenshotCapturedBubbleController::Get(web_contents);
  controller->Capture(browser);
}

void SavePage(Browser* browser) {
  base::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(current_tab);
  if (current_tab->GetContentsMimeType() == "application/pdf")
    base::RecordAction(UserMetricsAction("PDF.SavePage"));
  current_tab->OnSavePage();
}

bool CanSavePage(const Browser* browser) {
  // LocalState can be NULL in tests.
  if (g_browser_process->local_state() &&
      !g_browser_process->local_state()->GetBoolean(
          prefs::kAllowFileSelectionDialogs)) {
    return false;
  }
  if (static_cast<DownloadPrefs::DownloadRestriction>(
          browser->profile()->GetPrefs()->GetInteger(
              prefs::kDownloadRestrictions)) ==
      DownloadPrefs::DownloadRestriction::ALL_FILES) {
    return false;
  }
  return !browser->is_type_devtools() &&
         !(GetContentRestrictions(browser) & CONTENT_RESTRICTION_SAVE);
}

void Print(Browser* browser) {
#if BUILDFLAG(ENABLE_PRINTING)
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  printing::StartPrint(
      web_contents,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      /*print_renderer=*/mojo::NullAssociatedRemote(),
#endif
      browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled),
      /*has_selection=*/false);
#endif
}

bool CanPrint(Browser* browser) {
#if BUILDFLAG(ENABLE_PRINTING)
  // Do not print when printing is disabled via pref or policy.
  // Do not print when a page has crashed.
  // Do not print when a constrained window is showing. It's confusing.
  // TODO(gbillock): Need to re-assess the call to
  // IsShowingWebContentsModalDialog after a popup management policy is
  // refined -- we will probably want to just queue the print request, not
  // block it.
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (current_tab && !current_tab->IsCrashed()) &&
         !(IsShowingWebContentsModalDialog(browser) ||
           GetContentRestrictions(browser) & CONTENT_RESTRICTION_PRINT);
#else   // BUILDFLAG(ENABLE_PRINTING)
  return false;
#endif  // BUILDFLAG(ENABLE_PRINTING)
}

#if BUILDFLAG(ENABLE_PRINTING)
void BasicPrint(Browser* browser) {
  printing::StartBasicPrint(browser->tab_strip_model()->GetActiveWebContents());
}

bool CanBasicPrint(Browser* browser) {
#if BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
  // If printing is not disabled via pref or policy, it is always possible to
  // advanced print when the print preview is visible.
  return browser->profile()->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
         (PrintPreviewShowing(browser) || CanPrint(browser));
#else
  return false;  // The print dialog is disabled.
#endif  // BUILDFLAG(ENABLE_BASIC_PRINT_DIALOG)
}
#endif  // BUILDFLAG(ENABLE_PRINTING)

bool CanRouteMedia(Browser* browser) {
  // Do not allow user to open Media Router dialog when there is already an
  // active modal dialog. This avoids overlapping dialogs.
  return media_router::MediaRouterEnabled(browser->profile()) &&
         !IsShowingWebContentsModalDialog(browser);
}

void RouteMediaInvokedFromAppMenu(Browser* browser) {
  DCHECK(CanRouteMedia(browser));

  media_router::MediaRouterDialogController* dialog_controller =
      media_router::MediaRouterDialogController::GetOrCreateForWebContents(
          browser->tab_strip_model()->GetActiveWebContents());
  if (!dialog_controller)
    return;

  dialog_controller->ShowMediaRouterDialog(
      media_router::MediaRouterDialogActivationLocation::APP_MENU);
}

void CutCopyPaste(Browser* browser, int command_id) {
  if (command_id == IDC_CUT)
    base::RecordAction(UserMetricsAction("Cut"));
  else if (command_id == IDC_COPY)
    base::RecordAction(UserMetricsAction("Copy"));
  else
    base::RecordAction(UserMetricsAction("Paste"));
  browser->window()->CutCopyPaste(command_id);
}

void Find(Browser* browser) {
  base::RecordAction(UserMetricsAction("Find"));
  FindInPage(browser, false, true);
}

void FindNext(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(browser, true, true);
}

void FindPrevious(Browser* browser) {
  base::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(browser, true, false);
}

void FindInPage(Browser* browser, bool find_next, bool forward_direction) {
  browser->GetFindBarController()->Show(find_next, forward_direction);
}

void ShowTabSearch(Browser* browser) {
  browser->window()->CreateTabSearchBubble();
}

void CloseTabSearch(Browser* browser) {
  browser->window()->CloseTabSearchBubble();
}

bool CanCloseFind(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;

  find_in_page::FindTabHelper* find_helper =
      find_in_page::FindTabHelper::FromWebContents(current_tab);
  return find_helper ? find_helper->find_ui_active() : false;
}

void CloseFind(Browser* browser) {
  browser->GetFindBarController()->EndFindSession(
      find_in_page::SelectionAction::kKeep, find_in_page::ResultAction::kKeep);
}

void Zoom(Browser* browser, content::PageZoom zoom) {
  zoom::PageZoom::Zoom(browser->tab_strip_model()->GetActiveWebContents(),
                       zoom);
}

void FocusToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusToolbar"));
  browser->window()->FocusToolbar();
}

void FocusLocationBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusLocation"));
  browser->window()->SetFocusToLocationBar(true);
}

void FocusSearch(Browser* browser) {
  // TODO(beng): replace this with FocusLocationBar
  base::RecordAction(UserMetricsAction("FocusSearch"));
  browser->window()->GetLocationBar()->FocusSearch();
}

void FocusAppMenu(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusAppMenu"));
  browser->window()->FocusAppMenu();
}

void FocusBookmarksToolbar(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  browser->window()->FocusBookmarksToolbar();
}

void FocusInactivePopupForAccessibility(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusInactivePopupForAccessibility"));
  browser->window()->FocusInactivePopupForAccessibility();
}

void FocusNextPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusNextPane"));
  browser->window()->RotatePaneFocus(true);
}

void FocusPreviousPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusPreviousPane"));
  browser->window()->RotatePaneFocus(false);
}

void FocusWebContentsPane(Browser* browser) {
  base::RecordAction(UserMetricsAction("FocusWebContentsPane"));
  browser->window()->FocusWebContentsPane();
}

void ToggleDevToolsWindow(Browser* browser,
                          DevToolsToggleAction action,
                          DevToolsOpenedByAction opened_by) {
  if (action.type() == DevToolsToggleAction::kShowConsolePanel)
    base::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  else
    base::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
  DevToolsWindow::ToggleDevToolsWindow(browser, action, opened_by);
}

bool CanOpenTaskManager() {
#if !BUILDFLAG(IS_ANDROID)
  return true;
#else
  return false;
#endif
}

void OpenTaskManager(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Open linux version of task manager UI if ash TaskManager
  // interface is in an old version.
  if (chromeos::LacrosService::Get()
          ->GetInterfaceVersion<crosapi::mojom::TaskManager>() < 1) {
    base::RecordAction(UserMetricsAction("TaskManager"));
    chrome::ShowTaskManager(browser);
    return;
  }
  // Invoke task manager UI in ash, which will call chrome::OpenTaskManager()
  // in ash to run through the code path in the next section
  // (!BUILDFLAG(IS_ANDROID)).
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TaskManager>()
      ->ShowTaskManager();
#elif !BUILDFLAG(IS_ANDROID)
  base::RecordAction(UserMetricsAction("TaskManager"));
  chrome::ShowTaskManager(browser);
#else
  NOTREACHED();
#endif
}

void OpenFeedbackDialog(Browser* browser,
                        FeedbackSource source,
                        const std::string& description_template) {
  base::RecordAction(UserMetricsAction("Feedback"));
  chrome::ShowFeedbackPage(browser, source, description_template,
                           std::string() /* description_placeholder_text */,
                           std::string() /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

void ToggleBookmarkBar(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  ToggleBookmarkBarWhenVisible(browser->profile());
}

void ToggleShowFullURLs(Browser* browser) {
  bool pref_enabled = browser->profile()->GetPrefs()->GetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox);
  browser->profile()->GetPrefs()->SetBoolean(
      omnibox::kPreventUrlElisionsInOmnibox, !pref_enabled);
}

void ShowAppMenu(Browser* browser) {
  // We record the user metric for this event in AppMenu::RunMenu.
  browser->window()->ShowAppMenu();
}

void ShowAvatarMenu(Browser* browser) {
  browser->window()->ShowAvatarBubbleFromAvatarButton(
      /*is_source_accelerator=*/true);
}

void OpenUpdateChromeDialog(Browser* browser) {
  if (UpgradeDetector::GetInstance()->is_outdated_install()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstall();
  } else if (UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    UpgradeDetector::GetInstance()->NotifyOutdatedInstallNoAutoUpdate();
  } else {
    base::RecordAction(UserMetricsAction("UpdateChrome"));
    browser->window()->ShowUpdateChromeDialog();
  }
}

void ToggleDistilledView(Browser* browser) {
  auto* current_web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (dom_distiller::url_utils::IsDistilledPage(
          current_web_contents->GetLastCommittedURL())) {
    ReturnToOriginalPage(current_web_contents);
  } else {
    DistillCurrentPageAndView(current_web_contents);
  }
}

bool CanRequestTabletSite(WebContents* current_tab) {
  return current_tab &&
         current_tab->GetController().GetLastCommittedEntry() != nullptr;
}

bool IsRequestingTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;
  content::NavigationEntry* entry =
      current_tab->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;
  return entry->GetIsOverridingUserAgent();
}

void ToggleRequestTabletSite(Browser* browser) {
  WebContents* current_tab = browser->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return;
  NavigationController& controller = current_tab->GetController();
  NavigationEntry* entry = controller.GetLastCommittedEntry();
  if (!entry)
    return;
  if (entry->GetIsOverridingUserAgent())
    entry->SetIsOverridingUserAgent(false);
  else
    SetAndroidOsForTabletSite(current_tab);
  controller.Reload(content::ReloadType::ORIGINAL_REQUEST_URL, true);
}

void SetAndroidOsForTabletSite(content::WebContents* current_tab) {
  DCHECK(current_tab);
  NavigationEntry* entry = current_tab->GetController().GetLastCommittedEntry();
  if (entry) {
    entry->SetIsOverridingUserAgent(true);
    std::string product = embedder_support::GetProductAndVersion() + " Mobile";
    blink::UserAgentOverride ua_override;
    ua_override.ua_string_override = content::BuildUserAgentFromOSAndProduct(
        kOsOverrideForTabletSite, product);
    ua_override.ua_metadata_override = embedder_support::GetUserAgentMetadata(
        g_browser_process->local_state());
    ua_override.ua_metadata_override->mobile = true;
    ua_override.ua_metadata_override->platform =
        kChPlatformOverrideForTabletSite;
    ua_override.ua_metadata_override->platform_version = std::string();
    current_tab->SetUserAgentOverride(ua_override, false);
  }
}

void ToggleFullscreenMode(Browser* browser) {
  DCHECK(browser);
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ToggleBrowserFullscreenMode();
}

void ClearCache(Browser* browser) {
  content::BrowsingDataRemover* remover =
      browser->profile()->GetBrowsingDataRemover();
  remover->Remove(base::Time(), base::Time::Max(),
                  content::BrowsingDataRemover::DATA_TYPE_CACHE,
                  content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB);
  // BrowsingDataRemover takes care of deleting itself when done.
}

bool IsDebuggerAttachedToCurrentTab(Browser* browser) {
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  return contents ? content::DevToolsAgentHost::IsDebuggerAttached(contents)
                  : false;
}

void CopyURL(content::WebContents* web_contents) {
  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteText(base::UTF8ToUTF16(web_contents->GetVisibleURL().spec()));
}

Browser* OpenInChrome(Browser* hosted_app_browser) {
  // Find a non-incognito browser.
  Browser* target_browser =
      chrome::FindTabbedBrowser(hosted_app_browser->profile(), false);

  if (!target_browser) {
    target_browser = Browser::Create(
        Browser::CreateParams(hosted_app_browser->profile(), true));
  }

  TabStripModel* source_tabstrip = hosted_app_browser->tab_strip_model();

  // Clear bounds once a PWA with window controls overlay display override opens
  // in browser.
  if (hosted_app_browser->app_controller()->IsWindowControlsOverlayEnabled()) {
    source_tabstrip->GetActiveWebContents()->UpdateWindowControlsOverlay(
        gfx::Rect());
  }

  target_browser->tab_strip_model()->AppendWebContents(
      source_tabstrip->DetachWebContentsAtForInsertion(
          source_tabstrip->active_index()),
      true);
  auto* web_contents =
      target_browser->tab_strip_model()->GetActiveWebContents();
  apps::MaybeShowIntentPicker(web_contents);
#if BUILDFLAG(IS_CHROMEOS)
  apps::SupportedLinksInfoBarDelegate::RemoveSupportedLinksInfoBar(
      web_contents);
#endif
  target_browser->window()->Show();
  return target_browser;
}

bool CanViewSource(const Browser* browser) {
  if (browser->is_type_devtools()) {
    return false;
  }

  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // Disallow ViewSource if DevTools are disabled.
  if (!DevToolsWindow::AllowDevToolsFor(browser->profile(), web_contents)) {
    return false;
  }
  return web_contents->GetController().CanViewSource();
}

bool CanToggleCaretBrowsing(Browser* browser) {
#if BUILDFLAG(IS_MAC)
  // On Mac, ignore the keyboard shortcut unless web contents is focused,
  // because the keyboard shortcut interferes with a Japenese IME when the
  // omnibox is focused.  See https://crbug.com/1138475
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  content::RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  return rwhv && rwhv->HasFocus();
#else
  return true;
#endif  // BUILDFLAG(IS_MAC)
}

void ToggleCaretBrowsing(Browser* browser) {
  if (!CanToggleCaretBrowsing(browser))
    return;

  PrefService* prefService = browser->profile()->GetPrefs();
  bool enabled = prefService->GetBoolean(prefs::kCaretBrowsingEnabled);

  if (enabled) {
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CaretBrowsing.DisableWithKeyboard"));
    prefService->SetBoolean(prefs::kCaretBrowsingEnabled, false);
    return;
  }

  // Show a confirmation dialog, unless either (1) the command-line
  // flag was used, or (2) the user previously checked the box
  // indicating not to ask them next time.
  if (prefService->GetBoolean(prefs::kShowCaretBrowsingDialog) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCaretBrowsing)) {
    browser->window()->ShowCaretBrowsingDialog();
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CaretBrowsing.EnableWithKeyboard"));
    prefService->SetBoolean(prefs::kCaretBrowsingEnabled, true);
  }
}

void PromptToNameWindow(Browser* browser) {
  chrome::ShowWindowNamePrompt(browser);
}

#if BUILDFLAG(IS_CHROMEOS)
void ToggleMultitaskMenu(Browser* browser) {
  DCHECK(chromeos::wm::features::IsWindowLayoutMenuEnabled());
  browser->window()->ToggleMultitaskMenu();
}
#endif

void ToggleCommander(Browser* browser) {
  commander::Commander::Get()->ToggleForBrowser(browser);
}

#if !defined(TOOLKIT_VIEWS)
absl::optional<int> GetKeyboardFocusedTabIndex(const Browser* browser) {
  return absl::nullopt;
}
#endif

void ShowIncognitoClearBrowsingDataDialog(Browser* browser) {
  browser->window()->ShowIncognitoClearBrowsingDataDialog();
}

void ShowIncognitoHistoryDisclaimerDialog(Browser* browser) {
  browser->window()->ShowIncognitoHistoryDisclaimerDialog();
}

bool ShouldInterceptChromeURLNavigationInIncognito(Browser* browser,
                                                   const GURL& url) {
  if (!browser || !browser->profile()->IsIncognitoProfile())
    return false;

  bool show_clear_browsing_data_dialog =
      url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage);

  bool show_history_disclaimer_dialog =
      url == GURL(chrome::kChromeUIHistoryURL);

  return show_clear_browsing_data_dialog || show_history_disclaimer_dialog;
}

void ProcessInterceptedChromeURLNavigationInIncognito(Browser* browser,
                                                      const GURL& url) {
  if (url == GURL(chrome::kChromeUISettingsURL)
                 .Resolve(chrome::kClearBrowserDataSubPage)) {
    ShowIncognitoClearBrowsingDataDialog(browser);
  } else if (url == GURL(chrome::kChromeUIHistoryURL)) {
    ShowIncognitoHistoryDisclaimerDialog(browser);
  } else {
    NOTREACHED();
  }
}

void FollowSite(content::WebContents* web_contents) {
  DCHECK(!Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsIncognitoProfile());
  feed::FollowSite(web_contents);
}

void UnfollowSite(content::WebContents* web_contents) {
  DCHECK(!Profile::FromBrowserContext(web_contents->GetBrowserContext())
              ->IsIncognitoProfile());
  feed::UnfollowSite(web_contents);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void RunScreenAIVisualAnnotation(Browser* browser) {
  browser->RunScreenAIAnnotator();
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

void ExecLensRegionSearch(Browser* browser) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  Profile* profile = browser->profile();
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(profile);
  WebContents* contents = browser->tab_strip_model()->GetActiveWebContents();
  GURL url = contents->GetController().GetLastCommittedEntry()->GetURL();

  if (lens::IsRegionSearchEnabled(browser, profile, service, url)) {
    auto lens_region_search_controller_data =
        std::make_unique<lens::LensRegionSearchControllerData>();
    lens_region_search_controller_data->lens_region_search_controller =
        std::make_unique<lens::LensRegionSearchController>(browser);
    lens_region_search_controller_data->lens_region_search_controller->Start(
        contents, lens::features::IsLensFullscreenSearchEnabled(),
        search::DefaultSearchProviderIsGoogle(profile));
    browser->SetUserData(lens::LensRegionSearchControllerData::kDataKey,
                         std::move(lens_region_search_controller_data));
  }
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
}

}  // namespace chrome
