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

#include "privacy/net/krypton/auth_and_sign_response.h"

#include <cstddef>
#include <string>

#include "privacy/net/krypton/json_keys.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/status/statusor.h"
#include "third_party/jsoncpp/value.h"
#include "third_party/jsoncpp/writer.h"

namespace privacy {
namespace krypton {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

constexpr char kGoldenZincResponse[] = R"string(
  {"jwt":"TODO","blinded_token_signature":["token1","token2"],"session_manager_ips":[""],"copper_controller_hostname":"test.b.g-tun.com","region_token_and_signature":"US123.sig","apn_type":"ppn"})string";

constexpr char kGoldenPublicKeyResponse[] =
    R"string({"pem": "some_pem"}})string";

// TODO: Write fuzz testing of the JSON body.
TEST(AuthAndSignResponse, TestAuthParameter) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({"jwt": "some_jwt_token"})string");

  KryptonConfig config;
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  ASSERT_OK(auth_response.DecodeFromProto(proto, config));
  EXPECT_EQ(auth_response.jwt_token(), "some_jwt_token");
}

TEST(AuthAndSignResponse, TestAllParametersFromGolden) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(kGoldenZincResponse);

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  ASSERT_OK(auth_response.DecodeFromProto(proto, config));
  EXPECT_EQ(auth_response.jwt_token(), "TODO");
  EXPECT_EQ(auth_response.copper_controller_hostname(), "test.b.g-tun.com");
  EXPECT_EQ(auth_response.region_token_and_signatures(), "US123.sig");
  EXPECT_EQ(auth_response.apn_type(), "ppn");
  EXPECT_THAT(auth_response.blinded_token_signatures(),
              testing::ElementsAre("token1", "token2"));
}

TEST(AuthAndSignResponse, TestEmptyHostname) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({"copper_controller_hostname":""})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  ASSERT_OK(auth_response.DecodeFromProto(proto, config));
  EXPECT_EQ(auth_response.copper_controller_hostname(), "");
}

TEST(AuthAndSignResponse, TestWrongTypeHostname) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(
      R"string({"copper_controller_hostname":["test.b.g-tun.com"]})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  EXPECT_THAT(auth_response.DecodeFromProto(proto, config),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "copper_controller_hostname is not a string"));
}

TEST(AuthAndSignResponse, TestWrongTypeRegionTokenAndSig) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(
      R"string({"region_token_and_signature":["US123.sig"]})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  EXPECT_THAT(auth_response.DecodeFromProto(proto, config),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "region_token_and_sig is not a string"));
}

TEST(AuthAndSignResponse, TestWrongTypeAPNType) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({"apn_type":["ppn"]})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  EXPECT_THAT(
      auth_response.DecodeFromProto(proto, config),
      StatusIs(absl::StatusCode::kInvalidArgument, "apn_type is not a string"));
}

TEST(AuthAndSignResponse, TestWrongAPNType) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({"apn_type":"xxx"})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  EXPECT_THAT(
      auth_response.DecodeFromProto(proto, config),
      StatusIs(absl::StatusCode::kInvalidArgument, "unexpected apn_type"));
}

TEST(AuthAndSignResponse, TestEmptyZincResponse) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  AuthAndSignResponse auth_response;
  ASSERT_OK(auth_response.DecodeFromProto(proto, config));
  EXPECT_EQ(auth_response.jwt_token(), "");
  EXPECT_EQ(auth_response.copper_controller_hostname(), "");
  EXPECT_EQ(auth_response.region_token_and_signatures(), "");
  EXPECT_EQ(auth_response.apn_type(), "");
  EXPECT_THAT(auth_response.blinded_token_signatures().size(), testing::Eq(0));
}

TEST(AuthAndSignResponse, TestEmptySuffixList) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(R"string({"copper_controller_hostname":"xxx"})string");

  KryptonConfig config{};
  AuthAndSignResponse auth_response;
  EXPECT_THAT(
      auth_response.DecodeFromProto(proto, config),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("suffix")));
}

TEST(AuthAndSignResponse, TestSuffixListMultipleElements) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(
      R"string({"copper_controller_hostname":"na.ppn-test"})string");

  KryptonConfig config{};
  config.add_copper_hostname_suffix("g-tun.com");
  config.add_copper_hostname_suffix("ppn-test");
  AuthAndSignResponse auth_response;
  ASSERT_OK(auth_response.DecodeFromProto(proto, config));
  EXPECT_EQ(auth_response.copper_controller_hostname(), "na.ppn-test");
}

TEST(PublicKeyResponse, TestSuccessful) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body(kGoldenPublicKeyResponse);

  PublicKeyResponse response;
  ASSERT_OK(response.DecodeFromProto(proto));
  EXPECT_EQ(response.pem(), "some_pem");
  EXPECT_OK(response.parsing_status());
}

TEST(PublicKeyResponse, TestMissingBody) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");

  PublicKeyResponse response;
  EXPECT_THAT(response.DecodeFromProto(proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("response missing json body")));
}

TEST(PublicKeyResponse, TestEmptyBody) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body("");

  PublicKeyResponse response;
  EXPECT_THAT(response.DecodeFromProto(proto),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("response missing json body")));
}

TEST(PublicKeyResponse, TestMissingPem) {
  HttpResponse proto;
  proto.mutable_status()->set_code(200);
  proto.mutable_status()->set_message("OK");
  proto.set_json_body("{}");

  PublicKeyResponse response;
  // Test failure due to missing pem attribute.
  EXPECT_THAT(
      response.DecodeFromProto(proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("missing pem")));
}
}  // namespace
}  // namespace krypton
}  // namespace privacy
