package com.sty.ne.audio.decodesync;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import android.Manifest;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    private static final int MY_PERMISSIONS_REQUEST_WRITE_EXTERNAL_STORAGE = 1;
    private static final String FILE_DIR= Environment.getExternalStorageDirectory() + File.separator
            + "sty" + File.separator; //  /storage/emulated/0/sty/
    private Button btnStart;
    private AudioPlayer audioPlayer;

    private String inputStr;
    private String outputStr;

    // Used to load the 'native-lib' library on application startup.
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

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}
