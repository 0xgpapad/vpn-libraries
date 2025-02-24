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

#include "privacy/net/krypton/fd_packet_pipe.h"

#include <netinet/in.h>

#include "privacy/net/krypton/datapath/android_ipsec/data_test.proto.h"
#include "privacy/net/krypton/datapath/android_ipsec/simple_udp_server.h"
#include "privacy/net/krypton/pal/packet.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "third_party/absl/functional/bind_front.h"
#include "third_party/absl/status/status.h"
#include "third_party/absl/synchronization/notification.h"
#include "third_party/absl/time/clock.h"
#include "third_party/absl/time/time.h"

namespace privacy {
namespace krypton {
namespace {

using ::testing::DoAll;
using ::testing::EqualsProto;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

MATCHER_P(PacketEquals, expected,
          absl::StrCat("Packet data equals ", expected->data())) {
  return arg.data() == expected->data();
}

class TestForwarder {
 public:
  bool ReadPacket(absl::Status status, Packet packet) {
    return DoReadPacket(status, packet);
  }
  MOCK_METHOD(bool, DoReadPacket, (absl::Status, const Packet& packet), ());
};

class FdPacketPipeTest : public ::testing::Test {
 public:
  ~FdPacketPipeTest() override { packet_pipe_.Close(); }

  void SetUp() override {
    // Connect both the sockets to each other.
    ASSERT_OK(packet_pipe_socket_.Connect(copper_.port_));
    ASSERT_OK(copper_.Connect(packet_pipe_socket_.port_));

    packet_pipe_.ReadPackets(
        absl::bind_front(&TestForwarder::ReadPacket, &forwarder_));

    // wait for read thread to start.
    do {
      absl::SleepFor(absl::Milliseconds(100));
    } while (packet_pipe_.SocketListeningTestOnly() == false);
  }

  void TearDown() override { packet_pipe_.StopReadingPackets().IgnoreError(); }

  // Testing Path
  datapath::testing::SimpleUdpServer packet_pipe_socket_;
  datapath::testing::SimpleUdpServer copper_;
  FdPacketPipe packet_pipe_{packet_pipe_socket_.fd_};
  TestForwarder forwarder_;
};

TEST_F(FdPacketPipeTest, WritePacket) {
  datapath::Packet packet_buffer;
  packet_buffer.set_sequence_number(1);
  auto buffer = packet_buffer.SerializeAsString();
  EXPECT_OK(packet_pipe_.WritePacket(
      Packet(buffer.c_str(), buffer.length(), IPProtocol::kIPv4, []() {})));

  auto received_buffer = copper_.ReceivePacket();
  datapath::Packet expected;
  expected.ParseFromString(received_buffer);
  LOG(INFO) << "Received " << expected.DebugString();
  EXPECT_THAT(packet_buffer, EqualsProto(expected));
}

TEST_F(FdPacketPipeTest, WritePacketFailureDueToFdClosure) {
  close(packet_pipe_socket_.fd_);
  datapath::Packet packet_buffer;
  packet_buffer.set_sequence_number(1);
  auto buffer = packet_buffer.SerializeAsString();

  EXPECT_THAT(packet_pipe_.WritePacket(Packet(buffer.c_str(), buffer.length(),
                                              IPProtocol::kIPv4, []() {})),
              ::testing::status::StatusIs(absl::StatusCode::kInternal));
}

TEST_F(FdPacketPipeTest, ReadPacket) {
  datapath::Packet packet_buffer;
  packet_buffer.set_sequence_number(1);
  auto buffer = packet_buffer.SerializeAsString();
  Packet packet(buffer.c_str(), buffer.length(), IPProtocol::kIPv4, []() {});
  absl::Notification done;

  EXPECT_CALL(forwarder_, DoReadPacket(absl::OkStatus(), PacketEquals(&packet)))
      .WillOnce(DoAll(InvokeWithoutArgs(&done, &absl::Notification::Notify),
                      Return(false)));

  auto write_bytes =
      send(copper_.fd_, reinterpret_cast<const void*>(buffer.c_str()),
           buffer.length(), MSG_CONFIRM);

  EXPECT_EQ(write_bytes, buffer.length());
  EXPECT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(2)));
}

TEST_F(FdPacketPipeTest, ReadTwoPackets) {
  Packet packet1("foo", 3, IPProtocol::kUnknown, []() {});
  Packet packet2("bar", 3, IPProtocol::kUnknown, []() {});

  absl::Notification done1;
  absl::Notification done2;

  EXPECT_CALL(forwarder_,
              DoReadPacket(absl::OkStatus(), PacketEquals(&packet1)))
      .WillOnce(DoAll(InvokeWithoutArgs(&done1, &absl::Notification::Notify),
                      Return(true)));
  EXPECT_CALL(forwarder_,
              DoReadPacket(absl::OkStatus(), PacketEquals(&packet2)))
      .WillOnce(DoAll(InvokeWithoutArgs(&done2, &absl::Notification::Notify),
                      Return(false)));

  auto write_bytes = send(copper_.fd_, "foo", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  write_bytes = send(copper_.fd_, "bar", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  EXPECT_TRUE(done1.WaitForNotificationWithTimeout(absl::Seconds(2)));
  EXPECT_TRUE(done2.WaitForNotificationWithTimeout(absl::Seconds(2)));
}

TEST_F(FdPacketPipeTest, StopReadingPackets) {
  Packet packet1("foo", 3, IPProtocol::kUnknown, []() {});
  Packet packet2("bar", 3, IPProtocol::kUnknown, []() {});

  absl::Notification done;

  EXPECT_CALL(forwarder_,
              DoReadPacket(absl::OkStatus(), PacketEquals(&packet1)))
      .WillOnce(DoAll(InvokeWithoutArgs(&done, &absl::Notification::Notify),
                      Return(true)));

  auto write_bytes = send(copper_.fd_, "foo", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  EXPECT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(2)));

  EXPECT_OK(packet_pipe_.StopReadingPackets());

  write_bytes = send(copper_.fd_, "bar", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  // Add a little time just to make sure the second packet doesn't get read.
  absl::SleepFor(absl::Milliseconds(100));
}

TEST_F(FdPacketPipeTest, StopReadingOnReturnFalse) {
  Packet packet1("foo", 3, IPProtocol::kUnknown, []() {});
  Packet packet2("bar", 3, IPProtocol::kUnknown, []() {});

  absl::Notification done;

  EXPECT_CALL(forwarder_,
              DoReadPacket(absl::OkStatus(), PacketEquals(&packet1)))
      .WillOnce(DoAll(InvokeWithoutArgs(&done, &absl::Notification::Notify),
                      Return(false)));

  auto write_bytes = send(copper_.fd_, "foo", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  write_bytes = send(copper_.fd_, "bar", 3, MSG_CONFIRM);
  EXPECT_EQ(write_bytes, 3);

  EXPECT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(2)));

  // Add a little time just to make sure the second packet doesn't get read.
  absl::SleepFor(absl::Milliseconds(100));
}

}  // namespace
}  // namespace krypton
}  // namespace privacy
