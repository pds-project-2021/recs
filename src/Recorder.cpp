//
// Created by gabriele on 31/10/21.
//

#include "Recorder.h"


/** Initialize ffmpeg codecs and devices */
Recorder::Recorder() {

//	av_register_all(); // todo: maybe useless
//	avcodec_register_all(); // todo: maybe useless
	avdevice_register_all();

//	record_video = false;
//	frame_count = 0;

}


// todo: add params for recording: width, height, offsets
void Recorder::init() {
	format.setup_source();

	auto audioPar = format.get_source_audio_codec();
	auto videoPar = format.get_source_video_codec();

	codec.set_source_audio_parameters(audioPar);
	codec.set_source_video_parameters(videoPar);
	codec.setup_source();

	print_source_info();

	// format
	auto dest_path = "../media/output.mp4";
	format.setup_destination(dest_path);

	auto audio_codec = "aac";
	auto video_codec = "libx264"; // oppure mpeg2 per linux
	codec.find_encoders(audio_codec, video_codec);

	stream = Stream{format, codec};
	stream.get_video()->time_base = {1, 30};
	stream.get_audio()->time_base = {1, codec.inputContext.get_audio()->sample_rate};

	codec.set_destination_audio_parameters(stream.get_audio()->codecpar);
	codec.set_destination_video_parameters(stream.get_video()->codecpar);

	codec.setup_destination();

	create_out_file(dest_path);
	rescaler.scale_audio(codec);
	rescaler.scale_video(codec);

	format.write_header(options);

	print_destination_info(dest_path);

}

void Recorder::print_source_info(){
	// todo cambiare nomi device
	av_dump_format(format.inputContext.get_video(), 0 , DEFAULT_VIDEO_INPUT_DEVICE, 0);
	av_dump_format(format.inputContext.get_audio(), 0, DEFAULT_AUDIO_INPUT_DEVICE, 0);
}

void Recorder::print_destination_info(const string &dest) {
	av_dump_format(format.outputContext.get_audio(), 0, dest.c_str(), 1);
	av_dump_format(format.outputContext.get_video(), 0, dest.c_str(), 1);
}

void Recorder::create_out_file(const string &dest) {
	auto ctx = format.outputContext.get_video();

	/* create empty video file */
	if (!(ctx->flags & AVFMT_NOFILE)) {
		auto ret = avio_open2(&(ctx->pb), dest.c_str(), AVIO_FLAG_WRITE,nullptr, nullptr);
		if (ret < 0) {
			char buf[35];
			av_strerror(ret, buf, sizeof(buf));
			throw avException(buf);
		}
	}
}


