// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the );
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an  BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PRIVACY_NET_KRYPTON_PAL_MOCK_VPN_SERVICE_INTERFACE_H_
#define PRIVACY_NET_KRYPTON_PAL_MOCK_VPN_SERVICE_INTERFACE_H_

#include <memory>

#include "privacy/net/krypton/pal/vpn_service_interface.h"
#include "privacy/net/krypton/proto/network_info.proto.h"
#include "privacy/net/krypton/proto/tun_fd_data.proto.h"
#include "testing/base/public/gmock.h"
#include "third_party/absl/status/statusor.h"

namespace privacy {
namespace krypton {

// Mock interface for VPN Service.
class MockVpnService : public VpnServiceInterface {
 public:
  MOCK_METHOD(absl::StatusOr<std::unique_ptr<PacketPipe>>, CreateTunnel,
              (const TunFdData&), (override));

  MOCK_METHOD(absl::StatusOr<int>, CreateProtectedNetworkSocket,
              (const NetworkInfo&), (override));

  MOCK_METHOD(absl::Status, ConfigureIpSec, (const IpSecTransformParams&),
              (override));

  MOCK_METHOD(absl::StatusOr<std::unique_ptr<PacketPipe>>, CreateNetworkPipe,
              (const NetworkInfo&, const Endpoint&), (override));
};

}  // namespace krypton
}  // namespace privacy

#endif  // PRIVACY_NET_KRYPTON_PAL_MOCK_VPN_SERVICE_INTERFACE_H_
