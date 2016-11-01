/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstring>
#include <map>
#include <set>
#include <utility>

#include "webrtc/base/random.h"
#include "webrtc/modules/video_coding/frame_object.h"
#include "webrtc/modules/video_coding/packet_buffer.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace video_coding {

class TestPacketBuffer : public ::testing::Test,
                         public OnReceivedFrameCallback {
 protected:
  TestPacketBuffer()
      : rand_(0x7732213),
        clock_(new SimulatedClock(0)),
        packet_buffer_(
            PacketBuffer::Create(clock_.get(), kStartSize, kMaxSize, this)) {}

  uint16_t Rand() { return rand_.Rand<uint16_t>(); }

  void OnReceivedFrame(std::unique_ptr<RtpFrameObject> frame) override {
    uint16_t first_seq_num = frame->first_seq_num();
    if (frames_from_callback_.find(first_seq_num) !=
        frames_from_callback_.end()) {
      ADD_FAILURE() << "Already received frame with first sequence number "
                    << first_seq_num << ".";
      return;
    }
    frames_from_callback_.insert(
        std::make_pair(frame->first_seq_num(), std::move(frame)));
  }

  enum IsKeyFrame { kKeyFrame, kDeltaFrame };
  enum IsFirst { kFirst, kNotFirst };
  enum IsLast { kLast, kNotLast };

  bool Insert(uint16_t seq_num,           // packet sequence number
              IsKeyFrame keyframe,        // is keyframe
              IsFirst first,              // is first packet of frame
              IsLast last,                // is last packet of frame
              int data_size = 0,          // size of data
              uint8_t* data = nullptr) {  // data pointer
    VCMPacket packet;
    packet.codec = kVideoCodecGeneric;
    packet.seqNum = seq_num;
    packet.frameType = keyframe ? kVideoFrameKey : kVideoFrameDelta;
    packet.isFirstPacket = first == kFirst;
    packet.markerBit = last == kLast;
    packet.sizeBytes = data_size;
    packet.dataPtr = data;

    return packet_buffer_->InsertPacket(packet);
  }

  void CheckFrame(uint16_t first_seq_num) {
    auto frame_it = frames_from_callback_.find(first_seq_num);
    ASSERT_FALSE(frame_it == frames_from_callback_.end())
        << "Could not find frame with first sequence number " << first_seq_num
        << ".";
  }

  const int kStartSize = 16;
  const int kMaxSize = 64;

  Random rand_;
  std::unique_ptr<Clock> clock_;
  rtc::scoped_refptr<PacketBuffer> packet_buffer_;
  std::map<uint16_t, std::unique_ptr<RtpFrameObject>> frames_from_callback_;
};

TEST_F(TestPacketBuffer, InsertOnePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertMultiplePackets) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, InsertDuplicatePacket) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, SeqNumWrap) {
  EXPECT_TRUE(Insert(0xFFFF, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(0x0, kKeyFrame, kFirst, kLast));

  CheckFrame(0xFFFF);
}

TEST_F(TestPacketBuffer, InsertOldPackets) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast));
  ASSERT_EQ(2UL, frames_from_callback_.size());

  frames_from_callback_.erase(seq_num + 2);
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  frames_from_callback_.erase(frames_from_callback_.find(seq_num));
  ASSERT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));

  packet_buffer_->ClearTo(seq_num + 2);
  EXPECT_FALSE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
  ASSERT_EQ(2UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, NackCount) {
  const uint16_t seq_num = Rand();

  VCMPacket packet;
  packet.codec = kVideoCodecGeneric;
  packet.seqNum = seq_num;
  packet.frameType = kVideoFrameKey;
  packet.isFirstPacket = true;
  packet.markerBit = false;
  packet.timesNacked = 0;

  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.isFirstPacket = false;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.timesNacked = 3;
  packet_buffer_->InsertPacket(packet);

  packet.seqNum++;
  packet.markerBit = true;
  packet.timesNacked = 1;
  packet_buffer_->InsertPacket(packet);

  ASSERT_EQ(1UL, frames_from_callback_.size());
  RtpFrameObject* frame = frames_from_callback_.begin()->second.get();
  EXPECT_EQ(3, frame->times_nacked());
}

TEST_F(TestPacketBuffer, FrameSize) {
  const uint16_t seq_num = Rand();
  uint8_t data[] = {1, 2, 3, 4, 5};

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast, 5, data));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast, 5, data));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kNotLast, 5, data));
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kNotFirst, kLast, 5, data));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  EXPECT_EQ(20UL, frames_from_callback_.begin()->second->size());
}

TEST_F(TestPacketBuffer, ExpandBuffer) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kStartSize + 1; ++i) {
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kLast));
  }
}

TEST_F(TestPacketBuffer, SingleFrameExpandsBuffer) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  for (int i = 1; i < kStartSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kStartSize, kKeyFrame, kNotFirst, kLast));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, ExpandBufferOverflow) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kKeyFrame, kFirst, kLast));
  EXPECT_FALSE(Insert(seq_num + kMaxSize + 1, kKeyFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, OnePacketOneFrame) {
  const uint16_t seq_num = Rand();
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, TwoPacketsTwoFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kFirst, kLast));

  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
}

TEST_F(TestPacketBuffer, TwoPacketsOneFrames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kLast));

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, ThreePacketReorderingOneFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kKeyFrame, kNotFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kKeyFrame, kNotFirst, kNotLast));

  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, Frames) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, ClearSinglePacket) {
  const uint16_t seq_num = Rand();

  for (int i = 0; i < kMaxSize; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kDeltaFrame, kFirst, kLast));

  packet_buffer_->ClearTo(seq_num);
  EXPECT_TRUE(Insert(seq_num + kMaxSize, kDeltaFrame, kFirst, kLast));
}

TEST_F(TestPacketBuffer, OneIncompleteFrame) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kDeltaFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kLast));
  EXPECT_TRUE(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
}

TEST_F(TestPacketBuffer, TwoIncompleteFramesFullBuffer) {
  const uint16_t seq_num = Rand();

  for (int i = 1; i < kMaxSize - 1; ++i)
    EXPECT_TRUE(Insert(seq_num + i, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num, kDeltaFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num - 1, kDeltaFrame, kNotFirst, kLast));

  ASSERT_EQ(0UL, frames_from_callback_.size());
}

TEST_F(TestPacketBuffer, FramesReordered) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 3, kDeltaFrame, kFirst, kLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kFirst, kLast));

  ASSERT_EQ(4UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  CheckFrame(seq_num + 1);
  CheckFrame(seq_num + 2);
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, GetBitstreamFromFrame) {
  // "many bitstream, such data" with null termination.
  uint8_t many[] = {0x6d, 0x61, 0x6e, 0x79, 0x20};
  uint8_t bitstream[] = {0x62, 0x69, 0x74, 0x73, 0x74, 0x72,
                         0x65, 0x61, 0x6d, 0x2c, 0x20};
  uint8_t such[] = {0x73, 0x75, 0x63, 0x68, 0x20};
  uint8_t data[] = {0x64, 0x61, 0x74, 0x61, 0x0};
  uint8_t
      result[sizeof(many) + sizeof(bitstream) + sizeof(such) + sizeof(data)];

  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast, sizeof(many), many));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast,
                     sizeof(bitstream), bitstream));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kNotLast,
                     sizeof(such), such));
  EXPECT_TRUE(
      Insert(seq_num + 3, kDeltaFrame, kNotFirst, kLast, sizeof(data), data));

  ASSERT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);
  EXPECT_TRUE(frames_from_callback_[seq_num]->GetBitstream(result));
  EXPECT_EQ(memcmp(result, "many bitstream, such data", sizeof(result)), 0);
}

TEST_F(TestPacketBuffer, FreeSlotsOnFrameDestruction) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  frames_from_callback_.clear();

  // Insert frame that fills the whole buffer.
  EXPECT_TRUE(Insert(seq_num + 3, kKeyFrame, kFirst, kNotLast));
  for (int i = 0; i < kMaxSize - 2; ++i)
    EXPECT_TRUE(Insert(seq_num + i + 4, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kMaxSize + 2, kKeyFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num + 3);
}

TEST_F(TestPacketBuffer, Clear) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(1UL, frames_from_callback_.size());
  CheckFrame(seq_num);

  packet_buffer_->Clear();

  EXPECT_TRUE(Insert(seq_num + kStartSize, kKeyFrame, kFirst, kNotLast));
  EXPECT_TRUE(
      Insert(seq_num + kStartSize + 1, kDeltaFrame, kNotFirst, kNotLast));
  EXPECT_TRUE(Insert(seq_num + kStartSize + 2, kDeltaFrame, kNotFirst, kLast));
  EXPECT_EQ(2UL, frames_from_callback_.size());
  CheckFrame(seq_num + kStartSize);
}

TEST_F(TestPacketBuffer, InvalidateFrameByClearing) {
  const uint16_t seq_num = Rand();

  EXPECT_TRUE(Insert(seq_num, kKeyFrame, kFirst, kLast));
  ASSERT_EQ(1UL, frames_from_callback_.size());

  packet_buffer_->Clear();
  EXPECT_FALSE(frames_from_callback_.begin()->second->GetBitstream(nullptr));
}

}  // namespace video_coding
}  // namespace webrtc
