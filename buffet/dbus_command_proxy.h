// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DBUS_COMMAND_PROXY_H_
#define BUFFET_DBUS_COMMAND_PROXY_H_

#include <string>

#include <base/macros.h>
#include <base/scoped_observer.h>
#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/dbus_object.h>
#include <weave/command.h>

#include "buffet/dbus_bindings/com.android.Weave.Command.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace buffet {

class DBusCommandProxy : public com::android::Weave::CommandInterface {
 public:
  DBusCommandProxy(chromeos::dbus_utils::ExportedObjectManager* object_manager,
                   const scoped_refptr<dbus::Bus>& bus,
                   const std::weak_ptr<weave::Command>& command,
                   std::string object_path);
  ~DBusCommandProxy() override = default;

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

 private:
  // Handles calls to com.android.Weave.Command.SetProgress(progress).
  bool SetProgress(chromeos::ErrorPtr* error,
                   const chromeos::VariantDictionary& progress) override;
  // Handles calls to com.android.Weave.Command.Complete(results).
  bool Complete(chromeos::ErrorPtr* error,
                const chromeos::VariantDictionary& results) override;
  // Handles calls to com.android.Weave.Command.Abort().
  bool Abort(chromeos::ErrorPtr* error,
             const std::string& code,
             const std::string& message) override;
  // Handles calls to com.android.Weave.Command.Cancel().
  bool Cancel(chromeos::ErrorPtr* error) override;

  std::weak_ptr<weave::Command> command_;
  com::android::Weave::CommandAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;

  friend class DBusCommandProxyTest;
  friend class DBusCommandDispacherTest;
  DISALLOW_COPY_AND_ASSIGN(DBusCommandProxy);
};

}  // namespace buffet

#endif  // BUFFET_DBUS_COMMAND_PROXY_H_
