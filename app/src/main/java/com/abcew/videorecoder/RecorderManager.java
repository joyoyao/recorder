package com.abcew.videorecoder;

import android.app.Activity;
import android.content.Context;
import android.hardware.Camera;
import android.hardware.Camera.PreviewCallback;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Environment;
import android.util.Log;
import android.view.SurfaceHolder;

import com.abcew.videorecoder.view.VideoPreviewView;
import com.abcew.videorecoder.view.VideoProgressView;

import java.io.File;
import java.nio.ByteBuffer;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.Semaphore;
import java.util.concurrent.atomic.AtomicBoolean;

@SuppressWarnings("deprecation")
public class RecorderManager implements PreviewCallback, SurfaceHolder.Callback {

	private static final String TAG = RecorderManager.class.getSimpleName();
	private CameraManager cameraManager = null;
	private long videoStartTime;
	private long videoTimeSingle;
	private long[] times = new long[2];

	private volatile AtomicBoolean recording = new AtomicBoolean(false);
	private volatile AtomicBoolean stopRecord = new AtomicBoolean(false);

	private AudioRecord audioRecorder;

	private String videoFile;
	private String finalVideoFile;
	private volatile boolean inBackspaceMode = true;

	private LinkedList<VideoSegment> recorderFiles = new LinkedList<VideoSegment>();

	private VideoProgressView mProgressView;

	private VideoPreviewView cameraTextureView;

	private String sessionVedioForder;

	private Semaphore semp = new Semaphore(1);

	private VideoEncodeThread videoEncodeThread;
	private AudioRecordThread audioRecordThread;
	private AudioEncodeThread audioEncodeThread;

	private int vieoNameIndexer = 0;
	private SurfaceHolder mSurfaceHolder = null;

	ARecorder mRecorder;

	boolean mPreviewRunning = false;

	private MyRecorderListener mRecorderListener = new MyRecorderListener();

	private class MyRecorderListener implements ARecorder.ARecorderListener {

		@Override
		public void onStart() {
			mPreviewRunning = true;
		}

		@Override
		public void onStop() {

		}

	}


	public RecorderManager(Activity activity, VideoPreviewView cameraTextureView, VideoProgressView mProgressView) {
		cameraManager = new CameraManager(activity);
		this.mProgressView = mProgressView;
		this.cameraTextureView = cameraTextureView;
		mSurfaceHolder = cameraTextureView.getHolder();
		mSurfaceHolder.addCallback(this);
	}

	public interface VideoRecordListener {
		public void cameraReady();

		public void timeUpdate(long totalTime);

		public void segmentUpdate(int segment);
	}

	private VideoRecordListener l = new VideoRecordListener() {

		@Override
		public void cameraReady() {
			Logger.d("cameraReady");
		}

		@Override
		public void timeUpdate(long totalTime) {
			Logger.d("totalTime: " + totalTime);
		}

		@Override
		public void segmentUpdate(int segment) {
			Logger.d("segment: " + segment);
		}
	};

	private String genSegVideoName() {
		String root = Environment.getExternalStorageDirectory().getAbsolutePath();
		StringBuilder vName = new StringBuilder(root);
		return vName.append(Conf.VIDEO_TMP_DIR).append("/").append(vieoNameIndexer++).append(".mp4").toString();
	}

	private String genMegVideoName() {
		String root = Environment.getExternalStorageDirectory().getAbsolutePath();
		StringBuilder vName = new StringBuilder(root);
		return vName.append(Conf.VIDEO_TMP_DIR).append("/").append("merge").append(".mp4").toString();
	}

	public int[] getPreviewSize() {
		return cameraManager.getPreviewSize();
	}

	public static final byte NO_CAMERA_PERMISSION = 0x01;
	public static final byte NO_AUDIO_PERMISSION = 0x02;

	private byte getPermission = 0x00;

	public boolean getCheckPermissionReslut(byte type) {
		return (getPermission & type) == type;
	}

	public byte checkPermission(Context context) {

		Thread vThread = new Thread(new Runnable() {

			@Override
			public void run() {
				try {
					Camera testCamera = Camera.open();
					if (testCamera != null) {
						try {
							testCamera.getParameters();//recheck 
						} catch (Exception e) {
							getPermission |= NO_CAMERA_PERMISSION;
						}
						testCamera.release();
						testCamera = null;
					} else {
						getPermission |= NO_CAMERA_PERMISSION;
					}
				} catch (Exception e) {
					getPermission |= NO_CAMERA_PERMISSION;
				}
			}
		});
		Thread aThread = new Thread(new Runnable() {

			@Override
			public void run() {
				try {
					int samp_rate = 44100;
					int minSize = android.media.AudioRecord.getMinBufferSize(samp_rate, AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT);
					AudioRecord testAudioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, samp_rate, AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT, minSize);
					if (testAudioRecord.getState() == AudioRecord.STATE_UNINITIALIZED) {
						try {
							testAudioRecord.release();
							testAudioRecord = null;
						} catch (Exception e) {
						}
						getPermission |= NO_AUDIO_PERMISSION;
						return;
					}
					testAudioRecord.startRecording();
					byte[] testByte = new byte[100];
					//					byte[] bak = new byte[100];
					int bufferReadResult = testAudioRecord.read(testByte, 0, 100);
					if (bufferReadResult <= 0) {
						getPermission |= NO_AUDIO_PERMISSION;
					} else {
					}
					testAudioRecord.stop();
					testAudioRecord.release();
					testAudioRecord = null;
				} catch (Exception e) {
					e.printStackTrace();
					getPermission |= NO_AUDIO_PERMISSION;
				}
			}
		});
		vThread.start();
		aThread.start();
		try {
			vThread.join();
			aThread.join();
		} catch (Exception e) {
			//ignore
		}
		return getPermission;
	}

	public synchronized void initRecorder() {
		videoFile = genSegVideoName();
//		int size[] = cameraManager.getPreviewSize();
//		int rotate = 0;
//		if (cameraManager.isUseBackCamera()) {
//			rotate = 90;
//		}
//		if (cameraManager.isUseFrontCamera()) {
//			rotate = 270;
//		}
//		AInfo ainfo = new AInfo((short) 1, 44100, 2);
//		VInfo vinfo = new VInfo(size[0], size[1], 480, 480, (short) Conf.VIDEO_FRAMERATE, 400000, rotate);
//
//		VideoRecoder.newFile(ainfo, vinfo, videoFile);


		mRecorder = new ARecorder();
		SimpleDateFormat sDateFormat = new SimpleDateFormat("/yyyy-MM-dd_hh-mm-ss");
		String date = sDateFormat.format(new java.util.Date());
		String path = Environment.getExternalStorageDirectory().toString() + date + ".flv";
		Log.i("RecorderActivity", path);
		mRecorder.setOutputFile(path);
		mRecorder.setListener(mRecorderListener);
		mRecorder.setChannels(1);
		mRecorder.setSampleRate(44100);

		int[] previewSize=cameraManager.getPreviewSize();
		mRecorder.setVideoSize(previewSize[0], previewSize[1]);
		mRecorder.setColorFormat("nv21");
		mRecorder.start();
		Log.i(TAG, "recorder initialize success");

		videoEncodeThread = new VideoEncodeThread();
		audioRecordThread = new AudioRecordThread();
		audioEncodeThread = new AudioEncodeThread();
		audioRecordThread.start();
		videoEncodeThread.start();
		audioEncodeThread.start();
	}

	public void startRecord() {
		Logger.d("traceTouch", " RecorderManager startRecord");

		try {
			semp.acquire();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		recording.set(true);
		stopRecord.set(false);
		initRecorder();
		times[0] = times[1];
		videoStartTime = System.currentTimeMillis();
		videoStatusChangeListener.onVideoStatusChange(2);
		mProgressView.setCurrentState(VideoProgressView.State.START);
	}

	public void pauseRecord() {
		Logger.d("traceTouch", "RecorderManager pauseRecord recording: " + recording);
		Logger.d("traceTouch", "RecorderManager pauseRecord stopRecord: " + stopRecord);

		if (recording.get() && !stopRecord.get()) {
			try {
				stopRecord.set(true);
				recording.set(false);
				mProgressView.setCurrentState(VideoProgressView.State.PAUSE);
				mProgressView.putTimeList((int) times[1]);

				videoEncodeThread.join();
				audioRecordThread.join();
				audioEncodeThread.join();

				mRecorder.stop();
				mRecorder.release();
				mRecorder=null;

				recorderFiles.add(new VideoSegment(videoTimeSingle, videoFile));
				l.segmentUpdate(recorderFiles.size());
			} catch (Exception e) {
				e.printStackTrace();
			} finally {
				semp.release();
			}
		}
	}

	public void stopRecord(final VideoMergeListener l) {
		finalVideoFile = genMegVideoName();
		LinkedList<String> videos = new LinkedList<String>();
		for (VideoSegment se : recorderFiles) {
			videos.add(se.name);
		}
		final String[] videoArr = new String[videos.size()];
		videos.toArray(videoArr);
		new Thread(new Runnable() {

			@Override
			public void run() {
				try {
					if (videoArr.length == 0) {
						if (l != null) {
							l.onComplete(-1, finalVideoFile);
						}
					} else if (videoArr.length == 1) {
						if (l != null) {
							l.onComplete(1, videoArr[0]);
						}
					} else {
//						int rst = VideoRecoder.mergeVideo(videoArr, finalVideoFile);
//						if (l != null) {
//							l.onComplete(rst, finalVideoFile);//TODO notice here
//						}
					}
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		}).start();
	}

	public static interface VideoMergeListener {
		public void onComplete(int status, String file);
	}

	public void release() {
		cameraManager.closeCamera();
		videoFrameQ.clear();
	}

	public void stopPreview() {
		cameraManager.stopPreview();
	}

	public void changeCamera() {
		cameraManager.changeCamera();
		l.cameraReady();
		cameraManager.updateParameters();

		cameraManager.setPreviewDisplay(mSurfaceHolder);
//		cameraManager.setPreviewTexture(videoSurface);
//		int[] previewSize = cameraManager.getPreviewSize();
//		cameraManager.setPreviewCallBackWithBuffer(previewSize[0], previewSize[1], this);
		cameraManager.setPreviewCallBack(this);
		cameraManager.startPreview();
	}

	public interface VideoStatusChangeListener {
		public void onVideoStatusChange(int status);
	}

	private VideoStatusChangeListener videoStatusChangeListener = new VideoStatusChangeListener() {

		@Override
		public void onVideoStatusChange(int totalTime) {

		}
	};

	public void backspace() {
		if (recorderFiles.size() > 0) {
			if (inBackspaceMode) {
				videoStatusChangeListener.onVideoStatusChange(1);
				inBackspaceMode = false;
				mProgressView.setCurrentState(VideoProgressView.State.BACKSPACE);
			} else {
				videoStatusChangeListener.onVideoStatusChange(2);
				inBackspaceMode = true;
				VideoSegment se = recorderFiles.pollLast();
				times[1] -= se.during;
				mProgressView.setCurrentState(VideoProgressView.State.DELETE);
				//deleteFileThreadStart(se.name);
				l.segmentUpdate(recorderFiles.size());
				l.timeUpdate(times[1]);
			}
		}
	}

	public static void deleteFileThreadStart(final String name) {
		new Thread(new Runnable() {

			@Override
			public void run() {
				File tmp = new File(name);
				if (tmp.exists()) {
					tmp.delete();
				}
			}
		}).start();
	}

	public void clear() {
		vieoNameIndexer = 0;
		if (recorderFiles.size() > 0) {
			deleteFileDir(sessionVedioForder);
			mProgressView.clearTimeList();
			recorderFiles.clear();
			Arrays.fill(times, 0);
			videoFrameQ.clear();
		}
	}

	long lastTs;

	@Override
	public void onPreviewFrame(byte[] data, Camera camera) {
		try {
			long now = System.currentTimeMillis();
			if (recording.get()) {
				videoTimeSingle = now - videoStartTime;
				times[1] = times[0] + videoTimeSingle;
				byte[] dataCopy = new byte[data.length];
				System.arraycopy(data, 0, dataCopy, 0, data.length);
				videoFrameQ.add(new Frame(now, dataCopy));
				l.timeUpdate(times[1]);
				lastTs = now;
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	private LinkedBlockingQueue<Frame> videoFrameQ = new LinkedBlockingQueue<Frame>();

	class VideoEncodeThread extends Thread {

		@Override
		public void run() {
			while (true) {
				if (recording.get()) {
					if (!videoFrameQ.isEmpty()) {
						Frame v = videoFrameQ.poll();
						if(mRecorder!=null&&mPreviewRunning){
							mRecorder.writeVideo(v.data,v.data.length);
						}

					}
				}
				if (stopRecord.get()) {
					if (!videoFrameQ.isEmpty()) {
						Frame v = videoFrameQ.poll();
						if(mRecorder!=null&&mPreviewRunning){
							mRecorder.writeVideo(v.data,v.data.length);
						}
					} else {
						break;

					}
				}
			}
		}
	}

	private LinkedBlockingQueue<Frame> audioFrameQ = new LinkedBlockingQueue<Frame>();

	class AudioRecordThread extends Thread {

		@Override
		public void run() {
			android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
			int sampleAudioRateInHz = 44100;
			int minBufferSize = AudioRecord.getMinBufferSize(sampleAudioRateInHz, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
			int bufferSize = 6114 >= minBufferSize ? 6114 : minBufferSize;
			audioRecorder = new AudioRecord(MediaRecorder.AudioSource.MIC, sampleAudioRateInHz, AudioFormat.CHANNEL_CONFIGURATION_MONO, AudioFormat.ENCODING_PCM_16BIT, bufferSize);

			while (audioRecorder.getState() == AudioRecord.STATE_UNINITIALIZED) {
				try {
					Thread.sleep(10);
				} catch (InterruptedException e) {
					e.printStackTrace();
				}
			}

			ByteBuffer audioData = ByteBuffer.allocate(bufferSize);
			audioRecorder.startRecording();

			while (recording.get()) {
				audioData.position(0).limit(0);
				int bufferReadResult = audioRecorder.read(audioData.array(), 0, 2048);
				audioData.limit(bufferReadResult);
				if (bufferReadResult > 0) {
					long ts = System.currentTimeMillis();
					byte[] dataCopy = new byte[audioData.array().length];
					System.arraycopy(audioData.array(), 0, dataCopy, 0, audioData.array().length);
					//					Log.e("AudioRecordThread", "ts=" + ts);
					audioFrameQ.add(new Frame(ts, dataCopy));
					//FaceVideoLib.writeAFrame(ts, audioData.array());
				}
			}
			if (audioRecorder != null) {
				try {
					audioRecorder.stop();
					audioRecorder.release();
					audioRecorder = null;
				} catch (Exception e) {
					e.printStackTrace();
				}
			}

		}

	}

	class AudioEncodeThread extends Thread {

		@Override
		public void run() {
			while (true) {
				if (recording.get()) {
					if (!audioFrameQ.isEmpty()) {
						Frame v = audioFrameQ.poll();

						if(mRecorder!=null&&mPreviewRunning){
							mRecorder.writeAudio(v.data,v.data.length);
						}
//						v.writeAFrame(v.ts, v.data);
					}
				}
				if (stopRecord.get()) {
					if (!audioFrameQ.isEmpty()) {
						Frame v = audioFrameQ.poll();

						if(mRecorder!=null&&mPreviewRunning){
							mRecorder.writeAudio(v.data,v.data.length);
						}
//						VideoRecoder.writeAFrame(v.ts, v.data);
					} else {
						break;
					}
				}
			}
		}
	}

	public String getFinalVideoFile() {
		return finalVideoFile;
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		cameraManager.openCamera();
		cameraManager.updateParameters();
		cameraManager.setPreviewDisplay(holder);
		int[] previewSize = cameraManager.getPreviewSize();

		cameraTextureView.setAspectRatio(previewSize[1], previewSize[0], AndroidUtils.getScreenSize()[0]);
		l.cameraReady();

		cameraManager.setPreviewCallBack(this);
		cameraManager.startPreview();
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		cameraManager.closeCamera();
	}

	public void setVideoRecordListener(VideoRecordListener l) {
		this.l = l;
	}

	class Frame {
		long ts;
		byte[] data;

		public Frame(long ts, byte[] data) {
			super();
			this.ts = ts;
			this.data = data;
		}
	}

	class VideoSegment {
		long during;

		public VideoSegment(long during, String name) {
			super();
			this.during = during;
			this.name = name;
		}

		String name;

		public long getDuring() {
			return during;
		}

		public VideoSegment setDuring(long during) {
			this.during = during;
			return this;
		}

		public String getName() {
			return name;
		}

		public VideoSegment setName(String name) {
			this.name = name;
			return this;
		}
	}

	public CameraManager cameraManager() {
		return cameraManager;
	}

	public void setCameraManager(CameraManager cameraManager) {
		this.cameraManager = cameraManager;
	}

	public static void deleteFileDir(String dir) {
		deleteFileDir(new File(dir));
	}

	public static void deleteFileDir(File dir) {
		try {
			if (dir.exists() && dir.isDirectory()) {
				if (dir.listFiles().length == 0) {
					dir.delete();
				} else {
					File delFile[] = dir.listFiles();
					int len = dir.listFiles().length;
					for (int j = 0; j < len; j++) {
						if (delFile[j].isDirectory()) {
							deleteFileDir(delFile[j]);
						} else {
							delFile[j].delete();
						}
					}
					delFile = null;
				}
				deleteFileDir(dir);
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public void setVideoStatusChangeListener(VideoStatusChangeListener videoStatusChangeListener) {
		this.videoStatusChangeListener = videoStatusChangeListener;
	}
}
