# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'webview_native',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base_static',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../cc/cc.gyp:cc',
        '../../components/components.gyp:autofill_content_browser',
        '../../components/components.gyp:web_contents_delegate_android',
        '../../content/content.gyp:content_common',
        '../../net/net.gyp:net',
        '../../skia/skia.gyp:skia',
        '../../ui/base/ui_base.gyp:ui_base',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/gfx/gfx.gyp:gfx_geometry',
        '../../webkit/storage_browser.gyp:webkit_storage_browser',
        '../../webkit/storage_common.gyp:webkit_storage_common',
        '../../third_party/boringssl/boringssl.gyp:boringssl',
        'android_webview_native_jni',
      ],
      'include_dirs': [
        '../..',
        '../../skia/config',
      ],
      'sources': [
        'android_protocol_handler.cc',
        'android_protocol_handler.h',
        'android_webview_jni_registrar.cc',
        'android_webview_jni_registrar.h',
        'aw_assets.cc',
        'aw_assets.h',
        'aw_autofill_client.cc',
        'aw_autofill_client.h',
        'aw_browser_dependency_factory.cc',
        'aw_browser_dependency_factory.h',
        'aw_contents.cc',
        'aw_contents.h',
        'aw_contents_client_bridge.cc',
        'aw_contents_client_bridge.h',
        'aw_contents_io_thread_client_impl.cc',
        'aw_contents_io_thread_client_impl.h',
        'aw_contents_statics.cc',
        'aw_contents_statics.h',
        'aw_dev_tools_server.cc',
        'aw_dev_tools_server.h',
        'aw_form_database.cc',
        'aw_form_database.h',
        'aw_http_auth_handler.cc',
        'aw_http_auth_handler.h',
        'aw_pdf_exporter.cc',
        'aw_pdf_exporter.h',
        'aw_picture.cc',
        'aw_picture.h',
        'aw_quota_manager_bridge_impl.cc',
        'aw_quota_manager_bridge_impl.h',
        'aw_resource.cc',
        'aw_resource.h',
        'aw_settings.cc',
        'aw_settings.h',
        'aw_web_contents_delegate.cc',
        'aw_web_contents_delegate.h',
        'aw_web_contents_view_delegate.cc',
        'aw_web_contents_view_delegate.h',
        'aw_web_preferences_populater_impl.cc',
        'aw_web_preferences_populater_impl.h',
        'aw_web_resource_response_impl.cc',
        'aw_web_resource_response_impl.h',
        'cookie_manager.cc',
        'cookie_manager.h',
        'input_stream_impl.cc',
        'input_stream_impl.h',
        'java_browser_view_renderer_helper.cc',
        'java_browser_view_renderer_helper.h',
        'net_init_native_callback.cc',
        'permission/aw_permission_request.cc',
        'permission/aw_permission_request.h',
        'permission/aw_permission_request_delegate.cc',
        'permission/aw_permission_request_delegate.h',
        'permission/media_access_permission_request.cc',
        'permission/media_access_permission_request.h',
        'permission/permission_request_handler.cc',
        'permission/permission_request_handler.h',
        'permission/permission_request_handler_client.cc',
        'permission/permission_request_handler_client.h',
        'permission/simple_permission_request.cc',
        'permission/simple_permission_request.h',
        'state_serializer.cc',
        'state_serializer.h',
      ],
      'conditions': [
        ['video_hole==1', {
          'sources': [
            'external_video_surface_container_impl.cc',
            'external_video_surface_container_impl.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'input_stream_android_jar_jni_headers',
      'type': 'none',
      'variables': {
        'jni_gen_package': 'android_webview',
        'input_java_class': 'java/io/InputStream.class',
      },
      'includes': [ '../../build/jar_file_jni_generator.gypi' ],
    },
    {
      'target_name': 'cancellation_signal_android_jar_jni_headers',
      'type': 'none',
      'variables': {
        'jni_gen_package': 'android_webview',
        'input_java_class': 'android/os/CancellationSignal.class',
      },
      'includes': [ '../../build/jar_file_jni_generator.gypi' ],
    },
    {
      'target_name': 'android_webview_native_jni',
      'type': 'none',
      'sources': [
          '../java/src/org/chromium/android_webview/AndroidProtocolHandler.java',
          '../java/src/org/chromium/android_webview/AwAssets.java',
          '../java/src/org/chromium/android_webview/AwAutofillClient.java',
          '../java/src/org/chromium/android_webview/AwContents.java',
          '../java/src/org/chromium/android_webview/AwContentsClientBridge.java',
          '../java/src/org/chromium/android_webview/AwContentsIoThreadClient.java',
          '../java/src/org/chromium/android_webview/AwContentsStatics.java',
          '../java/src/org/chromium/android_webview/AwCookieManager.java',
          '../java/src/org/chromium/android_webview/AwDevToolsServer.java',
          '../java/src/org/chromium/android_webview/AwFormDatabase.java',
          '../java/src/org/chromium/android_webview/AwHttpAuthHandler.java',
          '../java/src/org/chromium/android_webview/AwPdfExporter.java',
          '../java/src/org/chromium/android_webview/AwPicture.java',
          '../java/src/org/chromium/android_webview/AwQuotaManagerBridge.java',
          '../java/src/org/chromium/android_webview/AwResource.java',
          '../java/src/org/chromium/android_webview/AwSettings.java',
          '../java/src/org/chromium/android_webview/AwWebContentsDelegate.java',
          '../java/src/org/chromium/android_webview/AwWebResourceResponse.java',
          '../java/src/org/chromium/android_webview/ExternalVideoSurfaceContainer.java',
          '../java/src/org/chromium/android_webview/JavaBrowserViewRendererHelper.java',
          '../java/src/org/chromium/android_webview/permission/AwPermissionRequest.java',
      ],
      'variables': {
        'jni_gen_package': 'android_webview',
      },
      'includes': [ '../../build/jni_generator.gypi' ],
      'dependencies': [
        'input_stream_android_jar_jni_headers',
        'cancellation_signal_android_jar_jni_headers',
      ],
    },
  ],
}
