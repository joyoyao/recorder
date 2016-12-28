#include "ffmpegEncoder.h"

#define MIN(a,b) (a)<(b)?(a):(b)
#define MAX(a,b) (a)>(b)?(a):(b)

#define DefaultSampleRate 44100

static jbyte* g_buffer;

FFEncoder::FFEncoder() {

	m_codec = NULL;
	m_codecContext = NULL;
	m_InVideoFrame = NULL;
	m_OutVideoFrame = NULL;
	m_frameIndex = 0;
	m_convertRgbToYuv = NULL;
	m_pInBufferPtr = NULL;
	m_nInBufferSize = 0;
	g_video_buffer = NULL;
}
;

FFEncoder::~FFEncoder() {

	CloseVideoFile();

}
;

bool FFEncoder::CreateVideoFile(const char * pVideoFile) {

	/* register all the codecs */
	avcodec_register_all();

	m_pVideoFile = fopen(pVideoFile, "wb");

	if (!m_pVideoFile) {
		LOGI( "Could not open %s\n", pVideoFile);
		return false;
	}

	/* find the mpeg1 video encoder */
	m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);

	if (!m_codec) {
		LOGI("Codec not found\n");
		return false;
	}

	m_codecContext = avcodec_alloc_context3(m_codec);

	if (!m_codecContext) {
		LOGI("Could not allocate video codec context\n");
		return false;
	}

	/* put sample parameters */
//    m_codecContext->bit_rate = 400000;
	/* resolution must be a multiple of two */
//    m_codecContext->width = 352;
//    m_codecContext->height = 288;
	/* frames per second */
//    m_codecContext->time_base = (AVRational){1,25};
	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	m_codecContext->bit_rate = m_bitRate;
	m_codecContext->width = m_dstWidth;
	m_codecContext->height = m_dstHeight;
	m_codecContext->time_base = (AVRational) {1,m_fps};
	m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

	m_codecContext->gop_size = 10;
	m_codecContext->max_b_frames = 1;

//    if (codec_id == AV_CODEC_ID_H264)
	av_opt_set(m_codecContext->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(m_codecContext, m_codec, NULL) < 0) {
		LOGI("Could not open codec\n");
		return false;
	}

	return true;
}

/*
 传入为rgba，需要进行转换
 */
bool FFEncoder::InitVideoFrame() {

	if (m_InVideoFrame) {
		av_freep(&m_InVideoFrame->data[0]);
		av_frame_free(&m_InVideoFrame);
		m_InVideoFrame = NULL;
	}

	if (m_OutVideoFrame) {
		av_freep(&m_OutVideoFrame->data[0]);
		av_frame_free(&m_OutVideoFrame);
		m_OutVideoFrame = NULL;
	}

	m_InVideoFrame = av_frame_alloc();

	if (!m_InVideoFrame) {
		LOGI("Could not allocate video frame\n");
		return false;
	}

	m_InVideoFrame->format = AV_PIX_FMT_RGBA;
	m_InVideoFrame->width = m_codecContext->width;
	m_InVideoFrame->height = m_codecContext->height;

	/* the image can be allocated by any means and av_image_alloc() is
	 * just the most convenient way if av_malloc() is to be used */
	int ret = av_image_alloc(m_InVideoFrame->data, m_InVideoFrame->linesize,
			m_InVideoFrame->width, m_InVideoFrame->height, AV_PIX_FMT_RGBA, 32);
	if (ret < 0) {
		LOGI("Could not allocate raw picture buffer\n");
		return false;
	}

	m_pInBufferPtr = m_InVideoFrame->data[0];
	m_nInBufferSize = m_InVideoFrame->height * m_InVideoFrame->width * 4;

	m_OutVideoFrame = av_frame_alloc();

	if (!m_OutVideoFrame) {
		LOGI("Could not allocate video frame\n");
		return false;
	}

	m_OutVideoFrame->format = m_codecContext->pix_fmt;
	m_OutVideoFrame->width = m_codecContext->width;
	m_OutVideoFrame->height = m_codecContext->height;

	/* the image can be allocated by any means and av_image_alloc() is
	 * just the most convenient way if av_malloc() is to be used */
	ret = av_image_alloc(m_OutVideoFrame->data, m_OutVideoFrame->linesize,
			m_OutVideoFrame->width, m_OutVideoFrame->height, AV_PIX_FMT_YUV420P,
			32);
	if (ret < 0) {
		LOGI("Could not allocate raw picture buffer\n");

		return false;
	}

	if (m_convertRgbToYuv)
		sws_freeContext(m_convertRgbToYuv);

	static int sws_flags = SWS_FAST_BILINEAR;
	m_convertRgbToYuv = sws_getContext(m_InVideoFrame->width,
			m_InVideoFrame->height, AV_PIX_FMT_RGBA, m_OutVideoFrame->width,
			m_OutVideoFrame->height, m_codecContext->pix_fmt, sws_flags, NULL,
			NULL, NULL);

	if (!m_convertRgbToYuv) {
		LOGI("m_convertRgbToYuv failed\n");
		return false;
	}

	return true;
}

//获取java层，图像的rgba地址

bool FFEncoder::jni_video_buffer_init(JNIEnv* env, jobject obj) {

//  // Get a reference to this object's class
	jclass thisClass = env->GetObjectClass(obj);
//
//  // Get the method that you want to call
	jmethodID javaSurfaceInit = env->GetMethodID(thisClass, "videoBufInit",
			"()Ljava/nio/ByteBuffer;");
//
	if (NULL == javaSurfaceInit)
		return false;

	jobject buf = env->CallObjectMethod(obj, javaSurfaceInit);

	if (buf == NULL)
		return false;
//
	g_video_buffer = (jbyte*) (env->GetDirectBufferAddress(buf));
	if (g_video_buffer == NULL)
		return false;

	return true;
}

void FFEncoder::CloseVideoFile() {

	if (m_codecContext) {
		avcodec_close(m_codecContext);
		av_free(m_codecContext);
		m_codecContext = NULL;
	}

	if (m_convertRgbToYuv) {
		sws_freeContext(m_convertRgbToYuv);
		m_convertRgbToYuv = NULL;
	}

	if (m_InVideoFrame) {
		av_freep(&m_InVideoFrame->data[0]);
		av_frame_free(&m_InVideoFrame);
	}

	if (m_OutVideoFrame) {
		av_freep(&m_OutVideoFrame->data[0]);
		av_frame_free(&m_OutVideoFrame);
	}

	if (!m_pVideoFile)
		return;

	fclose(m_pVideoFile);
	m_pVideoFile = NULL;

}

bool FFEncoder::EncodeFrame(bool bEnd) {

	if (!m_pVideoFile) {
		LOGI("未创建视频文件");
		return false;
	}

	LOGI("EncodeFrame in \n");

	int i, ret, x, y, got_output;
//
//    /* encode 1 second of video */
	i = m_frameIndex;
	av_init_packet(&m_videoPkt);
	m_videoPkt.data = NULL; // packet data will be allocated by the encoder
	m_videoPkt.size = 0;
//
//    fflush(stdout);

	if (!bEnd) {

//        uint8_t * pBuf = (uint8_t*)calloc(m_nInBufferSize, sizeof(uint8_t));

//        memcpy(m_InVideoFrame->data[0], pBuf, 100 * 100);

		LOGI("memcopy  in  %d\n", m_nInBufferSize);
		memcpy(m_pInBufferPtr, g_video_buffer, m_nInBufferSize);
		LOGI("memecopy over \n");

		sws_scale(m_convertRgbToYuv, m_InVideoFrame->data,
				m_InVideoFrame->linesize, 0, m_OutVideoFrame->height,
				m_OutVideoFrame->data, m_OutVideoFrame->linesize);

		m_OutVideoFrame->pts = m_frameIndex++;

		/* encode the image */
		ret = avcodec_encode_video2(m_codecContext, &m_videoPkt,
				m_OutVideoFrame, &got_output);

//        free(pBuf);

		if (ret < 0) {
			LOGI("Error encoding frame\n");
			return false;
		}

		if (got_output) {
			LOGI("Write frame %3d (size=%5d)\n", i, m_videoPkt.size);
			fwrite(m_videoPkt.data, 1, m_videoPkt.size, m_pVideoFile);
			av_free_packet(&m_videoPkt);
		}

	} else {
		/* get the delayed frames */
		for (got_output = 1; got_output; i++) {

			ret = avcodec_encode_video2(m_codecContext, &m_videoPkt, NULL,
					&got_output);
			if (ret < 0) {
				LOGI("Error encoding frame\n");
				return false;
			}

			if (got_output) {
				LOGI("Write delay frame %3d (size=%5d)\n", i, m_videoPkt.size);
				fwrite(m_videoPkt.data, 1, m_videoPkt.size, m_pVideoFile);
				av_free_packet(&m_videoPkt);
			}
		}
	}

	return true;
}

extern "C" {
JNIEXPORT jlong JNICALL Java_cc_fotoplace_video_VideoEncoding_createHandleForFFEncoding(
		JNIEnv* env, jobject obj);

JNIEXPORT void JNICALL Java_cc_fotoplace_video_VideoEncoding_releaseHandleForFFEncoding(
		JNIEnv* env, jobject obj, jlong lH);
JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_CreateVideoFile(
		JNIEnv * env, jobject obj, jlong lH, jstring videoPath);
JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_setEncodingParam(
		JNIEnv * env, jobject obj, jlong lH, jfloat bitRate, jint width,
		jint height, jfloat fps, jfloat pixfmt);
JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_EncodeFrame(
		JNIEnv * env, jobject obj, jlong lH, jboolean isEnd);

}
;

// 创建 handle For FFEncoder
JNIEXPORT jlong JNICALL Java_cc_fotoplace_video_VideoEncoding_createHandleForFFEncoding(
		JNIEnv* env, jobject obj) {

//    FFEncoder * pFFEncoder = (FFEncoder *)calloc(1, sizeof(FFEncoder));
	FFEncoder * pFFEncoder = new FFEncoder();
	return (jlong) pFFEncoder;

}

JNIEXPORT void JNICALL Java_cc_fotoplace_video_VideoEncoding_releaseHandleForFFEncoding(
		JNIEnv* env, jobject obj, jlong lH) {

	if (!lH)
		return;

	FFEncoder * pFFEncoder = (FFEncoder*) lH;

	pFFEncoder->CloseVideoFile();
	delete pFFEncoder;
//	free(pFFEncoder);
}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_CreateVideoFile(
		JNIEnv * env, jobject obj, jlong lH, jstring videoPath) {

	FFEncoder * pFFEncoder = (FFEncoder*) lH;

	jboolean isCopy;
	jboolean isCreated;

	const char *pVideoPath = env->GetStringUTFChars(videoPath, &isCopy);

	isCreated = pFFEncoder->CreateVideoFile(pVideoPath);

	if (isCopy == JNI_TRUE) {
		(env)->ReleaseStringUTFChars(videoPath, pVideoPath);
	}

	if (!isCreated) {
		LOGI("CreateVideoFile failed \n");

		pFFEncoder->CloseVideoFile();

		return false;
	}

	isCreated = pFFEncoder->InitVideoFrame();

	if (!isCreated) {
		LOGI("InitVideoFrame failed \n");

		pFFEncoder->CloseVideoFile();
		return false;
	}

// //获取java层 rgba数据首地址
	isCreated = pFFEncoder->jni_video_buffer_init(env, obj);

	if (!isCreated) {
		LOGI("jni_video_buffer_init failed \n");

		pFFEncoder->CloseVideoFile();

		return false;
	}

	return isCreated;

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_EncodeFrame(
		JNIEnv * env, jobject obj, jlong lH, jboolean isEnd) {

	FFEncoder * pFFEncoder = (FFEncoder*) lH;

	if (!pFFEncoder->EncodeFrame(isEnd)) {
		LOGI("jni_video_buffer_init failed \n");
		pFFEncoder->CloseVideoFile();
		return false;
	}

	return true;
}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_VideoEncoding_setEncodingParam(
		JNIEnv * env, jobject obj, jlong lH, jfloat bitRate, jint width,
		jint height, jfloat fps, jfloat pixfmt) {

	FFEncoder * pFFEncoder = (FFEncoder*) lH;

	pFFEncoder->m_bitRate = bitRate;
	pFFEncoder->m_dstWidth = width;
	pFFEncoder->m_dstHeight = height;
	pFFEncoder->m_fps = fps;
	pFFEncoder->m_pixFmt = pixfmt;

	return true;
}

