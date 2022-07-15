#pragma once
#include "pch.h"
#include "ffmpeg-decode.h"
#include "ffmpeg-compat.h"


static const char* av_make_error(int errnum) {
	static char str[AV_ERROR_MAX_STRING_SIZE];
	memset(str, 0, sizeof(str));
	return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}
//#define OutputDebugStringFormat(...) {char cad[512]; sprintf(cad, __VA_ARGS__);  OutputDebugString(cad);}

/******************************************************************************
	Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

static const uint8_t* ff_avc_find_startcode_internal(const uint8_t* p,
	const uint8_t* end)
{
	const uint8_t* a = p + 4 - ((intptr_t)p & 3);

	for (end -= 3; p < a && p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	for (end -= 3; p < end; p += 4) {
		uint32_t x = *(const uint32_t*)p;

		if ((x - 0x01010101) & (~x) & 0x80808080) {
			if (p[1] == 0) {
				if (p[0] == 0 && p[2] == 1)
					return p;
				if (p[2] == 0 && p[3] == 1)
					return p + 1;
			}

			if (p[3] == 0) {
				if (p[2] == 0 && p[4] == 1)
					return p + 2;
				if (p[4] == 0 && p[5] == 1)
					return p + 3;
			}
		}
	}

	for (end += 3; p < end; p++) {
		if (p[0] == 0 && p[1] == 0 && p[2] == 1)
			return p;
	}

	return end + 3;
}

const uint8_t* obs_nal_find_startcode(const uint8_t* p, const uint8_t* end)
{
	const uint8_t* out = ff_avc_find_startcode_internal(p, end);
	if (p < out && out < end && !out[-1])
		out--;
	return out;
}

#define OBS_NAL_SLICE  1
#define OBS_NAL_SLICE_IDR 5

bool obs_avc_keyframe(const uint8_t* data, size_t size)
{
	const uint8_t* nal_start, * nal_end;
	const uint8_t* end = data + size;

	nal_start = obs_nal_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		const uint8_t type = nal_start[0] & 0x1F;

		if (type == OBS_NAL_SLICE_IDR || type == OBS_NAL_SLICE)
			return type == OBS_NAL_SLICE_IDR;

		nal_end = obs_nal_find_startcode(nal_start, end);
		nal_start = nal_end;
	}

	return false;
}

AVPixelFormat get_format(struct AVCodecContext* s, const enum AVPixelFormat* fmt) {
	OutputDebugString(L"LibDShow:: negotiating decoder pixel format...");
	int* current = (int*)fmt;
	std::wstringstream ss;
	ss << "LibDShow::decoder get_format: ";
	int defaultFmt = *fmt;
	bool hasYuv = false;
	while (*current != AVPixelFormat::AV_PIX_FMT_NONE) {
		ss << *current << ", ";
		if (*current == AVPixelFormat::AV_PIX_FMT_UYVY422) {
			ss << "Found UYVY4222 format!";
			OutputDebugString(ss.str().c_str());
			return AVPixelFormat::AV_PIX_FMT_UYVY422;
		}
		else if (*current == AVPixelFormat::AV_PIX_FMT_YUV420P) {
			hasYuv = true;
		}
		

		++current;
	}
	ss << "Couldn't find UYVY. ";
	if (hasYuv) {
		ss << "Returning YUV420P";
		defaultFmt = 0;
	}
	else {
		ss << "Returning default: " << defaultFmt;
	}
	
	OutputDebugString(ss.str().c_str());
	return (AVPixelFormat)defaultFmt;
}

int ffmpeg_decode_init(struct ffmpeg_decode* decode, enum AVCodecID id,
	bool use_hw)
{
	int ret;
	
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	avcodec_register_all();
#endif
	memset(decode, 0, sizeof(*decode));

	decode->codec = avcodec_find_decoder(id);
	
	if (!decode->codec)
		return -1;

	std::wstringstream ss;

	/*
	TODO: Use hw acceleration
	pix_fmt: AV_PIX_FMT_D3D11
	device type: AV_HWDEVICE_TYPE_D3D11VA

	AVBufferRef *hw_ctx = NULL;
	int ret = av_hwdevice_ctx_create(&hw_ctx, *priority,NULL, NULL, 0);
	if (hw_ctx) {
		decode->hw_device_ctx = hw_ctx;
		decode->decoder->hw_device_ctx = av_buffer_ref(hw_ctx);
		decode->hw = true;
	}

	....
	if (decode->hw_device_ctx)
		av_buffer_unref(&decode->hw_device_ctx);

	....

	if (decode->hw && !decode->hw_frame) {
			decode->hw_frame = av_frame_alloc();
			if (!decode->hw_frame)
				return false;
		}

	out_frame = decode->hw ? decode->hw_frame : decode->frame;
	..
	ret = avcodec_receive_frame(decode->decoder, out_frame);
	..
	//hw_frame exists in GPU memory, so it needs to be copied to CPU memory 
	....
	if (got_frame && decode->hw) {
		ret = av_hwframe_transfer_data(decode->frame, out_frame, 0);
		if (ret < 0) {
			return false;
		}
	}
	*/

	ss << "LibDShow:: Checking Codec HW Config.  [format,type,is_device_ctx] ";
	for (int i = 0;; i++) {
		const AVCodecHWConfig* config = avcodec_get_hw_config(decode->codec, i);
		if (!config) break;
		ss <<"["<< config->pix_fmt << ","<<config->device_type<<","<< ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)>0) << "] ";
		
	}
	OutputDebugString(ss.str().c_str());

	decode->decoder = avcodec_alloc_context3(decode->codec);

	decode->decoder->thread_count = 0;
	decode->decoder->pix_fmt = AVPixelFormat::AV_PIX_FMT_UYVY422;
	decode->decoder->width = 1920;
	decode->decoder->height = 1080;
	decode->decoder->get_format = &get_format;
#ifdef USE_NEW_HARDWARE_CODEC_METHOD
	if (use_hw)
		init_hw_decoder(decode);
#elseno
	(void)use_hw;
#endif
	//AVDictionary** unusedOpts;
	ret = avcodec_open2(decode->decoder, decode->codec, NULL);
	
	if (ret < 0) {
		ffmpeg_decode_free(decode);
		return ret;
	}

	if (decode->codec->capabilities & CODEC_CAP_TRUNC)
		decode->decoder->flags |= CODEC_FLAG_TRUNC;
	ret=av_image_alloc(decode->dst_data, decode->dst_linesize, 1920, 1080, AVPixelFormat::AV_PIX_FMT_UYVY422, 32);
	if (ret <= 0) {
		OutputDebugString(L"LibDShow::ERROR:Failed to allocate decoded image buffer");
	}
	

	//decode->decoded_framebuffer =(uint8_t*) malloc(1920 * 1080 * 4);
	//if (decode->decoded_framebuffer == NULL) {
	//	OutputDebugString(L"LibDShow::ERROR: ffmpeg-decoder.init failed to allocate decoded_framebuffer");
	//	decode->decoded_framebuffer_size = 0;
	//}
	//else {
	//	decode->decoded_framebuffer_size = (1920 * 1080 * 4);
	//}
	return 0;
}


void ffmpeg_decode_free(struct ffmpeg_decode* decode)
{
	if (decode->hw_frame)
		av_frame_free(&decode->hw_frame);

	if (decode->decoder)
		avcodec_free_context(&decode->decoder);

	if (decode->frame)
		av_frame_free(&decode->frame);

	if (decode->hw_device_ctx)
		av_buffer_unref(&decode->hw_device_ctx);

	/*if (decode->packet_buffer)
		bfree(decode->packet_buffer);*/
	//decode->packet_buffer.clear();
	free(decode->packet_buffer);
	//free(decode->decoded_framebuffer);
	av_freep(&decode->dst_data[0]);
	memset(decode, 0, sizeof(*decode));
}

static inline bool copy_data(struct ffmpeg_decode* decode, uint8_t* data,
	size_t size)
{
	size_t new_size = size + INPUT_BUFFER_PADDING_SIZE;

	if (decode->packet_size < new_size) {
		
		//OutputDebugString(L"LibDShow::Resizing buffer");
		auto newPtr = (uint8_t*)std::realloc(decode->packet_buffer, new_size);
		if (newPtr == NULL) {
			free(decode->packet_buffer);
			decode->packet_size = 0;
			//OutputDebugString(L"LibDShow::ERROR:failed to reallocate buffer!");
			return false;
		}
		decode->packet_buffer = newPtr;
		decode->packet_size = new_size;
	}
	//OutputDebugString(L"LibDShow::Filling Vector");
	memset(decode->packet_buffer + size, 0, INPUT_BUFFER_PADDING_SIZE);
	//OutputDebugString(L"LibDShow::Copying buffer to Vector");
	memcpy(decode->packet_buffer, data, size);
	return true;

}

void OutputDebugAVError(int ret) {
	std::wstringstream ss;
	ss << "LibDShow::ERROR sending packet to decoder:" << ret << ":" << av_make_error(ret);
	OutputDebugString(ss.str().c_str());
	ss.clear();
}


int ffmpeg_decode_video(struct ffmpeg_decode* decode, uint8_t* data,
	size_t size, long long* ts,
	enum video_range_type range)
{
	
	AVFrame* out_frame;
	int ret;
	
	
	//copy_data will return false if it failed to allocate enough memory.  Any other errors just hard crash the program :P 
	if (!copy_data(decode, data, size)) return 0;

	if (!decode->frame) {
		decode->frame = av_frame_alloc();
		if (!decode->frame)
			return false;

		if (decode->hw && !decode->hw_frame) {
			decode->hw_frame = av_frame_alloc();
			if (!decode->hw_frame)
				return false;
		}
	}
	out_frame = decode->hw ? decode->hw_frame : decode->frame;

	AVPacket* packet = av_packet_alloc();
	packet->data =(decode->packet_buffer);
	packet->size = (int)size;
	packet->pts = *ts;

	switch (decode->codec->id) {
		case AV_CODEC_ID_H264:
			if (obs_avc_keyframe(data, size))
				packet->flags |= AV_PKT_FLAG_KEY;
	}

	ret = avcodec_send_packet(decode->decoder, packet);
	if (ret == 0) {
		ret = avcodec_receive_frame(decode->decoder, out_frame);
	}
	av_packet_free(&packet);
	

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		return 1;
	if (ret == 0)
		return 2;
	OutputDebugAVError(ret);
	return 0;

	//if (ret == 0||ret==AVERROR(EAGAIN)) {
	//	OutputDebugString(L"LibDShow::OK: PACKET SENT!");
	//	ret = avcodec_receive_frame(decode->decoder, out_frame);
	//}else if (ret == AVERROR(EAGAIN)) {
	//	OutputDebugString(L"LibDShow::Codec buffering");
	//	return 1;
	//}
	//av_packet_free(&packet);
	//if(ret!=0) {
	//	OutputDebugAVError(ret);
	//}
	//
	//

	//got_frame = (ret == 0);
	//
	//if (got_frame) {

	//	//if ( std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - decode->lastSnapshot).count() >= 1) {
	//		ss.clear();
	//		ss << "LibDShow::Decoder output:";
	//		ss << ret << ":(";
	//		ss << decode->frame->width << "x" << decode->frame->height << ":)";
	//		ss << ":Format (" << decode->frame->format << "):";
	//		
	//		ss<<"Linesize["<<decode->frame->linesize[0]<<","<< decode->frame->linesize[1]<<","<< decode->frame->linesize[2]<<","<< decode->frame->linesize[3]<<"]:";
	//		ss << decode->frame->format;
	//		OutputDebugString(ss.str().c_str());
	//		ss.clear();
	//	//	decode->lastSnapshot = std::chrono::system_clock::now();
	////	}


	//	/*unsigned char* data = new unsigned char[out_frame->width * out_frame->height * 4];
	//	for (int x = 0; x < out_frame->width; ++x) {
	//		for (int y = 0; y < out_frame->height; ++y) {
	//			data[y * out_frame->width * 4 + x * 4] = out_frame->data[0][y * out_frame->linesize[0] + x];
	//			data[y * out_frame->width * 4 + x * 4+1] = out_frame->data[1][y * out_frame->linesize[1] + x];
	//			data[y * out_frame->width * 4 + x * 4+2] = out_frame->data[2][y * out_frame->linesize[2] + x];
	//			data[y * out_frame->width * 4 + x * 4+3] = out_frame->data[3][y * out_frame->linesize[3] + x];

	//		}
	//	}
	//	*(frame->data) = data;
	//	frame->width = out_frame->width;
	//	frame->height = out_frame->height;*/
	//	//*(frame->linesize) = out_frame->linesize;
	//}
	//else if (ret == AVERROR(EAGAIN)) {
	//	return 1;
	//}else {
	//	ss.clear();
	//	ss.str(std::wstring());
	//	ss << "LibDShow::ERROR: Did not got frame:" << ret<<":";
	//	/* AVERROR(EAGAIN):   output is not available in this state - user must try
 //*                         to send new input
 //*      AVERROR_EOF:       the decoder has been fully flushed, and there will be
 //*                         no more output frames
 //*      AVERROR(EINVAL):   codec not opened, or it is an encoder
 //*      AVERROR_INPUT_CHANGED:   current decoded frame has changed parameters
 //*                               with respect to first decoded frame. Applicable
 //*                               when flag AV_CODEC_FLAG_DROPCHANGED is set.*/
	//	switch (ret) {
	//	case AVERROR(EAGAIN):
	//		ss << "AVERROR(EAGAIN) - output is not available in this state - user must try to send new input";
	//		break;
	//	case AVERROR_EOF:
	//		ss << " the decoder has been fully flushed, and there will be no more output frames";
	//		break;
	//	case AVERROR(EINVAL):
	//		ss << "codec not opened, or it is an encoder";
	//		break;
	//	case  AVERROR_INPUT_CHANGED:
	//		ss << "current decoded frame has changed parameters with respect to first decoded frame.Applicable when flag AV_CODEC_FLAG_DROPCHANGED is set.";
	//		break;
	//	default:
	//		ss << "Decoding Error:" << av_make_error(ret);
	//		break;
	//	}
	//	OutputDebugString(ss.str().c_str());
	//}

	//if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
	//	ret = 0;
	//ss.clear();
	//if (ret < 0)
	//	return 0;
	//else if (got_frame) {
	//	return 2;
	//}
	//
	//
	//ss << "LibDShow::Decoder returned unhandle code:";
	//ss << ret;
	//OutputDebugString(ss.str().c_str());
	//ss.clear();
	//return false;
	/*
	for (size_t i = 0; i < MAX_AV_PLANES; i++) {
		frame->data[i] = decode->frame->data[i];
		frame->linesize[i] = decode->frame->linesize[i];
	}

	const enum video_format format =
		convert_pixel_format(decode->frame->format);
	frame->format = format;

	if (range == VIDEO_RANGE_DEFAULT) {
		range = (decode->frame->color_range == AVCOL_RANGE_JPEG)
			? VIDEO_RANGE_FULL
			: VIDEO_RANGE_PARTIAL;
	}

	const enum video_colorspace cs = convert_color_space(
		decode->frame->colorspace, decode->frame->color_trc);

	const bool success = video_format_get_parameters_for_format(
		cs, range, format, frame->color_matrix, frame->color_range_min,
		frame->color_range_max);
	if (!success) {
		OutputDebugString(L"Failed to get video format parameters for video format %u");//,cs);
		return false;
	}

	frame->range = range;

	*ts = decode->frame->pts;

	frame->width = decode->frame->width;
	frame->height = decode->frame->height;
	frame->flip = false;

	switch (decode->frame->color_trc) {
	case AVCOL_TRC_BT709:
	case AVCOL_TRC_GAMMA22:
	case AVCOL_TRC_GAMMA28:
	case AVCOL_TRC_SMPTE170M:
	case AVCOL_TRC_SMPTE240M:
	case AVCOL_TRC_IEC61966_2_1:
		frame->trc = VIDEO_TRC_SRGB;
		break;
	case AVCOL_TRC_SMPTE2084:
		frame->trc = VIDEO_TRC_PQ;
		break;
	case AVCOL_TRC_ARIB_STD_B67:
		frame->trc = VIDEO_TRC_HLG;
		break;
	default:
		frame->trc = VIDEO_TRC_DEFAULT;
	}

	if (frame->format == VIDEO_FORMAT_NONE)
		return false;

	*got_output = true;
	return true;
	*/
}

