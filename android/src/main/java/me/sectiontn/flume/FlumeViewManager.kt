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

  override fun getExportedCustomDirectEventTypeConstants(): MutableMap<String, Any> = mutableMapOf(
    FlumePressEvent.PRESS to mapOf("registrationName" to "onPress"),
    FlumePressEvent.LONG_PRESS to mapOf("registrationName" to "onLongPress"),
  )

  @ReactProp(name = "itemAnimations", defaultBoolean = true)
  override fun setItemAnimations(view: FlumeView, value: Boolean) {
    view.itemAnimations = value
  }

  @ReactProp(name = "horizontal")
  override fun setHorizontal(view: FlumeView, value: Boolean) {
    view.horizontal = value
  }

  @ReactProp(name = "numColumns", defaultInt = 1)
  override fun setNumColumns(view: FlumeView, value: Int) {
    view.numColumns = value
  }

  @ReactProp(name = "inverted")
  override fun setInverted(view: FlumeView, value: Boolean) {
    view.inverted = value
  }

  @ReactProp(name = "contentPaddingTop")
  override fun setContentPaddingTop(view: FlumeView, value: Double) = view.setContentPadding(1, value)

  @ReactProp(name = "contentPaddingLeft")
  override fun setContentPaddingLeft(view: FlumeView, value: Double) = view.setContentPadding(0, value)

  @ReactProp(name = "contentPaddingBottom")
  override fun setContentPaddingBottom(view: FlumeView, value: Double) = view.setContentPadding(3, value)

  @ReactProp(name = "contentPaddingRight")
  override fun setContentPaddingRight(view: FlumeView, value: Double) = view.setContentPadding(2, value)

  @ReactProp(name = "drawDistance", defaultDouble = -1.0)
  override fun setDrawDistance(view: FlumeView, value: Double) {
    view.drawDistanceDp = value
  }

  @ReactProp(name = "initialScrollIndex", defaultInt = -1)
  override fun setInitialScrollIndex(view: FlumeView, value: Int) {
    view.initialScrollIndex = value
  }

  @ReactProp(name = "separatorSize")
  override fun setSeparatorSize(view: FlumeView, value: Double) = view.setSeparator(sizeDp = value)

  @ReactProp(name = "separatorColor", customType = "Color")
  override fun setSeparatorColor(view: FlumeView, value: Int?) = view.setSeparator(color = value ?: 0)

  @ReactProp(name = "separatorInsetStart")
  override fun setSeparatorInsetStart(view: FlumeView, value: Double) = view.setSeparator(insetStartDp = value)

  @ReactProp(name = "separatorInsetEnd")
  override fun setSeparatorInsetEnd(view: FlumeView, value: Double) = view.setSeparator(insetEndDp = value)

  companion object {
    const val NAME = "FlumeView"
  }
}
