package com.abcew.videorecoder;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.RectF;
import android.os.Build;
import android.os.Environment;
import android.os.StatFs;
import android.view.Display;
import android.view.WindowManager;

import java.io.File;
import java.math.BigDecimal;

public class AndroidUtils {

	public static Context context;

	public static void init(Context context) {
		AndroidUtils.context = context;
	}

	public static void pauseAudioPlayback() {
		Intent i = new Intent("com.android.music.musicservicecommand");
		i.putExtra("command", "pause");
		context.sendBroadcast(i);
	}

	private static boolean checkFsWritable() {
		String directoryName = Environment.getExternalStorageDirectory().toString() + "/face";
		File directory = new File(directoryName);
		if (!directory.isDirectory()) {
			if (!directory.mkdirs()) {
				return false;
			}
		}
		return directory.canWrite();
	}

	public static boolean hasStorage(boolean requireWriteAccess) {
		String state = Environment.getExternalStorageState();
		if (Environment.MEDIA_MOUNTED.equals(state)) {
			if (requireWriteAccess) {
				boolean writable = checkFsWritable();
				return writable;
			} else {
				return true;
			}
		} else if (!requireWriteAccess && Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
			return true;
		}
		return false;
	}

	@SuppressWarnings("deprecation")
	public static long getAvailableStorage() {
		try {
			if (!hasStorage(true)) {
				return 0;
			} else {
				String storageDirectory = Environment.getExternalStorageDirectory().toString();
				StatFs stat = new StatFs(storageDirectory);
				return (long) stat.getAvailableBlocks() * (long) stat.getBlockSize();
			}
		} catch (Exception ex) {
			return 0;
		}
	}

	@SuppressWarnings("deprecation")
	@SuppressLint("NewApi")
	public static int[] getScreenSize() {
		int[] res = new int[2];
		Display display = ((WindowManager) context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
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

	public static Bitmap creatBitmapARGB8888(int width, int height) {
		return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

	}

	public static int dip2px(float dpValue) {
		final float scale = context.getResources().getDisplayMetrics().density;
		return (int) (dpValue * scale + 0.5f);
	}

	public static int px2dip(float pxValue) {
		final float scale = context.getResources().getDisplayMetrics().density;
		return (int) (pxValue / scale + 0.5f);
	}

	/**
	 * 按正方形裁切图片
	 */
	public static Bitmap cropImageToSquare(Bitmap bitmap) {
		int w = bitmap.getWidth(); // 得到图片的宽，高
		int h = bitmap.getHeight();
		if (w == h) {
			return bitmap;
		}
		int wh = w > h ? h : w;// 裁切后所取的正方形区域边长
		int retX = w > h ? (w - h) / 2 : 0;// 基于原图，取正方形左上角x坐标
		int retY = w > h ? 0 : (h - w) / 2;
		return Bitmap.createBitmap(bitmap, retX, retY, wh, wh, null, false);
	}

	public static Bitmap cropImage(Bitmap bitmap, int x, int y, int width, int height) {
		return Bitmap.createBitmap(bitmap, x, y, width, height, null, false);
	}

	public static Bitmap cropImage(Bitmap bitmap, RectF rect) {
		return Bitmap.createBitmap(bitmap, (int) rect.left, (int) rect.top, (int) (rect.right - rect.left),
				(int) (rect.bottom - rect.top), null, false);
	}

	public static Bitmap addToImage(Bitmap src, Bitmap add, int x, int y) {
		Bitmap tmp = src.copy(Bitmap.Config.ARGB_8888, true);
		Canvas canvas = new Canvas(tmp);
		canvas.drawBitmap(add, x, y, null);//插入图标 
		canvas.save(Canvas.ALL_SAVE_FLAG);
		//存储新合成的图片
		canvas.restore();
		return tmp;
	}
	// 放大缩小图片
	public static Bitmap zoomBitmap(Bitmap bitmap, int w, int h) {
		int width = bitmap.getWidth();
		int height = bitmap.getHeight();
		if (width == w && height == h) {
			return bitmap;
		}
		Matrix matrix = new Matrix();
		float scaleWidht = ((float) w / width);
		float scaleHeight = ((float) h / height);
		matrix.postScale(scaleWidht, scaleHeight);
		Bitmap newbmp = Bitmap.createBitmap(bitmap, 0, 0, width, height, matrix, true);
		return newbmp;
	}

	public static Bitmap reduce(Bitmap bitmap, int width, int height, boolean isAdjust) {
		if (bitmap.getWidth() < width && bitmap.getHeight() < height) {
			return bitmap;
		}
		float sx = new BigDecimal(width)
			.divide(new BigDecimal(bitmap.getWidth()), 4, BigDecimal.ROUND_DOWN)
			.floatValue();
		float sy = new BigDecimal(height)
			.divide(new BigDecimal(bitmap.getHeight()), 4, BigDecimal.ROUND_DOWN)
			.floatValue();
		if (isAdjust) {
			sx = (sx < sy ? sx : sy);
			sy = sx;
		}
		Matrix matrix = new Matrix();
		matrix.postScale(sx, sy);
		return Bitmap.createBitmap(bitmap, 0, 0, bitmap.getWidth(), bitmap.getHeight(), matrix, true);
	}
}
