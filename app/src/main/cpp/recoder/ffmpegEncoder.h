#ifndef FFMPEGENCODER_H
#define FFMPEGENCODER_H

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/pixfmt.h>
    #include <libswresample/swresample.h>

    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/common.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/samplefmt.h>
}

#include <math.h>


#define INBUF_SIZE 4096
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096

#include <stdio.h>
#include <wchar.h>

#include <jni.h>

/*for android logs*/
#include <android/log.h>

#define LOG_TAG "FFMPEGENCODER"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

struct StreamStruct{

    int nStreamIndex;

    AVCodecContext * pStreamCtx;
    AVCodec * pStreamCodec;
    AVFrame * pFrame;
    AVFrame * pRGBAFrame;
    AVFrame * pAudioFrame;
    AVPicture pPicture;
    AVStream * pStream;
    struct SwsContext *pSwsCtx;  // 视频专用
    struct SwrContext *pSwrCtx;  // 音频专用

    float framePerSecond;
    float timeBase;
    bool isDecodeing;
    bool isShowing;
    int nPlayedFrames;
    int nInserted;

};

struct VideoState{

  AVFormatContext *pFormatCtx;
  int             videoStream, audioStream;

  StreamStruct    *pVideoStream;
  StreamStruct    *pAudioStream;

  float           fRotation;    // 视频专用，获取播放旋转角度

  bool            bEndOfDecode;

  int             nOutWidth;
  int             nOutHeight;


  AVStream        *audio_st;
//  PacketQueue     audioq;
//  uint8_t         audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  unsigned int    audio_buf_size;
  unsigned int    audio_buf_index;
  AVFrame         audio_frame;
  AVPacket        audio_pkt;
  uint8_t         *audio_pkt_data;
  int             audio_pkt_size;
  AVStream        *video_st;
//  PacketQueue     videoq;

//  VideoPicture    pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int             pictq_size, pictq_rindex, pictq_windex;

  char            filename[1024];
  int             quit;

  AVIOContext     *io_context;
  struct SwsContext *sws_ctx;
};

class FFEncoder{

public:
    FFEncoder();
    ~FFEncoder();

public:

    bool CreateVideoFile(const char * pVideoFile);
    bool InitVideoFrame();
    bool EncodeFrame(bool bEnd);
    void CloseVideoFile();

    bool jni_video_buffer_init(JNIEnv* env, jobject obj);
    bool jni_audio_buffer_init(JNIEnv* env, jobject obj);

    jbyte* g_video_buffer;
    jbyte* g_audio_buffer;
    uint8_t ** GetRGBAFramePtr(int * pOutW, int * pOutH);

    AVCodecContext* m_codecContext;

    float       m_bitRate;
    int         m_dstWidth;
    int         m_dstHeight;
    float       m_fps;
    int         m_pixFmt;

private:

    FILE*       m_pVideoFile;
    AVCodec*    m_codec;

    AVFrame*    m_InVideoFrame;
    uint8_t*    m_pInBufferPtr;
    int         m_nInBufferSize;
    AVFrame*    m_OutVideoFrame;

    // rgba convert to yuv420
    struct SwsContext * m_convertRgbToYuv;
    AVPacket    m_videoPkt;

    int         m_frameIndex;

    VideoState * m_pVideoState;
    float m_position; //媒体文件，当前播放位置

    uint8_t * m_swrBuffer;
    short * m_audio_buffter;

};

#endif //FFMPEGENCODER_H