package me.sectiontn.flume

import kotlin.math.min
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test

class FakeView(val kind: Int) {
  val children = mutableListOf<FakeView>()
  var text: TextSpec? = null
  var insetPx: Pair<Float, Float>? = null
  var imageInsetsPx: List<Float>? = null
  var pressId: String? = null
}

class FakeViews : CellViews<FakeView> {
  override val root = FakeView(0)
  val created = mutableListOf<FakeView>()
  var removeAllCalls = 0

  override fun create(kind: Int): FakeView = FakeView(kind).also { created += it }
  override fun add(parent: FakeView, child: FakeView, index: Int) { parent.children.add(min(index, parent.children.size), child) }
  override fun remove(parent: FakeView, child: FakeView) { parent.children.remove(child) }
  override fun move(parent: FakeView, child: FakeView, index: Int) {
    parent.children.remove(child)
    parent.children.add(min(index, parent.children.size), child)
  }
  override fun removeAll() { root.children.clear(); removeAllCalls++ }
  override fun viewStyle(view: FakeView, backgroundColor: Int, borderColor: Int, opacity: Float, radiiPx: FloatArray, borderWidthPx: Float, elevationPx: Float) = Unit
  override fun text(view: FakeView, spec: TextSpec, color: Int, insetLeftPx: Float, insetTopPx: Float) {
    view.text = spec
    view.insetPx = insetLeftPx to insetTopPx
  }
  override fun image(view: FakeView, spec: ImageSpec) = Unit
  override fun imageInsets(view: FakeView, leftPx: Float, topPx: Float, rightPx: Float, bottomPx: Float) {
    view.imageInsetsPx = listOf(leftPx, topPx, rightPx, bottomPx)
  }
  override fun pressable(view: FakeView, id: String, pressedOpacity: Float) { view.pressId = id }
  override fun accessibility(view: FakeView, label: String?, testId: String?, role: Int, accessible: Int) = Unit
}

private fun batch(ints: IntArray, floats: FloatArray = FloatArray(0), first: Boolean = false, strings: Array<String> = emptyArray()) =
  FlumeBatch(ints, floats, strings, 0f, 0f, first, null)

class CellTreeTest {
  private val views = FakeViews()
  private val tree = CellTree(views, 2f)

  @Test
  fun imageInsetsArriveInPixelsAndTheOpAfterThemStillApplies() {
    tree.apply(
      batch(
        intArrayOf(1, 1, 2, 0, 0, 10, 1, 7, 1, 0),
        floatArrayOf(7f, 5f, 4f, 3f, 0.5f),
        first = true,
        strings = arrayOf("after"),
      ),
    )
    assertEquals(listOf(14f, 10f, 8f, 6f), views.created[0].imageInsetsPx)
    assertEquals("after", views.created[0].pressId)
  }

  @Test
  fun buildsATreeFromCreateOps() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0, 1, 2, 1, 1, 0), first = true))
    assertEquals(1, views.root.children.size)
    assertSame(views.created[1], views.root.children[0].children[0])
    assertEquals(1, views.root.children[0].children[0].kind)
  }

  @Test
  fun aFirstBindClearsThePreviousTree() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0), first = true))
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0), first = true))
    assertEquals(1, views.root.children.size)
    assertEquals(2, views.removeAllCalls)
  }

  @Test
  fun removeDetachesTheView() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0, 1, 2, 1, 1, 0), first = true))
    tree.apply(batch(intArrayOf(2, 2)))
    assertEquals(0, views.root.children[0].children.size)
    tree.apply(batch(intArrayOf(3, 2), floatArrayOf(1f, 2f, 3f, 4f)))
    var seen = 0
    tree.frames { _, _, _, _, _ -> seen++ }
    assertEquals(0, seen)
  }

  @Test
  fun moveReordersAChild() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0, 1, 2, 0, 0, 1, 1, 3, 0, 0, 2), first = true))
    tree.apply(batch(intArrayOf(9, 3, 0)))
    assertEquals(listOf(views.created[2], views.created[0], views.created[1]), views.root.children)
  }

  @Test
  fun framesArePerViewInPixels() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0, 3, 1), floatArrayOf(10f, 5f, 100f, 20f), first = true))
    var frame: List<Int>? = null
    tree.frames { view, left, top, width, height -> if (view === views.created[0]) frame = listOf(left, top, width, height) }
    assertEquals(listOf(20, 10, 200, 40), frame)
  }

  @Test
  fun aViewStyleWithAShadowLeavesTheNextOpIntact() {
    val style = intArrayOf(4, 1, 0, 0xFF000000.toInt(), 0xFF000000.toInt())
    val styleFloats = floatArrayOf(1f, 0f, 0f, 0f, 0f, 0f, 0f, 0.5f, 3f, 0f, -3f)
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0) + style + intArrayOf(3, 1), styleFloats + floatArrayOf(10f, 5f, 100f, 20f), first = true))
    var frame: List<Int>? = null
    tree.frames { view, left, top, width, height -> if (view === views.created[0]) frame = listOf(left, top, width, height) }
    assertEquals(listOf(20, 10, 200, 40), frame)
  }

  @Test
  fun theRootFrameSetsTheCellHeight() {
    assertEquals(128, tree.apply(batch(intArrayOf(3, 0), floatArrayOf(0f, 0f, 360f, 64f), first = true)))
    assertNull(views.root.text)
  }
}

class CellTreeTextTest {
  private val views = FakeViews()
  private val tree = CellTree(views, 2f)

  @Test
  fun aTextOpCarriesAllowFontScaling() {
    val ints = intArrayOf(1, 1, 1, 0, 0, 5, 1, 0, 0xff000000.toInt(), 400, 0, -1, 0, 0, 0, 0)
    tree.apply(batch(ints, floatArrayOf(14f, 100f, 0f, 0f, 0f, 0f), first = true, strings = arrayOf("hello")))
    val spec = views.created[0].text
    assertEquals("hello", spec?.text)
    assertEquals(false, spec?.allowFontScaling)
  }

  @Test
  fun aTextOpCarriesItsInsetsInPx() {
    val ints = intArrayOf(1, 1, 1, 0, 0, 5, 1, 0, 0xff000000.toInt(), 400, 0, -1, 0, 0, 0, 1)
    tree.apply(batch(ints, floatArrayOf(14f, 100f, 0f, 0f, 13f, 5f), first = true, strings = arrayOf("hello")))
    assertEquals(26f to 10f, views.created[0].insetPx)
  }
  @Test
  fun theRootFrameSetsBothSizes() {
    tree.apply(batch(intArrayOf(1, 1, 0, 0, 0, 3, 0), floatArrayOf(0f, 0f, 120f, 48f), first = true))
    assertEquals(240, tree.rootWidthPx)
    assertEquals(96, tree.rootHeightPx)
  }
}
