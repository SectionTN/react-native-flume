package expo.modules.benchbaseline

import expo.modules.kotlin.modules.Module
import expo.modules.kotlin.modules.ModuleDefinition

class BenchBaselineModule : Module() {
  override fun definition() = ModuleDefinition {
    Name("BenchBaseline")

    View(BenchBaselineView::class) {
      Prop("rows") { view: BenchBaselineView, rows: List<ChatRowRecord> -> view.setRows(rows) }
    }
  }
}
