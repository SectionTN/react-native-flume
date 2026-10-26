#import "FLMView.h"

#include "bindings/Store.h"

@interface FLMView (FLMStoreListener)
- (void)applyListOps:(const flume::ListOps &)ops count:(int)count;
@end

// Hands the store's list changes to its view on the main thread; weak, since the store can outlive the view.
class FLMStoreListener final : public flume::StoreListener {
 public:
  explicit FLMStoreListener(FLMView *view) : view_(view) {}

  void onListChanged(const flume::ListOps &ops, int count) override
  {
    if (NSThread.isMainThread) {
      [view_ applyListOps:ops count:count];
      return;
    }
    __weak FLMView *view = view_;
    flume::ListOps copy = ops;
    dispatch_async(dispatch_get_main_queue(), ^{
      [view applyListOps:copy count:count];
    });
  }

 private:
  __weak FLMView *view_;
};
