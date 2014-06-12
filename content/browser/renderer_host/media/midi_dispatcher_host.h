// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class BrowserContext;

// MidiDispatcherHost handles permissions for using system exclusive messages.
// It works as BrowserMessageFilter to handle IPC messages between
// MidiDispatcher running as a RenderViewObserver.
class MidiDispatcherHost : public BrowserMessageFilter {
 public:
  MidiDispatcherHost(int render_process_id, BrowserContext* browser_context);

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message, BrowserThread::ID* thread) OVERRIDE;

 protected:
  virtual ~MidiDispatcherHost();

 private:
  void OnRequestSysExPermission(int render_view_id,
                                int bridge_id,
                                const GURL& origin,
                                bool user_gesture);
  void OnCancelSysExPermissionRequest(int render_view_id,
                                      int bridge_id,
                                      const GURL& requesting_frame);
  void WasSysExPermissionGranted(int render_view_id,
                                 int bridge_id,
                                 bool is_allowed);

  int render_process_id_;
  BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(MidiDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MIDI_DISPATCHER_HOST_H_
