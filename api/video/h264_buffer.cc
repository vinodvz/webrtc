/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/video/h264_buffer.h"

#include <string.h>

#include <algorithm>
#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/keep_ref_until_done.h"

// Aligning pointer to 64 bytes for improved performance, e.g. use SIMD.
static const int kBufferAlignment = 64;

namespace webrtc {

H264Buffer::H264Buffer(int width,
                       int height,
                       int data_len)
    : width_(width),
      height_(height),
      data_len_(data_len),
      data_(static_cast<uint8_t*>(AlignedMalloc(data_len,kBufferAlignment))) {
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);
  RTC_DCHECK_GT(data_len, 0);
}

H264Buffer::~H264Buffer() {}

// static
rtc::scoped_refptr<H264Buffer> H264Buffer::Create(int width, int height,
                                                  int data_len) {
  return new rtc::RefCountedObject<H264Buffer>(width, height, data_len);
}

// static
rtc::scoped_refptr<H264Buffer> H264Buffer::Copy(int width,
                                                int height,
                                                const uint8_t* buffer,
                                                int buffer_len) {
  rtc::scoped_refptr<H264Buffer> h264buf = Create(width, height, buffer_len);
  memcpy((void *)h264buf->Data(),buffer,buffer_len);
  return h264buf;
}

int H264Buffer::width() const {
  return width_;
}

int H264Buffer::height() const {
  return height_;
}

VideoFrameBuffer::Type H264Buffer::type() const {
  return Type::kH264;
}

const uint8_t* H264Buffer::Data() const {
  return data_.get();
}

int H264Buffer::Data_len() const {
  return data_len_;
}

}  // namespace webrtc
