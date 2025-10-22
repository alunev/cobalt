package dev.cobalt.coat;

import android.util.Log;

/**
 * Abstract class to handle hang reports from native code.
 * This class provides a JNI bridge for reporting hangs from Cobalt to the application layer.
 */
public abstract class NativeHangReportHandler {
    private static final String TAG = "NativeHangRptHdlr";
    private static volatile NativeHangReportHandler instance;

    public static void setInstance(NativeHangReportHandler impl) {
        if (instance == null) {
            instance = impl;
            Log.i(TAG, "NativeHangReportHandler instance set.");
        } else {
            Log.w(TAG, "NativeHangReportHandler instance already set.");
        }
    }

    public static NativeHangReportHandler getInstance() {
        return instance;
    }

    /**
     * Called by the native C++ code via JNI to report a detected hang.
     *
     * @param threadTypeName The type of thread that hung (e.g., "Renderer").
     * @param threadId The native thread ID (TID) of the hung thread.
     * @param hangDurationMs The duration of the hang in milliseconds.
     * @param stackTrace The captured native stack trace of the hung thread.
     */
    public abstract void reportHangFromNative(String threadTypeName, long threadId, long hangDurationMs, String stackTrace);

    /**
     * Static method called from the JNI layer.
     */
    protected static void onNativeHang(String threadTypeName, long threadId, long hangDurationMs, String stackTrace) {
        Log.i(TAG, "onNativeHang called from JNI. TID: " + threadId);
        if (instance != null) {
            instance.reportHangFromNative(threadTypeName, threadId, hangDurationMs, stackTrace);
        } else {
            Log.e(TAG, "NativeHangReportHandler instance is null. Cannot report hang.");
        }
    }
}
