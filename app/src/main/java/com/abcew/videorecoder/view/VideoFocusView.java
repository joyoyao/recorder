package com.abcew.videorecoder.view;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.TextView;

import com.abcew.videorecoder.R;


public class VideoFocusView extends RelativeLayout {

	private static final String GUIDE_PREFERENCE_KEY = "SHOWNOTICE";
	private static final String GUIDE_SHOWFILPCAMREA_KEY = "SHOWFILPCAMREA";
	private static final String VIDEO_SETTING_NAME = "VIDEO_SETTING";
	public static final int FOCUS_IMG_WH = 120;
	private boolean mHaveTouch = false;
	private boolean mShowGrid = false;
	private ImageView mFocusImg;
	private ImageView mChangeCamera;
	private TextView mGuideNotice;
	private GuideStep mCurrentStep = GuideStep.STEP1;
	private Context context;

	@SuppressWarnings("deprecation")
	public VideoFocusView(Context context, AttributeSet attrs) {
		super(context, attrs);
		this.context = context;
		mHaveTouch = false;

		mFocusImg = new ImageView(context);
		mFocusImg.setVisibility(View.GONE);
		mFocusImg.setBackgroundResource(R.drawable.video_record_image_focus);
		addView(mFocusImg);

		if (isShowFilpCamrea()) {
			mChangeCamera = new ImageView(context);
			mChangeCamera.setBackgroundResource(R.drawable.video_record_chage_camrea_arrow);
			LayoutParams layout = new LayoutParams(android.view.ViewGroup.LayoutParams.WRAP_CONTENT,
					android.view.ViewGroup.LayoutParams.WRAP_CONTENT);
			layout.addRule(RelativeLayout.CENTER_IN_PARENT);
			mChangeCamera.setLayoutParams(layout);
			mChangeCamera.setOnClickListener(new OnClickListener() {
				
				@Override
				public void onClick(View v) {
					mChangeCamera.setVisibility(View.GONE);
				}
			});
			addView(mChangeCamera);
		}

		if (isShowGuide()) {
			mGuideNotice = new TextView(context);
			mGuideNotice.setVisibility(View.GONE);
			mGuideNotice.setTextColor(Color.WHITE);
			mGuideNotice.setBackgroundColor(context.getResources().getColor(R.color.video_record_guide_notice));
			mGuideNotice.setGravity(Gravity.CENTER);
			LayoutParams noticeParams = new LayoutParams(android.view.ViewGroup.LayoutParams.MATCH_PARENT,
					android.view.ViewGroup.LayoutParams.WRAP_CONTENT);
			noticeParams.addRule(RelativeLayout.ALIGN_PARENT_BOTTOM);
			mGuideNotice.setLayoutParams(noticeParams);
			mGuideNotice.setPadding(0, 25, 0, 25);
			mGuideNotice.setText("按住拍摄键，开始拍摄");
			addView(mGuideNotice);
		}
	}

	public boolean isChangeCameraShow() {
		if (mChangeCamera == null) {
			return false;
		} else {
			return mChangeCamera.getVisibility() == View.VISIBLE;
		}
	}

	public void hideChangeCamera() {
		if (mChangeCamera != null) {
			mChangeCamera.setVisibility(View.GONE);
			addShowCameraFlipCount();
		}
	}

	float downY;

	public void setDownY(float downY) {
		this.downY = downY;
	}

	public float getDownY() {
		return downY;
	}

	public void changeCamreaFlipUp() {
		if (mChangeCamera == null) {
			return;
		}
		final ObjectAnimator anim = ObjectAnimator.ofFloat(mChangeCamera, "rotationX", 0, -100);
		anim.setDuration(1000);
		Interpolator decelerator = new DecelerateInterpolator();
		anim.setInterpolator(decelerator);
		anim.addListener(new AnimatorListenerAdapter() {
			@Override
			public void onAnimationEnd(Animator anim) {
				mChangeCamera.setVisibility(View.GONE);
				addShowCameraFlipCount();
			}
		});
		anim.start();
	}

	public void setHaveTouch(boolean val, Rect rect) {
		mHaveTouch = val;
		if (mHaveTouch) {
			LayoutParams params = (LayoutParams) mFocusImg.getLayoutParams();
			params.leftMargin = rect.left;
			params.topMargin = rect.top;
			params.width = rect.right - rect.left;
			//params.width = rect.right - rect.left+50;
			params.height = rect.bottom - rect.top;
			mFocusImg.setLayoutParams(params);
			mFocusImg.setVisibility(View.VISIBLE);
			startAnimation(mFocusImg, 600, 0, 0, 1.25f, 1f);
		} else {
			mFocusImg.setVisibility(View.GONE);
			mFocusImg.clearAnimation();
		}
	}

	public void setShowGrid(boolean isShow) {
		mShowGrid = isShow;
	}

	public boolean getShowGrid() {

		return mShowGrid;
	}

	private AnimatorSet startAnimation(View view, long duration, int repeatCount, long delay, float... scale) {

		AnimatorSet as = new AnimatorSet();
		as.setInterpolator(new AccelerateDecelerateInterpolator());
		as.setStartDelay(delay);
		ObjectAnimator animY = ObjectAnimator.ofFloat(view, "scaleY", scale);
		ObjectAnimator animX = ObjectAnimator.ofFloat(view, "scaleX", scale);
		animY.setDuration(duration);
		animX.setDuration(duration);
		animY.setRepeatCount(repeatCount);
		animX.setRepeatCount(repeatCount);

		as.playTogether(animY, animX);
		as.start();
		return as;
	}

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {

		int specWidthSize = MeasureSpec.getSize(widthMeasureSpec);
		setMeasuredDimension(specWidthSize, specWidthSize);
		super.onMeasure(widthMeasureSpec, widthMeasureSpec);
	}

	public boolean isShowFilpCamrea() {
		return context.getSharedPreferences(VIDEO_SETTING_NAME, Context.MODE_PRIVATE).getInt(GUIDE_SHOWFILPCAMREA_KEY,
				0) < 1;
	}

	public void addShowCameraFlipCount() {
		SharedPreferences sp = context.getSharedPreferences(VIDEO_SETTING_NAME, Context.MODE_PRIVATE);
		int count = sp.getInt(GUIDE_SHOWFILPCAMREA_KEY, 0);
		sp.edit().putInt(GUIDE_SHOWFILPCAMREA_KEY, count + 1).commit();
	}

	public boolean isShowGuide() {
		SharedPreferences sp = context.getSharedPreferences(VIDEO_SETTING_NAME, Context.MODE_PRIVATE);
		return sp.getInt(GUIDE_PREFERENCE_KEY, 0) < 1;
	}

	public void addShowGuideCount() {
		SharedPreferences sp = context.getSharedPreferences(VIDEO_SETTING_NAME, Context.MODE_PRIVATE);
		int count = sp.getInt(GUIDE_PREFERENCE_KEY, 0);
		sp.edit().putInt(GUIDE_PREFERENCE_KEY, count + 1).commit();
	}

	public void finishGuide() {
		if (isShowGuide()) {
			addShowGuideCount();
			dismissGuideNotice();
		}
	}

	public void showGuideStep(int step) {
		if (!isShowGuide()) {
			return;
		}
		if (mGuideNotice == null) {
			return;
		}
		mGuideNotice.setVisibility(View.VISIBLE);
		switch (step) {
		case 1:
			mCurrentStep = GuideStep.STEP1;
			mGuideNotice.setText("按住拍摄键，开始拍摄");
			break;
		case 2:
			mCurrentStep = GuideStep.STEP2;
			mGuideNotice.setText("松开暂停");
			break;
		case 3:
			mCurrentStep = GuideStep.STEP3;
			mGuideNotice.setText("试试换个场景继续拍摄");
			break;
		}
	}

	public void dismissGuideNotice() {
		if (mGuideNotice != null) {
			mGuideNotice.setVisibility(View.GONE);
		}
	}

	public GuideStep getCurrentStep() {
		return mCurrentStep;
	}

	public enum GuideStep {
		STEP1, STEP2, STEP3
	};

}
