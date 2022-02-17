#pragma once

#include "ffmpeg_cpp.h"
#include <thread>

class Recorder {
  private:
    std::unique_ptr<Codec> codec = nullptr;
    Dictionary options;
    std::unique_ptr<Format> format = nullptr;
    std::unique_ptr<Rescaler> rescaler = nullptr;
    std::unique_ptr<Stream> stream = nullptr;

	Screen screen;

	// output codec
	enum AudioLayout audio_layout = MONO;
	std::string audio_codec = DEFAULT_AUDIO_CODEC;
	std::string video_codec = DEFAULT_VIDEO_CODEC;
	std::string destination_path = get_default_path();
    std::string err_string;

    std::thread th_audio_demux;
	std::thread th_audio_convert;
	std::thread th_video_demux;
	std::thread th_video_convert;
	std::thread th_video;
	std::thread th_audio;

	int64_t ref_time = 0;
	unsigned int num_core = std::thread::hardware_concurrency();

	// action variable for pause and terminate
	std::atomic<bool> stopped = false;
	std::atomic<bool> pausedVideo = false;
	std::atomic<bool> pausedAudio = false;
	std::atomic<bool> finishedVideoDemux = false;
	std::atomic<bool> finishedAudioDemux = false;
    bool rec_error = false;

    std::atomic<bool> capturing = false;

	std::mutex vD;
	std::mutex aD;
	std::mutex wR;
    std::mutex eM;

	std::condition_variable videoCnv;
	std::condition_variable audioCnv;

	/* private functions */

	void init();
    void reset();
	void join_all();
	void create_out_file(const std::string &dest) const;
    void handle_rec_error(const std::string& th_name, const unsigned int& th_num, const char* what = nullptr);
	// single thread (de)muxing
	void CaptureAudioFrames();
	void CaptureVideoFrames();

	// multithread (de)muxing
	void ConvertAudioFrames();
	void DemuxAudioInput();
	void ConvertVideoFrames();
	void DemuxVideoInput();

  public:
	Recorder();
	~Recorder();

//    TODO: da sistemare
//    Recorder& operator=(const Recorder& source);
//    Recorder& operator=(Recorder&& source) noexcept;

	// recorder parameters functions
	[[maybe_unused]] enum AudioLayout get_audio_layout();
	[[maybe_unused]] void set_audio_layout(enum AudioLayout layout);

	[[maybe_unused]] std::string get_audio_codec();
	[[maybe_unused]] std::string get_video_codec();

    [[maybe_unused]] void set_audio_codec(const std::string &cod);
    [[maybe_unused]] void set_video_codec(const std::string &cod);

	[[maybe_unused]] void set_destination(const std::string &);
	std::string get_destination();

	void set_screen_params(const Screen &params);
	[[maybe_unused]] Screen get_screen_params();

	[[maybe_unused]] void set_threads(unsigned int th);

	// recorder functions
	void capture();
	void pause();
	void resume();
	void terminate();

	bool is_paused();
	bool is_capturing();

    std::string get_exec_error(bool&);

	// log functions
	[[maybe_unused]] void print_source_info();
	[[maybe_unused]] void print_destination_info(const std::string &dest) const;
};

