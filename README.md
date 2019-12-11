# NeAudioDecodeSync FFmpeg音频解码
## 一、概念
### 1.1 FFmpeg音频解码流程（理论层）
![image](https://github.com/tianyalu/NeAudioDecodeSync/blob/master/show/audio_decode_process1.png)  
### 1.1 FFmpeg音频解码流程（代码层）
![image](https://github.com/tianyalu/NeAudioDecodeSync/blob/master/show/audio_decode_process2.png)

## 二、关键代码
### 2.1 CMakeLists.txt
```cmake
cmake_minimum_required(VERSION 3.4.1)

# file(GLOB SOURCE src/main/cpp/*.cpp)
file(GLOB SOURCE ${CMAKE_SOURCE_DIR}/*.cpp)
add_library( # Sets the name of the library.
        neplayer
        SHARED
        ${SOURCE})

find_library( # Sets the name of the path variable.
        log-lib
        log)

# include_directories(src/main/cpp/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
set(my_lib_path ${CMAKE_SOURCE_DIR}/../../../libs/${CMAKE_ANDROID_ARCH_ABI})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${my_lib_path}")

# message(WARNING "cmake_source_dir = ${CMAKE_SOURCE_DIR}") #E:/AndroidWangYiCloud/NDKWorkspace/NeVideoDecodeSync/app/src/main/cpp

target_link_libraries( # Specifies the target library.
        neplayer
        # avcodec avfilter avformat avutil swresample swscale
        avfilter avformat avcodec avutil swresample swscale
        ${log-lib}
        android #系统库,在 D:\AndroidDev\AndroidStudio\sdk\ndk-bundle\platforms\android-21\arch-arm\usr\lib
        z
        OpenSLES)
```
### 2.2 native-lib.cpp
```c++
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
```
#### 2.2.1 packet转frame流程
![image](https://github.com/tianyalu/NeAudioDecodeSync/blob/master/show/packet_to_frame.png)  
#### 2.2.1 frame转audio可播放的统一格式流程
![image](https://github.com/tianyalu/NeAudioDecodeSync/blob/master/show/frame_to_audio.png)  

### 2.3 MainActivity.java
```java
public class MainActivity extends AppCompatActivity {
    private static final int MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE = 1;
    private static final String FILE_DIR= Environment.getExternalStorageDirectory() + File.separator
            + "sty" + File.separator; //  /storage/emulated/0/sty/
    private Button btnStart;
    private AudioPlayer audioPlayer;

    private String inputStr;
    private String outputStr;

    static {
        System.loadLibrary("neplayer");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initView();
        requestPermission();
    }

    private void initView() {
        btnStart = findViewById(R.id.btn_start);
        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                decodeAudio();
            }
        });
    }

    private void decodeAudio() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                audioPlayer = new AudioPlayer();
                inputStr = new File(FILE_DIR, "xpg.mp3").getAbsolutePath();
                outputStr = new File(FILE_DIR, "out.pcm").getAbsolutePath();
                audioPlayer.sound(inputStr, outputStr);
            }
        }).start();
    }

    private void requestPermission() {
        if(ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED){
            if(ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE)){
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE);
            }else {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE: {
                if(grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED){
                    Log.i("sty", "onRequestPermissionResult granted");
                }else {
                    Log.i("sty", "onRequestPermissionResult denied");
                    showWarningDialog();
                }
                break;
            }
            default:
                break;
        }
    }

    private void showWarningDialog() {
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("警告")
                .setMessage("请前往设置->应用—>PermissionDemo->权限中打开相关权限，否则功能无法正常使用！")
                .setPositiveButton("确定", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        //finish();
                    }
                }).show();
    }
}
```
## 三、采坑&经验
### 3.1 `avcodec_receive_frame()` 返回-23
虽然代码终于是编译通过了，但是安装到真机后，点击“开始解码”按钮生成的文件为0kb。单步调试后发现
`int ret = avcodec_receive_frame(codecContext, frame);` 这一步的返回值为-23，百思不得其解。于是添加如下代码：  
```c++
char buf[1024];
av_strerror(ret, buf, 1024);
LOGE("ERROR INFO: %s", buf); 
```  
输出的结果为`invalid argument`，后经仔细检查，发现是漏掉了打开解码器的方法：  
`avcodec_open2(codecContext, dec, NULL);`，加上之后才解决了问题。  
在此获取一种解析错误原因，然后根据原因找错误的解决问题的思路。
