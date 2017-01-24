package com.abcew.videorecoder;

public class Conf {
	public static final int VIDEO_FRAMERATE = 20;
	public static final String BASE_DIR = "/org.easydarwin.video";
	public static final String TMP_DIR = BASE_DIR + "/tmp";
	public static final String VS_DIR = BASE_DIR + "/.vs";
	public static final String VIDEO_TMP_DIR = TMP_DIR + "/.video";

	public static final float MAX_RECORD_TIME = 15 * 1000f;
	public static final float MIN_RECORD_TIME = 2 * 1000f;
}
