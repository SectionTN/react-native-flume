package me.sectiontn.flume

import com.facebook.react.module.annotations.ReactModule
import com.facebook.react.uimanager.SimpleViewManager
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.ViewManagerDelegate
import com.facebook.react.uimanager.annotations.ReactProp
import com.facebook.react.viewmanagers.FlumeViewManagerDelegate
import com.facebook.react.viewmanagers.FlumeViewManagerInterface

@ReactModule(name = FlumeViewManager.NAME)
class FlumeViewManager : SimpleViewManager<FlumeView>(), FlumeViewManagerInterface<FlumeView> {
  private val delegate = FlumeViewManagerDelegate(this)

  override fun getDelegate(): ViewManagerDelegate<FlumeView> = delegate

  override fun getName(): String = NAME

  override fun createViewInstance(context: ThemedReactContext): FlumeView = FlumeView(context)

  @ReactProp(name = "storeId")
  override fun setStoreId(view: FlumeView, value: Int) {
    view.storeId = value
  }

  companion object {
    const val NAME = "FlumeView"
  }
}
