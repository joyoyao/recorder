package com.abcew.videorecoder;

import android.app.ProgressDialog;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.hardware.Camera.Area;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Toast;

import com.abcew.videorecoder.view.VideoFocusView;
import com.abcew.videorecoder.view.VideoPreviewView;
import com.abcew.videorecoder.view.VideoProgressView;

import java.util.ArrayList;
import java.util.List;

import butterknife.BindView;
import butterknife.OnClick;
import butterknife.OnTouch;


@SuppressWarnings("deprecation")
public class VideoRecorderActivity extends BaseActivity implements RecorderManager.VideoRecordListener, RecorderManager.VideoStatusChangeListener, Handler.Callback {
	private final static String TAG = VideoRecorderActivity.class.getSimpleName();

	private PowerManager.WakeLock mWakeLock;
	private RecorderManager recorderManager;

	public static final long LOW_STORAGE_THRESHOLD = 5L * 1024L * 1024L;
	private Handler handler;

	private static final int MSG_VIDEOTIME_UPDATE = 0;
	private static final int MSG_VIDEOSEGMENT_UPDATE = 1;
	private static final int MSG_VIDEOCAMERA_READY = 2;
	private static final int MSG_STARTRECORD = 3;
	private static final int MSG_PAUSERECORD = 4;
	private static final int MSG_CHANGE_FLASH = 66;
	private static final int MSG_CHANGE_CAMERA = 8;
	private static final int MSG_AUTO_FOCUS = 9;
	private static final int MSG_FOCUS_FINISH = 10;

	public static final int REQUEST_VIDEOPROCESS = 5;

	private boolean mAllowTouchFocus = false;

	@BindView(R.id.quitBtn)
	Button quitBtn;
	@BindView(R.id.recorder_flashlight)
	Button flashBtn;
	@BindView(R.id.recorder_cancel)
	Button cancelBtn;
	@BindView(R.id.recorder_next)
	Button nextBtn;
	@BindView(R.id.recorder_video)
	Button recorderVideoBtn;
	@BindView(R.id.recorder_frontcamera)
	Button switchCameraIcon;
	@BindView(R.id.recorder_progress)
	VideoProgressView progressView;
	@BindView(R.id.recorder_surface)
	VideoPreviewView cameraTextureView;
	@BindView(R.id.video_focus_view)
	VideoFocusView focusView;

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.video_record_activity);
		AndroidUtils.init(context);
		initData();
		initView();
		initAction();
	}

	protected void initData() {
		handler = new Handler(this);
		recorderManager = new RecorderManager(this, cameraTextureView, progressView);
		recorderManager.setVideoRecordListener(this);
		recorderManager.setVideoStatusChangeListener(this);
	}

	protected void initView() {
		if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_CAMERA_FRONT)) {
			switchCameraIcon.setVisibility(View.VISIBLE);
		}
	}

	private void showToast(String msg) {
		Toast.makeText(getApplicationContext(), "", Toast.LENGTH_LONG).show();
	}

	protected void initAction() {
		long size = AndroidUtils.getAvailableStorage();
		if (size == -1) {
			showToast("无存储卡");
			finish();
		} else if (size == 0) {
			showToast("存储卡不可用");
			finish();
		} else {
			if (size > 0 && size < LOW_STORAGE_THRESHOLD) {
				showToast("存储卡空间不足");
				finish();
			}
		}
		recorderManager.checkPermission(this);
		if (recorderManager.getCheckPermissionReslut(RecorderManager.NO_CAMERA_PERMISSION) && recorderManager
			.getCheckPermissionReslut(RecorderManager.NO_AUDIO_PERMISSION)) {
			showToast("请打开系统相机和录音权限");
			finish();
		} else if (recorderManager.getCheckPermissionReslut(RecorderManager.NO_CAMERA_PERMISSION)) {
			showToast("请打开系统相机权限");
			finish();
		} else if (recorderManager.getCheckPermissionReslut(RecorderManager.NO_AUDIO_PERMISSION)) {
			showToast("请打开系统录音权限");
			finish();
		}
	}

	@Override
	protected void onResume() {
		super.onResume();
		if (mWakeLock == null) {
			PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
			mWakeLock = pm.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK, TAG);
			mWakeLock.acquire();
		}
	}

	@Override
	protected void onPause() {
		super.onPause();
		recorderManager.pauseRecord();
		if (mWakeLock != null) {
			mWakeLock.release();
			mWakeLock = null;
		}
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		recorderManager.release();
//		VideoRecoder.unInit();
	}

	@Override
	public void timeUpdate(long totalTime) {
		handler.obtainMessage(MSG_VIDEOTIME_UPDATE, (int) totalTime, 0).sendToTarget();
	}

	@Override
	public void segmentUpdate(int segment) {
		handler.obtainMessage(MSG_VIDEOSEGMENT_UPDATE, segment, 0).sendToTarget();
	}

	@Override
	public void cameraReady() {
		handler.obtainMessage(MSG_VIDEOCAMERA_READY).sendToTarget();
	}

	@Override
	public boolean handleMessage(Message msg) {
		switch (msg.what) {
			case MSG_VIDEOTIME_UPDATE:
				int tm = msg.arg1;
				if (tm < Conf.MIN_RECORD_TIME) {
					nextBtn.setVisibility(View.INVISIBLE);
				} else if (tm >= Conf.MIN_RECORD_TIME && tm < Conf.MAX_RECORD_TIME) {
					nextBtn.setVisibility(View.VISIBLE);
				} else if (tm >= Conf.MAX_RECORD_TIME) {
					recorderManager.pauseRecord();
					recordEnd();
				}
				break;

			case MSG_VIDEOSEGMENT_UPDATE:
				int se = msg.arg1;
				if (se < 1) {
					cancelBtn.setVisibility(View.INVISIBLE);
				} else {
					cancelBtn.setVisibility(View.VISIBLE);
				}
				break;
			case MSG_STARTRECORD:
				recorderVideoBtn.setBackgroundResource(R.drawable.video_record_start_btn_pressed);
				AndroidUtils.pauseAudioPlayback();
				focusView.showGuideStep(2);
				recorderManager.startRecord();
				break;
			case MSG_PAUSERECORD:
				recorderVideoBtn.setBackgroundResource(R.drawable.video_record_start_btn);
				focusView.showGuideStep(3);
				recorderManager.pauseRecord();
				break;
			case MSG_VIDEOCAMERA_READY:
				resetVideoLayout();
				handler.sendEmptyMessageDelayed(MSG_AUTO_FOCUS, 300);
				break;
			case MSG_CHANGE_CAMERA:
				focusView.hideChangeCamera();
				recorderManager.changeCamera();
				handler.sendEmptyMessageDelayed(MSG_AUTO_FOCUS, 300);
				break;
			case MSG_CHANGE_FLASH:
				recorderManager.cameraManager().changeFlash();
				break;
			case MSG_AUTO_FOCUS:
				doAutoFocus();
				handler.sendEmptyMessageDelayed(MSG_FOCUS_FINISH, 1000);
				break;
			case MSG_FOCUS_FINISH:
				if (focusView != null) {
					focusView.setHaveTouch(false, new Rect(0, 0, 0, 0));
					mAllowTouchFocus = true;
				}
				break;
		}
		return false;
	}

	@Override
	public void onVideoStatusChange(int status) {
		switch (status) {
			case 0:
				break;
			case 1:
				cancelBtn.setBackgroundResource(R.drawable.video_record_delete);
				break;
			case 2:
				cancelBtn.setBackgroundResource(R.drawable.video_record_backspace);
				break;
			default:
				break;
		}
	}

	private void recordEnd() {
		final ProgressDialog dialog = new ProgressDialog(this);
		dialog.show();
		recorderManager.stopRecord(new RecorderManager.VideoMergeListener() {
			@Override
			public void onComplete(final int status, final String file) {
				runOnUiThread(new Runnable() {

					@Override
					public void run() {
						dialog.dismiss();
						if (status >= 0) {
							Intent intent = new Intent();
							intent.setAction("VideoBeautifyActivity");
							intent.putExtra("path", file);
							startActivityForResult(intent, REQUEST_VIDEOPROCESS);
						} else if (status == -1) {
							Toast.makeText(context, "没有视频生成", Toast.LENGTH_LONG).show();
						} else {
							Toast.makeText(context, "视频合成失败", Toast.LENGTH_LONG).show();
						}
					}
				});
			}
		});
	}

	private void doAutoFocus() {
		boolean con = recorderManager.cameraManager().supportFocus() && recorderManager.cameraManager().isPreviewing();
		if (con) {
			if (mAllowTouchFocus && focusView != null && focusView.getWidth() > 0) {
				mAllowTouchFocus = false;
				int w = focusView.getWidth();
				Rect rect = doTouchFocus(w / 2, w / 2);
				if (rect != null) {
					focusView.setHaveTouch(true, rect);
				}
			}
		}
	}

	private Rect doTouchFocus(float x, float y) {
		int w = cameraTextureView.getWidth();
		int h = cameraTextureView.getHeight();
		int left = 0;
		int top = 0;
		if (x - VideoFocusView.FOCUS_IMG_WH / 2 <= 0) {
			left = 0;
		} else if (x + VideoFocusView.FOCUS_IMG_WH / 2 >= w) {
			left = w - VideoFocusView.FOCUS_IMG_WH;
		} else {
			left = (int) (x - VideoFocusView.FOCUS_IMG_WH / 2);
		}
		if (y - VideoFocusView.FOCUS_IMG_WH / 2 <= 0) {
			top = 0;
		} else if (y + VideoFocusView.FOCUS_IMG_WH / 2 >= w) {
			top = w - VideoFocusView.FOCUS_IMG_WH;
		} else {
			top = (int) (y - VideoFocusView.FOCUS_IMG_WH / 2);
		}
		Rect rect = new Rect(left, top, left + VideoFocusView.FOCUS_IMG_WH, top + VideoFocusView.FOCUS_IMG_WH);
		Rect targetFocusRect = new Rect(rect.left * 2000 / w - 1000, rect.top * 2000 / h - 1000, rect.right * 2000 / w - 1000, rect.bottom * 2000 / h - 1000);
		try {
			List<Area> focusList = new ArrayList<Area>();
			Area focusA = new Area(targetFocusRect, 1000);
			focusList.add(focusA);
			recorderManager.cameraManager().doFocus(focusList);
			return rect;
		} catch (Exception e) {
			e.printStackTrace();
		}
		return null;
	}

	private void resetVideoLayout() {
		if (recorderManager.cameraManager().flashEnable()) {
			flashBtn.setVisibility(View.VISIBLE);
		} else {
			flashBtn.setVisibility(View.GONE);
		}
		if (!recorderManager.cameraManager().cameraChangeEnable()) {
			focusView.hideChangeCamera();
		}
		focusView.showGuideStep(1);

		int screenWidth = screenSize[0];

		FrameLayout cameraPreviewArea = (FrameLayout) findViewById(R.id.cameraPreviewArea);
		int cameraPreviewAreaHeight = cameraPreviewArea.getHeight();
		int salt = screenWidth + (cameraPreviewAreaHeight - screenWidth) / 2;
		//
		View recorder_surface_mask1 = findViewById(R.id.recorder_surface_mask1);
		View recorder_surface_mask2 = findViewById(R.id.recorder_surface_mask2);
		//
		FrameLayout.LayoutParams layoutParam2 = new FrameLayout.LayoutParams(android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.MATCH_PARENT);
		layoutParam2.bottomMargin = salt;

		FrameLayout.LayoutParams layoutParam3 = new FrameLayout.LayoutParams(android.view.ViewGroup.LayoutParams.MATCH_PARENT, android.view.ViewGroup.LayoutParams.MATCH_PARENT);
		layoutParam3.topMargin = salt;
		//
		recorder_surface_mask1.setLayoutParams(layoutParam2);
		recorder_surface_mask2.setLayoutParams(layoutParam3);
		//
		FrameLayout.LayoutParams layoutParam4 = (FrameLayout.LayoutParams) progressView.getLayoutParams();
		layoutParam4.topMargin = salt;
		progressView.setLayoutParams(layoutParam4);

		FrameLayout recorder_handl_area = (FrameLayout) findViewById(R.id.recorder_handl_area);
		FrameLayout.LayoutParams layoutParam5 = new FrameLayout.LayoutParams(screenWidth, screenWidth);
		layoutParam5.topMargin = (cameraPreviewAreaHeight - screenWidth) / 2;
		recorder_handl_area.setLayoutParams(layoutParam5);
	}

	// events --------------------------------------------------

	@OnTouch(R.id.recorder_video)
	public boolean onRecorderVideoTouch(View v, MotionEvent event) {
		switch (event.getAction()) {
			case MotionEvent.ACTION_DOWN:
				handler.sendEmptyMessage(MSG_STARTRECORD);
				if (focusView.isChangeCameraShow()) {
					focusView.hideChangeCamera();
				}
				break;
			case MotionEvent.ACTION_UP:
				handler.sendEmptyMessage(MSG_PAUSERECORD);
				break;
		}
		return true;
	}

	@Override
	public boolean onTouchEvent(MotionEvent event) {
		focusView.hideChangeCamera();
		return super.onTouchEvent(event);
	}

	@OnTouch(R.id.video_focus_view)
	public boolean onSquareFocusViewTouch(View v, MotionEvent event) {
		switch (event.getAction()) {
			case MotionEvent.ACTION_DOWN:
				focusView.setDownY(event.getY());
				boolean con = recorderManager.cameraManager().supportFocus() && recorderManager.cameraManager().isPreviewing();
				if (con) {// 对焦
					if (mAllowTouchFocus) {
						mAllowTouchFocus = false;
						Rect rect = doTouchFocus(event.getX(), event.getY());
						if (rect != null) {
							focusView.setHaveTouch(true, rect);
						}
						handler.sendEmptyMessageDelayed(MSG_FOCUS_FINISH, 1000);
					}
				}
				break;
			case MotionEvent.ACTION_UP:
				float upY = event.getY();
				float dis = upY - focusView.getDownY();
				if (Math.abs(dis) >= 100) {
					if (recorderManager.cameraManager().cameraChangeEnable()) {
						focusView.changeCamreaFlipUp();
						handler.sendEmptyMessage(MSG_CHANGE_CAMERA);
					}
				}
				break;
		}
		return true;
	}

	@OnClick(R.id.recorder_cancel)
	public void onRecorderCancelBtnClick(View v) {
		recorderManager.backspace();
	}

	@OnClick(R.id.recorder_frontcamera)
	public void onSwitchCameraBtnClick(View v) {
		handler.sendEmptyMessage(MSG_CHANGE_CAMERA);
	}

	@OnClick(R.id.recorder_next)
	public void onRecorderOKBtnClick(View v) {
		recordEnd();
		focusView.finishGuide();
	}

	@OnClick(R.id.recorder_flashlight)
	public void onRecorderFlashlightBtnClick(View v) {
		handler.sendEmptyMessage(MSG_CHANGE_FLASH);
	}

	@OnClick(R.id.quitBtn)
	public void onQuitBtnClick(View v) {
		onBackPressed();
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode == REQUEST_VIDEOPROCESS) {
			if (resultCode == RESULT_OK) {
				finish();
			}
		}
	}

//	@Override
//	public void cameraReady() {
//
//	}
//
//	@Override
//	public void timeUpdate(long totalTime) {
//
//	}
//
//	@Override
//	public void segmentUpdate(int segment) {
//
//	}
}
