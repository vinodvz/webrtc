/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_

namespace webrtc {
namespace testing {
namespace bwe {
class CongestionWindow {
 public:
  void set_gain(float gain);
  size_t data_inflight();
  int64_t GetCongestionWindow();

  // Packet sent from sender, meaning it is inflight
  // until we receive it and we should add packet's size to data_inflight.
  void PacketSent();

  // Ack was received by sender, meaning
  // packet is no longer inflight.
  void AckReceived();
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_TEST_ESTIMATORS_CONGESTION_WINDOW_H_
