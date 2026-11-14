#import <UIKit/UIKit.h>

#include "adapter/CellTree.h"

// The cell tree's views on UIKit, built under a root view that fills the container.
class FLMCellViews final : public flume::CellViews {
 public:
  explicit FLMCellViews(UIView *container);

  flume::ViewHandle root() override;
  flume::ViewHandle create(flume::ViewKind kind) override;
  void add(flume::ViewHandle parent, flume::ViewHandle child, int index) override;
  void remove(flume::ViewHandle parent, flume::ViewHandle child) override;
  void move(flume::ViewHandle parent, flume::ViewHandle child, int index) override;
  void removeAll() override;
  void frame(flume::ViewHandle view, float x, float y, float width, float height) override;
  void viewStyle(flume::ViewHandle view, const flume::ViewStyleSpec &style) override;
  void text(flume::ViewHandle view, const flume::TextSpec &text, float insetLeft, float insetTop) override;
  void image(flume::ViewHandle view, const flume::ImageSpec &image) override;
  void imageInsets(flume::ViewHandle view, float left, float top, float right, float bottom) override;
  void pressable(flume::ViewHandle view, const std::string &pressId, float pressedOpacity) override;
  void accessibility(flume::ViewHandle view, const std::optional<std::string> &label,
                     const std::optional<std::string> &testID, int role, int accessible) override;

  void (^onPress)(NSString *pressId, BOOL longPress) = nil;  // given to every pressable the tree creates

 private:
  __weak UIView *root_;
  NSMutableSet<UIView *> *pending_;  // created views not yet in the tree, kept alive until added
};
