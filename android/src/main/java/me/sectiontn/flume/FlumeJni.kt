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

  // Per position: the view type id, the stable item id (-1 when unknown) and the key ("" when unknown).
  @JvmStatic external fun viewType(storeId: Int, position: Int): Int

  @JvmStatic external fun itemId(storeId: Int, position: Int): Long

  @JvmStatic external fun keyAt(storeId: Int, position: Int): String

  // Null when the store is gone.
  @JvmStatic external fun bind(storeId: Int, holderId: Int, index: Int, widthDp: Float): FlumeBatch?

  @JvmStatic external fun releaseHolder(storeId: Int, holderId: Int)

  // Drops the measure cache so the next binds measure at the current width and font scale.
  @JvmStatic external fun invalidateMeasures(storeId: Int)
}
