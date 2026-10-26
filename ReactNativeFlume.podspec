require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "ReactNativeFlume"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/SectionTN/react-native-flume.git", :tag => "#{s.version}" }

  s.source_files = "ios/**/*.{h,mm}", "cpp/core/**/*.{h,cpp}", "cpp/bindings/**/*.{h,cpp}", "cpp/adapter/**/*.{h,cpp}"
  s.private_header_files = "ios/**/*.h", "cpp/**/*.h"

  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/cpp\" \"$(PODS_ROOT)/Headers/Public/RNWorklets\""
  }

  s.dependency "RNWorklets"
  s.dependency "React-RCTImage"

  install_modules_dependencies(s)
end
