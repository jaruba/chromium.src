// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/update_screen.h"

#include <algorithm>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/error_screens_histogram_helper.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/update_view.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_state.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using pairing_chromeos::HostPairingController;

namespace chromeos {

namespace {

// If reboot didn't happen, ask user to reboot device manually.
const int kWaitForRebootTimeSec = 3;

// Progress bar stages. Each represents progress bar value
// at the beginning of each stage.
// TODO(nkostylev): Base stage progress values on approximate time.
// TODO(nkostylev): Animate progress during each state.
const int kBeforeUpdateCheckProgress = 7;
const int kBeforeDownloadProgress = 14;
const int kBeforeVerifyingProgress = 74;
const int kBeforeFinalizingProgress = 81;
const int kProgressComplete = 100;

// Defines what part of update progress does download part takes.
const int kDownloadProgressIncrement = 60;

const char kUpdateDeadlineFile[] = "/tmp/update-check-response-deadline";

// Minimum timestep between two consecutive measurements for the
// download rate.
const base::TimeDelta kMinTimeStep = base::TimeDelta::FromSeconds(1);

// Smooth factor that is used for the average downloading speed
// estimation.
// avg_speed = smooth_factor * cur_speed + (1.0 - smooth_factor) * avg_speed.
const double kDownloadSpeedSmoothFactor = 0.1;

// Minumum allowed value for the average downloading speed.
const double kDownloadAverageSpeedDropBound = 1e-8;

// An upper bound for possible downloading time left estimations.
const double kMaxTimeLeft = 24 * 60 * 60;

// Invoked from call to RequestUpdateCheck upon completion of the DBus call.
void StartUpdateCallback(UpdateScreen* screen,
                         UpdateEngineClient::UpdateCheckResult result) {
  VLOG(1) << "Callback from RequestUpdateCheck, result " << result;
  if (UpdateScreen::HasInstance(screen)) {
    if (result == UpdateEngineClient::UPDATE_RESULT_SUCCESS)
      screen->SetIgnoreIdleStatus(false);
    else
      screen->ExitUpdate(UpdateScreen::REASON_UPDATE_INIT_FAILED);
  }
}

}  // anonymous namespace

// static
UpdateScreen::InstanceSet& UpdateScreen::GetInstanceSet() {
  CR_DEFINE_STATIC_LOCAL(std::set<UpdateScreen*>, instance_set, ());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));  // not threadsafe.
  return instance_set;
}

// static
bool UpdateScreen::HasInstance(UpdateScreen* inst) {
  InstanceSet& instance_set = GetInstanceSet();
  InstanceSet::iterator found = instance_set.find(inst);
  return (found != instance_set.end());
}

// static
UpdateScreen* UpdateScreen::Get(ScreenManager* manager) {
  return static_cast<UpdateScreen*>(
      manager->GetScreen(WizardController::kUpdateScreenName));
}

UpdateScreen::UpdateScreen(BaseScreenDelegate* base_screen_delegate,
                           UpdateView* view,
                           HostPairingController* remora_controller)
    : UpdateModel(base_screen_delegate),
      state_(STATE_IDLE),
      reboot_check_delay_(kWaitForRebootTimeSec),
      is_checking_for_update_(true),
      is_downloading_update_(false),
      is_ignore_update_deadlines_(false),
      is_shown_(false),
      ignore_idle_status_(true),
      view_(view),
      remora_controller_(remora_controller),
      is_first_detection_notification_(true),
      is_first_portal_notification_(true),
      histogram_helper_(new ErrorScreensHistogramHelper("Update")),
      weak_factory_(this) {
  if (view_)
    view_->Bind(*this);

  GetInstanceSet().insert(this);
}

UpdateScreen::~UpdateScreen() {
  if (view_)
    view_->Unbind();

  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  NetworkPortalDetector::Get()->RemoveObserver(this);
  GetInstanceSet().erase(this);
}

void UpdateScreen::UpdateStatusChanged(
    const UpdateEngineClient::Status& status) {
  if (is_checking_for_update_ &&
      status.status > UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE) {
    is_checking_for_update_ = false;
  }
  if (ignore_idle_status_ && status.status >
      UpdateEngineClient::UPDATE_STATUS_IDLE) {
    ignore_idle_status_ = false;
  }

  switch (status.status) {
    case UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      // Do nothing in these cases, we don't want to notify the user of the
      // check unless there is an update.
      SetHostPairingControllerStatus(
          HostPairingController::UPDATE_STATUS_UPDATING);
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeDownloadProgress)
          .SetBoolean(kContextKeyShowEstimatedTimeLeft, false);
      if (!HasCriticalUpdate()) {
        VLOG(1) << "Noncritical update available: " << status.new_version;
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      } else {
        VLOG(1) << "Critical update available: " << status.new_version;
        GetContextEditor()
            .SetString(kContextKeyProgressMessage,
                       l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE))
            .SetBoolean(kContextKeyShowProgressMessage, true)
            .SetBoolean(kContextKeyShowCurtain, false);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      {
        MakeSureScreenIsShown();
        if (!is_downloading_update_) {
          // Because update engine doesn't send UPDATE_STATUS_UPDATE_AVAILABLE
          // we need to is update critical on first downloading notification.
          is_downloading_update_ = true;
          download_start_time_ = download_last_time_ = base::Time::Now();
          download_start_progress_ = status.download_progress;
          download_last_progress_ = status.download_progress;
          is_download_average_speed_computed_ = false;
          download_average_speed_ = 0.0;
          if (!HasCriticalUpdate()) {
            VLOG(1) << "Non-critical update available: " << status.new_version;
            ExitUpdate(REASON_UPDATE_NON_CRITICAL);
          } else {
            VLOG(1) << "Critical update available: " << status.new_version;
            GetContextEditor()
                .SetString(kContextKeyProgressMessage,
                           l10n_util::GetStringUTF16(IDS_INSTALLING_UPDATE))
                .SetBoolean(kContextKeyShowProgressMessage, true)
                .SetBoolean(kContextKeyShowCurtain, false);
          }
        }
        UpdateDownloadingStats(status);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeVerifyingProgress)
          .SetString(kContextKeyProgressMessage,
                     l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING))
          .SetBoolean(kContextKeyShowProgressMessage, true);
      break;
    case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kBeforeFinalizingProgress)
          .SetString(kContextKeyProgressMessage,
                     l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING))
          .SetBoolean(kContextKeyShowProgressMessage, true);
      break;
    case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      MakeSureScreenIsShown();
      GetContextEditor()
          .SetInteger(kContextKeyProgress, kProgressComplete)
          .SetBoolean(kContextKeyShowEstimatedTimeLeft, false);
      if (HasCriticalUpdate()) {
        GetContextEditor().SetBoolean(kContextKeyShowCurtain, false);
        VLOG(1) << "Initiate reboot after update";
        SetHostPairingControllerStatus(
            HostPairingController::UPDATE_STATUS_REBOOTING);
        DBusThreadManager::Get()->GetUpdateEngineClient()->RebootAfterUpdate();
        reboot_timer_.Start(FROM_HERE,
                            base::TimeDelta::FromSeconds(reboot_check_delay_),
                            this,
                            &UpdateScreen::OnWaitForRebootTimeElapsed);
      } else {
        ExitUpdate(REASON_UPDATE_NON_CRITICAL);
      }
      break;
    case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      VLOG(1) << "Attempting rollback";
      break;
    case UpdateEngineClient::UPDATE_STATUS_IDLE:
      if (ignore_idle_status_) {
        // It is first IDLE status that is sent before we initiated the check.
        break;
      }
      // else no break
    case UpdateEngineClient::UPDATE_STATUS_ERROR:
    case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
      ExitUpdate(REASON_UPDATE_ENDED);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  LOG(WARNING) << "UpdateScreen::PortalDetectionCompleted(): "
               << "network=" << (network ? network->path() : "") << ", "
               << "state.status=" << state.status << ", "
               << "state.response_code=" << state.response_code;

  // Wait for the sane detection results.
  if (network &&
      state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN) {
    return;
  }

  // Restart portal detection for the first notification about offline state.
  if ((!network ||
       state.status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE) &&
      is_first_detection_notification_) {
    is_first_detection_notification_ = false;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            base::IgnoreResult(&NetworkPortalDetector::StartDetectionIfIdle),
            base::Unretained(NetworkPortalDetector::Get())));
    return;
  }
  is_first_detection_notification_ = false;

  NetworkPortalDetector::CaptivePortalStatus status = state.status;
  if (state_ == STATE_ERROR) {
    // In the case of online state hide error message and proceed to
    // the update stage. Otherwise, update error message content.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE)
      StartUpdateCheck();
    else
      UpdateErrorMessage(network, status);
  } else if (state_ == STATE_FIRST_PORTAL_CHECK) {
    // In the case of online state immediately proceed to the update
    // stage. Otherwise, prepare and show error message.
    if (status == NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      StartUpdateCheck();
    } else {
      UpdateErrorMessage(network, status);
      ShowErrorMessage();
    }
  }
}

void UpdateScreen::StartNetworkCheck() {
  // If portal detector is enabled and portal detection before AU is
  // allowed, initiate network state check. Otherwise, directly
  // proceed to update.
  if (!NetworkPortalDetector::Get()->IsEnabled()) {
    StartUpdateCheck();
    return;
  }
  state_ = STATE_FIRST_PORTAL_CHECK;
  is_first_detection_notification_ = true;
  is_first_portal_notification_ = true;
  NetworkPortalDetector::Get()->AddAndFireObserver(this);
}

void UpdateScreen::PrepareToShow() {
  if (!view_)
    return;

  view_->PrepareToShow();
}

void UpdateScreen::Show() {
  is_shown_ = true;
  histogram_helper_->OnScreenShow();

#if !defined(OFFICIAL_BUILD)
  GetContextEditor().SetBoolean(kContextKeyCancelUpdateShortcutEnabled, true);
#endif
  GetContextEditor().SetInteger(kContextKeyProgress,
                                kBeforeUpdateCheckProgress);

  if (view_)
    view_->Show();
}

void UpdateScreen::Hide() {
  if (view_)
    view_->Hide();
  is_shown_ = false;
}

void UpdateScreen::Initialize(::login::ScreenContext* context) {
  UpdateModel::Initialize(context);
}

void UpdateScreen::OnViewDestroyed(UpdateView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void UpdateScreen::OnUserAction(const std::string& action_id) {
#if !defined(OFFICIAL_BUILD)
  if (action_id == kUserActionCancelUpdateShortcut)
    CancelUpdate();
#endif
}

void UpdateScreen::OnContextKeyUpdated(
    const ::login::ScreenContext::KeyType& key) {
  UpdateModel::OnContextKeyUpdated(key);
}

void UpdateScreen::OnConnectToNetworkRequested() {
  if (state_ == STATE_ERROR) {
    LOG(WARNING) << "Hiding error message since AP was reselected";
    StartUpdateCheck();
  }
}

void UpdateScreen::ExitUpdate(UpdateScreen::ExitReason reason) {
  DBusThreadManager::Get()->GetUpdateEngineClient()->RemoveObserver(this);
  NetworkPortalDetector::Get()->RemoveObserver(this);
  SetHostPairingControllerStatus(HostPairingController::UPDATE_STATUS_UPDATED);


  switch (reason) {
    case REASON_UPDATE_CANCELED:
      Finish(BaseScreenDelegate::UPDATE_NOUPDATE);
      break;
    case REASON_UPDATE_INIT_FAILED:
      Finish(BaseScreenDelegate::UPDATE_ERROR_CHECKING_FOR_UPDATE);
      break;
    case REASON_UPDATE_NON_CRITICAL:
    case REASON_UPDATE_ENDED:
      {
        UpdateEngineClient* update_engine_client =
            DBusThreadManager::Get()->GetUpdateEngineClient();
        switch (update_engine_client->GetLastStatus().status) {
          case UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
            break;
          case UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
          case UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
          case UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
          case UpdateEngineClient::UPDATE_STATUS_FINALIZING:
          case UpdateEngineClient::UPDATE_STATUS_VERIFYING:
            DCHECK(!HasCriticalUpdate());
            // Noncritical update, just exit screen as if there is no update.
            // no break
          case UpdateEngineClient::UPDATE_STATUS_IDLE:
            Finish(BaseScreenDelegate::UPDATE_NOUPDATE);
            break;
          case UpdateEngineClient::UPDATE_STATUS_ERROR:
          case UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
            Finish(is_checking_for_update_
                       ? BaseScreenDelegate::UPDATE_ERROR_CHECKING_FOR_UPDATE
                       : BaseScreenDelegate::UPDATE_ERROR_UPDATING);
            break;
          default:
            NOTREACHED();
        }
      }
      break;
    default:
      NOTREACHED();
  }
}

void UpdateScreen::OnWaitForRebootTimeElapsed() {
  LOG(ERROR) << "Unable to reboot - asking user for a manual reboot.";
  MakeSureScreenIsShown();
  GetContextEditor().SetString(kContextKeyUpdateMessage,
                               l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
}

void UpdateScreen::MakeSureScreenIsShown() {
  if (!is_shown_)
    get_base_screen_delegate()->ShowCurrentScreen();
}

void UpdateScreen::SetIgnoreIdleStatus(bool ignore_idle_status) {
  ignore_idle_status_ = ignore_idle_status;
}

void UpdateScreen::CancelUpdate() {
  VLOG(1) << "Forced update cancel";
  ExitUpdate(REASON_UPDATE_CANCELED);
}

void UpdateScreen::UpdateDownloadingStats(
    const UpdateEngineClient::Status& status) {
  base::Time download_current_time = base::Time::Now();
  if (download_current_time >= download_last_time_ + kMinTimeStep) {
    // Estimate downloading rate.
    double progress_delta =
        std::max(status.download_progress - download_last_progress_, 0.0);
    double time_delta =
        (download_current_time - download_last_time_).InSecondsF();
    double download_rate = status.new_size * progress_delta / time_delta;

    download_last_time_ = download_current_time;
    download_last_progress_ = status.download_progress;

    // Estimate time left.
    double progress_left = std::max(1.0 - status.download_progress, 0.0);
    if (!is_download_average_speed_computed_) {
      download_average_speed_ = download_rate;
      is_download_average_speed_computed_ = true;
    }
    download_average_speed_ =
        kDownloadSpeedSmoothFactor * download_rate +
        (1.0 - kDownloadSpeedSmoothFactor) * download_average_speed_;
    if (download_average_speed_ < kDownloadAverageSpeedDropBound) {
      time_delta =
          (download_current_time - download_start_time_).InSecondsF();
      download_average_speed_ =
          status.new_size *
          (status.download_progress - download_start_progress_) /
          time_delta;
    }
    double work_left = progress_left * status.new_size;
    double time_left = work_left / download_average_speed_;
    // |time_left| may be large enough or even +infinity. So we must
    // |bound possible estimations.
    time_left = std::min(time_left, kMaxTimeLeft);

    GetContextEditor()
        .SetBoolean(kContextKeyShowEstimatedTimeLeft, true)
        .SetInteger(kContextKeyEstimatedTimeLeftSec,
                    static_cast<int>(time_left));
  }

  int download_progress = static_cast<int>(
      status.download_progress * kDownloadProgressIncrement);
  GetContextEditor().SetInteger(kContextKeyProgress,
                                kBeforeDownloadProgress + download_progress);
}

bool UpdateScreen::HasCriticalUpdate() {
  if (is_ignore_update_deadlines_)
    return true;

  std::string deadline;
  // Checking for update flag file causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crosbug.com/11106
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::FilePath update_deadline_file_path(kUpdateDeadlineFile);
  if (!base::ReadFileToString(update_deadline_file_path, &deadline) ||
      deadline.empty()) {
    return false;
  }

  // TODO(dpolukhin): Analyze file content. Now we can just assume that
  // if the file exists and not empty, there is critical update.
  return true;
}

ErrorScreen* UpdateScreen::GetErrorScreen() {
  return get_base_screen_delegate()->GetErrorScreen();
}

void UpdateScreen::StartUpdateCheck() {
  NetworkPortalDetector::Get()->RemoveObserver(this);
  if (state_ == STATE_ERROR)
    HideErrorMessage();
  state_ = STATE_UPDATE;
  DBusThreadManager::Get()->GetUpdateEngineClient()->AddObserver(this);
  VLOG(1) << "Initiate update check";
  DBusThreadManager::Get()->GetUpdateEngineClient()->RequestUpdateCheck(
      base::Bind(StartUpdateCallback, this));
}

void UpdateScreen::ShowErrorMessage() {
  LOG(WARNING) << "UpdateScreen::ShowErrorMessage()";
  state_ = STATE_ERROR;
  GetErrorScreen()->SetUIState(ErrorScreen::UI_STATE_UPDATE);
  get_base_screen_delegate()->ShowErrorScreen();
  histogram_helper_->OnErrorShow(GetErrorScreen()->GetErrorState());
}

void UpdateScreen::HideErrorMessage() {
  LOG(WARNING) << "UpdateScreen::HideErrorMessage()";
  get_base_screen_delegate()->HideErrorScreen(this);
  histogram_helper_->OnErrorHide();
}

void UpdateScreen::UpdateErrorMessage(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalStatus status) {
  switch (status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      NOTREACHED();
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      GetErrorScreen()->SetErrorState(ErrorScreen::ERROR_STATE_OFFLINE,
                                      std::string());
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      DCHECK(network);
      GetErrorScreen()->SetErrorState(ErrorScreen::ERROR_STATE_PORTAL,
                                      network->name());
      if (is_first_portal_notification_) {
        is_first_portal_notification_ = false;
        GetErrorScreen()->FixCaptivePortal();
      }
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      GetErrorScreen()->SetErrorState(ErrorScreen::ERROR_STATE_PROXY,
                                      std::string());
      break;
    default:
      NOTREACHED();
      break;
  }
}

void UpdateScreen::SetHostPairingControllerStatus(
    HostPairingController::UpdateStatus update_status) {
  if (remora_controller_) {
    remora_controller_->OnUpdateStatusChanged(update_status);
  }
}

}  // namespace chromeos
