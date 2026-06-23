#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint lgpl_ffmpeg_flutter.podspec` to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'lgpl_ffmpeg_flutter'
  s.version          = '0.1.2'
  s.summary          = 'Controlled Flutter wrapper around LGPL FFmpeg.'
  s.description      = <<-DESC
Controlled Flutter wrapper around LGPL FFmpeg dynamic libraries for video info and cover extraction.
                       DESC
  s.homepage         = 'https://ffmpeg.org/legal.html'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Internal' => 'internal@example.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.vendored_frameworks = 'Frameworks/*.xcframework'
  s.dependency 'Flutter'
  s.platform = :ios, '13.0'

  # Flutter.framework does not contain a i386 slice.
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'EXCLUDED_ARCHS[sdk=iphonesimulator*]' => 'i386',
    'HEADER_SEARCH_PATHS' => '$(inherited) "${PODS_XCFRAMEWORKS_BUILD_DIR}/lgpl_ffmpeg_flutter/libavcodec.framework/Headers" "${PODS_XCFRAMEWORKS_BUILD_DIR}/lgpl_ffmpeg_flutter/libavformat.framework/Headers" "${PODS_XCFRAMEWORKS_BUILD_DIR}/lgpl_ffmpeg_flutter/libavutil.framework/Headers" "${PODS_XCFRAMEWORKS_BUILD_DIR}/lgpl_ffmpeg_flutter/libswresample.framework/Headers" "${PODS_XCFRAMEWORKS_BUILD_DIR}/lgpl_ffmpeg_flutter/libswscale.framework/Headers"',
    'OTHER_LDFLAGS' => '$(inherited) -framework "libavcodec" -framework "libavformat" -framework "libavutil" -framework "libswresample" -framework "libswscale"',
  }
  s.swift_version = '5.0'

  # If your plugin requires a privacy manifest, for example if it uses any
  # required reason APIs, update the PrivacyInfo.xcprivacy file to describe your
  # plugin's privacy impact, and then uncomment this line. For more information,
  # see https://developer.apple.com/documentation/bundleresources/privacy_manifest_files
  # s.resource_bundles = {'lgpl_ffmpeg_flutter_privacy' => ['Resources/PrivacyInfo.xcprivacy']}
end
