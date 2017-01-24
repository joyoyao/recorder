#ifndef OPENAMEDIA_FFMPEGER_H
#define OPENAMEDIA_FFMPEGER_H
#define __STDC_FORMAT_MACROS

#include "utils/MetaData.h"
#include "utils/Errors.h"
#include "utils/MediaBuffer.h"

#include "openmax/OMX_IVCommon.h"

#include "utils/Common.h"


extern "C"{
#include "libavutil/common.h"

#include "libavutil/channel_layout.h"
#include "libavutil/opt.h"
#include "libavutil/mathematics.h"
#include "libavutil/timestamp.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/avassert.h"
}

namespace ARecoder {

	class FFMPEGer {
	public:
		FFMPEGer();
		~FFMPEGer();

		void setOutputFile(const char* path);
		void setVideoSize(int width, int height);
		void setVideoColorFormat(OMX_COLOR_FORMATTYPE fmt);
		
		bool init(MetaData* meta);
		
		int getAudioEncodeBufferSize();

		status_t encodeVideo(void *data);


		status_t encodeAudio(MediaBuffer* src, MediaBuffer* dst);
		status_t encodeVideo(MediaBuffer* src, MediaBuffer* dst);

		status_t encodeAudio(void *data,int len);


		status_t finish();
		
		void mux();
		
		MetaData* getMetaData();
		
	private:
		bool mInited;
		AVOutputFormat *fmt;
		AVFormatContext *fmt_ctx;
		AVCodec *audio_codec;
		AVCodec *video_codec;
		bool have_video;
		bool have_audio;
		MetaData* mMetaData;

		struct OutputStream {
			AVStream *st;
			/* pts of the next frame that will be generated */
			int64_t next_pts;
			int samples_count;
			AVFrame *frame;
			AVFrame *tmp_frame;
			SwsContext *sws_ctx;
			SwrContext *swr_ctx;
		};

		OutputStream video_st;
		OutputStream audio_st;

	    AVPacket video_pkt, audio_pkt;

		char mOutputFile[MAX_STRING_PATH_LEN];

		int mWidth;
		int mHeight;
		OMX_COLOR_FORMATTYPE mColor;
		AVPixelFormat mPixFmt;

		bool deInit();
		void reset();

		bool add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);
		bool open_audio(AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
		AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples);
		bool open_video(AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
		AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
		void close_stream(AVFormatContext *oc, OutputStream *ost);
		int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);

		FFMPEGer(const FFMPEGer&);
		FFMPEGer& operator=(const FFMPEGer&);

		bool recordAudioFrame(AVFrame *pAudioFrame);
	};
	
}//namespace

#endif//OPENAMEDIA_FFMPEGER_H
