package me.sectiontn.flume

import com.facebook.react.turbomodule.core.interfaces.BindingsInstallerHolder
import com.facebook.soloader.SoLoader

object FlumeJni {
  init {
    SoLoader.loadLibrary("flume")
  }

  @JvmStatic external fun createBindingsInstaller(): BindingsInstallerHolder
}
