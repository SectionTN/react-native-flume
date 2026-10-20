package me.sectiontn.flume

interface ListNotifier {
  fun reset()
  fun removed(index: Int, count: Int)
  fun inserted(index: Int, count: Int)
  fun moved(from: Int, to: Int)
  fun changed(index: Int)
}

// Removes arrive descending, inserts ascending and moves sequential, so they apply in that order; reset replaces all.
object ListOpsApplier {
  fun apply(reset: Boolean, removed: IntArray, inserted: IntArray, moved: IntArray, changed: IntArray, notifier: ListNotifier) {
    if (reset) {
      notifier.reset()
      return
    }
    for (i in removed.indices step 2) notifier.removed(removed[i], removed[i + 1])
    for (i in inserted.indices step 2) notifier.inserted(inserted[i], inserted[i + 1])
    for (i in moved.indices step 2) notifier.moved(moved[i], moved[i + 1])
    for (index in changed) notifier.changed(index)
  }
}
