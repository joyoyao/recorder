package com.abcew.videorecoder;

import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;
import android.os.Build;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

import java.util.Collections;
import java.util.Comparator;
import java.util.List;

@SuppressWarnings("deprecation")
public class CameraManager {
	private Camera camera = null;
	private boolean isPreviewOn = false;
	private int cameraFacingType = CameraInfo.CAMERA_FACING_BACK;
	private Activity activity;
	private int defaultScreenResolution = -1;

	private int previewSize[] = new int[2];

	public CameraManager(Activity activity) {
		super();
		this.activity = activity;
	}

	Thread startThread;

	public void openCamera() {
		startThread = new Thread(new Runnable() {
			@Override
			public void run() {
				int n = Camera.getNumberOfCameras();
				if (n > 1) {
					for (int i = 0; i < n; i++) {
						CameraInfo info = new CameraInfo();
						Camera.getCameraInfo(i, info);
						if (info.facing == cameraFacingType) {
							camera = Camera.open(i);
							break;
						}
					}
				} else {
					camera = Camera.open();
					cameraFacingType = CameraInfo.CAMERA_FACING_BACK;
				}
			}
		});
		startThread.start();
	}

	public boolean flashEnable() {
		return activity.getPackageManager().hasSystemFeature(PackageManager.FEATURE_CAMERA_FLASH)
				&& cameraFacingType == CameraInfo.CAMERA_FACING_BACK;

	}

	private void startThreadJoin() {
		if (startThread != null) {
			try {
				startThread.join();
			} catch (Exception e) {
				//ignore
			}
		}
	}

	public void setPreviewDisplay(SurfaceHolder holder) {
		try {
			startThreadJoin();
			camera.setPreviewDisplay(holder);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public void setPreviewTexture(SurfaceTexture surfaceTexture) {
		try {
			startThreadJoin();
			camera.setPreviewTexture(surfaceTexture);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	public void setPreviewCallBack(PreviewCallback callback) {
		camera.setPreviewCallback(callback);
	}

	boolean previewing = false;

	public boolean isPreviewing() {
		return previewing;
	}

	public boolean cameraChangeEnable() {
		return Camera.getNumberOfCameras() > 1;
	}

	public void setPreviewCallBackWithBuffer(int previewWidth, int previewHeight, PreviewCallback callback) {
		int buffersize = previewWidth * previewHeight * ImageFormat.getBitsPerPixel(ImageFormat.NV21) / 8;
		byte[] previewBuffer = new byte[buffersize];
		startThreadJoin();
		camera.addCallbackBuffer(previewBuffer);
		camera.setPreviewCallbackWithBuffer(callback);
	}

	public void startPreview() {
		if (!isPreviewOn && camera != null) {
			isPreviewOn = true;
			startThreadJoin();
			camera.startPreview();
			previewing = true;
		}
	}

	public void stopPreview() {
		if (isPreviewOn && camera != null) {
			isPreviewOn = false;
			camera.stopPreview();
			previewing = false;
		}
	}

	public void closeCamera() {
		if (camera != null) {
			stopPreview();
			camera.setPreviewCallback(null);
			camera.release();
			camera = null;
		}
	}

	public boolean supportFocus() {
		Parameters mParameters = camera.getParameters();
		return (mParameters.getMaxNumFocusAreas() > 0 ? true : false);
	}

	public boolean isUseBackCamera() {
		startThreadJoin();
		return cameraFacingType == CameraInfo.CAMERA_FACING_BACK;
	}

	public boolean isUseFrontCamera() {
		startThreadJoin();
		return cameraFacingType == CameraInfo.CAMERA_FACING_FRONT;
	}

	public void useBackCamera() {
		startThreadJoin();
		cameraFacingType = CameraInfo.CAMERA_FACING_BACK;
	}

	public void useFrontCamera() {
		startThreadJoin();
		cameraFacingType = CameraInfo.CAMERA_FACING_FRONT;
	}

	public void changeCamera() {
		startThreadJoin();
		if (cameraFacingType == CameraInfo.CAMERA_FACING_BACK) {
			useFrontCamera();
		} else if (cameraFacingType == CameraInfo.CAMERA_FACING_FRONT) {
			useBackCamera();
		}
		closeCamera();
		openCamera();
	}

	public void updateParameters() {
		startThreadJoin();
		Camera.Parameters camParams = camera.getParameters();

		Parameters parameters = camera.getParameters();
		List<Size> supportedPreviewSizes = parameters.getSupportedPreviewSizes();
		if (supportedPreviewSizes != null && supportedPreviewSizes.size() > 0) {
			Collections.sort(supportedPreviewSizes, new ResolutionComparator());
			Camera.Size preSize = null;
			if (defaultScreenResolution == -1) {
				boolean hasSize = false;
				for (int i = 0; i < supportedPreviewSizes.size(); i++) {
					Size size = supportedPreviewSizes.get(i);
					if (size != null && size.width == 640 && size.height == 480) {
						preSize = size;
						hasSize = true;
						break;
					}
				}
				if (!hasSize) {
					int mediumResolution = supportedPreviewSizes.size() / 2;
					if (mediumResolution >= supportedPreviewSizes.size()) {
						mediumResolution = supportedPreviewSizes.size() - 1;
					}
					preSize = supportedPreviewSizes.get(mediumResolution);
				}
			} else {
				if (defaultScreenResolution >= supportedPreviewSizes.size()) {
					defaultScreenResolution = supportedPreviewSizes.size() - 1;
				}
				preSize = supportedPreviewSizes.get(defaultScreenResolution);
			}
			if (preSize != null) {
				previewSize[0] = preSize.width;
				previewSize[1] = preSize.height;
				camParams.setPreviewSize(previewSize[0], previewSize[1]);
			}
		}

		if (Build.VERSION.SDK_INT > Build.VERSION_CODES.FROYO) {
			camera.setDisplayOrientation(determineDisplayOrientation(activity, cameraFacingType));
			List<String> focusModes = camParams.getSupportedFocusModes();
			if (cameraFacingType == CameraInfo.CAMERA_FACING_BACK && focusModes != null) {//  fix
				Log.i("video", Build.MODEL);
				if (((Build.MODEL.startsWith("GT-I950")) || (Build.MODEL.endsWith("SCH-I959")) || (Build.MODEL
					.endsWith("MEIZU MX3"))) && focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
					camParams.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
				} else if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
					camParams.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
				} else {
					camParams.setFocusMode(Camera.Parameters.FOCUS_MODE_FIXED);
				}
			}
		} else {
			camera.setDisplayOrientation(90);

		}
		camera.setParameters(camParams);
	}

	public void doFocus(List<Camera.Area> focusList) {
		Camera.Parameters param = camera.getParameters();
		param.setFocusAreas(focusList);
		param.setMeteringAreas(focusList);
		try {
			camera.setParameters(param);
		} catch (Exception e) {
			camera.autoFocus(null);
		}
	}

	public boolean changeFlash() {
		boolean flashOn = false;
		if (flashEnable()) {
			Parameters params = camera.getParameters();
			if (Parameters.FLASH_MODE_TORCH.equals(params.getFlashMode())) {
				params.setFlashMode(Parameters.FLASH_MODE_OFF);
				flashOn = false;
			} else {
				params.setFlashMode(Parameters.FLASH_MODE_TORCH);
				flashOn = true;
			}
			camera.setParameters(params);
		}
		return flashOn;
	}

	public void closeFlash() {
		if (flashEnable()) {
			Parameters params = camera.getParameters();
			if (Parameters.FLASH_MODE_TORCH.equals(params.getFlashMode())) {
				params.setFlashMode(Parameters.FLASH_MODE_OFF);
				camera.setParameters(params);
			}
		}
	}

	public Camera getCamera() {
		startThreadJoin();

		return camera;
	}

	public int[] getPreviewSize() {
		startThreadJoin();

		return previewSize;
	}

	public static int determineDisplayOrientation(Activity activity, int defaultCameraId) {
		int displayOrientation = 0;
		if (Build.VERSION.SDK_INT > Build.VERSION_CODES.FROYO) {
			CameraInfo cameraInfo = new CameraInfo();
			Camera.getCameraInfo(defaultCameraId, cameraInfo);

			int degrees = getRotationAngle(activity);

			if (cameraInfo.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
				displayOrientation = (cameraInfo.orientation + degrees) % 360;
				displayOrientation = (360 - displayOrientation) % 360;
			} else {
				displayOrientation = (cameraInfo.orientation - degrees + 360) % 360;
			}
		}
		return displayOrientation;
	}

	public static int getRotationAngle(Activity activity) {
		int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();
		int degrees = 0;

		switch (rotation) {
		case Surface.ROTATION_0:
			degrees = 0;
			break;
		case Surface.ROTATION_90:
			degrees = 90;
			break;
		case Surface.ROTATION_180:
			degrees = 180;
			break;
		case Surface.ROTATION_270:
			degrees = 270;
			break;
		}
		return degrees;
	}

	class ResolutionComparator implements Comparator<Size> {
		@Override
		public int compare(Camera.Size size1, Camera.Size size2) {
			if (size1.height != size2.height) {
				return size1.height - size2.height;
			} else {
				return size1.width - size2.width;
			}
		}
	}
}
