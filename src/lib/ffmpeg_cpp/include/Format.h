
/**
 * Class wrapper for Codec and Context
 */

#pragma once

#include "wrapper.h"
#include "platform.h"
#include "Screen.h"

class Format {
  public:
	wrapper<AVInputFormat> input;
	wrapper<AVDictionary> inputOptions;

	wrapper<AVOutputFormat> output;
	wrapper<AVDictionary> outputOptions;

	wrapper<AVFormatContext> inputContext;
	wrapper<AVFormatContext> outputContext;

	int videoStreamIndex = 0;
	int audioStreamIndex = 0;

	// input device
	std::string audioDevice;
	std::string videoDevice;

	AudioLayout audio_layout = MONO;

	// save screen parameters for initialization of AVDictionary
	Screen screen;

  private:
	void source_audio_context();
	void source_video_context();
	void set_screen_parameters(AVDictionary *options) const;
	void set_audio_parameters(AVDictionary *options) const;

	/* NOTE: The destination is a file, so it will have the only one context */
	void destination_context(const std::string &dest);

	void find_source_audio_stream_info();
	void find_source_video_stream_info();

  public:
	Format() = default;
	~Format() = default;
	void set_screen_params(const Screen &params);
	void set_audio_layout(AudioLayout layout);
	void setup_source();
	void setup_destination(const std::string &dest);

	void reset();

	[[nodiscard]] std::string get_audio_device() const;
	[[nodiscard]] std::string get_video_device() const;

	[[nodiscard]] AVCodecParameters *get_source_audio_codec() const;
	[[nodiscard]] AVCodecParameters *get_source_video_codec() const;

	void write_header() const;
};



