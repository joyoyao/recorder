package com.abcew.videorecoder;

import android.util.Log;

public class Logger {
	private static String DEFAULT_TAG = "FaceRecorder ==>";
	private final static boolean DEBUG = true;

	public static void setTag(String tag) {
		DEFAULT_TAG = tag;
	}

	public static int d(Object msg) {
		return Log.d(Logger.DEFAULT_TAG, String.valueOf(msg));
	}

	public static int d(Object msg, Throwable tr) {
		return Log.d(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
	}

	public static int d(String tag, Object msg) {
		return Log.d(tag, String.valueOf(msg));
	}

	public static int d(String tag, Object msg, Throwable tr) {
		return Log.d(tag, String.valueOf(msg), tr);
	}

	public static int e(Object msg) {
		return Log.e(Logger.DEFAULT_TAG, String.valueOf(msg));
	}

	public static int e(Object msg, Throwable tr) {
		return Log.e(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
	}

	public static int e(String tag, Object msg) {
		return Log.d(tag, String.valueOf(msg));
	}

	public static int e(String tag, Object msg, Throwable tr) {
		return Log.e(tag, String.valueOf(msg), tr);
	}

	public static int i(Object msg) {
		return Log.i(Logger.DEFAULT_TAG, String.valueOf(msg));
	}

	public static int i(Object msg, Throwable tr) {
		return Log.i(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
	}

	public static int i(String tag, Object msg) {
		return Log.i(tag, String.valueOf(msg));
	}

	public static int i(String tag, Object msg, Throwable tr) {
		return Log.i(tag, String.valueOf(msg), tr);
	}

	public static void printStackTrace(Throwable e) {
		if (e == null) {
			return;
		}
		if (DEBUG) {
			e.printStackTrace();
		}
	}

	public static void t(Object msg) {
		if (DEBUG) {
			Log.v(Logger.DEFAULT_TAG, String.valueOf(msg));
		}
	}

	public static void t(Object msg, Throwable tr) {
		if (DEBUG) {
			Log.v(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
		}
	}

	public static void t(String tag, Object msg) {
		if (DEBUG) {
			Log.v(tag, String.valueOf(msg));
		}
	}

	public static int v(Object msg) {
		return Log.v(Logger.DEFAULT_TAG, String.valueOf(msg));
	}

	public static int v(Object msg, Throwable tr) {
		return Log.v(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
	}

	public static int v(String tag, Object msg) {
		return Log.v(tag, String.valueOf(msg));
	}

	public static int v(String tag, Object msg, Throwable tr) {
		return Log.v(tag, String.valueOf(msg), tr);
	}

	public static int w(Object msg) {
		return Log.w(Logger.DEFAULT_TAG, String.valueOf(msg));
	}

	public static int w(Object msg, Throwable tr) {
		return Log.w(Logger.DEFAULT_TAG, String.valueOf(msg), tr);
	}

	public static int w(String tag, Object msg) {
		return Log.w(tag, String.valueOf(msg));
	}

	public static int w(String tag, Object msg, Throwable tr) {
		return Log.w(tag, String.valueOf(msg), tr);
	}

	public static int w(String tag, Throwable tr) {
		return Log.w(tag, tr);
	}

	public static int w(Throwable tr) {
		return Log.w(Logger.DEFAULT_TAG, tr);
	}

	private Logger() {
	}
}
