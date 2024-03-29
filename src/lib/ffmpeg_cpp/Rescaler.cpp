//
// Created by gabriele on 21/11/21.
//

#include "Rescaler.h"

Rescaler::~Rescaler() {
	if (swrCtx != nullptr) {
		swr_free(&swrCtx);
	}
	if (swsCtx != nullptr) {
		sws_freeContext(swsCtx);
	}
}

void Rescaler::set_video_scaler(const Codec &codec) {
	if (swsCtx != nullptr) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}

	swsCtx = sws_getCachedContext(swsCtx,
	                              codec.inputPar.get_video()->width,
	                              codec.inputPar.get_video()->height,
	                              (AVPixelFormat) codec.inputPar.get_video()->format,
	                              codec.outputPar.get_video()->width,
	                              codec.outputPar.get_video()->height,
	                              (AVPixelFormat) codec.outputPar.get_video()->format,
	                              SWS_BICUBIC,
	                              nullptr,
	                              nullptr,
	                              nullptr);
	if (!swsCtx) {
		throw avException("Impossible to create scale context for video conversion");
	}
}

void Rescaler::set_audio_scaler(const Codec &codec) {
	if (audio_layout == NONE) return;

	if (swrCtx != nullptr) {
		swr_free(&swrCtx);
		swrCtx = nullptr;
	}

	swrCtx = swr_alloc_set_opts(
		swrCtx, (int64_t) codec.outputContext.get_audio()->channel_layout,
		codec.outputContext.get_audio()->sample_fmt, codec.outputContext.get_audio()->sample_rate,
		(int64_t) codec.inputContext.get_audio()->channel_layout,
		codec.inputContext.get_audio()->sample_fmt, codec.inputContext.get_audio()->sample_rate,
		0, nullptr);
	if (!swrCtx) {
		throw avException("Impossible to create resample context for audio conversion");
	}

	auto res = swr_init(swrCtx);
	if (res < 0) {
		throw avException("Impossible to initialize resample context for audio conversion");
	}
}

void Rescaler::set_audio_layout(AudioLayout layout) {
	audio_layout = layout;
}

SwsContext *Rescaler::get_sws() {
	return swsCtx;
}

SwrContext *Rescaler::get_swr() {
	return swrCtx;
}

void Rescaler::reset() {
	if (swsCtx != nullptr) {
		sws_freeContext(swsCtx);
		swsCtx = nullptr;
	}
	if (swrCtx != nullptr) {
		swr_free(&swrCtx);
		swrCtx = nullptr;
	}
}