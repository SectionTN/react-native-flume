package me.sectiontn.flume

// What a cell tree needs from its platform views; the JVM tests drive it with fakes.
interface CellViews<V : Any> {
  val root: V

  fun create(kind: Int): V

  fun add(parent: V, child: V, index: Int)

  fun remove(parent: V, child: V)

  fun move(parent: V, child: V, index: Int)

  fun removeAll()

  fun viewStyle(view: V, backgroundColor: Int, borderColor: Int, opacity: Float, radiiPx: FloatArray, borderWidthPx: Float, elevationPx: Float)

  fun text(view: V, spec: TextSpec, color: Int, insetLeftPx: Float, insetTopPx: Float)

  fun image(view: V, spec: ImageSpec)

  fun pressable(view: V, id: String, pressedOpacity: Float)

  fun accessibility(view: V, label: String?, testId: String?, role: Int, accessible: Int)
}
