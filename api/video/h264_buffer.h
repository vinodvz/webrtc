/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_H264_BUFFER_H_
#define API_VIDEO_H264_BUFFER_H_

#include <stdint.h>
#include <memory>

#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/memory/aligned_malloc.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Encoded H264 buffer in standard memory.
class RTC_EXPORT H264Buffer : public H264BufferInterface {
 public:
  static rtc::scoped_refptr<H264Buffer> Create(int width,
                                               int height,
                                               int data_len);

  static rtc::scoped_refptr<H264Buffer> Copy(int width,
                                             int height,
                                             const uint8_t* buffer,
                                             int buffer_len);
  Type type() const override;

  // The resolution of the frame in pixels. For formats where some planes are
  // subsampled, this is the highest-resolution plane.
  int width() const override;
  int height() const override;

  // Returns a memory-backed frame buffer in I420 format. If the pixel data is
  // in another format, a conversion will take place. All implementations must
  // provide a fallback to I420 for compatibility with e.g. the internal WebRTC
  // software encoders.
  rtc::scoped_refptr<I420BufferInterface> ToI420() override;

  const uint8_t* Data() const override;
  int Data_len() const override;

 protected:
  H264Buffer(int width, int height, int data_len);

  ~H264Buffer() override;

 private:
  const int width_;
  const int height_;
  const int data_len_;
  const std::unique_ptr<uint8_t, AlignedFreeDeleter> data_;
};

}  // namespace webrtc

#endif  // API_VIDEO_H264_BUFFER_H_
