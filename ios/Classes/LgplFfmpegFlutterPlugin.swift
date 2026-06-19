import Flutter
import UIKit
import lgpl_ffmpeg_flutter

public class LgplFfmpegFlutterPlugin: NSObject, FlutterPlugin {
  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "lgpl_ffmpeg_flutter", binaryMessenger: registrar.messenger())
    let instance = LgplFfmpegFlutterPlugin()
    registrar.addMethodCallDelegate(instance, channel: channel)
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "readInfo":
      handleReadInfo(call, result: result)
    case "backendInfo":
      result(mapResult(from: lgpl_ffmpeg_flutter_backend_info()))
    case "generateCover":
      handleGenerateCover(call, result: result)
    case "extractFrame":
      handleExtractFrame(call, result: result)
    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private func handleReadInfo(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    guard let arguments = call.arguments as? [String: Any],
          let videoPath = arguments["videoPath"] as? String,
          !videoPath.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
      result(FlutterError(code: "invalidPath", message: "videoPath must not be empty.", details: nil))
      return
    }

    result(mapResult(from: lgpl_ffmpeg_flutter_read_info(videoPath)))
  }

  private func handleGenerateCover(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    guard let arguments = call.arguments as? [String: Any],
          let videoPath = arguments["videoPath"] as? String,
          !videoPath.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
      result(FlutterError(code: "invalidPath", message: "videoPath must not be empty.", details: nil))
      return
    }

    let maxLongEdge = arguments["maxLongEdge"] as? Int ?? 1920
    let quality = arguments["quality"] as? Int ?? 95
    guard maxLongEdge > 0, (1...100).contains(quality) else {
      result(FlutterError(
        code: "invalidArgument",
        message: "maxLongEdge must be greater than 0 and quality must be between 1 and 100.",
        details: nil
      ))
      return
    }

    let preferredTimesMs = arguments["preferredTimesMs"] as? [Int64] ?? []
    let cacheDir = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)
      .first?
      .path ?? NSTemporaryDirectory()
    let nativeResult = preferredTimesMs.withUnsafeBufferPointer { buffer in
      lgpl_ffmpeg_flutter_generate_cover(
        videoPath,
        buffer.baseAddress,
        Int32(buffer.count),
        Int32(maxLongEdge),
        Int32(quality),
        cacheDir
      )
    }
    result(mapResult(from: nativeResult))
  }

  private func handleExtractFrame(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    guard let arguments = call.arguments as? [String: Any],
          let videoPath = arguments["videoPath"] as? String,
          !videoPath.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
      result(FlutterError(code: "invalidPath", message: "videoPath must not be empty.", details: nil))
      return
    }

    let maxLongEdge = arguments["maxLongEdge"] as? Int ?? 1920
    let quality = arguments["quality"] as? Int ?? 95
    guard maxLongEdge > 0, (1...100).contains(quality) else {
      result(FlutterError(
        code: "invalidArgument",
        message: "maxLongEdge must be greater than 0 and quality must be between 1 and 100.",
        details: nil
      ))
      return
    }

    let timesMs = [arguments["timeMs"] as? Int64 ?? 0]
    let cacheDir = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)
      .first?
      .path ?? NSTemporaryDirectory()
    let nativeResult = timesMs.withUnsafeBufferPointer { buffer in
      lgpl_ffmpeg_flutter_generate_cover(
        videoPath,
        buffer.baseAddress,
        Int32(buffer.count),
        Int32(maxLongEdge),
        Int32(quality),
        cacheDir
      )
    }
    result(mapResult(from: nativeResult))
  }

  private func mapResult(from nativeResult: UnsafeMutablePointer<CChar>?) -> Any {
    guard let nativeResult else {
      return FlutterError(
        code: "ffmpegUnavailable",
        message: "LGPL FFmpeg backend did not return a result.",
        details: nil
      )
    }
    let jsonText = String(cString: nativeResult)
    lgpl_ffmpeg_flutter_free_string(nativeResult)

    guard let data = jsonText.data(using: .utf8),
          let object = try? JSONSerialization.jsonObject(with: data),
          let map = object as? [String: Any] else {
      return FlutterError(code: "unknown", message: "Invalid backend JSON.", details: nil)
    }
    if let errorCode = map["errorCode"] as? String, !errorCode.isEmpty {
      return FlutterError(
        code: errorCode,
        message: map["message"] as? String ?? errorCode,
        details: nil
      )
    }
    return map
  }
}
