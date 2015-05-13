// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/google/google_update_win.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"

namespace {

// Windows implementation of version update functionality, used by the WebUI
// About/Help page.
class VersionUpdaterWin : public VersionUpdater, public UpdateCheckDelegate {
 public:
  // |owner_widget| is the parent widget hosting the update check UI. Any UI
  // needed to install an update (e.g., a UAC prompt for a system-level install)
  // will be parented to this widget.
  explicit VersionUpdaterWin(gfx::AcceleratedWidget owner_widget);
  ~VersionUpdaterWin() override;

  // VersionUpdater:
  void CheckForUpdate(const StatusCallback& callback) override;
  void RelaunchBrowser() const override;

  // UpdateCheckDelegate:
  void OnUpdateCheckComplete(const base::string16& new_version) override;
  void OnUpgradeProgress(int progress,
                         const base::string16& new_version) override;
  void OnUpgradeComplete(const base::string16& new_version) override;
  void OnError(GoogleUpdateErrorCode error_code,
               const base::string16& error_message,
               const base::string16& new_version) override;

 private:
#if defined(GOOGLE_CHROME_BUILD)
  void BeginUpdateCheckOnFileThread(bool install_update_if_possible);
#endif  // GOOGLE_CHROME_BUILD

  // The widget owning the UI for the update check.
  gfx::AcceleratedWidget owner_widget_;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Used for callbacks.
  base::WeakPtrFactory<VersionUpdaterWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterWin);
};

VersionUpdaterWin::VersionUpdaterWin(gfx::AcceleratedWidget owner_widget)
    : owner_widget_(owner_widget), weak_factory_(this) {
}

VersionUpdaterWin::~VersionUpdaterWin() {
}

void VersionUpdaterWin::CheckForUpdate(const StatusCallback& callback) {
  // There is no supported integration with Google Update for Chromium.
#if defined(GOOGLE_CHROME_BUILD)
  callback_ = callback;

  // On-demand updates for Chrome don't work in Vista RTM when UAC is turned
  // off. So, in this case, the version updater must not mention
  // on-demand updates. Silent updates (in the background) should still
  // work as before - enabling UAC or installing the latest service pack
  // for Vista is another option.
  if (!(base::win::GetVersion() == base::win::VERSION_VISTA &&
        (base::win::OSInfo::GetInstance()->service_pack().major == 0) &&
        !base::win::UserAccountControlIsEnabled())) {
    callback_.Run(CHECKING, 0, base::string16());
    BeginUpdateCheckOnFileThread(false /* !install_update_if_possible */);
  }
#endif
}

void VersionUpdaterWin::RelaunchBrowser() const {
  chrome::AttemptRestart();
}

void VersionUpdaterWin::OnUpdateCheckComplete(
    const base::string16& new_version) {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Status status = CHECKING;
  if (new_version.empty()) {
    // Google Update says that no new version is available. Check to see if a
    // restart is needed for a previously-applied update to take effect.
    status = upgrade_util::IsRunningOldChrome() ? NEARLY_UPDATED : UPDATED;
  } else {
    // Notify the caller that the update is now beginning and initiate it.
    status = UPDATING;
    BeginUpdateCheckOnFileThread(true /* install_update_if_possible */);
  }
  callback_.Run(status, 0, base::string16());
#endif  // GOOGLE_CHROME_BUILD
}

void VersionUpdaterWin::OnUpgradeProgress(int progress,
                                          const base::string16& new_version) {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_.Run(UPDATING, progress, base::string16());
#endif  // GOOGLE_CHROME_BUILD
}

void VersionUpdaterWin::OnUpgradeComplete(const base::string16& new_version) {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback_.Run(NEARLY_UPDATED, 0, base::string16());
#endif  // GOOGLE_CHROME_BUILD
}

void VersionUpdaterWin::OnError(GoogleUpdateErrorCode error_code,
                                const base::string16& error_message,
                                const base::string16& new_version) {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::string16 message;

  // Current versions of Google Update provide a nice message for the policy
  // case. Use this generic error for the policy case only if no message from
  // Google Update is present.
  if (error_code != GOOGLE_UPDATE_DISABLED_BY_POLICY || error_message.empty())
    message = l10n_util::GetStringFUTF16Int(IDS_UPGRADE_ERROR, error_code);

  if (!error_message.empty()) {
    message += l10n_util::GetStringFUTF16(
        IDS_ABOUT_BOX_ERROR_DURING_UPDATE_CHECK, error_message);
  }
  callback_.Run(FAILED, 0, message);
#endif  // GOOGLE_CHROME_BUILD
}

#if defined(GOOGLE_CHROME_BUILD)
void VersionUpdaterWin::BeginUpdateCheckOnFileThread(
    bool install_update_if_possible) {
  BeginUpdateCheck(content::BrowserThread::GetMessageLoopProxyForThread(
                       content::BrowserThread::FILE),
                   g_browser_process->GetApplicationLocale(),
                   install_update_if_possible, owner_widget_,
                   weak_factory_.GetWeakPtr());
}
#endif  // GOOGLE_CHROME_BUILD

}  // namespace

VersionUpdater* VersionUpdater::Create(content::WebContents* web_contents) {
  // Retrieve the HWND for the browser window that is hosting the update check.
  // This will be used as the parent for a UAC prompt, if needed. It's possible
  // this this window will no longer have focus by the time UAC is needed. In
  // that case, the UAC prompt will appear in the taskbar and will require a
  // user click. This is the least surprising thing we can do for the user, and
  // is the intended behavior for Windows applications. It's also possible that
  // the browser window hosting the update check will have been closed by the
  // time the UAC prompt is needed. This will behave similarly.
  return new VersionUpdaterWin(web_contents->GetTopLevelNativeWindow()
                                   ->GetHost()
                                   ->GetAcceleratedWidget());
}
