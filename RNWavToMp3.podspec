require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

# Define minimum iOS version
min_ios_version_supported = '11.0'

Pod::Spec.new do |s|
  s.name         = "RNWavToMp3"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => min_ios_version_supported }
  s.source       = { :git => "https://github.com/BITNET-Infotech/react-native-wav-to-mp3.git"}

  s.source_files = "ios/**/*.{h,m,mm}"
  
  # Add LAME library dependency - using the correct pod name
  s.dependency "LAME-xcframework", "~> 3.100"
  
  # Add library search paths
  s.pod_target_xcconfig = {
    'LIBRARY_SEARCH_PATHS' => '$(inherited) $(PODS_ROOT)/lame/lib'
  }

# Use install_modules_dependencies helper to install the dependencies if React Native version >=0.71.0.
# See https://github.com/facebook/react-native/blob/febf6b7f33fdb4904669f99d795eba4c0f95d7bf/scripts/cocoapods/new_architecture.rb#L79.
if respond_to?(:install_modules_dependencies, true)
  install_modules_dependencies(s)
else
  s.dependency "React-Core"
end
end
