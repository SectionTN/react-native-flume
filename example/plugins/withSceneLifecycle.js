const { withAppDelegate, withInfoPlist } = require('expo/config-plugins');

// Apps built with the iOS 27 SDK must adopt the scene life cycle; Expo ships the scene delegate but the SDK 57 template does not use it.
const startInAppDelegate =
  /#if os\(iOS\) \|\| os\(tvOS\)\n\s*window = UIWindow\(frame: UIScreen\.main\.bounds\)\n\s*factory\.startReactNative\([\s\S]*?\)\n#endif\n/;

function replaceOnce(contents, pattern, replacement) {
  if (!pattern.test(contents)) {
    throw new Error(`withSceneLifecycle: AppDelegate.swift no longer matches ${pattern}`);
  }
  return contents.replace(pattern, replacement);
}

module.exports = function withSceneLifecycle(config) {
  config = withInfoPlist(config, (plist) => {
    plist.modResults.UIApplicationSceneManifest = {
      UIApplicationSupportsMultipleScenes: false,
      UISceneConfigurations: {
        UIWindowSceneSessionRoleApplication: [
          {
            UISceneConfigurationName: 'Default Configuration',
            UISceneDelegateClassName: 'EXExpoAppSceneDelegate',
          },
        ],
      },
    };
    return plist;
  });
  return withAppDelegate(config, (delegate) => {
    let contents = delegate.modResults.contents;
    if (!contents.includes('ExpoReactNativeFactoryProvider')) {
      contents = replaceOnce(
        contents,
        /class AppDelegate: ExpoAppDelegate \{/,
        'class AppDelegate: ExpoAppDelegate, ExpoReactNativeFactoryProvider {',
      );
      contents = replaceOnce(contents, startInAppDelegate, '');
    }
    delegate.modResults.contents = contents;
    return delegate;
  });
};
