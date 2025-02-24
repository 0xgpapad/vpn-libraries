// Copyright 2021 Google LLC
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

#ifndef PRIVACY_NET_KRYPTON_DATAPATH_CRYPTOR_INTERFACE_H_
#define PRIVACY_NET_KRYPTON_DATAPATH_CRYPTOR_INTERFACE_H_

#include "privacy/net/krypton/pal/vpn_service_interface.h"
#include "third_party/absl/strings/string_view.h"

namespace privacy {
namespace krypton {
namespace datapath {

/// Common interface for how a packet is encrypted/decrypted in the Wireguard
/// datapath.
class CryptorInterface {
 public:
  virtual ~CryptorInterface() = default;

  // Encrypts/Decrypts the packet.
  //
  // Encryption could potentially change the size of the passed-in packet,
  // therefore it's unsafe to write the packet back to the same memory address.
  // After encryption, a new packet will be created and returned.
  virtual absl::StatusOr<Packet> Process(const Packet& packet) = 0;

  // Updates crypto keys.
  //
  // It's client's responsibility to ensure method Process is not invoked while
  // `Rekey` is happening.
  virtual absl::Status Rekey(const TransformParams& params) = 0;
};

}  // namespace datapath
}  // namespace krypton
}  // namespace privacy

#endif  // PRIVACY_NET_KRYPTON_DATAPATH_CRYPTOR_INTERFACE_H_
