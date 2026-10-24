package me.sectiontn.flume

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.UiThreadUtil
import com.facebook.react.turbomodule.core.interfaces.BindingsInstallerHolder
import com.facebook.react.turbomodule.core.interfaces.TurboModuleWithJSIBindings

// Creating the module installs globalThis.__flume on the React runtime; the methods drive the opt-in stats.
class NativeFlumeModule(context: ReactApplicationContext) :
  NativeFlumeSpec(context), TurboModuleWithJSIBindings {
  override fun getBindingsInstaller(): BindingsInstallerHolder = FlumeJni.createBindingsInstaller()

  // Binds touch the stats rings on the UI thread.
  override fun setStats(enabled: Boolean) {
    UiThreadUtil.runOnUiThread {
      FlumeJni.setStatsEnabled(enabled)
      BindStats.setEnabled(enabled)
    }
  }

  override fun logStats() {
    UiThreadUtil.runOnUiThread { BindStats.log() }
  }
}
