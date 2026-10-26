package me.sectiontn.flume

import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.WritableMap
import com.facebook.react.uimanager.events.Event

// topCellPress and topCellLongPress carry the row index, its key and the pressable id, "" for the cell itself.
class FlumePressEvent(
  surfaceId: Int,
  viewTag: Int,
  private val long: Boolean,
  private val index: Int,
  private val key: String,
  private val id: String?,
) : Event<FlumePressEvent>(surfaceId, viewTag) {
  override fun getEventName(): String = if (long) LONG_PRESS else PRESS

  override fun getEventData(): WritableMap = Arguments.createMap().apply {
    putInt("index", index)
    putString("key", key)
    putString("id", id ?: "")
  }

  companion object {
    const val PRESS = "topCellPress"
    const val LONG_PRESS = "topCellLongPress"
  }
}
