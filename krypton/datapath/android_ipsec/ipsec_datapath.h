// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "LICENSE");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PRIVACY_NET_KRYPTON_DATAPATH_ANDROID_IPSEC_IPSEC_DATAPATH_H_
#define PRIVACY_NET_KRYPTON_DATAPATH_ANDROID_IPSEC_IPSEC_DATAPATH_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "privacy/net/krypton/add_egress_response.h"
#include "privacy/net/krypton/crypto/session_crypto.h"
#include "privacy/net/krypton/datapath/packet_forwarder.h"
#include "privacy/net/krypton/datapath_interface.h"
#include "privacy/net/krypton/fd_packet_pipe.h"
#include "privacy/net/krypton/proto/network_info.proto.h"
#include "third_party/absl/base/call_once.h"
#include "third_party/absl/base/thread_annotations.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/synchronization/mutex.h"

namespace privacy {
namespace krypton {
namespace datapath {
namespace android {

// Manages the Ipsec data path.
// Class is thread safe.
class IpSecDatapath : public DatapathInterface,
                      public datapath::PacketForwarder::NotificationInterface {
 public:
  explicit IpSecDatapath(utils::LooperThread* looper,
                         VpnServiceInterface* vpn_service)
      : notification_thread_(looper), vpn_service_(vpn_service) {}
  ~IpSecDatapath() override = default;

  // Initialize the Ipsec data path.
  // TODO: Remove AddEgressResponse and Bridge specific stuff from
  // Start method.
  absl::Status Start(std::shared_ptr<AddEgressResponse> egress_response,
                     const TransformParams& params) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Terminate the data path connection.
  void Stop() override ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status SwitchNetwork(uint32_t session_id, const Endpoint& endpoint,
                             absl::optional<NetworkInfo> network_info,
                             PacketPipe* tunnel, int counter) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Status SetKeyMaterials(const TransformParams& params) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  void PacketForwarderFailed(const absl::Status&) override;

  void PacketForwarderPermanentFailure(const absl::Status&) override;

  void PacketForwarderConnected() override;

 private:
  void ShutdownPacketForwarder() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  absl::Mutex mutex_;

  utils::LooperThread* notification_thread_;
  VpnServiceInterface* vpn_service_;

  absl::optional<IpSecTransformParams> key_material_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<PacketForwarder> forwarder_ ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<PacketPipe> network_pipe_ ABSL_GUARDED_BY(mutex_);

  std::atomic_bool shutting_down_ = false;
};

}  // namespace android
}  // namespace datapath
}  // namespace krypton
}  // namespace privacy

#endif  // PRIVACY_NET_KRYPTON_DATAPATH_ANDROID_IPSEC_IPSEC_DATAPATH_H_
