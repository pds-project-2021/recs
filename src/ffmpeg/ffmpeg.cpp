//
// Created by gabriele on 23/12/21.
//

#include "ffmpeg.h"

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
			// frame
			if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
				return result;
			}
			else if (result >= 0) {
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


void writeFrameToOutput(AVFormatContext *outputFormatContext,
                        AVPacket *outPacket,
                        mutex *wR,
                        condition_variable *writeFrame) {
	int result = 0;
	if (outputFormatContext == nullptr || outPacket == nullptr) {
		throw avException("Provided null output values, data could not be written");
	}
	if (wR->try_lock()) {
		result = av_interleaved_write_frame(outputFormatContext,outPacket); // Write packet to file
		if (result < 0) throw avException("Error in writing video frame");
		else writeFrame->notify_one(); // Notify other writer thread to resume if halted
		wR->unlock();
	} else { //could not acquire lock, wait and retry
		unique_lock<mutex> ul(*wR);
//        writeFrame->notify_one();// Notify other writer thread to resume if halted
		writeFrame->wait(ul);// Wait for resume signal
		result = av_interleaved_write_frame(outputFormatContext,
		                                    outPacket); // Write packet to file
		if (result < 0) throw avException("Error in writing video frame");
		else writeFrame->notify_one(); // Notify other writer thread to resume if halted
	}
}

int convertAndWriteVideoFrame(SwsContext *swsContext,
                              AVCodecContext *outputCodecContext,
                              AVCodecContext *inputCodecContext,
                              AVStream *videoStream,
                              AVFormatContext *outputFormatContext,
                              AVFrame *frame,
                              const int64_t *pts_p,
                              mutex *wR,
                              condition_variable *writeFrame) {

	auto out_frame =  Frame{outputCodecContext->width, outputCodecContext->height,(AVPixelFormat) outputCodecContext->pix_fmt, 32};
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

	// Send converted frame to encoder
	outputFrame->pts = *pts_p - 1;

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

		// Write packet to file
		writeFrameToOutput(outputFormatContext, outputPacket, wR, writeFrame);
	}
	return 0;
}



int convertAndWriteDelayedVideoFrames(AVCodecContext *outputCodecContext,
                                      AVStream *videoStream,
                                      AVFormatContext *outputFormatContext,
                                      mutex *wR,
                                      condition_variable *writeFrame) {


	for (int result;;) {
		auto out_packet = Packet{};
		auto outPacket = out_packet.into();

		avcodec_send_frame(outputCodecContext, nullptr);

		if (avcodec_receive_packet(outputCodecContext, outPacket) == 0) { // Try to get packet
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
			writeFrameToOutput(outputFormatContext, outPacket, wR, writeFrame);
//			av_packet_unref(outPacket);
		} else { // No remaining frames to handle
			break;
		}
	}

	return 0;
}



//static int convertAndWriteDelayedAudioFrames(AVCodecContext *inputCodecContext,
//                                             AVCodecContext *outputCodecContext,
//                                             AVStream *audioStream,
//                                             AVFormatContext *outputFormatContext,
//                                             int finalSize,
//                                             mutex *wR,
//                                             condition_variable *writeFrame) {
//	// Create encoder audio packet
//	AVPacket *outPacket = alloc_packet();
//	AVPacket *nextOutPacket = alloc_packet();
//	AVPacket *prevPacket = nullptr;
//	int result;
//	avcodec_send_frame(outputCodecContext, nullptr);
//	auto receive = avcodec_receive_packet(outputCodecContext, outPacket);
//	while (receive >= 0) {// Try to get packet
//		avcodec_send_frame(outputCodecContext, nullptr);
//		receive = avcodec_receive_packet(outputCodecContext, nextOutPacket);
//		if (receive >= 0) {//if this is not the last packet
//			if (outPacket->pts != AV_NOPTS_VALUE) {
//				outPacket->pts =
//					av_rescale_q(outPacket->pts,
//					             {1,
//						             inputCodecContext->sample_rate *
//							             inputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			if (outPacket->dts != AV_NOPTS_VALUE) {
//				outPacket->dts =
//					av_rescale_q(outPacket->dts,
//					             {1,
//						             inputCodecContext->sample_rate *
//							             inputCodecContext->channels},
//					             audioStream->time_base);
//			}//frame size equals codec frame size
//			outPacket->duration =
//				av_rescale_q(outputCodecContext->frame_size,
//				             {1,
//					             inputCodecContext->sample_rate *
//						             inputCodecContext->channels},
//				             audioStream->time_base);
//			// Write packet to file
//			outPacket->stream_index = 1;
//			writeFrameToOutput(outputFormatContext, outPacket, wR, writeFrame);
//			av_packet_unref(outPacket);
//			prevPacket = outPacket;
//			outPacket = nextOutPacket;
//			nextOutPacket = prevPacket;
//		} else {//if this is the last packet
//			if (outPacket->pts != AV_NOPTS_VALUE) {
//				outPacket->pts =
//					av_rescale_q(outPacket->pts,
//					             {1,
//						             inputCodecContext->sample_rate *
//							             inputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			if (outPacket->dts != AV_NOPTS_VALUE) {
//				outPacket->dts =
//					av_rescale_q(outPacket->dts,
//					             {1,
//						             inputCodecContext->sample_rate *
//							             inputCodecContext->channels},
//					             audioStream->time_base);
//			}//frame size equals finalSize
//			outPacket->duration =
//				av_rescale_q(finalSize,
//				             {1,
//					             inputCodecContext->sample_rate *
//						             inputCodecContext->channels},
//				             audioStream->time_base);
//			// Write packet to file
//			outPacket->stream_index = 1;
//			writeFrameToOutput(outputFormatContext, outPacket, wR, writeFrame);
//			cout << "Final pts value is: " << outPacket->pts << endl;
//			av_packet_unref(outPacket);
//			av_packet_unref(nextOutPacket);
//			av_packet_free(&outPacket);
//			av_packet_free(&nextOutPacket);
//		}
//	}
//	return 0;
//}
//
//static int convertAndWriteAudioFrames(SwrContext *swrContext,
//                                      AVCodecContext *audioOutputCodecContext,
//                                      AVCodecContext *audioInputCodecContext,
//                                      AVStream *audioStream,
//                                      AVFormatContext *outputFormatContext,
//                                      AVFrame *audioFrame,
//                                      AVFrame *audioOutputFrame,
//                                      AVPacket *audioOutputPacket,
//                                      int64_t *pts_p,
//                                      mutex *wR,
//                                      condition_variable *writeFrame) {
//	// Create encoder audio frame
////    auto audioOutputFrame = make_unique<AVFrame>(*alloc_audio_frame(
////            audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
////            audioOutputCodecContext->channel_layout, 0));
////    AVFrame *audioOutputFrame = alloc_audio_frame(
////            audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
////            audioOutputCodecContext->channel_layout, 0);
//	// Create encoder audio packet
////    auto audioOutputPacket = make_unique<AVPacket>(*alloc_packet());
////    AVPacket *audioOutputPacket = alloc_packet();
//	init_audio_frame(audioOutputFrame, audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
//	                 audioOutputCodecContext->channel_layout, 0);
//	int got_packet = 0;
//	int got_samples = swr_convert(
//		swrContext, audioOutputFrame->data, audioOutputFrame->nb_samples,
//		(const uint8_t **) audioFrame->data, audioFrame->nb_samples);
//	if (got_samples < 0) {
//		throw avException("error: swr_convert()");
//	} else if (got_samples > 0) {
//		*pts_p += got_samples;
//		audioOutputFrame->nb_samples = got_samples;
//		audioOutputFrame->pts = *pts_p;
//		encode(audioOutputCodecContext, audioOutputPacket, &got_packet,
//		       audioOutputFrame);
//		av_frame_unref(audioOutputFrame);
//		init_audio_frame(audioOutputFrame, audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
//		                 audioOutputCodecContext->channel_layout, 0);
//		// Frame was sent successfully
//		if (got_packet > 0) { // Packet received successfully
//			if (audioOutputPacket->pts != AV_NOPTS_VALUE) {
//				audioOutputPacket->pts =
//					av_rescale_q(audioOutputPacket->pts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			if (audioOutputPacket->dts != AV_NOPTS_VALUE) {
//				audioOutputPacket->dts =
//					av_rescale_q(audioOutputPacket->dts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			audioOutputPacket->duration =
//				av_rescale_q(audioOutputCodecContext->frame_size,
//				             {1,
//					             audioInputCodecContext->sample_rate *
//						             audioInputCodecContext->channels},
//				             audioStream->time_base);
//			// Write packet to file
//			audioOutputPacket->stream_index = 1;
//			writeFrameToOutput(outputFormatContext, audioOutputPacket, wR, writeFrame);
//			av_packet_unref(audioOutputPacket);
//		}
//	}
//	while (swr_get_out_samples(swrContext, 0) >= audioOutputCodecContext->frame_size) {
//		got_samples = swr_convert(swrContext, audioOutputFrame->data, audioOutputFrame->nb_samples, nullptr, 0);
//		*pts_p += got_samples;
//		//audioOutputFrame->nb_samples=got_samples;
//		audioOutputFrame->pts = *pts_p;
//		encode(audioOutputCodecContext, audioOutputPacket, &got_packet,
//		       audioOutputFrame);
//		av_frame_unref(audioOutputFrame);
//		init_audio_frame(audioOutputFrame, audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
//		                 audioOutputCodecContext->channel_layout, 0);
//		// Frame was sent successfully
//		if (got_packet > 0) { // Packet received successfully
//			if (audioOutputPacket->pts != AV_NOPTS_VALUE) {
//				audioOutputPacket->pts =
//					av_rescale_q(audioOutputPacket->pts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			if (audioOutputPacket->dts != AV_NOPTS_VALUE) {
//				audioOutputPacket->dts =
//					av_rescale_q(audioOutputPacket->dts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			audioOutputPacket->duration =
//				av_rescale_q(audioOutputCodecContext->frame_size,
//				             {1,
//					             audioInputCodecContext->sample_rate *
//						             audioInputCodecContext->channels},
//				             audioStream->time_base);
//			// Write packet to file
//			audioOutputPacket->stream_index = 1;
//			writeFrameToOutput(outputFormatContext, audioOutputPacket, wR, writeFrame);
//			av_packet_unref(audioOutputPacket);
//		}
//	}
//	av_packet_unref(audioOutputPacket);
//	av_frame_unref(audioOutputFrame);
////    av_frame_free(&audioOutputFrame);
////    av_packet_free(&audioOutputPacket);
//	return 0;
//}
//
//static int convertAndWriteLastAudioFrames(SwrContext *swrContext,
//                                          AVCodecContext *audioOutputCodecContext,
//                                          AVCodecContext *audioInputCodecContext,
//                                          AVStream *audioStream,
//                                          AVFormatContext *outputFormatContext,
//                                          AVFrame *audioOutputFrame,
//                                          AVPacket *audioOutputPacket,
//                                          int64_t *pts_p,
//                                          mutex *wR,
//                                          condition_variable *writeFrame) {
//	// Create encoder audio frame
////    auto audioOutputFrame = make_unique<AVFrame>(*alloc_audio_frame(
////            audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
////            audioOutputCodecContext->channel_layout, 0));
////    AVFrame *audioOutputFrame = alloc_audio_frame(
////            audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
////            audioOutputCodecContext->channel_layout, 0);
//	// Create encoder audio packet
////    auto audioOutputPacket = make_unique<AVPacket>(*alloc_packet());
////    AVPacket *audioOutputPacket = alloc_packet();
//	init_audio_frame(audioOutputFrame, audioOutputCodecContext->frame_size, audioOutputCodecContext->sample_fmt,
//	                 audioOutputCodecContext->channel_layout, 0);
//	int got_packet = 0;
//	int got_samples = swr_convert(swrContext, audioOutputFrame->data, audioOutputFrame->nb_samples, nullptr, 0);
//	if (got_samples < 0) {
//		throw avException("error: swr_convert()");
//	} else if (got_samples > 0) {
//		*pts_p += got_samples;
//		audioOutputFrame->nb_samples = got_samples;
//		audioOutputFrame->pts = *pts_p;
//		encode(audioOutputCodecContext, audioOutputPacket, &got_packet,
//		       audioOutputFrame);
//		// Frame was sent successfully
//		if (got_packet > 0) { // Packet received successfully
//			if (audioOutputPacket->pts != AV_NOPTS_VALUE) {
//				audioOutputPacket->pts =
//					av_rescale_q(audioOutputPacket->pts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			if (audioOutputPacket->dts != AV_NOPTS_VALUE) {
//				audioOutputPacket->dts =
//					av_rescale_q(audioOutputPacket->dts,
//					             {1,
//						             audioInputCodecContext->sample_rate *
//							             audioInputCodecContext->channels},
//					             audioStream->time_base);
//			}
//			audioOutputPacket->duration =
//				av_rescale_q(audioOutputFrame->nb_samples,
//				             {1,
//					             audioInputCodecContext->sample_rate *
//						             audioInputCodecContext->channels},
//				             audioStream->time_base);
//			// Write packet to file
//			audioOutputPacket->stream_index = 1;
//			auto result = av_write_frame(outputFormatContext, audioOutputPacket);
//			if (result != 0) {
//				throw avException("Error in writing video frame");
//			}
//		}
//		convertAndWriteDelayedAudioFrames(audioInputCodecContext,
//		                                  audioOutputCodecContext,
//		                                  audioStream,
//		                                  outputFormatContext,
//		                                  got_samples,
//		                                  wR,
//		                                  writeFrame);
//	}
//	av_packet_unref(audioOutputPacket);
//	av_frame_unref(audioOutputFrame);
////    av_packet_free(&audioOutputPacket);
////    av_frame_free(&audioOutputFrame);
//	return 0;
//}
//
