// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothAdapterClient simulates the behavior of the Bluetooth Daemon
// adapter objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothAdapterClient
    : public BluetoothAdapterClient {
 public:
  struct Properties : public BluetoothAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) override;
    virtual void GetAll() override;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothAdapterClient();
  virtual ~FakeBluetoothAdapterClient();

  // BluetoothAdapterClient overrides
  virtual void Init(dbus::Bus* bus) override;
  virtual void AddObserver(Observer* observer) override;
  virtual void RemoveObserver(Observer* observer) override;
  virtual std::vector<dbus::ObjectPath> GetAdapters() override;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      override;
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) override;
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            const base::Closure& callback,
                            const ErrorCallback& error_callback) override;

  // Sets the current simulation timeout interval.
  void SetSimulationIntervalMs(int interval_ms);

  // Mark the adapter and second adapter as visible or invisible.
  void SetVisible(bool visible);
  void SetSecondVisible(bool visible);

  // Object path, name and addresses of the adapters we emulate.
  static const char kAdapterPath[];
  static const char kAdapterName[];
  static const char kAdapterAddress[];

  static const char kSecondAdapterPath[];
  static const char kSecondAdapterName[];
  static const char kSecondAdapterAddress[];

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const std::string& property_name);

  // Posts the delayed task represented by |callback| onto the current
  // message loop to be executed after |simulation_interval_ms_| milliseconds.
  void PostDelayedTask(const base::Closure& callback);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  scoped_ptr<Properties> properties_;
  scoped_ptr<Properties> second_properties_;

  // Whether the adapter and second adapter should be visible or not.
  bool visible_;
  bool second_visible_;

  // Number of times we've been asked to discover.
  int discovering_count_;

  // Current timeout interval used when posting delayed tasks.
  int simulation_interval_ms_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_ADAPTER_CLIENT_H_
