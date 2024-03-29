
#include "ffmpeg_cpp.h"

/**
 * Decode a packet and write a frame
 *
 * @param avctx Context
 * @param pkt Packet to decode
 * @param frame Frame to write
 * @param got_frame Control flag
 * @return 0 if OK, -1 if an error occurred
 */
int decode(AVCodecContext *avctx, AVPacket *pkt, AVFrame *frame, int *got_frame) {
	int result;
	*got_frame = 0;

	if (pkt != nullptr) {
		// Send packet to decoder
		result = avcodec_send_packet(avctx, pkt);

		// In particular, we don't expect AVERROR(EAGAIN), because we read all
		// decoded frames with avcodec_receive_frame() until done.

		// Check result
		if (result < 0 && result != AVERROR_EOF) {
			// Decoder error
			throw avException("Failed to send packet to decoder");
		} else if (result >= 0) {
			result = avcodec_receive_frame(avctx, frame); // Try to get a decoded

			if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
				return result;
			} else if (result >= 0) {
				*got_frame = 1;
				return 0;
			}
		}
	}
	return -1;
}

/**
 * Encode a frame and write a packet
 *
 * @param avctx Context
 * @param pkt Packet to write
 * @param frame Frame to encode
 * @param got_frame Control flag
 * @return 0 if OK, -1 if an error occurred
 */
int encode(AVCodecContext *avctx, AVPacket *pkt, AVFrame *frame, int *got_packet) {
	int result;
	*got_packet = 0;

	// Send frame to encoder
	result = avcodec_send_frame(avctx, frame);

	if (result < 0) {
		if (result == AVERROR(EAGAIN)) { // Buffer is full
			int ret;

			while (result == AVERROR(EAGAIN)) { // while encoder buffer is full
				ret = avcodec_receive_packet(avctx, pkt); // Try to receive packet
				result = avcodec_send_frame(avctx, frame);
			}
			result = ret;
		} else {
			throw avException("Failed to send frame to encoder"); // Error ending frame to encoder
		}
	} else {
		result = avcodec_receive_packet(avctx, pkt); // Try to receive packet
	}
	if (result >= 0) {
		*got_packet = 1;
		return result;
	} else if (result == AVERROR(EAGAIN))
		return 0;
	else {
		throw avException("Failed to receive frame from encoder"); // Error ending frame to encoder
	}
}

/**
 * Encode a frame and write a packet
 *
 * @param avctoutputFormatContextx Context
 * @param outPacket Packet to write
 * @param wR Access mutex
 */
void writeFrameToOutput(AVFormatContext *outputFormatContext,
                        AVPacket *outPacket,
                        std::mutex *wR) {
	if (outputFormatContext == nullptr || outPacket == nullptr) {
		throw avException("Provided null output values, data could not be written");
	}
    //write to file using mutex
	std::lock_guard<std::mutex> lg(*wR);
	auto res = av_interleaved_write_frame(outputFormatContext, outPacket); // Write packet to file
	if (res < 0) {
		throw avException("Error in writing media frame");
	}
}

/**
 * Encode a frame and write a packet
 *
 * @param swsContext Scaler context
 * @param outputCodecContext Output codec context
 * @param inputCodecContext Input codec context
 * @param videoStream Input video stream
 * @param outputFormatContext Output format context
 * @param frame Frame to rescale, encode and write
 * @param pts_p Frame counter
 * @param wR File write mutex
 * @param r Frame resync mutex
 * @param mx_pts Max pts value
 * @param mn_pts Min pts value
 * @param paused Pause state atomic variable
 * @param resync Resync enabled variable
 */
void
convertAndWriteVideoFrame(SwsContext *swsContext, AVCodecContext *outputCodecContext, AVCodecContext *inputCodecContext,
                          AVStream *videoStream, AVFormatContext *outputFormatContext, AVFrame *frame,
                          int64_t *pts_p, std::mutex *wR, std::mutex *r, int64_t *mx_pts, int64_t *mn_pts,
                          const bool *paused, bool resync) {

	int64_t min_pts = 0;
	int64_t max_pts = 0;
    //check resync values if enabled
	if (resync) {
		std::lock_guard<std::mutex> rl(*r);
		min_pts = *mn_pts;
		max_pts = *mx_pts;
	}
    //init data structures
	auto out_frame =
		Frame{outputCodecContext->width, outputCodecContext->height, (AVPixelFormat) outputCodecContext->pix_fmt, 32};
	auto out_packet = Packet{};
	auto outputFrame = out_frame.into();
	auto outputPacket = out_packet.into();

	int got_packet = 0;

	// Convert frame picture format
	sws_scale(swsContext,
	          frame->data,
	          frame->linesize,
	          0,
	          inputCodecContext->height,
	          outputFrame->data,
	          outputFrame->linesize);

	//pts resync to current min pts value
	if (resync) {
		auto pts_test = av_rescale_q(min_pts, {1, PTS_SYNC_MULTIPLIER}, outputCodecContext->time_base);
		if (*pts_p < pts_test + 1) *pts_p = pts_test + 1;
	}
	outputFrame->pts = *pts_p - 1;

	// Send converted frame to encoder
	encode(outputCodecContext, outputPacket, outputFrame, &got_packet);

	// Frame was sent successfully
	if (got_packet > 0) { // Packet received successfully
		if (outputPacket->pts != AV_NOPTS_VALUE) {
			outputPacket->pts = av_rescale_q(outputPacket->pts, outputCodecContext->time_base, videoStream->time_base);
		}
		if (outputPacket->dts != AV_NOPTS_VALUE) {
			outputPacket->dts = av_rescale_q(outputPacket->dts, outputCodecContext->time_base, videoStream->time_base);
		}
		outputPacket->duration = av_rescale_q(1, outputCodecContext->time_base, videoStream->time_base);

		if (resync) {
			auto curr_pts = (outputPacket->pts * PTS_SYNC_MULTIPLIER / videoStream->time_base.den);
			if (curr_pts > max_pts) max_pts = curr_pts;
		}
		// Write packet to file
		writeFrameToOutput(outputFormatContext, outputPacket, wR);
	}

    //update resync values if enabled
	if (resync) {
        std::lock_guard<std::mutex> rl(*r);
		if(max_pts > *mx_pts) *mx_pts = max_pts;
		if (*paused) *mn_pts = *mx_pts;
	}
}

/**
 * Encode a frame and write a packet
 *
 * @param outputCodecContext Output codec context
 * @param videoStream Input video stream
 * @param outputFormatContext Output format context
 * @param wR File write mutex
 */
void convertAndWriteDelayedVideoFrames(AVCodecContext *outputCodecContext, AVStream *videoStream,
                                       AVFormatContext *outputFormatContext, std::mutex *wR) {

	while (true) {
		auto out_packet = Packet{};
		auto outPacket = out_packet.into();
        //empty frame queue
		avcodec_send_frame(outputCodecContext, nullptr);

		if (avcodec_receive_packet(outputCodecContext, outPacket) == 0) { // Try to get packet
            //set correct output pts values
			if (outPacket->pts != AV_NOPTS_VALUE) {
				outPacket->pts =
					av_rescale_q(outPacket->pts, outputCodecContext->time_base,
					             videoStream->time_base);
			}
			if (outPacket->dts != AV_NOPTS_VALUE) {
				outPacket->dts =
					av_rescale_q(outPacket->dts, outputCodecContext->time_base,
					             videoStream->time_base);
			}
			outPacket->duration = av_rescale_q(1, outputCodecContext->time_base,
			                                   videoStream->time_base);
            //write frame to file
			writeFrameToOutput(outputFormatContext, outPacket, wR);
		} else { // No remaining frames to handle
			break;
		}
	}
}

/**
 * Encode a frame and write a packet
 *
 * @param swrContext Resampler context
 * @param outputCodecContext Output codec context
 * @param inputCodecContext Input codec context
 * @param audioStream Input audio stream
 * @param outputFormatContext Output format context
 * @param frame Frame to resample, encode and write
 * @param pts_p Frame counter
 * @param wR File write mutex
 * @param r Frame resync mutex
 * @param mx_pts Max pts value
 * @param mn_pts Min pts value
 * @param paused Pause state atomic variable
 * @param resync Resync enabled variable
 */
void convertAndWriteAudioFrames(SwrContext *swrContext,
                                AVCodecContext *outputCodecContext,
                                AVCodecContext *inputCodecContext,
                                AVStream *audioStream,
                                AVFormatContext *outputFormatContext,
                                AVFrame *frame,
                                int64_t *pts_p,
                                std::mutex *wR,
                                std::mutex *r,
                                int64_t *mx_pts,
                                int64_t *mn_pts,
                                const bool *paused,
                                bool resync) {

	int64_t min_pts = 0;
	int64_t max_pts = 0;
    //check resync values if enabled
	if (resync) {
		std::lock_guard<std::mutex> rl(*r);
		min_pts = *mn_pts;
		max_pts = *mx_pts;
	}

	auto out_frame =
		Frame{outputCodecContext->frame_size, outputCodecContext->sample_fmt, outputCodecContext->channel_layout, 0};
	auto outputFrame = out_frame.into();
	auto out_packet = Packet{};
	auto outputPacket = out_packet.into();

	AVRational bq = {1, inputCodecContext->sample_rate * inputCodecContext->channels};

	int got_packet = 0;
	int got_samples = swr_convert(swrContext,
	                              outputFrame->data,
	                              outputFrame->nb_samples,
	                              (const uint8_t **) frame->data,
	                              frame->nb_samples);
	if (got_samples < 0) {
		throw avException("error: swr_convert()");
	}

	*pts_p += got_samples;
	if (resync) {
		auto pts_test = av_rescale_q(*pts_p, bq, {1, PTS_SYNC_MULTIPLIER});
		if (pts_test < min_pts) *pts_p = av_rescale_q(min_pts, {1, PTS_SYNC_MULTIPLIER}, bq);
	}
	outputFrame->nb_samples = got_samples;
	outputFrame->pts = *pts_p;

	encode(outputCodecContext, outputPacket, outputFrame, &got_packet);

	// Frame was sent successfully
	if (got_packet > 0) { // Packet received successfully
        //set correct output pts values
		if (outputPacket->pts != AV_NOPTS_VALUE) {
			outputPacket->pts = av_rescale_q(outputPacket->pts, bq, audioStream->time_base);
		}

		if (outputPacket->dts != AV_NOPTS_VALUE) {
			outputPacket->dts = av_rescale_q(outputPacket->dts, bq, audioStream->time_base);
		}

		outputPacket->duration = av_rescale_q(outputCodecContext->frame_size, bq, audioStream->time_base);

		if (resync) {
			auto curr_pts = (outputPacket->pts * PTS_SYNC_MULTIPLIER / audioStream->time_base.den);
			if (curr_pts > max_pts) max_pts = curr_pts;
		}
		// Write packet to file
		outputPacket->stream_index = 1;
		writeFrameToOutput(outputFormatContext, outputPacket, wR);
	}

	while (swr_get_out_samples(swrContext, 0) >= outputCodecContext->frame_size) {
		got_samples = swr_convert(swrContext, outputFrame->data, outputFrame->nb_samples, nullptr, 0);
		*pts_p += got_samples;
		//outputFrame->nb_samples=got_samples;
		outputFrame->pts = *pts_p;

		encode(outputCodecContext, outputPacket, outputFrame, &got_packet);

		// Frame was sent successfully
		if (got_packet > 0) { // Packet received successfully
            //set correct output pts values
			if (outputPacket->pts != AV_NOPTS_VALUE) {
				outputPacket->pts = av_rescale_q(outputPacket->pts, bq, audioStream->time_base);
			}
			if (outputPacket->dts != AV_NOPTS_VALUE) {
				outputPacket->dts = av_rescale_q(outputPacket->dts, bq, audioStream->time_base);
			}
			outputPacket->duration = av_rescale_q(outputCodecContext->frame_size, bq, audioStream->time_base);

			if (resync) {
				auto curr_pts = (outputPacket->pts * PTS_SYNC_MULTIPLIER / audioStream->time_base.den);
				if (curr_pts > max_pts) max_pts = curr_pts;
			}
			// Write packet to file
			outputPacket->stream_index = 1;
			writeFrameToOutput(outputFormatContext, outputPacket, wR);
		}
	}

    //update resync values if enabled
    if (resync) {
        std::lock_guard<std::mutex> rl(*r);
        if(max_pts > *mx_pts) *mx_pts = max_pts;
        if (*paused) *mn_pts = *mx_pts;
    }
}

/**
 * Encode a frame and write a packet
 *
 * @param inputCodecContext Input codec context
 * @param outputCodecContext Output codec context
 * @param audioStream Input audio stream
 * @param outputFormatContext Output format context
 * @param finalSize Final frame sample size
 * @param wR File write mutex
 */
void convertAndWriteDelayedAudioFrames(AVCodecContext *inputCodecContext, AVCodecContext *outputCodecContext,
                                       AVStream *audioStream, AVFormatContext *outputFormatContext, int finalSize,
                                       std::mutex *wR) {
	auto out_packet = Packet{};
	auto outPacket = out_packet.into();
	auto next_packet = Packet{};
	auto nextOutPacket = next_packet.into();

	AVPacket *prevPacket;
	AVRational bq = {1, inputCodecContext->sample_rate * inputCodecContext->channels};
	if (finalSize == 0) finalSize = outputCodecContext->frame_size;
	avcodec_send_frame(outputCodecContext, nullptr);
	auto receive = avcodec_receive_packet(outputCodecContext, outPacket);
	while (receive >= 0) {// Try to get packet
        //empty frame queue
		avcodec_send_frame(outputCodecContext, nullptr);
		receive = avcodec_receive_packet(outputCodecContext, nextOutPacket);

        //set correct output pts values
		if (outPacket->pts != AV_NOPTS_VALUE) {
			outPacket->pts = av_rescale_q(outPacket->pts, bq, audioStream->time_base);
		}
		if (outPacket->dts != AV_NOPTS_VALUE) {
			outPacket->dts = av_rescale_q(outPacket->dts, bq, audioStream->time_base);
		}
		outPacket->stream_index = 1;

		if (receive >= 0) {//if this is not the last packet
			//frame size equals codec frame size
			outPacket->duration = av_rescale_q(outputCodecContext->frame_size, bq, audioStream->time_base);

			// Write packet to file
			writeFrameToOutput(outputFormatContext, outPacket, wR);
			prevPacket = outPacket;
			outPacket = nextOutPacket;
			nextOutPacket = prevPacket;
		} else {//if this is the last packet

			//frame size equals finalSize
			outPacket->duration = av_rescale_q(finalSize, bq, audioStream->time_base);
			// Write packet to file
			writeFrameToOutput(outputFormatContext, outPacket, wR);

			log_debug("Final pts value is: " + std::to_string(outPacket->pts));
		}
	}
}

/**
 * Encode a frame and write a packet
 *
 * @param swrContext Resampler context
 * @param outputCodecContext Output codec context
 * @param inputCodecContext Input codec context
 * @param audioStream Input audio stream
 * @param outputFormatContext Output format context
 * @param pts_p Frame counter
 * @param wR File write mutex
 */
void convertAndWriteLastAudioFrames(SwrContext *swrContext, AVCodecContext *outputCodecContext,
                                    AVCodecContext *inputCodecContext, AVStream *audioStream,
                                    AVFormatContext *outputFormatContext, int64_t *pts_p, std::mutex *wR) {

	auto out_packet = Packet{};
	auto outputPacket = out_packet.into();
	auto out_frame = Frame{};
	auto outputFrame = out_frame.into();
	AVRational bq = {1, inputCodecContext->sample_rate * inputCodecContext->channels};

	int got_packet = 0;
	//handle remaining samples < default frame size in resampler queue
	int got_samples = swr_convert(swrContext, outputFrame->data, outputFrame->nb_samples, nullptr, 0);
	if (got_samples < 0) {
		throw avException("swr_convert() error on last frame");
	} else if (got_samples > 0) {
		*pts_p += got_samples;
		outputFrame->nb_samples = got_samples;
		outputFrame->pts = *pts_p;

		encode(outputCodecContext, outputPacket, outputFrame, &got_packet);

		// Frame was sent successfully
		if (got_packet > 0) { // Packet received successfully
            //set correct output pts values
			if (outputPacket->pts != AV_NOPTS_VALUE) {
				outputPacket->pts = av_rescale_q(outputPacket->pts, bq, audioStream->time_base);
			}
			if (outputPacket->dts != AV_NOPTS_VALUE) {
				outputPacket->dts = av_rescale_q(outputPacket->dts, bq, audioStream->time_base);
			}
			outputPacket->duration = av_rescale_q(outputFrame->nb_samples, bq, audioStream->time_base);

			// Write packet to file
			outputPacket->stream_index = 1;
			auto result = av_write_frame(outputFormatContext, outputPacket);
			if (result != 0) {
				throw avException("Error in writing audio frame");
			}
		} else std::cout << "Last not delayed audio frame could not be encoded successfully" << std::endl;

		convertAndWriteDelayedAudioFrames(inputCodecContext,
		                                  outputCodecContext,
		                                  audioStream,
		                                  outputFormatContext,
		                                  got_samples,
		                                  wR);
	} else {
		convertAndWriteDelayedAudioFrames(inputCodecContext,
		                                  outputCodecContext,
		                                  audioStream,
		                                  outputFormatContext,
		                                  got_samples,
		                                  wR);
	}
}

