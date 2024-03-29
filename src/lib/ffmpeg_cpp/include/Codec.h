
#pragma once

#include "wrapper.h"
#include "platform.h"
#include "Format.h"

class Codec {
  public:
	wrapper<AVCodec> input;
	wrapper<AVCodec> output;

	wrapper<AVCodecContext> inputContext;
	wrapper<AVCodecContext> outputContext;

	wrapper<AVCodecParameters> inputPar;
	wrapper<AVCodecParameters> outputPar;

	wrapper<AVStream> streams;

	AudioLayout audio_layout;
	uint64_t channel_layout;
	uint16_t channels;

  private:
	void source_audio_context();
	void source_video_context();
	void destination_audio_context();
	void destination_video_context();
	void find_audio_encoder(const std::string &codec_name);
	void find_video_encoder(const std::string &codec_name);

  public:
	void setup_source();
	void setup_destination();
	void find_encoders(const std::string &audio_codec, const std::string &video_codec);
	void open_streams(const Format &format);
	void reset();

	void set_audio_layout(AudioLayout);
	void set_source_audio_parameters(AVCodecParameters *par);
	void set_source_video_parameters(AVCodecParameters *par);
	void set_destination_audio_parameters(AVCodecParameters *par);
	void set_destination_video_parameters(AVCodecParameters *par);
};



