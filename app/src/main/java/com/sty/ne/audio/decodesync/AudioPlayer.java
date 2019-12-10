package com.sty.ne.audio.decodesync;

public class AudioPlayer {
    static {
        System.loadLibrary("neplayer");
    }

    //xpg.mp3  out.pcm
    public native void sound(String input, String output);
}
