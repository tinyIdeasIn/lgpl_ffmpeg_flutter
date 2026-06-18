package com.addcn.lgpl_ffmpeg_flutter

import android.content.Context
import org.json.JSONObject
import io.flutter.embedding.engine.plugins.FlutterPlugin
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.common.MethodChannel.MethodCallHandler
import io.flutter.plugin.common.MethodChannel.Result

/** Controlled Flutter wrapper for an LGPL FFmpeg backend. */
class LgplFfmpegFlutterPlugin :
    FlutterPlugin,
    MethodCallHandler {
    private lateinit var channel: MethodChannel
    private lateinit var applicationContext: Context

    override fun onAttachedToEngine(flutterPluginBinding: FlutterPlugin.FlutterPluginBinding) {
        applicationContext = flutterPluginBinding.applicationContext
        channel = MethodChannel(flutterPluginBinding.binaryMessenger, "lgpl_ffmpeg_flutter")
        channel.setMethodCallHandler(this)
    }

    override fun onMethodCall(
        call: MethodCall,
        result: Result
    ) {
        when (call.method) {
            "readInfo" -> handleReadInfo(call, result)
            "generateCover" -> handleGenerateCover(call, result)
            else -> result.notImplemented()
        }
    }

    override fun onDetachedFromEngine(binding: FlutterPlugin.FlutterPluginBinding) {
        channel.setMethodCallHandler(null)
    }

    private fun handleGenerateCover(
        call: MethodCall,
        result: Result
    ) {
        val videoPath = call.argument<String>("videoPath")
        if (videoPath.isNullOrBlank()) {
            result.error("invalidPath", "videoPath must not be empty.", null)
            return
        }

        val maxLongEdge = call.argument<Int>("maxLongEdge") ?: 1920
        val quality = call.argument<Int>("quality") ?: 95
        if (maxLongEdge <= 0 || quality !in 1..100) {
            result.error(
                "invalidArgument",
                "maxLongEdge must be greater than 0 and quality must be between 1 and 100.",
                null
            )
            return
        }

        runBackend(result) {
            nativeGenerateCover(
                videoPath,
                call.argument<List<Number>>("preferredTimesMs")?.map { it.toLong() }?.toLongArray(),
                maxLongEdge,
                quality,
                applicationContext.cacheDir.absolutePath
            )
        }
    }

    private fun handleReadInfo(
        call: MethodCall,
        result: Result
    ) {
        val videoPath = call.argument<String>("videoPath")
        if (videoPath.isNullOrBlank()) {
            result.error("invalidPath", "videoPath must not be empty.", null)
            return
        }

        runBackend(result) { nativeReadInfo(videoPath) }
    }

    private fun runBackend(
        result: Result,
        block: () -> String
    ) {
        if (!backendLoaded) {
            result.error(
                "ffmpegUnavailable",
                backendLoadError ?: "LGPL FFmpeg dynamic libraries are not bundled.",
                null
            )
            return
        }

        try {
            val json = JSONObject(block())
            val errorCode = json.optString("errorCode", "")
            if (errorCode.isNotEmpty()) {
                result.error(
                    errorCode,
                    json.optString("message", errorCode),
                    null
                )
                return
            }
            result.success(jsonToMap(json))
        } catch (exception: NativeVideoProcessException) {
            result.error(exception.code, exception.message, exception.details)
        } catch (exception: Throwable) {
            result.error("unknown", exception.message ?: "Unexpected native error.", null)
        }
    }

    private fun jsonToMap(json: JSONObject): Map<String, Any?> {
        val map = mutableMapOf<String, Any?>()
        val keys = json.keys()
        while (keys.hasNext()) {
            val key = keys.next()
            map[key] = if (json.isNull(key)) null else json.get(key)
        }
        return map
    }

    private external fun nativeReadInfo(videoPath: String): String

    private external fun nativeGenerateCover(
        videoPath: String,
        preferredTimesMs: LongArray?,
        maxLongEdge: Int,
        quality: Int,
        cacheDir: String
    ): String

    private class NativeVideoProcessException(
        val code: String,
        override val message: String,
        val details: Any? = null
    ) : RuntimeException(message)

    companion object {
        private val backendLoadError: String?
        private val backendLoaded: Boolean

        init {
            var loaded = false
            var error: String? = null
            try {
                System.loadLibrary("avutil")
                System.loadLibrary("swresample")
                System.loadLibrary("swscale")
                System.loadLibrary("avcodec")
                System.loadLibrary("avformat")
                System.loadLibrary("lgpl_ffmpeg_flutter_backend")
                loaded = true
            } catch (throwable: Throwable) {
                error = throwable.message
            }
            backendLoaded = loaded
            backendLoadError = error
        }
    }
}
