/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/api/test/fakevideotrackrenderer.h"
#include "webrtc/api/videocapturertracksource.h"
#include "webrtc/api/videotrack.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/fakevideocapturer.h"
#include "webrtc/media/base/fakemediaengine.h"
#include "webrtc/media/engine/webrtcvideoframe.h"

using webrtc::FakeVideoTrackRenderer;
using webrtc::FakeVideoTrackRendererOld;
using webrtc::VideoTrackSource;
using webrtc::VideoTrack;
using webrtc::VideoTrackInterface;


class VideoTrackTest : public testing::Test {
 public:
  VideoTrackTest() {
    static const char kVideoTrackId[] = "track_id";
    video_track_ = VideoTrack::Create(
        kVideoTrackId,
        new rtc::RefCountedObject<VideoTrackSource>(
            &capturer_, rtc::Thread::Current(), true /* remote */));
    capturer_.Start(
        cricket::VideoFormat(640, 480, cricket::VideoFormat::FpsToInterval(30),
                             cricket::FOURCC_I420));
  }

 protected:
  cricket::FakeVideoCapturer capturer_;
  rtc::scoped_refptr<VideoTrackInterface> video_track_;
};

// Test adding renderers to a video track and render to them by providing
// frames to the source.
TEST_F(VideoTrackTest, RenderVideo) {
  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer_1(
      new FakeVideoTrackRenderer(video_track_.get()));

  capturer_.CaptureFrame();
  EXPECT_EQ(1, renderer_1->num_rendered_frames());

  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer_2(
      new FakeVideoTrackRenderer(video_track_.get()));
  capturer_.CaptureFrame();
  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(1, renderer_2->num_rendered_frames());

  video_track_->RemoveSink(renderer_1.get());
  capturer_.CaptureFrame();
  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(2, renderer_2->num_rendered_frames());
}

// Test adding renderers to a video track and render to them by
// providing frames to the source. Uses the old VideoTrack interface
// with AddRenderer and RemoveRenderer.
TEST_F(VideoTrackTest, RenderVideoOld) {
  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRendererOld> renderer_1(
      new FakeVideoTrackRendererOld(video_track_.get()));

  capturer_.CaptureFrame();
  EXPECT_EQ(1, renderer_1->num_rendered_frames());

  // FakeVideoTrackRenderer register itself to |video_track_|
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer_2(
      new FakeVideoTrackRenderer(video_track_.get()));

  capturer_.CaptureFrame();
  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(1, renderer_2->num_rendered_frames());

  video_track_->RemoveRenderer(renderer_1.get());
  capturer_.CaptureFrame();
  EXPECT_EQ(2, renderer_1->num_rendered_frames());
  EXPECT_EQ(2, renderer_2->num_rendered_frames());
}

// Test that disabling the track results in blacked out frames.
TEST_F(VideoTrackTest, DisableTrackBlackout) {
  rtc::scoped_ptr<FakeVideoTrackRenderer> renderer(
      new FakeVideoTrackRenderer(video_track_.get()));

  capturer_.CaptureFrame();
  EXPECT_EQ(1, renderer->num_rendered_frames());
  EXPECT_FALSE(renderer->black_frame());

  video_track_->set_enabled(false);
  capturer_.CaptureFrame();
  EXPECT_EQ(2, renderer->num_rendered_frames());
  EXPECT_TRUE(renderer->black_frame());

  video_track_->set_enabled(true);
  capturer_.CaptureFrame();
  EXPECT_EQ(3, renderer->num_rendered_frames());
  EXPECT_FALSE(renderer->black_frame());
}
