import ExpoModulesCore

// The baseline exists for the Android bench only; iOS gets an empty view so the module links.
public class BenchBaselineModule: Module {
  public func definition() -> ModuleDefinition {
    Name("BenchBaseline")

    View(BenchBaselineView.self) {}
  }
}

class BenchBaselineView: ExpoView {}
