// Copyright (c) 2024 Huawei Device Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/ohos/ohos_audio_input_stream.h"

#include "base/test/task_environment.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_device_info_accessor_for_tests.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/audio_unittest_util.h"
#include "media/audio/test_audio_thread.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

const double kMaxVolume = 1.0;
const double kNewVolume = 0.2345;
const double kMutedVolume = 0.0;
const int kCallbackCount = 2;
const std::string kOutputDeviceId = "0x123";

class TestInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  TestInputCallback(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)),
        callback_count_(0),
        had_error_(0) {}
  void OnData(const AudioBus* source,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    if (!quit_closure_.is_null()) {
      ++callback_count_;
      if (callback_count_ >= kCallbackCount) {
        std::move(quit_closure_).Run();
      }
    }
  }
  void OnError() override {
    if (!quit_closure_.is_null()) {
      ++had_error_;
      std::move(quit_closure_).Run();
    }
  }
  // Returns how many times OnData() has been called. This should not be called
  // until |quit_closure_| has run.
  int callback_count() const {
    DCHECK(quit_closure_.is_null());
    return callback_count_;
  }
  // Returns how many times the OnError callback was called. This should not be
  // called until |quit_closure_| has run.
  int had_error() const {
    DCHECK(quit_closure_.is_null());
    return had_error_;
  }

 private:
  base::OnceClosure quit_closure_;
  int callback_count_;
  int had_error_;
};

}  // namespace

class OHOSAudioInputStreamTest : public testing::Test {
 public:
  OHOSAudioInputStreamTest()
      : audio_manager_(AudioManager::CreateForTesting(
            std::make_unique<TestAudioThread>())),
        input_stream_(nullptr) {
    base::RunLoop().RunUntilIdle();
  }

  OHOSAudioInputStreamTest(const OHOSAudioInputStreamTest&) = delete;
  OHOSAudioInputStreamTest& operator=(const OHOSAudioInputStreamTest&) = delete;

  ~OHOSAudioInputStreamTest() override { audio_manager_->Shutdown(); }

  void InitializeStream() {
    RunOnAudioThread(
        base::BindOnce(&OHOSAudioInputStreamTest::MakeAudioInputStream,
                       base::Unretained(this)));
  }

  void MakeAudioInputStream() {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    AudioParameters params =
        AudioDeviceInfoAccessorForTests(audio_manager_.get())
            .GetInputStreamParameters(AudioDeviceDescription::kDefaultDeviceId);
    input_stream_ = audio_manager_->MakeAudioInputStream(
        params, AudioDeviceDescription::kDefaultDeviceId,
        base::BindRepeating(&OHOSAudioInputStreamTest::OnLogMessage,
                            base::Unretained(this)));
  }
  void OnLogMessage(const std::string& message) {}

  void RunOnAudioThread(base::OnceClosure closure) {
    DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
    std::move(closure).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<AudioManager> audio_manager_;
  raw_ptr<AudioInputStream> input_stream_;
  bool InputDevicesAvailable() {
    return AudioDeviceInfoAccessorForTests(audio_manager_.get())
        .HasAudioInputDevices();
  }
};

// Test create, open, stop and close of an AudioInputStream without recording.
TEST_F(OHOSAudioInputStreamTest, OpenStopAndClose) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  ASSERT_TRUE(input_stream_);
  EXPECT_EQ(input_stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);
  input_stream_->Stop();
  input_stream_->Close();
  input_stream_ = nullptr;
}

// Test a normal recording sequence using an AudioInputStream.
// Very simple test which starts capturing and verifies that recording starts.
TEST_F(OHOSAudioInputStreamTest, InitializeAndStart) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();

  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  ASSERT_TRUE(input_stream_);
  EXPECT_EQ(input_stream_->Open(), AudioInputStream::OpenOutcome::kSuccess);

  base::RunLoop run_loop;
  TestInputCallback test_callback(run_loop.QuitClosure());
  input_stream_->Start(&test_callback);

  run_loop.Run();
  EXPECT_GE(test_callback.callback_count(), kCallbackCount);
  EXPECT_FALSE(test_callback.had_error());

  input_stream_->Stop();
  input_stream_->Close();
  input_stream_ = nullptr;
}

TEST_F(OHOSAudioInputStreamTest, GetMaxVolume) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  EXPECT_EQ(input_stream_->GetMaxVolume(), kMaxVolume);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, SetVolumeAndGetVolume) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  input_stream_->SetVolume(kNewVolume);
  EXPECT_EQ(input_stream_->GetVolume(), kNewVolume);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControlTrue) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  EXPECT_EQ(input_stream_->SetAutomaticGainControl(true), true);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, SetAutomaticGainControlFalse) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  EXPECT_EQ(input_stream_->SetAutomaticGainControl(false), true);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, GetAutomaticGainControl_True) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  EXPECT_EQ(input_stream_->SetAutomaticGainControl(true), true);
  EXPECT_EQ(input_stream_->GetAutomaticGainControl(), true);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, GetAutomaticGainControl_False) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  EXPECT_EQ(input_stream_->SetAutomaticGainControl(false), true);
  EXPECT_EQ(input_stream_->GetAutomaticGainControl(), false);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, SetVolumeAndIsMuted) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  input_stream_->SetVolume(kMutedVolume);
  EXPECT_EQ(input_stream_->IsMuted(), true);
  input_stream_->Close();
}

TEST_F(OHOSAudioInputStreamTest, SetOutputDeviceForAec) {
  ABORT_AUDIO_TEST_IF_NOT(InputDevicesAvailable());
  InitializeStream();
  input_stream_->SetOutputDeviceForAec(kOutputDeviceId);
  input_stream_->Close();
}

}  // namespace media
