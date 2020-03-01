// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"

#include "interop/global_factory.h"
#include "local_audio_track.h"
#include "peer_connection.h"

namespace Microsoft::MixedReality::WebRTC {

LocalAudioTrack::LocalAudioTrack(
    RefPtr<GlobalFactory> global_factory,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    mrsLocalAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(std::move(global_factory), ObjectType::kLocalAudioTrack),
      track_(std::move(track)),
      interop_handle_(interop_handle),
      track_name_(track_->id()) {
  RTC_CHECK(track_);
  kind_ = mrsTrackKind::kAudioTrack;
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::LocalAudioTrack(
    RefPtr<GlobalFactory> global_factory,
    PeerConnection& owner,
    RefPtr<AudioTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::AudioTrackInterface> track,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
    mrsLocalAudioTrackInteropHandle interop_handle) noexcept
    : MediaTrack(std::move(global_factory),
                 ObjectType::kLocalAudioTrack,
                 owner),
      track_(std::move(track)),
      sender_(std::move(sender)),
      transceiver_(std::move(transceiver)),
      interop_handle_(interop_handle),
      track_name_(track_->id()) {
  RTC_CHECK(owner_);
  RTC_CHECK(transceiver_);
  RTC_CHECK(track_);
  RTC_CHECK(sender_);
  kind_ = mrsTrackKind::kAudioTrack;
  transceiver_->OnLocalTrackAdded(this);
  track_->AddSink(this);  //< FIXME - Implementation is no-op
}

LocalAudioTrack::~LocalAudioTrack() {
  track_->RemoveSink(this);
  if (owner_) {
    owner_->RemoveLocalAudioTrack(*this);
  }
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!owner_);
}

void LocalAudioTrack::SetEnabled(bool enabled) const noexcept {
  track_->set_enabled(enabled);
}

bool LocalAudioTrack::IsEnabled() const noexcept {
  return track_->enabled();
}

RefPtr<AudioTransceiver> LocalAudioTrack::GetTransceiver() const noexcept {
  return transceiver_;
}

webrtc::AudioTrackInterface* LocalAudioTrack::impl() const {
  return track_.get();
}

webrtc::RtpSenderInterface* LocalAudioTrack::sender() const {
  RTC_DCHECK(transceiver_->IsUnifiedPlan());  // sender invalid in Plan B
  return sender_.get();
}

void LocalAudioTrack::OnAddedToPeerConnection(
    PeerConnection& owner,
    RefPtr<AudioTransceiver> transceiver,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> sender) {
  RTC_CHECK(!owner_);
  RTC_CHECK(!transceiver_);
  RTC_CHECK(!sender_);
  RTC_CHECK(transceiver);
  // In Plan B the RTP sender is not always available (depends on transceiver
  // direction) so |sender| is invalid here.
  RTC_CHECK(transceiver->IsPlanB() || sender);
  owner_ = &owner;
  sender_ = std::move(sender);  // NULL in Plan B
  transceiver_ = std::move(transceiver);
  transceiver_->OnLocalTrackAdded(this);
}

void LocalAudioTrack::OnRemovedFromPeerConnection(
    PeerConnection& old_owner,
    RefPtr<AudioTransceiver> old_transceiver,
    rtc::scoped_refptr<webrtc::RtpSenderInterface> old_sender) {
  RTC_CHECK_EQ(owner_, &old_owner);
  RTC_CHECK_EQ(transceiver_.get(), old_transceiver.get());
  // In Plan B the RTP sender is not always available (depends on transceiver
  // direction) so |old_sender| is invalid here.
  RTC_CHECK(old_transceiver->IsPlanB() || (sender_ == old_sender.get()));
  owner_ = nullptr;
  sender_ = nullptr;
  transceiver_->OnLocalTrackRemoved(this);
  transceiver_ = nullptr;
}

void LocalAudioTrack::RemoveFromPeerConnection(
    webrtc::PeerConnectionInterface& peer) {
  if (transceiver_->IsUnifiedPlan()) {
    if (sender_) {
      peer.RemoveTrack(sender_);
      sender_ = nullptr;
      owner_ = nullptr;
      transceiver_->OnLocalTrackRemoved(this);
      transceiver_ = nullptr;
    }
  } else {
    transceiver_->SetTrackPlanB(nullptr);
    transceiver_->SyncSenderPlanB(false, &peer, nullptr, nullptr);
    owner_ = nullptr;
    transceiver_->OnLocalTrackRemoved(this);
    transceiver_ = nullptr;
  }
}

}  // namespace Microsoft::MixedReality::WebRTC
