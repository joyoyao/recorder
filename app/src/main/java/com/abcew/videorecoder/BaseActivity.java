package com.abcew.videorecoder;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Point;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;

import butterknife.ButterKnife;

public abstract class BaseActivity extends Activity {
	public int[] screenSize;
	public Context context;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		context = this;
		super.onCreate(savedInstanceState);
	}

	@Override
	public void setContentView(int layoutResID) {
		super.setContentView(layoutResID);
		ButterKnife.bind(this);
		init();
	}

	private void init() {
		screenSize = getScreenSize();
	}

	@SuppressWarnings("deprecation")
	@SuppressLint("NewApi")
	private int[] getScreenSize() {
		int[] res = new int[2];
		Display display = getWindowManager().getDefaultDisplay();
		if (Build.VERSION.SDK_INT >= 13) {
			Point size = new Point();
			display.getSize(size);
			res[0] = size.x;
			res[1] = size.y;
		} else {
			res[0] = display.getWidth(); // deprecated
			res[1] = display.getHeight(); // deprecated
		}
		return res;
	}

	protected void startActivity(final Class<?> activityClass, Bundle extras) {
		Intent intent = new Intent(this, activityClass);
		intent.putExtras(extras);
		startActivity(intent);
	}

	protected void startActivity(final Class<?> activityClass) {
		startActivity(new Intent(this, activityClass));
	}
}
