import Flutter
import UIKit
import XCTest


@testable import lgpl_ffmpeg_flutter

// This demonstrates a simple unit test of the Swift portion of this plugin's implementation.
//
// See https://developer.apple.com/documentation/xctest for more information about using XCTest.

class RunnerTests: XCTestCase {

  func testReadInfoReportsUnavailableUntilBackendIsLinked() {
    let plugin = LgplFfmpegFlutterPlugin()

    let call = FlutterMethodCall(methodName: "readInfo", arguments: ["videoPath": "/tmp/video.mp4"])

    let resultExpectation = expectation(description: "result block must be called.")
    plugin.handle(call) { result in
      XCTAssertEqual((result as! FlutterError).code, "ffmpegUnavailable")
      resultExpectation.fulfill()
    }
    waitForExpectations(timeout: 1)
  }

}
