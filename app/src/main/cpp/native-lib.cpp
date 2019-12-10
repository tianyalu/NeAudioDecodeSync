#include <jni.h>
#include <string>
#include <android/log.h>
#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO, "sty", FORMAT, ##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR, "sty", FORMAT, ## __VA_ARGS__);

#define MAX_AUDIO_FRME_SIZE 48000 * 4
extern "C" {
//封装格式
#include <libavformat/avformat.h>
//解码
#include <libavcodec/avcodec.h>
//缩放
#include <libswscale/swscale.h>
//重采样
#include <libswresample/swresample.h>
}
extern "C" JNIEXPORT jstring JNICALL
Java_com_sty_ne_audio_decodesync_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT void JNICALL
Java_com_sty_ne_audio_decodesync_AudioPlayer_sound(JNIEnv *env, jobject thiz, jstring input_,
                                                   jstring output_) {
    const char* input = env->GetStringUTFChars(input_, 0);
    const char* output = env->GetStringUTFChars(output_, 0);
    avformat_network_init();

    //总上下文
    AVFormatContext* formatContext = avformat_alloc_context();
    //打开音频文件
    if(avformat_open_input(&formatContext, input, NULL, NULL) != 0) {
        LOGI("%s", "无法打开音频文件");
        return;
    }
    //获取输入文件信息
    if(avformat_find_stream_info(formatContext, NULL) < 0) {
        LOGI("%s", "无法获取输入文件信息");
        return;
    }

    //音频时长（单位：微妙us，转换为秒需要除以1000000）
    int audio_stream_idx = -1;
    for(int i = 0; i < formatContext->nb_streams; i++){
        if(formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_idx = i;
            break;
        }
    }

    //解码器参数
    AVCodecParameters* codecpar = formatContext->streams[audio_stream_idx]->codecpar;
    //找到解码器
    AVCodec* dec = avcodec_find_decoder(codecpar->codec_id);
    //创建上下文（packet->frame）
    AVCodecContext* codecContext = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(codecContext, codecpar);
    //打开解码器
    avcodec_open2(codecContext, dec, NULL);

    //转换上下文（frame->audio统一格式）
    SwrContext* swrContext = swr_alloc();
    //输入参数
    AVSampleFormat in_sample = codecContext->sample_fmt;
    //输入采样率
    int in_sample_rate = codecContext->sample_rate;
    //输入声道布局
    uint64_t in_ch_layout = codecContext->channel_layout;
    //输出参数（固定的）
    //输出采样格式
    AVSampleFormat out_sample = AV_SAMPLE_FMT_S16;
    //输出采样
    int out_sample_rate = 44100;
    //输出声道布局
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    swr_alloc_set_opts(swrContext, out_ch_layout, out_sample, out_sample_rate,
            in_ch_layout, in_sample, in_sample_rate, 0, NULL);
    //初始化转换器其他的默认参数
    swr_init(swrContext);
    uint8_t *out_buffer = static_cast<uint8_t *>(av_malloc(2 * 44100));
    FILE* fp_pcm = fopen(output, "wb");

    //读取包 压缩数据 转换过程参考：show/packet_to_frame.png
    AVPacket* packet = av_packet_alloc();
    int count = 0;
    while (av_read_frame(formatContext, packet) >=0) {
        avcodec_send_packet(codecContext, packet);
        //解压缩数据 未压缩
        AVFrame* frame = av_frame_alloc();
        int ret = avcodec_receive_frame(codecContext, frame);
        if(ret == AVERROR(EAGAIN)) {
            continue;
        }else if(ret < 0) {
            LOGE("解码完成");
            char buf[1024];
            av_strerror(ret, buf, 1024);
            LOGE("ERROR INFO: %s", buf);
            break;
        }
        if(packet->stream_index != audio_stream_idx) {
            continue;
        }
        LOGE("正在解码%d", count++);

        // frame->audio统一格式 转换过程可参考：show/frame_to_audio.png
        swr_convert(swrContext, &out_buffer, 2 * 44100, (const uint8_t **)frame->data, frame->nb_samples);
        //out_buffer->file
        int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
        //缓冲区的大小
        int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb, frame->nb_samples,
                out_sample, 1);
        //字节：最小1字节   像素：最小4字节
        fwrite(out_buffer, 1, out_buffer_size, fp_pcm);

    }

    fclose(fp_pcm);
    av_free(out_buffer);
    swr_free(&swrContext);
    avcodec_close(codecContext);
    avformat_close_input(&formatContext);


    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
}