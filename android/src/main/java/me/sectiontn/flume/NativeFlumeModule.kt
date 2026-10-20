package me.sectiontn.flume

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.turbomodule.core.interfaces.BindingsInstallerHolder
import com.facebook.react.turbomodule.core.interfaces.TurboModuleWithJSIBindings

// No methods: creating the module installs globalThis.__flume on the React runtime.
class NativeFlumeModule(context: ReactApplicationContext) :
  NativeFlumeSpec(context), TurboModuleWithJSIBindings {
  override fun getBindingsInstaller(): BindingsInstallerHolder = FlumeJni.createBindingsInstaller()
}
