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

#include "privacy/net/krypton/egress_manager.h"

#include <cstdint>
#include <memory>
#include <string>

#include "google/protobuf/duration.proto.h"
#include "privacy/net/krypton/add_egress_request.h"
#include "privacy/net/krypton/auth_and_sign_response.h"
#include "privacy/net/krypton/crypto/session_crypto.h"
#include "privacy/net/krypton/json_keys.h"
#include "privacy/net/krypton/pal/mock_http_fetcher_interface.h"
#include "privacy/net/krypton/proto/debug_info.proto.h"
#include "privacy/net/krypton/proto/krypton_config.proto.h"
#include "privacy/net/krypton/utils/looper.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/synchronization/notification.h"
#include "third_party/jsoncpp/reader.h"
#include "third_party/jsoncpp/value.h"
#include "third_party/jsoncpp/writer.h"

namespace privacy {
namespace krypton {
using ::testing::_;
using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

// Mock interface for Notification.  Used for testing.
class MockEgressManagerNotification
    : public EgressManager::NotificationInterface {
 public:
  MOCK_METHOD(void, EgressAvailable, (bool), (override));
  MOCK_METHOD(void, EgressUnavailable, (const absl::Status&), (override));
};

class EgressManagerTest : public ::testing::Test {
 public:
  MockEgressManagerNotification mock_notification_;
  KryptonConfig config_;
  crypto::SessionCrypto crypto_{&config_};

  std::shared_ptr<AuthAndSignResponse> BuildFakeAuthResponse() {
    auto fake_auth_response = std::make_shared<AuthAndSignResponse>();

    // Preconstruct some basic auth response parameters and construct json_body.
    HttpResponse response;
    response.mutable_status()->set_code(200);
    response.mutable_status()->set_message("OK");
    response.set_json_body(R"json({"jwt": "some_jwt_token"})json");

    KryptonConfig config;
    config.add_copper_hostname_suffix("g-tun.com");
    EXPECT_OK(fake_auth_response->DecodeFromProto(response, config));
    return fake_auth_response;
  }

  HttpRequest buildAddEgressRequestPpnIpSec(absl::string_view url,
                                            uint32_t spi) {
    Json::FastWriter writer;
    HttpRequest request;
    request.set_url(url);
    request.set_json_body(
        writer.write(buildJsonBodyForAddEgressRequestPpnIpSec(spi)));
    return request;
  }

  // Request from AddEgress.
  Json::Value buildJsonBodyForAddEgressRequestPpnIpSec(uint32_t spi) {
    Json::Value expected;
    Json::Reader reader;
    reader.parse(R"string({
      "ppn" : {
      },
      "unblinded_token" : "some_jwt_token",
      "region_token_and_signature" : "",
   })string",
                 expected);
    auto keys = crypto_.GetMyKeyMaterial();
    expected[JsonKeys::kPpn][JsonKeys::kClientNonce] = keys.nonce;
    expected[JsonKeys::kPpn][JsonKeys::kClientPublicValue] = keys.public_value;
    expected[JsonKeys::kPpn][JsonKeys::kControlPlaneSockAddr] =
        "192.168.0.10:1849";
    expected[JsonKeys::kPpn][JsonKeys::kApnType] = "ppn";

    expected[JsonKeys::kPpn][JsonKeys::kDownlinkSpi] = spi;
    expected[JsonKeys::kPpn][JsonKeys::kSuite] = "AES128_GCM";
    expected[JsonKeys::kPpn][JsonKeys::kDataplaneProtocol] = "IPSEC";
    expected[JsonKeys::kPpn][JsonKeys::kRekeyVerificationKey] =
        crypto_.GetRekeyVerificationKey().ValueOrDie();
    return expected;
  }

  // Add Egress response that doesn't contain any data path.
  HttpResponse buildAddEgressResponseWithNoDataPath() {
    HttpResponse response;
    response.mutable_status()->set_code(200);
    response.mutable_status()->set_message("OK");
    return response;
  }

  HttpResponse buildAddEgressResponseForPpnIpSec() {
    crypto::SessionCrypto server_crypto(&config_);
    auto keys = crypto_.GetMyKeyMaterial();
    Json::Reader reader;
    Json::Value json_body;
    reader.parse(R"json({
      "ppn_dataplane": {
        "user_private_ip": [{
          "ipv4_range": "127.0.0.1"
        }],
        "egress_point_sock_addr": [
          "addr1"
        ],
        "egress_point_public_value": "1234567890abcdef",
        "server_nonce": "abcd",
        "uplink_spi": 123,
        "expiry": "2020-08-07T01:06:13+00:00"
      }
    })json",
                 json_body);
    json_body[JsonKeys::kPpn][JsonKeys::kEgressPointPublicValue] =
        keys.public_value;
    json_body[JsonKeys::kPpn][JsonKeys::kServerNonce] = keys.nonce;

    Json::FastWriter writer;

    HttpResponse response;
    response.mutable_status()->set_code(200);
    response.mutable_status()->set_message("OK");
    response.set_json_body(writer.write(json_body));
    return response;
  }
  utils::LooperThread looper_thread_{"EgressManager Test"};
};

TEST_F(EgressManagerTest, SuccessfulEgress) {
  MockHttpFetcher http_fetcher;

  EgressManager egress_manager("http://www.example.com/addegress",
                               &http_fetcher, &looper_thread_);
  egress_manager.RegisterNotificationHandler(&mock_notification_);

  auto fake_auth_response = BuildFakeAuthResponse();
  absl::Notification http_fetcher_done;

  EXPECT_CALL(http_fetcher, PostJson(EqualsProto(R"pb(
                url: "http://www.example.com/addegress"
                json_body: "{\"unblinded_token\":\"some_jwt_token\"}\n"
              )pb")))
      .WillOnce(Return(buildAddEgressResponseForPpnIpSec()));

  EXPECT_CALL(mock_notification_, EgressAvailable)
      .WillOnce(
          InvokeWithoutArgs(&http_fetcher_done, &absl::Notification::Notify));

  EXPECT_OK(egress_manager.GetEgressNodeForBridge(fake_auth_response));

  http_fetcher_done.WaitForNotification();

  EXPECT_THAT(egress_manager.GetState(),
              Eq(EgressManager::State::kEgressSessionCreated));

  EgressDebugInfo debug_info;
  egress_manager.GetDebugInfo(&debug_info);
  EXPECT_EQ(debug_info.latency().size(), 1);
  EXPECT_GT(debug_info.latency(0).nanos(), 0);
  egress_manager.Stop();
}

TEST_F(EgressManagerTest, TestNoDataPath) {
  MockHttpFetcher http_fetcher;
  EgressManager egress_manager("http://www.example.com/addegress",
                               &http_fetcher, &looper_thread_);
  egress_manager.RegisterNotificationHandler(&mock_notification_);
  auto fake_auth_response = BuildFakeAuthResponse();
  absl::Notification http_fetcher_done;

  EXPECT_CALL(http_fetcher, PostJson(EqualsProto(R"pb(
                url: "http://www.example.com/addegress",
                json_body: "{\"unblinded_token\":\"some_jwt_token\"}\n"
              )pb")))
      .WillOnce(Return(buildAddEgressResponseWithNoDataPath()));

  EXPECT_CALL(mock_notification_, EgressUnavailable(_))
      .WillOnce(
          InvokeWithoutArgs(&http_fetcher_done, &absl::Notification::Notify));

  EXPECT_OK(egress_manager.GetEgressNodeForBridge(fake_auth_response));

  http_fetcher_done.WaitForNotification();

  EXPECT_THAT(egress_manager.GetState(),
              Eq(EgressManager::State::kEgressSessionError));

  egress_manager.Stop();
}

TEST_F(EgressManagerTest, SuccessfulEgressForPpnIpSec) {
  MockHttpFetcher http_fetcher;

  EgressManager egress_manager("http://www.example.com/addegress",
                               &http_fetcher, &looper_thread_);
  egress_manager.RegisterNotificationHandler(&mock_notification_);

  auto fake_auth_response = BuildFakeAuthResponse();
  absl::Notification http_fetcher_done;

  EXPECT_CALL(http_fetcher,
              PostJson(EqualsProto(buildAddEgressRequestPpnIpSec(
                  "http://www.example.com/addegress", crypto_.downlink_spi()))))
      .WillOnce(Return(buildAddEgressResponseForPpnIpSec()));

  EXPECT_CALL(mock_notification_, EgressAvailable)
      .WillOnce(
          InvokeWithoutArgs(&http_fetcher_done, &absl::Notification::Notify));

  AddEgressRequest::PpnDataplaneRequestParams params;
  params.auth_response = fake_auth_response;
  params.crypto = &crypto_;
  params.copper_control_plane_address = "192.168.0.10";
  params.dataplane_protocol = KryptonConfig::IPSEC;
  params.suite = ppn::PpnDataplaneRequest::AES128_GCM;
  params.is_rekey = false;
  params.blind_token_enabled = false;
  params.apn_type = "ppn";

  EXPECT_OK(egress_manager.GetEgressNodeForPpnIpSec(params));

  http_fetcher_done.WaitForNotification();

  EXPECT_THAT(egress_manager.GetState(),
              Eq(EgressManager::State::kEgressSessionCreated));

  egress_manager.Stop();
}

}  // namespace krypton
}  // namespace privacy
