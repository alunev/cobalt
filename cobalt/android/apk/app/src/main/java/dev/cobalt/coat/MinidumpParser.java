package dev.cobalt.coat;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Parses minidumps to extract stack traces for hung threads.
 */
@JNINamespace("cobalt::android")
public class MinidumpParser {
    public static String[] getStackTraces(String minidumpPath) {
        return MinidumpParserJni.get().getStackTracesFromMinidump(minidumpPath);
    }

    @NativeMethods
    interface Natives {
        String[] getStackTracesFromMinidump(String minidumpPath);
    }
}
