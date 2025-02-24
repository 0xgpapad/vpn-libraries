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

#ifndef PRIVACY_NET_KRYPTON_ADD_EGRESS_REQUEST_H_
#define PRIVACY_NET_KRYPTON_ADD_EGRESS_REQUEST_H_

#include <cstdint>
#include <memory>

#include "privacy/net/brass/rpc/brass.proto.h"
#include "privacy/net/krypton/auth_and_sign_response.h"
#include "privacy/net/krypton/crypto/session_crypto.h"
#include "privacy/net/krypton/proto/krypton_config.proto.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/jsoncpp/value.h"

namespace privacy {
namespace krypton {

// Builds AddEgressRequest to fetch the public keys of the egress nodes.
// Proto definition is brass.proto
class AddEgressRequest {
 public:
  AddEgressRequest() = default;
  ~AddEgressRequest() = default;

  // Parameters needed for PpnDataplane.
  struct PpnDataplaneRequestParams {
   public:
    PpnDataplaneRequestParams() = default;
    ~PpnDataplaneRequestParams() = default;

    std::shared_ptr<AuthAndSignResponse> auth_response;
    const crypto::SessionCrypto* crypto;  // Not owned.
    std::string copper_control_plane_address;
    ppn::PpnDataplaneRequest::CryptoSuite suite;
    KryptonConfig::DatapathProtocol dataplane_protocol;
    bool is_rekey;
    std::string signature;
    uint32_t uplink_spi;
    bool blind_token_enabled = true;
    // Raw text that was sent to Zinc also needs to be sent to Brass.
    std::string blind_message;
    // This is the unblinded signature after receiving the blinding signature
    // from Zinc that needs to be sent to Brass.
    std::string unblinded_token_signature;
    // This is the region overriding token and signature for sending to Brass.
    std::string region_token_and_signature;
    // This is the APN type from Zinc and used to decide APN in bridge-proxy.
    std::string apn_type;
  };

  // Returns the corresponding headers and json_body separately.
  absl::optional<HttpRequest> EncodeToProtoForBridge(
      std::shared_ptr<AuthAndSignResponse> auth_response);

  absl::optional<HttpRequest> EncodeToProtoForPpn(
      const PpnDataplaneRequestParams& params);

 private:
  Json::Value BuildBodyJson(std::shared_ptr<AuthAndSignResponse> auth_response);
  Json::Value BuildBodyJson(const PpnDataplaneRequestParams& params);
};
}  // namespace krypton
}  // namespace privacy

#endif  // PRIVACY_NET_KRYPTON_ADD_EGRESS_REQUEST_H_
