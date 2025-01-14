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

#include "privacy/net/krypton/datapath_address_selector.h"

#include <string>

#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace privacy {
namespace krypton {

using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

MATCHER_P(EndpointTo, expected, absl::StrCat("is an endpoint to ", expected)) {
  return expected == arg.ToString();
}

class DatapathAddressSelectorTest : public ::testing::Test {};

TEST_F(DatapathAddressSelectorTest, V6BeforeV4Test) {
  std::vector<std::string> input = {
      "192.168.1.1:80",
      "[2001:db8::]:80",
  };

  DatapathAddressSelector selector;
  selector.Reset(input, absl::nullopt);

  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.1:80")));
  }
  EXPECT_THAT(selector.SelectDatapathAddress(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST_F(DatapathAddressSelectorTest, BackoffAddressTest) {
  std::vector<std::string> input = {
      "192.168.1.1:80",
      "192.168.1.2:80",
      "[2001:db8::]:80",
      "[2001:db8::1]:80",
  };

  DatapathAddressSelector selector;
  selector.Reset(input, absl::nullopt);

  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.1:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::1]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.2:80")));
  }

  EXPECT_THAT(selector.SelectDatapathAddress(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST_F(DatapathAddressSelectorTest, V4V6NetworkTest) {
  std::vector<std::string> input = {
      "192.168.1.1:80",
      "192.168.1.2:80",
      "[2001:db8::]:80",
      "[2001:db8::1]:80",
  };

  NetworkInfo network_info;
  network_info.set_address_family(NetworkInfo::V4V6);

  DatapathAddressSelector selector;
  selector.Reset(input, absl::nullopt);

  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.1:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::1]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.2:80")));
  }

  EXPECT_THAT(selector.SelectDatapathAddress(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST_F(DatapathAddressSelectorTest, V4OnlyNetworkTest) {
  std::vector<std::string> input = {
      "192.168.1.1:80",
      "192.168.1.2:80",
      "[2001:db8::]:80",
      "[2001:db8::1]:80",
  };

  NetworkInfo network_info;
  network_info.set_address_family(NetworkInfo::V4);

  DatapathAddressSelector selector;
  selector.Reset(input, network_info);

  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.1:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.2:80")));
  }

  EXPECT_THAT(selector.SelectDatapathAddress(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

TEST_F(DatapathAddressSelectorTest, V6OnlyNetworkTest) {
  std::vector<std::string> input = {
      "192.168.1.1:80",
      "192.168.1.2:80",
      "[2001:db8::]:80",
      "[2001:db8::1]:80",
  };

  NetworkInfo network_info;
  network_info.set_address_family(NetworkInfo::V4V6);

  DatapathAddressSelector selector;
  selector.Reset(input, absl::nullopt);

  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.1:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("[2001:db8::1]:80")));
    EXPECT_THAT(selector.SelectDatapathAddress(),
                IsOkAndHolds(EndpointTo("192.168.1.2:80")));
  }

  EXPECT_THAT(selector.SelectDatapathAddress(),
              StatusIs(absl::StatusCode::kResourceExhausted));
}

}  // namespace krypton
}  // namespace privacy
