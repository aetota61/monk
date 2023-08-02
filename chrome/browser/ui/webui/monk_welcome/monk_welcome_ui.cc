#include "chrome/browser/ui/webui/monk_welcome/monk_welcome_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/blank_tab_page_ui/blank_tab_page_ui_constants.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#include "base/feature_list.h"
#include "chrome/grit/browser_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_resources.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/android_about_app_info.h"
#else
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/webui/version/version_handler_win.h"
#include "chrome/browser/ui/webui/version/version_util_win.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

using content::WebUIDataSource;

namespace {

void CreateAndAddMonkWelcomeUIDataSource(Profile* profile) {
  WebUIDataSource* html_source =
      WebUIDataSource::CreateAndAdd(profile, chrome::kChromeUIWelcomeHost);
  html_source->UseStringsJs();
  html_source->AddResourcePath("images/monk_logo.svg", IDR_MONK_WELCOME_LOGO);

  html_source->SetDefaultResource(IDR_MONK_WELCOME_UI_HTML);
}

}  // namespace

MonkWelcomeUI::MonkWelcomeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if !BUILDFLAG(IS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  CreateAndAddMonkWelcomeUIDataSource(profile);
}

MonkWelcomeUI::~MonkWelcomeUI() {}
