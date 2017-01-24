package com.abcew.videorecoder.view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.SurfaceView;

public class VideoPreviewView extends SurfaceView {
	public static float DONT_CARE = 0.0f;
	private double mAspectRatio = DONT_CARE;
	private int mCameraW;
	private int mCameraH;
	private int mSqueraW;

	public VideoPreviewView(Context context) {
		super(context);
	}

	public VideoPreviewView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public VideoPreviewView(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
	}

	public void setAspectRatio(int cameraW, int cameraH, int squareW) {
		mCameraW = cameraW;
		mCameraH = cameraH;
		mSqueraW = squareW;
		double ratio = (double) mCameraW / mCameraH;
		if (ratio <= 0.0) {
			throw new IllegalArgumentException();
		}
		if (mAspectRatio != ratio) {
			mAspectRatio = ratio;
			requestLayout();
			invalidate();
		}
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		if (mAspectRatio != DONT_CARE) {
			int widthSpecSize = MeasureSpec.getSize(widthMeasureSpec);
			int heightSpecSize = MeasureSpec.getSize(heightMeasureSpec);

			int width = widthSpecSize;
			int height = heightSpecSize;

			if (width > 0 && height > 0) {

				int min = Math.min(mCameraW, mCameraH);
				if (min < mSqueraW) {
					float scale = (float) mSqueraW / min;
					setMeasuredDimension((int) (mCameraW * scale), (int) (mCameraH * scale));
				} else {
					float scale = (float) min / mSqueraW;
					setMeasuredDimension((int) (mCameraW / scale), (int) (mCameraH / scale));
				}
				return;
			}
		}

		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
	}
}
