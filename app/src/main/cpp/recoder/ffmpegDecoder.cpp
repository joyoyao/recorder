
#include "ffmpegDecoder.h"
#include "time.h"

#include <string.h>
#include <pthread.h>

#define MIN(a,b) (a)<(b)?(a):(b)
#define MAX(a,b) (a)>(b)?(a):(b)

#define DefaultSampleRate 44100
#define DefaultSampleNum 1024
#define DefaultAudioBufferSize 1024
#define DefaultChannels 1
#define DefaultSampleFMT AV_SAMPLE_FMT_S16

FFDecoder::FFDecoder(){

    m_pVideoState = NULL;
    g_video_buffer = NULL;
    g_audio_buffer = NULL;
    m_audio_buffer_offset = 0;
    m_audio_buffter = NULL;
    m_audio_bufSize = 0;
    m_audioSeekTime = 0.0f;
    m_videoSeekTime = 0.0f;
    m_audio_index = 0;

    m_editFrameDuration = 0;
    m_position = 0;
    m_lastCopyTime = 0;
    m_skipped_frame = 0;

    m_bUseAudio = false;
    m_bUseVideo = false;

    m_bNeedSkip = false;
};

FFDecoder::~FFDecoder(){
};

void FFDecoder::InitVideoState(){

    if(!m_pVideoState){
        FreeVideoState();
    }

    m_pVideoState = (VideoState*)calloc(1, sizeof(VideoState));

    m_pVideoState->pFormatCtx = NULL;
    m_pVideoState->videoStream = -1;
    m_pVideoState->audioStream = -1;
    m_pVideoState->pVideoStream = NULL;
    m_pVideoState->pAudioStream = NULL;
    m_pVideoState->fRotation = 0;
    m_pVideoState->bEndOfDecode = false;
}

void FFDecoder::FreeVideoState(){

    if(!m_pVideoState)
        return;

    if(m_pVideoState->pVideoStream){

        av_free(m_pVideoState->pVideoStream->pFrame);
        av_free(m_pVideoState->pVideoStream->pRGBAFrame);

        // Close the codec
        avcodec_close(m_pVideoState->pVideoStream->pStreamCtx);
        sws_freeContext(m_pVideoState->pVideoStream->pSwsCtx);


    }

    if(m_pVideoState->pAudioStream){

        av_freep(&m_audio_buffter);
        m_audio_bufSize = 0;
        m_audio_buffter = NULL;

        av_free(m_pVideoState->pAudioStream->pAudioFrame);
        swr_free(&m_pVideoState->pAudioStream->pSwrCtx);
        // Close the codec
        avcodec_close(m_pVideoState->pAudioStream->pStreamCtx);
    }

    if(m_pVideoState->pFormatCtx){

        avformat_close_input(&m_pVideoState->pFormatCtx);
    }

    free(m_pVideoState);

    if (m_swrBuffer) {

        free(m_swrBuffer);
    }
}

bool FFDecoder::OpenFile(const char * pFilePath){

    if(!pFilePath){
//        LOGI("%s","pFilePath is nil");
        LOGI("FilePath is nil %s", "");
        return false;
    }

    // Register all formats and codecs
    av_register_all();

    if(m_pVideoState){
        FreeVideoState();
    }

    InitVideoState();

// Open video file
    if(avformat_open_input(&m_pVideoState->pFormatCtx, pFilePath, NULL, NULL)!=0){

        LOGI("avformat_open_input %s", "fail");
        return false; // Couldn't open file
    }

    // Retrieve stream information
    if(avformat_find_stream_info(m_pVideoState->pFormatCtx, NULL)<0)
        return false; // Couldn't find stream information

    if(!m_pVideoState->pFormatCtx){
        LOGI("pFormatCtx %s", "false");
    }

    // Find the first video stream
    m_pVideoState->videoStream = -1; m_pVideoState->audioStream = -1;
    for(int i=0; i<m_pVideoState->pFormatCtx->nb_streams; i++) {
        if(m_pVideoState->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
            m_pVideoState->videoStream=i;
            LOGI("i have video");
            continue;
        }

        if(m_pVideoState->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) {
            m_pVideoState->audioStream=i;
            LOGI("i have audio");
            continue;
        }
    }

    return true;
}

void avStreamFPSTimeBase(AVStream *st, float defaultTimeBase, float *pFPS, float *pTimeBase)
{
    float fps, timebase;

    if (st->time_base.den && st->time_base.num)
        timebase = av_q2d(st->time_base);
    else if(st->codec->time_base.den && st->codec->time_base.num)
        timebase = av_q2d(st->codec->time_base);
    else
        timebase = defaultTimeBase;

    if (st->avg_frame_rate.den && st->avg_frame_rate.num)
        fps = av_q2d(st->avg_frame_rate);
    else if (st->r_frame_rate.den && st->r_frame_rate.num)
        fps = av_q2d(st->r_frame_rate);
    else
        fps = 1.0 / timebase;

    if (pFPS)
        *pFPS = fps;
    if (pTimeBase)
        *pTimeBase = timebase;
}

bool FFDecoder::OpenVideoStream(){

    if (!m_pVideoState->pVideoStream) {
        m_pVideoState->pVideoStream = (StreamStruct*)calloc(1, sizeof(StreamStruct));
        m_pVideoState->pVideoStream->isDecodeing = false;
        m_pVideoState->pVideoStream->isShowing = false;
        m_pVideoState->pVideoStream->nPlayedFrames = 0;
    }

    int nStreamIndex = m_pVideoState->videoStream;

    // Get a pointer to the codec context for the video stream
    m_pVideoState->pVideoStream->pStreamCtx = m_pVideoState->pFormatCtx->streams[nStreamIndex]->codec;

    // Find the decoder for the video stream
    m_pVideoState->pVideoStream->pStreamCodec=avcodec_find_decoder(m_pVideoState->pVideoStream->pStreamCtx->codec_id);

    if(!m_pVideoState->pVideoStream->pStreamCodec){

        return false; // Codec not found
    }

    // Open codec
    if(avcodec_open2(m_pVideoState->pVideoStream->pStreamCtx, m_pVideoState->pVideoStream->pStreamCodec, NULL)){

        return false; // Could not open codec
    }

    m_pVideoState->pVideoStream->pFrame = av_frame_alloc();
//    m_pVideoState->pVideoStream->pRGBAFrame = av_frame_alloc();
    m_pVideoState->pVideoStream->pStream = m_pVideoState->pFormatCtx->streams[nStreamIndex];

    if (!m_pVideoState->pVideoStream->pFrame) {
        avcodec_close(m_pVideoState->pVideoStream->pStreamCtx);
        m_pVideoState->pVideoStream->pStreamCtx = NULL;
        return false;
    }

    avStreamFPSTimeBase(m_pVideoState->pVideoStream->pStream, 0.04, &m_pVideoState->pVideoStream->framePerSecond,
                        &m_pVideoState->pVideoStream->timeBase);

    // 获取旋转角度
    m_pVideoState->fRotation = 0;
    AVDictionaryEntry *tag = NULL;
    tag = av_dict_get(m_pVideoState->pVideoStream->pStream->metadata, "rotate", tag, 0);

    if(tag)
        m_pVideoState->fRotation = atoi(tag->value);

    return true;
}


bool FFDecoder::OpenAudioStream(){

    if (!m_pVideoState->pAudioStream) {
        m_pVideoState->pAudioStream = (StreamStruct*)calloc(1, sizeof(StreamStruct));
        m_pVideoState->pAudioStream->isDecodeing = false;
        m_pVideoState->pAudioStream->isShowing = false;
        m_pVideoState->pAudioStream->nPlayedFrames = 0;
//        m_pVideoState->pAudioStream->pSwrCtx = NULL;
    }

    // Find the first video stream
    int nStreamIndex = m_pVideoState->audioStream;

    // Get a pointer to the codec context for the audio stream
    m_pVideoState->pAudioStream->pStreamCtx = m_pVideoState->pFormatCtx->streams[nStreamIndex]->codec;

    // Find the decoder for the video stream
    m_pVideoState->pAudioStream->pStreamCodec=avcodec_find_decoder(m_pVideoState->pAudioStream->pStreamCtx->codec_id);
    if(!m_pVideoState->pAudioStream->pStreamCodec)
        return false; // Codec not found

    // Open codec
    //    if(avcodec_open(pCodecCtx, pCodec)<0)
    if(avcodec_open2(m_pVideoState->pAudioStream->pStreamCtx, m_pVideoState->pAudioStream->pStreamCodec, NULL))
        return false; // Could not open codec

    //重新采样
    if (m_pVideoState->pAudioStream->pSwrCtx) {
        swr_free(&m_pVideoState->pAudioStream->pSwrCtx);
        m_pVideoState->pAudioStream->pSwrCtx = NULL;
    }

    m_pVideoState->pAudioStream->pFrame = av_frame_alloc();
    m_pVideoState->pAudioStream->pStream = m_pVideoState->pFormatCtx->streams[nStreamIndex];
//
    if (!m_pVideoState->pAudioStream->pFrame) {
        avcodec_close(m_pVideoState->pAudioStream->pStreamCtx);
        m_pVideoState->pAudioStream->pStreamCtx = NULL;
        return false;
    }
//
    avStreamFPSTimeBase(m_pVideoState->pAudioStream->pStream, 0.025, &m_pVideoState->pAudioStream->framePerSecond, &m_pVideoState->pAudioStream->timeBase);

    return true;
}

bool FFDecoder::jni_video_buffer_init(JNIEnv* env, jobject obj) {

//  // Get a reference to this object's class
    jclass thisClass = env->GetObjectClass(obj);
//
//  // Get the method that you want to call
    jmethodID javaSurfaceInit = env->GetMethodID(thisClass, "videoBufInit", "()Ljava/nio/ByteBuffer;");
//
    if (NULL == javaSurfaceInit) return false;

    jobject buf = env->CallObjectMethod(obj, javaSurfaceInit);

    if (buf == NULL) return false;
//
    g_video_buffer = (jbyte*)(env->GetDirectBufferAddress( buf));
    if (g_video_buffer == NULL) return false;

    return true;
}

bool FFDecoder::jni_audio_buffer_init(JNIEnv* env, jobject obj) {

//  // Get a reference to this object's class
    jclass thisClass = env->GetObjectClass(obj);

//  // Get the method that you want to call
    jmethodID javaAudioBufInitInit = env->GetMethodID(thisClass, "audioBufInit", "()Ljava/nio/ByteBuffer;");

    if (NULL == javaAudioBufInitInit) return false;

    jobject buf = env->CallObjectMethod(obj, javaAudioBufInitInit);

    if (buf == NULL) return false;
//
    g_audio_buffer = (jbyte*)(env->GetDirectBufferAddress( buf));
    if (g_audio_buffer == NULL) return false;

    return true;
}


bool FFDecoder::OpenMediaFile(const char * pFilePath){

    // 打开文件
    if(!OpenFile(pFilePath)){
        LOGI("OpenFile %s", "fail");
        return false;
    }

    // 打开视频
    if(m_pVideoState->videoStream >= 0){

        if(! OpenVideoStream()){

            m_pVideoState->videoStream = -1;

            LOGI("OpenVideoStream %s", "fail");
//            return false;
        }
    }

//     打开音频
    if(m_pVideoState->audioStream >= 0){

        if(! OpenAudioStream()){
            LOGI("OpenAudioStream %s", "fail");
            m_pVideoState->audioStream = -1;
//            return false;
        }
    }

    if(m_pVideoState->videoStream == -1 && m_pVideoState->audioStream == -1)
        return false;
    else
        return true;
    //
//
//	if(videoStream==-1)
//		return false; // Didn't find a video stream
//
//	// Get a pointer to the codec context for the video stream
//	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

}

/*

*   R = Y + 1.140*Cr;
*   G = Y - 0.394*Cb - 0.581*Cr;
*   B = Y + 2.032*Cb;

***/
void YUVToRgb(uint8_t * y, uint8_t * u, uint8_t * v, int *rgb, int w, int h ){

    int             sz;
    int             i;
    int             j;
    int             Y;
    int             Cr = 0;
    int             Cb = 0;
    int             pixPtr = 0;
    int             jDiv2 = 0;
    int             R = 0;
    int             G = 0;
    int             B = 0;
    int             cOff;

    int cb_2032,cr_1140,cr_581_cb_394;

    int uvW = w >> 1;

    for(j = 0; j < h; j++) {

        pixPtr = j * w;
        jDiv2 = (j >> 1) * uvW;

        for(i = 0; i < w; i++) {

            Y = y[pixPtr]&0xff;

            if((i & 0x1) != 1) {
                cOff = jDiv2 + (i >> 1);
                Cb = u[cOff]&0xff;
                Cb -= 128; // -128 ~ 127, 0 ~ 255
                Cr = v[cOff]&0xff;
                Cr -= 128;

//                 cb_2032 = Cb +((Cb * 198) >> 8);
//                 cr_1140 = Cr + ((Cr * 103) >> 8);
//                 cr_581_cb_394 = ((Cb * 88 +Cr * 183) >> 8);
                cb_2032 = (Cb << 1);
                cr_1140 = Cr + (Cr >> 3);
                cr_581_cb_394 = (Cr >> 1) + (Cb >> 2);
            }

            R = Y + cr_1140;
            G = Y - cr_581_cb_394;
            B = Y + cb_2032;

            if(R < 0) R = 0; else if(R > 255) R = 255;
            if(G < 0) G = 0; else if(G > 255) G = 255;
            if(B < 0) B = 0; else if(B > 255) B = 255;

            rgb[pixPtr++] = 0xff000000 + (B << 16) + (G << 8) + R;
//            rgb[pixPtr++] = (B << 24) + (G << 16) + (R << 8) + 0xFF;
        }

    }
}

bool FFDecoder::DecodeFrame(JNIEnv *env, jobject obj){ // 1为视频， 2为音频 0 为结束

    if(!m_pVideoState)
        return false;

    if(!m_pVideoState->pFormatCtx)
        return false;
    if ( (m_pVideoState->videoStream == -1) && (m_pVideoState->audioStream == -1))
        return false;

    AVPacket packet;
    av_init_packet(&packet);
    int finished = 0;
    float decodedDuration = 0;
    int height = GetHeight();
    int width = GetWidth();

    //初始化音频， 视频buf
    if(HaveVideo() && m_bUseVideo && g_video_buffer == NULL){
        m_pVideoState->nOutWidth = width;
        m_pVideoState->nOutHeight = height;
        jni_video_buffer_init(env, obj);
    }

    if(HaveAudio() && m_bUseAudio && g_audio_buffer == NULL){
        jni_audio_buffer_init(env, obj);
    }


    while (!finished) {

        if (av_read_frame(m_pVideoState->pFormatCtx, &packet) < 0) {
            m_pVideoState->bEndOfDecode = true;
            break;
        }

        if ((m_pVideoState->videoStream !=-1) && packet.stream_index == m_pVideoState->videoStream ){

            if(!m_bUseVideo){
                finished = true;
                break;
            }

            LOGI("avcodec_decode_video2 video");

            avcodec_decode_video2(m_pVideoState->pVideoStream->pStreamCtx,
                                  m_pVideoState->pVideoStream->pFrame, &finished, &packet);

            if(finished) {

                double timestamp = (m_pVideoState->pVideoStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pVideoStream->pStream->time_base);

                if(m_videoSeekTime >timestamp){
                    //扔掉因为seek导致的包， 解决时间戳不准的问题
                    break;
                }

                m_videoSeekTime = 0.0f;

                if(!CopyVideoToJava(env,  obj)){
                    av_free_packet(&packet);
                    return false;
                }

                m_lastCopyTime = m_position;
                m_position += 1.0 / m_pVideoState->pVideoStream->framePerSecond;

            }else{

                m_skipped_frame ++;
            }

        }else if((m_pVideoState->audioStream !=-1) && packet.stream_index == m_pVideoState->audioStream ){

            if(!m_bUseAudio){
                finished = true;
                break;
            }

            LOGI("avcodec_decode_video2 audio");

            int pktSize = packet.size;

            while (pktSize > 0) {

                int gotframe = 0;
                int len = avcodec_decode_audio4(m_pVideoState->pAudioStream->pStreamCtx,
                                                m_pVideoState->pAudioStream->pFrame, &gotframe, &packet);

                if (len < 0) {
                    break;
                }

                if (!gotframe){
                    pktSize -= len;

                    if(packet.data){
                        packet.data += len;
                    }

                    if(!HaveVideo())
//                LOGI("  layout->pktSize %d", len);
                        continue;

                }else
                {

                    LOGI("avcodec_decode_video2 audio11111111");

                    double timestamp = (m_pVideoState->pAudioStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pAudioStream->pStream->time_base);

                    if(m_audioSeekTime >timestamp){
                        //扔掉因为seek导致的包， 解决时间戳不准的问题
                        break;
                    }

                    m_audioSeekTime = 0.0f;

                    //转码
                    int nb_samples = m_pVideoState->pAudioStream->pFrame->nb_samples;
                    int channels = m_pVideoState->pAudioStream->pFrame->channels;
                    AVFrame * pFrame = m_pVideoState->pAudioStream->pFrame;
                    AVSampleFormat avsf = m_pVideoState->pAudioStream->pStreamCtx->sample_fmt;
                    int sample_rate = m_pVideoState->pAudioStream->pStreamCtx->sample_rate;

                    int out_count = (int64_t)nb_samples * DefaultSampleRate / sample_rate + 256;
                    int bufSize = av_samples_get_buffer_size(NULL,DefaultChannels, out_count, DefaultSampleFMT, 0);

                    int dec_channel_layout = (pFrame->channel_layout &&
                                              av_frame_get_channels(pFrame) == av_get_channel_layout_nb_channels(pFrame->channel_layout)) ?
                                             pFrame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(pFrame));

                    if(!m_pVideoState->pAudioStream->pSwrCtx){
                        m_pVideoState->pAudioStream->pSwrCtx = swr_alloc_set_opts(
                                NULL,
                                av_get_default_channel_layout(DefaultChannels),
                                DefaultSampleFMT, DefaultSampleRate,
                                dec_channel_layout, avsf, sample_rate,
                                0, NULL);

                        if (!m_pVideoState->pAudioStream->pSwrCtx || swr_init(m_pVideoState->pAudioStream->pSwrCtx)) {

                            if (m_pVideoState->pAudioStream->pSwrCtx)
                                swr_free(&m_pVideoState->pAudioStream->pSwrCtx);
                            avcodec_close(m_pVideoState->pAudioStream->pStreamCtx);
                            m_pVideoState->pAudioStream->pStreamCtx = NULL;
                            return false;
                        }
                    }

                    if(!HaveVideo()){
//                    LOGI("avcodec_decode_a pktSize %d,nb_samples %d,channels %d,sample_rate %d,out_count %d, bufSize %d, dec_channel_layout %d ",
//                                 pktSize,nb_samples,channels,sample_rate, out_count, bufSize,dec_channel_layout);
                    }

//                if (DefaultSampleNum != nb_samples) {
//                    if (swr_set_compensation(m_pVideoState->pAudioStream->pSwrCtx,
//                     (DefaultSampleNum - nb_samples) * DefaultSampleRate / sample_rate,
//                                                DefaultSampleNum * DefaultSampleRate /sample_rate) < 0) {
//                        av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
//                        return false;
//                    }
//                }

                    const uint8_t **in = (const uint8_t **)pFrame->extended_data;
                    uint8_t **out = &m_audio_buffter;
                    av_fast_malloc(&m_audio_buffter, &m_audio_bufSize, bufSize);

                    int ret = swr_convert(m_pVideoState->pAudioStream->pSwrCtx, out, out_count, in, nb_samples);
                    if (ret < 0) {
                        LOGI("avcodec_decode_failed");
                        break;
                    }

                    bufSize = ret * DefaultChannels * av_get_bytes_per_sample(DefaultSampleFMT);

                    //数据复制
                    CopyAudioData(env, obj, bufSize);
                }

                if (0 == len)
                    break;

                pktSize -= len;
            }
//           if(!HaveVideo())
            {
                finished = true;
            }
        } //end of audio decode
    }

    //处理跳帧

    if(m_pVideoState->bEndOfDecode){

        for(int i=m_skipped_frame; i>0; i--)
        {
            if(!m_bUseVideo){
                finished = true;
                break;
            }

            packet.data = NULL;
            packet.size = 0;

            avcodec_decode_video2(m_pVideoState->pVideoStream->pStreamCtx,
                                  m_pVideoState->pVideoStream->pFrame, &finished, &packet);
            // Did we get a video frame?

            if(finished) {

                double timestamp = (m_pVideoState->pVideoStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pVideoStream->pStream->time_base);

                if(m_videoSeekTime >timestamp){
                    //扔掉因为seek导致的包， 解决时间戳不准的问题
                    break;
                }

                m_videoSeekTime = 0.0f;

                if(!CopyVideoToJava(env,  obj)){
                    LOGI("process skip decode false !");
                    av_free_packet(&packet);
                    return false;
                }

                m_lastCopyTime = m_position;
                m_position += 1.0 / m_pVideoState->pVideoStream->framePerSecond;
            }
        }
    }

    av_free_packet(&packet);

    return true;
}

bool FFDecoder::DecodeFrameWithRawYUV(JNIEnv *env, jobject obj){ // 1为视频， 2为音频 0 为结束

    if(!m_pVideoState)
        return false;

    if(!m_pVideoState->pFormatCtx)
        return false;
    if ( (m_pVideoState->videoStream == -1) && (m_pVideoState->audioStream == -1))
        return false;

    AVPacket packet;
    av_init_packet(&packet);
    int finished = 0;
    float decodedDuration = 0;
    int height = GetHeight();
    int width = GetWidth();

    //初始化音频， 视频buf
    if(HaveVideo() && m_bUseVideo && g_video_buffer == NULL){
        m_pVideoState->nOutWidth = width;
        m_pVideoState->nOutHeight = height;
        jni_video_buffer_init(env, obj);
    }

    if(HaveAudio() && m_bUseAudio && g_audio_buffer == NULL){
        jni_audio_buffer_init(env, obj);
    }


    while (!finished) {

        if (av_read_frame(m_pVideoState->pFormatCtx, &packet) < 0) {
            m_pVideoState->bEndOfDecode = true;
            break;
        }

        if ((m_pVideoState->videoStream !=-1) && packet.stream_index == m_pVideoState->videoStream ){

            if(!m_bUseVideo){
                finished = true;
                break;
            }

            LOGI("avcodec_decode_video2 video");

            avcodec_decode_video2(m_pVideoState->pVideoStream->pStreamCtx,
                                  m_pVideoState->pVideoStream->pFrame, &finished, &packet);

            if(finished) {

                double timestamp = (m_pVideoState->pVideoStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pVideoStream->pStream->time_base);

                if(m_videoSeekTime >timestamp){
                    //扔掉因为seek导致的包， 解决时间戳不准的问题
                    break;
                }

                m_videoSeekTime = 0.0f;

                //设置视频信息
                //    private float mDuration;// 时长入口
                //    private float mPosition;// 位置入口
                //    private int frameType;// 类型入口 0 none 1 video 2 audio

                // Get a reference to this object's class
                jclass thisClass = env->GetObjectClass( obj);
                jfieldID fIdDuration = env->GetFieldID( thisClass, "mDuration", "F");

                if (NULL == fIdDuration){
                    av_free_packet(&packet);
                    LOGI("DecodeFrame skip  mDuration");
                    return false;
                }

                //    m_position = fPositon;
                env->SetFloatField( obj, fIdDuration, 1.0/ m_pVideoState->pVideoStream->framePerSecond);

                /*****************************/
                jfieldID fIdPosition = env->GetFieldID( thisClass, "mPosition", "F");
                if (NULL == fIdPosition){
                    av_free_packet(&packet);
                    LOGI("DecodeFrame mPosition  mDuration");
                    return false;
                }

                // Change Position
                // env->SetFloatField( obj, fIdPosition, 1.0);

                /*****************************/
                jfieldID fIdFrameType = env->GetFieldID( thisClass, "frameType", "I");
                // LOGI("DecodeFrame  frameType");
                if (NULL == fIdFrameType){
                    av_free_packet(&packet);
                    return false;
                }
                // Change FrameType
                env->SetIntField( obj, fIdFrameType, 1);

                // copy data
                if (g_video_buffer != NULL) {

                    if(!m_bNeedSkip){

                        uint8_t * y = m_pVideoState->pVideoStream->pFrame->data[0];
                        uint8_t * dst = (uint8_t*)g_video_buffer;

                        //copy y
                        for (int ii = 0; ii < height; ++ii) {
                            for (int jj = 0; jj < width; ++jj) {
                                dst[ii * width * 4 + 4 * jj + 0] = y[ii * width + jj]&0xff;
                                dst[ii * width * 4 + 4 * jj + 1] = y[ii * width + jj]&0xff;
                                dst[ii * width * 4 + 4 * jj + 2] = y[ii * width + jj]&0xff;
                                dst[ii * width * 4 + 4 * jj + 3] = 200;
                            }
                        }

//                        for(j = 0; j < h; j++) {
//
//                            pixPtr = j * w;
//                            jDiv2 = (j >> 1) * uvW;
//
//                            for(i = 0; i < w; i++) {
//
//                                Y = y[pixPtr]&0xff;
//                                rgb[pixPtr++] = 0xff000000 + (B << 16) + (G << 8) + R;
//                            }
//                        }

                        //copy u

                        //copy v

//                        YUVToRgb(
//                                m_pVideoState->pVideoStream->pFrame->data[0],
//                                m_pVideoState->pVideoStream->pFrame->data[1],
//                                m_pVideoState->pVideoStream->pFrame->data[2],
//                                (int*)g_video_buffer, m_pVideoState->nOutWidth, m_pVideoState->nOutHeight
//                        );
                    }

                    if(m_bNeedSkip){
                        LOGI("DecodeFrame skip  true");
                    }
                    else{
                        LOGI("DecodeFrame skip  false");
                    }

                    m_bNeedSkip = false;

                    // Get the method that you want to call
                    jmethodID javaAddFrame = env->GetMethodID(thisClass, "AddFrame", "()V");

                    if (NULL == javaAddFrame){
                        av_free_packet(&packet);
                        LOGI("DecodeFrame skip  AddFrame");
                        return false;
                    }

                    env->CallVoidMethod(obj, javaAddFrame);
                }

                m_lastCopyTime = m_position;
                m_position += 1.0 / m_pVideoState->pVideoStream->framePerSecond;

            }else{

                m_skipped_frame ++;
            }

        }else if((m_pVideoState->audioStream !=-1) && packet.stream_index == m_pVideoState->audioStream ){

            if(!m_bUseAudio){
                finished = true;
                break;
            }

            LOGI("avcodec_decode_video2 audio");

            int pktSize = packet.size;

            while (pktSize > 0) {

                int gotframe = 0;
                int len = avcodec_decode_audio4(m_pVideoState->pAudioStream->pStreamCtx,
                                                m_pVideoState->pAudioStream->pFrame, &gotframe, &packet);

                if (len < 0) {
                    break;
                }

                if (!gotframe){
                    pktSize -= len;

                    if(packet.data){
                        packet.data += len;
                    }

                    if(!HaveVideo())
//                LOGI("  layout->pktSize %d", len);
                        continue;

                }else
                {

                    LOGI("avcodec_decode_video2 audio11111111");

                    double timestamp = (m_pVideoState->pAudioStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pAudioStream->pStream->time_base);

                    if(m_audioSeekTime >timestamp){
                        //扔掉因为seek导致的包， 解决时间戳不准的问题
                        break;
                    }

                    m_audioSeekTime = 0.0f;

                    //转码
                    int nb_samples = m_pVideoState->pAudioStream->pFrame->nb_samples;
                    int channels = m_pVideoState->pAudioStream->pFrame->channels;
                    AVFrame * pFrame = m_pVideoState->pAudioStream->pFrame;
                    AVSampleFormat avsf = m_pVideoState->pAudioStream->pStreamCtx->sample_fmt;
                    int sample_rate = m_pVideoState->pAudioStream->pStreamCtx->sample_rate;

                    int out_count = (int64_t)nb_samples * DefaultSampleRate / sample_rate + 256;
                    int bufSize = av_samples_get_buffer_size(NULL,DefaultChannels, out_count, DefaultSampleFMT, 0);

                    int dec_channel_layout = (pFrame->channel_layout &&
                                              av_frame_get_channels(pFrame) == av_get_channel_layout_nb_channels(pFrame->channel_layout)) ?
                                             pFrame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(pFrame));

                    if(!m_pVideoState->pAudioStream->pSwrCtx){
                        m_pVideoState->pAudioStream->pSwrCtx = swr_alloc_set_opts(
                                NULL,
                                av_get_default_channel_layout(DefaultChannels),
                                DefaultSampleFMT, DefaultSampleRate,
                                dec_channel_layout, avsf, sample_rate,
                                0, NULL);

                        if (!m_pVideoState->pAudioStream->pSwrCtx || swr_init(m_pVideoState->pAudioStream->pSwrCtx)) {

                            if (m_pVideoState->pAudioStream->pSwrCtx)
                                swr_free(&m_pVideoState->pAudioStream->pSwrCtx);
                            avcodec_close(m_pVideoState->pAudioStream->pStreamCtx);
                            m_pVideoState->pAudioStream->pStreamCtx = NULL;
                            return false;
                        }
                    }

                    if(!HaveVideo()){
//                    LOGI("avcodec_decode_a pktSize %d,nb_samples %d,channels %d,sample_rate %d,out_count %d, bufSize %d, dec_channel_layout %d ",
//                                 pktSize,nb_samples,channels,sample_rate, out_count, bufSize,dec_channel_layout);
                    }

//                if (DefaultSampleNum != nb_samples) {
//                    if (swr_set_compensation(m_pVideoState->pAudioStream->pSwrCtx,
//                     (DefaultSampleNum - nb_samples) * DefaultSampleRate / sample_rate,
//                                                DefaultSampleNum * DefaultSampleRate /sample_rate) < 0) {
//                        av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
//                        return false;
//                    }
//                }

                    const uint8_t **in = (const uint8_t **)pFrame->extended_data;
                    uint8_t **out = &m_audio_buffter;
                    av_fast_malloc(&m_audio_buffter, &m_audio_bufSize, bufSize);

                    int ret = swr_convert(m_pVideoState->pAudioStream->pSwrCtx, out, out_count, in, nb_samples);
                    if (ret < 0) {
                        LOGI("avcodec_decode_failed");
                        break;
                    }

                    bufSize = ret * DefaultChannels * av_get_bytes_per_sample(DefaultSampleFMT);

                    //数据复制
                    CopyAudioData(env, obj, bufSize);
                }

                if (0 == len)
                    break;

                pktSize -= len;
            }
//           if(!HaveVideo())
            {
                finished = true;
            }
        } //end of audio decode
    }

    //处理跳帧

    if(m_pVideoState->bEndOfDecode){

        for(int i=m_skipped_frame; i>0; i--)
        {
            if(!m_bUseVideo){
                finished = true;
                break;
            }

            packet.data = NULL;
            packet.size = 0;

            avcodec_decode_video2(m_pVideoState->pVideoStream->pStreamCtx,
                                  m_pVideoState->pVideoStream->pFrame, &finished, &packet);
            // Did we get a video frame?

            if(finished) {

                double timestamp = (m_pVideoState->pVideoStream->pFrame->pkt_pts)*av_q2d(m_pVideoState->pVideoStream->pStream->time_base);

                if(m_videoSeekTime >timestamp){
                    //扔掉因为seek导致的包， 解决时间戳不准的问题
                    break;
                }

                m_videoSeekTime = 0.0f;

                if(!CopyVideoToJava(env,  obj)){
                    LOGI("process skip decode false !");
                    av_free_packet(&packet);
                    return false;
                }

                m_lastCopyTime = m_position;
                m_position += 1.0 / m_pVideoState->pVideoStream->framePerSecond;
            }
        }
    }

    av_free_packet(&packet);

    return true;
}

//
void SavePCM(){

//    short* fO = (short *)m_audio_buffter;
//
//    if(!HaveVideo()){
//
//        FILE *pFile;
//
//        pFile=fopen("/storage/emulated/0/fotoplace/sc1.txt", "a+");
//        if(pFile==NULL)
//            exit(1);
//        for (int i=0; i<bufSize / 2; i++) {
//            char outc[1024] = {0};
//            sprintf(outc, "%d,", *fO++);
//            fwrite(outc, 1, strlen(outc), pFile);
//        }
//
//        // Close file
//        fclose(pFile);
//        LOGI("bufSize copy %d, %d", bufSize,m_audio_index);
//    }

}

bool FFDecoder::CopyVideoToJava(JNIEnv *env, jobject obj){

    //设置视频信息
    //    private float mDuration;// 时长入口
    //    private float mPosition;// 位置入口
    //    private int frameType;// 类型入口 0 none 1 video 2 audio

    // Get a reference to this object's class
    jclass thisClass = env->GetObjectClass( obj);
    jfieldID fIdDuration = env->GetFieldID( thisClass, "mDuration", "F");

    if (NULL == fIdDuration){
        LOGI("DecodeFrame skip  mDuration");
        return false;
    }

//    m_position = fPositon;
    env->SetFloatField( obj, fIdDuration, 1.0/ m_pVideoState->pVideoStream->framePerSecond);

    /*****************************/
    jfieldID fIdPosition = env->GetFieldID( thisClass, "mPosition", "F");
    if (NULL == fIdPosition){
        LOGI("DecodeFrame mPosition  mDuration");
        return false;
    }

    // Change Position
//                    env->SetFloatField( obj, fIdPosition, 1.0);

    /*****************************/
    jfieldID fIdFrameType = env->GetFieldID( thisClass, "frameType", "I");
//    LOGI("DecodeFrame  frameType");
    if (NULL == fIdFrameType) return false;
    // Change FrameType
    env->SetIntField( obj, fIdFrameType, 1);

    // copy data
    if (g_video_buffer != NULL) {

        if(!m_bNeedSkip){

            YUVToRgb(
                    m_pVideoState->pVideoStream->pFrame->data[0],
                    m_pVideoState->pVideoStream->pFrame->data[1],
                    m_pVideoState->pVideoStream->pFrame->data[2],
                    (int*)g_video_buffer, m_pVideoState->nOutWidth, m_pVideoState->nOutHeight
            );
        }

        if(m_bNeedSkip){
            LOGI("DecodeFrame skip  true");
        }
        else{
            LOGI("DecodeFrame skip  false");
        }

        m_bNeedSkip = false;

        // Get the method that you want to call
        jmethodID javaAddFrame = env->GetMethodID(thisClass, "AddFrame", "()V");

        if (NULL == javaAddFrame){
            LOGI("DecodeFrame skip  AddFrame");
            return false;
        }

        env->CallVoidMethod(obj, javaAddFrame);
    }

    return true;
}

void FFDecoder::CopyAudioData(JNIEnv *env, jobject obj,int bufSize){

    //数据复制
    short* fO = (short *)m_audio_buffter;
    short* fA = (short *)g_audio_buffer;
//    bufSize /= 2;//双通道
    bufSize /= 2;//单通道

    if(bufSize + m_audio_buffer_offset < DefaultAudioBufferSize){

        for(int i=m_audio_buffer_offset; i<bufSize + m_audio_buffer_offset;i++){
            fA[i] = *fO++;
//            *fO++;
        }

//        memcpy(&(fA[m_audio_buffer_offset]), fO, bufSize);

        m_audio_buffer_offset += bufSize;

    }else{

        //当数据充足时
        while(bufSize + m_audio_buffer_offset >= DefaultAudioBufferSize){

            for(int i=m_audio_buffer_offset; i<DefaultAudioBufferSize;i++){
                fA[i] = *fO++;
//               *fO++;
            }

//            memcpy(&(fA[m_audio_buffer_offset]), fO, DefaultAudioBufferSize - m_audio_buffer_offset);

            bufSize -= DefaultAudioBufferSize - m_audio_buffer_offset;
            m_audio_buffer_offset = 0;

            if(!CopyAudioToJava(env, obj)){
                break;
            };
        }

        //处理剩余数据
        if(bufSize > 0){

            for(int i=0; i<bufSize;i++){
                fA[i] = *fO++;
//            *fO++;
            }

//            memcpy(fA, fO, bufSize);

            m_audio_buffer_offset = bufSize;
        }
    }
}

bool FFDecoder::CopyAudioToJava(JNIEnv *env, jobject obj){

    // Get a reference to this object's class
    jclass thisClass = env->GetObjectClass(obj);
    jfieldID fIdDuration = env->GetFieldID( thisClass, "mDuration", "F");
    if (NULL == fIdDuration)  return false;

    if(!HaveVideo()){
        m_position += 0.023;
    }

    // Change Duration
    env->SetFloatField( obj, fIdDuration, 0.023);

    /*****************************/
    jfieldID fIdPosition = env->GetFieldID( thisClass, "mPosition", "F");
    if (NULL == fIdPosition)  return false;

    /*****************************/
    jfieldID fIdFrameType = env->GetFieldID( thisClass, "frameType", "I");
    if (NULL == fIdFrameType) return false;

    // Change FrameType
    env->SetIntField( obj, fIdFrameType, 2);

    /*****************************/
    jfieldID fIdAudioBufferSize = env->GetFieldID( thisClass, "mAudioBufferSize", "I");
    if (NULL == fIdAudioBufferSize) return false;

    // set mAudioBufferSize
    env->SetIntField( obj, fIdAudioBufferSize, DefaultAudioBufferSize);

    // Get the method that you want to call
    jmethodID javaAddFrame = env->GetMethodID(thisClass, "AddFrame", "()V");

    if (NULL == javaAddFrame)  return false;

    env->CallVoidMethod(obj, javaAddFrame);

    return true;
}



float FFDecoder::GetWidth(){

    if(!m_pVideoState)
        return 0;

    if(!m_pVideoState->pVideoStream)
        return 0;

    return m_pVideoState->pVideoStream->pStreamCtx->width;
}

float FFDecoder::GetHeight(){

    if(!m_pVideoState)
        return 0;

    if(!m_pVideoState->pVideoStream)
        return 0;

    return m_pVideoState->pVideoStream->pStreamCtx->height;
}

bool FFDecoder::HaveVideo(){
    return m_pVideoState->videoStream != -1 ? true : false;
}

bool FFDecoder::HaveAudio(){
    return m_pVideoState->audioStream != -1 ? true : false;
}

float FFDecoder::GetRotation(){
    return m_pVideoState->fRotation;
}

float FFDecoder::GetDuration(){

    if(!m_pVideoState)
        return 0;

    if(!m_pVideoState->pFormatCtx)
        return 0;

    return m_pVideoState->pFormatCtx->duration / (double)AV_TIME_BASE;
}

int FFDecoder::GetChannel(){

    if(!m_pVideoState)
        return 0;

    if(!m_pVideoState->pAudioStream)
        return 0;

    return m_pVideoState->pAudioStream->pStreamCtx->channels;
}

bool FFDecoder::GetEndOfDecode(){

    if(!m_pVideoState)
        return 0;

    return m_pVideoState->bEndOfDecode;
}

int FFDecoder::GetSampleRate(){

    if(!m_pVideoState)
        return 0;

    if(!m_pVideoState->pAudioStream)
        return 0;

    return m_pVideoState->pAudioStream->pStreamCtx->sample_rate;

}

void FFDecoder::Release(){

    FreeVideoState();
}

uint8_t ** FFDecoder::GetRGBAFramePtr(int * pOutW, int * pOutH){

    *pOutH = m_pVideoState->nOutHeight;
    *pOutW = m_pVideoState->nOutWidth;

    return m_pVideoState->pVideoStream->pRGBAFrame->data;
}

bool FFDecoder::SeekTime(JNIEnv *env, jobject obj, double fPositon){

    if(fPositon > GetDuration()){
        fPositon = GetDuration();
    }

    if(fPositon < 0.f){
        fPositon = 0.f;
    }

    m_pVideoState->bEndOfDecode = false;

    m_audioSeekTime = 0.0f;
    m_videoSeekTime = 0.0f;

    m_audio_buffer_offset = 0;

    m_skipped_frame = 0;

    if (HaveVideo()) {

        int64_t ts = (int64_t)(fPositon / m_pVideoState->pVideoStream->timeBase);

        av_seek_frame(m_pVideoState->pFormatCtx, m_pVideoState->videoStream, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_pVideoState->pVideoStream->pStreamCtx);

        m_videoSeekTime = fPositon;
//        LOGI("SinglePlayer timestamp %f m_videoSeekTime %f", 0.0f, m_videoSeekTime);
    }

    if (HaveAudio()) {

        int64_t ts = (int64_t)(fPositon / m_pVideoState->pAudioStream->timeBase);

        av_seek_frame(m_pVideoState->pFormatCtx, m_pVideoState->audioStream, ts, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(m_pVideoState->pAudioStream->pStreamCtx);

        m_audioSeekTime = fPositon;
    }

    m_position = fPositon;
    m_lastCopyTime = m_position;


    return true;
}

extern "C" {

JNIEXPORT jlong JNICALL Java_cc_fotoplace_video_SinglePlayer_createHandleForFFDecoder
        (JNIEnv* env, jobject obj);

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_releaseHandleForFFDecoder
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_openMediaFile
        (JNIEnv* env, jobject obj, jlong lH, jstring fileName);

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_DecodeFrame
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_DecodeFrameWithRawYUV
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getWidth
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getHeight
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setSkip
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getRotation
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getDuration
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getPosition
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_SeekTime
        (JNIEnv* env, jobject obj, jlong lH, jdouble positon);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_getMediaInfo
        (JNIEnv *env, jobject obj, jlong lH, jobject srcAudioTimeBase, jobject srcVideoTimeBase,
         jobject srcAudioFrameRate, jobject srcVideoFrameRate);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_haveVideo
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_haveAudio
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setUseAudio
        (JNIEnv* env, jobject obj, jlong lH, jboolean bUse);

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setUseVideo
        (JNIEnv* env, jobject obj, jlong lH, jboolean bUse);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_setEditFrameDuration
        (JNIEnv* env, jobject obj, jlong lH, jfloat editFDuration);

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_GetEndOfDecode
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_getChannel
        (JNIEnv* env, jobject obj, jlong lH);

JNIEXPORT jstring JNICALL Java_cc_fotoplace_video_SinglePlayer_avcodecinfo
        (JNIEnv* env, jobject obj);
};

// 创建 handle For FFDecoder
JNIEXPORT jlong JNICALL Java_cc_fotoplace_video_SinglePlayer_createHandleForFFDecoder
        (JNIEnv* env, jobject obj){

    FFDecoder * pFFDecoder = (FFDecoder *)calloc(1, sizeof(FFDecoder));

    return (jlong)pFFDecoder;

}

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_releaseHandleForFFDecoder
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->Release();
    free(pFFDecoder);
    pFFDecoder = NULL;

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_openMediaFile
        (JNIEnv* env, jobject obj, jlong lH, jstring fileName){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    //return (*env)->NewStringUTF(env, "Hello from JNI !");//如果是用C语言格式就用这种方式
    //return env->NewStringUTF((char *)"Hello from JNI !");//C++用这种格式

    jboolean isCopy;
    const char *videoFileName = env->GetStringUTFChars(fileName, &isCopy);

    pFFDecoder->OpenMediaFile(videoFileName);

    if (isCopy == JNI_TRUE) {
        (env)->ReleaseStringUTFChars(fileName, videoFileName);
    }

    return true;
}

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_DecodeFrame
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return -1;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->DecodeFrame(env, obj);

    return 0;

}

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_DecodeFrameWithRawYUV
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return -1;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->DecodeFrameWithRawYUV(env, obj);

    return 0;

}


//
JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getWidth
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    return pFFDecoder->GetWidth();

}

//
JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getHeight
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return -1;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    return pFFDecoder->GetHeight();

}

JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getRotation
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return -1;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    return pFFDecoder->GetRotation();

}



//
JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_GetEndOfDecode
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH){
//        LOGI("GetEndOfDecode true" );
        return true;
    }


    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->GetEndOfDecode();
}

//
JNIEXPORT jfloat JNICALL Java_cc_fotoplace_video_SinglePlayer_getDuration
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return 0.0f;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->GetDuration();

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_SeekTime
        (JNIEnv* env, jobject obj, jlong lH, jdouble positon){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->SeekTime(env, obj, positon);
}

JNIEXPORT jint JNICALL Java_cc_fotoplace_video_SinglePlayer_getChannel
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->GetChannel();

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_haveVideo
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->HaveVideo();

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_haveAudio
        (JNIEnv* env, jobject obj, jlong lH){

    if(!lH)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    return pFFDecoder->HaveAudio();

}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_setEditFrameDuration
        (JNIEnv* env, jobject obj, jlong lH, jfloat editFDuration){

    if(!lH)
        return false;
    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->m_editFrameDuration = editFDuration;

    return true;
}

/**
 * com.leixiaohua1020.sffmpegandroidhelloworld.MainActivity.avcodecinfo()
 * AVCodec Support Information
 */
JNIEXPORT jstring Java_cc_fotoplace_video_SinglePlayer_avcodecinfo(JNIEnv *env, jobject obj)
{
    char info[400000] = { 0 };

    av_register_all();

    AVCodec *c_temp = av_codec_next(NULL);

    while(c_temp!=NULL){
        if (c_temp->decode!=NULL){
            sprintf(info, "%s[Dec]", info);
        }
        else{
            sprintf(info, "%s[Enc]", info);
        }
        switch (c_temp->type){
            case AVMEDIA_TYPE_VIDEO:
                sprintf(info, "%s[Video]", info);
                break;
            case AVMEDIA_TYPE_AUDIO:
                sprintf(info, "%s[Audio]", info);
                break;
            default:
                sprintf(info, "%s[Other]", info);
                break;
        }

        sprintf(info, "%s[%10s]\n", info, c_temp->name);

        c_temp=c_temp->next;
    }

    LOGE("%s", info);

    //return env->NewStringUTF((char *)"Hello from JNI !");//C++用这种格式
    return env->NewStringUTF((char *)info);
}

JNIEXPORT jboolean JNICALL Java_cc_fotoplace_video_SinglePlayer_getMediaInfo
        (JNIEnv *env, jobject obj, jlong lH, jobject srcAudioTimeBase, jobject srcVideoTimeBase,
         jobject srcAudioFrameRate, jobject srcVideoFrameRate){

    if(lH == 0)
        return false;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;

    //audio
    jclass stu_srcAudioTimeBase = env->GetObjectClass(srcAudioTimeBase); //或得Student类引用

    if(stu_srcAudioTimeBase == NULL){
        LOGE("stu_srcTimeBase NULL");
        return false;
    }

    jfieldID fidNum = env->GetFieldID(stu_srcAudioTimeBase,"num", "I");
    jfieldID fidDen = env->GetFieldID(stu_srcAudioTimeBase,"den", "I");
    if (NULL == fidNum) return false;

//       // Change the variable
    env->SetIntField(srcAudioTimeBase, fidNum, pFFDecoder->m_pVideoState->pAudioStream->pStream->time_base.num);
    env->SetIntField(srcAudioTimeBase, fidDen, pFFDecoder->m_pVideoState->pAudioStream->pStream->time_base.den);

    //audio framerate
    jclass stu_srcAudioFrameRate = env->GetObjectClass(srcAudioFrameRate); //或得Student类引用

    if(stu_srcAudioFrameRate == NULL){
        LOGE("stu_srcFrameRate NULL");
        return false;
    }

    fidNum = env->GetFieldID(stu_srcAudioFrameRate,"num", "I");
    fidDen = env->GetFieldID(stu_srcAudioFrameRate,"den", "I");
    if (NULL == fidNum) return false;

    AVRational rFrameRate = av_guess_frame_rate(pFFDecoder->m_pVideoState->pFormatCtx,pFFDecoder->m_pVideoState->pAudioStream->pStream, NULL);

//       // Change the variable
    env->SetIntField(srcAudioFrameRate, fidNum, rFrameRate.num);
    env->SetIntField(srcAudioFrameRate, fidDen, rFrameRate.den);

    //video
    jclass stu_srcVideoTimeBase = env->GetObjectClass(srcVideoTimeBase); //或得Student类引用

    if(stu_srcVideoTimeBase == NULL){
        LOGE("stu_srcVideoTimeBase NULL");
        return false;
    }

    fidNum = env->GetFieldID(stu_srcVideoTimeBase,"num", "I");
    fidDen = env->GetFieldID(stu_srcVideoTimeBase,"den", "I");
    if (NULL == fidNum) return false;

//       // Change the variable
    env->SetIntField(srcVideoTimeBase, fidNum, pFFDecoder->m_pVideoState->pVideoStream->pStream->time_base.num);
    env->SetIntField(srcVideoTimeBase, fidDen, pFFDecoder->m_pVideoState->pVideoStream->pStream->time_base.den);

    //video framerate
    jclass stu_srcVideoFrameRate = env->GetObjectClass(srcVideoFrameRate); //或得Student类引用

    if(stu_srcVideoFrameRate == NULL){
        LOGE("stu_srcFrameRate NULL");
        return false;
    }

    fidNum = env->GetFieldID(stu_srcVideoFrameRate,"num", "I");
    fidDen = env->GetFieldID(stu_srcVideoFrameRate,"den", "I");
    if (NULL == fidNum) return false;

    rFrameRate = av_guess_frame_rate(pFFDecoder->m_pVideoState->pFormatCtx,pFFDecoder->m_pVideoState->pVideoStream->pStream, NULL);

//       // Change the variable
    env->SetIntField(srcVideoFrameRate, fidNum, pFFDecoder->m_pVideoState->pVideoStream->pStream->avg_frame_rate.num);
    env->SetIntField(srcVideoFrameRate, fidDen, pFFDecoder->m_pVideoState->pVideoStream->pStream->avg_frame_rate.den);



    return true;
}

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setSkip
        (JNIEnv* env, jobject obj, jlong lH){

    if(lH == 0)
        return;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->m_bNeedSkip = true;

}

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setUseAudio
        (JNIEnv* env, jobject obj, jlong lH, jboolean bUse){

    if(lH == 0)
        return;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->m_bUseAudio = bUse;
}

JNIEXPORT void JNICALL Java_cc_fotoplace_video_SinglePlayer_setUseVideo
        (JNIEnv* env, jobject obj, jlong lH, jboolean bUse){

    if(lH == 0)
        return;

    FFDecoder * pFFDecoder = (FFDecoder*)lH;
    pFFDecoder->m_bUseVideo = bUse;


}
