package me.sectiontn.flume

import kotlin.math.roundToInt

// Interprets op batches into a view tree through CellViews; a first bind starts from an empty tree.
class CellTree<V : Any>(private val views: CellViews<V>, private val density: Float) {
  private val byId = HashMap<Int, V>()
  private val parents = HashMap<Int, V>()
  private val frames = HashMap<Int, IntArray>()
  var rootHeightPx = 0
    private set

  fun apply(batch: FlumeBatch): Int {
    if (batch.stats[5] == 1L) reset()
    val ints = batch.ints
    val floats = batch.floats
    val strings = batch.strings
    var i = 0
    var f = 0
    while (i + 1 < ints.size) {
      val id = ints[i + 1]
      when (ints[i]) {
        1 -> {
          val view = views.create(ints[i + 2])
          val parent = if (ints[i + 3] == 0) views.root else byId[ints[i + 3]] ?: views.root
          byId[id] = view
          parents[id] = parent
          views.add(parent, view, ints[i + 4])
          i += 5
        }
        2 -> {
          val view = byId.remove(id)
          val parent = parents.remove(id)
          frames.remove(id)
          if (view != null && parent != null) views.remove(parent, view)
          i += 2
        }
        3 -> {
          val left = (floats[f] * density).roundToInt()
          val top = (floats[f + 1] * density).roundToInt()
          val width = (floats[f + 2] * density).roundToInt()
          val height = (floats[f + 3] * density).roundToInt()
          if (id == 0) rootHeightPx = height else frames[id] = intArrayOf(left, top, width, height)
          i += 2
          f += 4
        }
        4 -> {
          val radii = FloatArray(4) { floats[f + 1 + it] * density }
          target(id)?.let { views.viewStyle(it, ints[i + 2], ints[i + 3], floats[f], radii, floats[f + 5] * density, floats[f + 6] * density) }
          i += 4
          f += 7
        }
        5 -> {
          val spec = TextSpec(
            text = strings[ints[i + 2]],
            fontSizeDp = floats[f],
            fontWeight = ints[i + 4],
            fontFamily = if (ints[i + 6] >= 0) strings[ints[i + 6]] else null,
            italic = ints[i + 7] == 1,
            lineHeightDp = floats[f + 2],
            letterSpacingDp = floats[f + 3],
            textAlign = ints[i + 8],
            decoration = ints[i + 9],
            maxWidthDp = floats[f + 1],
            numberOfLines = ints[i + 5],
          )
          target(id)?.let { views.text(it, spec, ints[i + 3]) }
          i += 10
          f += 4
        }
        6 -> {
          val headerCount = ints[i + 6]
          val headers = HashMap<String, String>(headerCount)
          for (h in 0 until headerCount) headers[strings[ints[i + 7 + 2 * h]]] = strings[ints[i + 8 + 2 * h]]
          val spec = ImageSpec(
            uri = strings[ints[i + 2]],
            resizeMode = ints[i + 3],
            tintColor = if (ints[i + 4] == 1) ints[i + 5] else null,
            headers = headers,
            widthPx = (floats[f] * density).roundToInt(),
            heightPx = (floats[f + 1] * density).roundToInt(),
            radiiPx = FloatArray(4) { floats[f + 2 + it] * density },
          )
          target(id)?.let { views.image(it, spec) }
          i += 7 + 2 * headerCount
          f += 6
        }
        7 -> {
          target(id)?.let { views.pressable(it, strings[ints[i + 2]], floats[f]) }
          i += 3
          f += 1
        }
        8 -> {
          val label = if (ints[i + 2] >= 0) strings[ints[i + 2]] else null
          val testId = if (ints[i + 3] >= 0) strings[ints[i + 3]] else null
          target(id)?.let { views.accessibility(it, label, testId, ints[i + 4], ints[i + 5]) }
          i += 6
        }
        9 -> {
          val view = byId[id]
          val parent = parents[id]
          if (view != null && parent != null) views.move(parent, view, ints[i + 2])
          i += 3
        }
        else -> return rootHeightPx
      }
    }
    return rootHeightPx
  }

  // Every tracked view with its frame in px; the root's own frame is its height only.
  fun frames(block: (view: V, left: Int, top: Int, width: Int, height: Int) -> Unit) {
    for ((id, frame) in frames) byId[id]?.let { block(it, frame[0], frame[1], frame[2], frame[3]) }
  }

  private fun target(id: Int): V? = if (id == 0) views.root else byId[id]

  private fun reset() {
    byId.clear()
    parents.clear()
    frames.clear()
    rootHeightPx = 0
    views.removeAll()
  }
}
