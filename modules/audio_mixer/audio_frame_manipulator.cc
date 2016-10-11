/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_mixer/audio_frame_manipulator.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/include/audio_frame_operations.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace {
// Linear ramping over 80 samples.
// TODO(hellner): ramp using fix point?
const float kRampArray[] = {
    0.0000f, 0.0127f, 0.0253f, 0.0380f, 0.0506f, 0.0633f, 0.0759f, 0.0886f,
    0.1013f, 0.1139f, 0.1266f, 0.1392f, 0.1519f, 0.1646f, 0.1772f, 0.1899f,
    0.2025f, 0.2152f, 0.2278f, 0.2405f, 0.2532f, 0.2658f, 0.2785f, 0.2911f,
    0.3038f, 0.3165f, 0.3291f, 0.3418f, 0.3544f, 0.3671f, 0.3797f, 0.3924f,
    0.4051f, 0.4177f, 0.4304f, 0.4430f, 0.4557f, 0.4684f, 0.4810f, 0.4937f,
    0.5063f, 0.5190f, 0.5316f, 0.5443f, 0.5570f, 0.5696f, 0.5823f, 0.5949f,
    0.6076f, 0.6203f, 0.6329f, 0.6456f, 0.6582f, 0.6709f, 0.6835f, 0.6962f,
    0.7089f, 0.7215f, 0.7342f, 0.7468f, 0.7595f, 0.7722f, 0.7848f, 0.7975f,
    0.8101f, 0.8228f, 0.8354f, 0.8481f, 0.8608f, 0.8734f, 0.8861f, 0.8987f,
    0.9114f, 0.9241f, 0.9367f, 0.9494f, 0.9620f, 0.9747f, 0.9873f, 1.0000f};
const size_t kRampSize = sizeof(kRampArray) / sizeof(kRampArray[0]);
}  // namespace

uint32_t AudioMixerCalculateEnergy(const AudioFrame& audio_frame) {
  uint32_t energy = 0;
  for (size_t position = 0; position < audio_frame.samples_per_channel_;
       position++) {
    // TODO(aleloi): This can overflow. Convert to floats.
    energy += audio_frame.data_[position] * audio_frame.data_[position];
  }
  return energy;
}

void NewMixerRampIn(AudioFrame* audio_frame) {
  assert(kRampSize <= audio_frame->samples_per_channel_);
  for (size_t i = 0; i < kRampSize; i++) {
    audio_frame->data_[i] =
        static_cast<int16_t>(kRampArray[i] * audio_frame->data_[i]);
  }
}

void NewMixerRampOut(AudioFrame* audio_frame) {
  assert(kRampSize <= audio_frame->samples_per_channel_);
  for (size_t i = 0; i < kRampSize; i++) {
    const size_t kRampPos = kRampSize - 1 - i;
    audio_frame->data_[i] =
        static_cast<int16_t>(kRampArray[kRampPos] * audio_frame->data_[i]);
  }
  memset(&audio_frame->data_[kRampSize], 0,
         (audio_frame->samples_per_channel_ - kRampSize) *
             sizeof(audio_frame->data_[0]));
}

void RemixFrame(size_t target_number_of_channels, AudioFrame* frame) {
  RTC_DCHECK_GE(target_number_of_channels, static_cast<size_t>(1));
  RTC_DCHECK_LE(target_number_of_channels, static_cast<size_t>(2));
  if (frame->num_channels_ == 1 && target_number_of_channels == 2) {
    AudioFrameOperations::MonoToStereo(frame);
  } else if (frame->num_channels_ == 2 && target_number_of_channels == 1) {
    AudioFrameOperations::StereoToMono(frame);
  }
}

}  // namespace webrtc
