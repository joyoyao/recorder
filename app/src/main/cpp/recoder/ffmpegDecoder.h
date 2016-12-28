#ifndef FFMPEGDECODER_H
#define FFMPEGDECODER_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libswresample/swresample.h>
}

#include <stdio.h>
#include <wchar.h>

#include <jni.h>

/*for android logs*/
#include <android/log.h>

#define LOG_TAG "FFMEPGDECODER"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

//struct MediaInfo{
//
//    float fPosition;                        //媒体文件时长
//    float fNumsofStream;                    //流个数
//    float fNumsOfVideoStream;               //视频流个数 －－目前只支持单视频流
//    float fNumsOfAudioStream;               //音频流个数 －－目前只支持单音频流
//    float fRotation;                        //播放时，旋转角度
//};

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

class FFDecoder{

public:
    FFDecoder();
    ~FFDecoder();

public:
    bool OpenMediaFile(const char * pFilePath);
    bool CloseMediaFile();

    bool SetupScaleFormat(float width, float height);
    bool DecodeFrame(JNIEnv *env, jobject obj);  // 1为视频， 2为音频 0 为结束

    bool DecodeFrameWithRawYUV(JNIEnv *env, jobject obj);  // 1为视频， 2为音频 0 为结束

    bool jni_video_buffer_init(JNIEnv* env, jobject obj);
    bool jni_audio_buffer_init(JNIEnv* env, jobject obj);

    bool HaveAudio();
    bool HaveVideo();
    float GetWidth();
    float GetHeight();
    float GetRotation();
    float GetDuration();
    int   GetChannel();
    bool GetEndOfDecode();
    int   GetSampleRate();
    void Release();
    bool SeekTime(JNIEnv *env, jobject obj, double fPositon);

    bool m_bUseAudio;
    bool m_bUseVideo;


//    long g_buffer;
//    long g_audio_buffer;
    jbyte* g_video_buffer;
    jbyte* g_audio_buffer;
    uint8_t ** GetRGBAFramePtr(int * pOutW, int * pOutH);

    VideoState * m_pVideoState;
    bool m_bNeedSkip;
    float m_editFrameDuration;

private:
//    VideoState * m_pVideoState;
//    MediaInfo * m_pMediaInfo; //媒体文件时长
    float m_position; //媒体文件，当前播放位置
    double m_audioSeekTime;
    double m_videoSeekTime;

    uint8_t * m_swrBuffer;
    uint8_t * m_audio_buffter;
    int       m_audio_buffer_offset;
    int       m_audio_index;
    unsigned int       m_audio_bufSize;
    int       max_dst_nb_samples;
    int       dst_nb_samples;
    int       dst_nb_channels;
    int       dst_linesize;

    float     m_editPosition;
    float     m_lastCopyTime;

    int       m_skipped_frame;

    bool OpenFile(const char * pFilePath);
    bool OpenVideoStream();
    bool OpenAudioStream();
    void InitVideoState();
    void FreeVideoState();
    bool CopyVideoToJava(JNIEnv *env, jobject obj);
    bool CopyAudioToJava(JNIEnv *env, jobject obj);
    void CopyAudioData(JNIEnv *env, jobject obj, int bufSize);


};

#endif //FFMPEGDECODER_H