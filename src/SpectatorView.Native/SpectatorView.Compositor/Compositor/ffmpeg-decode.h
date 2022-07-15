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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif


//#include <obs.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
#include "video-io.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <stdbool.h>

struct ffmpeg_decode {
	AVBufferRef *hw_device_ctx;
	AVCodecContext *decoder;
	const AVCodec *codec;

	AVFrame *hw_frame;
	AVFrame *frame;
	bool hw;

	//std::vector<uint8_t> packet_buffer;
	uint8_t* packet_buffer;
	size_t packet_size=0;

	/*uint8_t* decoded_framebuffer;
	size_t decoded_framebuffer_size;*/
	uint8_t* dst_data[4];
	int dst_linesize[4];

	std::chrono::system_clock::time_point  lastSnapshot;
};

extern int ffmpeg_decode_init(struct ffmpeg_decode *decode, enum AVCodecID id,
			      bool use_hw);
extern void ffmpeg_decode_free(struct ffmpeg_decode *decode);

extern bool ffmpeg_decode_audio(struct ffmpeg_decode *decode, uint8_t *data,
				size_t size, struct obs_source_audio *audio,
				bool *got_output);

extern int ffmpeg_decode_video(struct ffmpeg_decode *decode, uint8_t *data,
				size_t size, long long *ts,
				enum video_range_type range);

static inline bool ffmpeg_decode_valid(struct ffmpeg_decode *decode)
{
	return decode->decoder != NULL;
}

struct obs_source_frame2 {
	uint8_t* data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
	uint32_t width;
	uint32_t height;
	uint64_t timestamp;

	enum video_format format;
	enum video_range_type range;
	float color_matrix[16];
	float color_range_min[3];
	float color_range_max[3];
	bool flip;
	uint8_t flags;
	uint8_t trc; /* enum video_trc */
};

#ifdef __cplusplus
}
#endif
