package me.sectiontn.flume

import com.facebook.proguard.annotations.DoNotStrip
import com.facebook.react.turbomodule.core.interfaces.BindingsInstallerHolder
import com.facebook.soloader.SoLoader

@DoNotStrip
object FlumeJni {
  init {
    SoLoader.loadLibrary("flume")
  }

  @JvmStatic external fun createBindingsInstaller(): BindingsInstallerHolder

  // Returns the store's item count; the listener and measurer are held until detach.
  @JvmStatic
  external fun attach(storeId: Int, listener: FlumeStoreListener, measurer: FlumeTextMeasurer, pointScale: Float): Int

  @JvmStatic external fun detach(storeId: Int)

  @JvmStatic external fun count(storeId: Int): Int

  // Null when the store is gone.
  @JvmStatic external fun bind(storeId: Int, holderId: Int, index: Int, widthDp: Float): FlumeBatch?

  @JvmStatic external fun releaseHolder(storeId: Int, holderId: Int)
}
