#import "FLMRefresh.h"

@implementation FLMRefresh {
  __weak UIScrollView *_scrollView;
  void (^_onRefresh)(void);
  UIRefreshControl *_control;
  BOOL _wantsRefreshing;  // the refreshing the list asked for last
  BOOL _movedOffset;  // a begin moved the offset, which endRefreshing leaves in place
  CGFloat _controlInset;  // what beginRefreshing added to the top inset
}

- (instancetype)initWithScrollView:(UIScrollView *)scrollView onRefresh:(void (^)(void))onRefresh
{
  if (self = [super init]) {
    _scrollView = scrollView;
    _onRefresh = [onRefresh copy];
  }
  return self;
}

- (void)setEnabled:(BOOL)enabled
{
  if (enabled == _enabled) {
    return;
  }
  _enabled = enabled;
  if (enabled) {
    _control = [UIRefreshControl new];
    [_control addTarget:self action:@selector(valueChanged) forControlEvents:UIControlEventValueChanged];
    [self applyLook];
    _scrollView.refreshControl = _control;
    if (_wantsRefreshing) {
      [self begin];
    }
  } else {
    // iOS requires the control to stop refreshing before it leaves the scroll view.
    [_control endRefreshing];
    _scrollView.refreshControl = nil;
    _control = nil;
  }
}

- (void)setTintColor:(UIColor *)tintColor
{
  _tintColor = tintColor;
  [self applyLook];
}

- (void)setProgressViewOffset:(CGFloat)progressViewOffset
{
  _progressViewOffset = progressViewOffset;
  [self applyLook];
}

- (void)applyLook
{
  if (_control == nil) {
    return;
  }
  _control.tintColor = _tintColor;
  // React Native moves the spinner down by the offset through the bounds origin.
  CGRect bounds = _control.bounds;
  bounds.origin.y = -_progressViewOffset;
  _control.bounds = bounds;
}

- (void)setRefreshing:(BOOL)refreshing
{
  _wantsRefreshing = refreshing;
  if (refreshing) {
    [self begin];
  } else {
    [self end];
  }
}

- (void)layoutSubviews
{
  if (_wantsRefreshing) {
    [self begin];
  }
}

// A begin before the first layout is ignored, so it waits for one; the spinner is revealed only at the top.
- (void)begin
{
  UIScrollView *scrollView = _scrollView;
  if (_control == nil || _control.isRefreshing || scrollView.window == nil || CGRectIsEmpty(scrollView.bounds)) {
    return;
  }
  const CGPoint offset = scrollView.contentOffset;
  const CGFloat inset = scrollView.adjustedContentInset.top;
  _movedOffset = offset.y <= -inset + 0.5;
  if (_movedOffset) {
    [scrollView setContentOffset:CGPointMake(offset.x, offset.y - _control.frame.size.height) animated:NO];
  }
  [_control beginRefreshing];
  _controlInset = scrollView.adjustedContentInset.top - inset;
}

// As RCTRefreshControl does, a refresh begun here goes back to the top before it ends, or the list stays pulled down.
- (void)end
{
  UIScrollView *scrollView = _scrollView;
  const CGFloat top = -(scrollView.adjustedContentInset.top - _controlInset);
  const BOOL back = _movedOffset && _control.isRefreshing && scrollView.contentOffset.y < top - 0.5;
  _movedOffset = NO;
  if (!back) {
    [_control endRefreshing];
    return;
  }
  UIRefreshControl *control = _control;
  [UIView animateWithDuration:0.25
      animations:^{
        scrollView.contentOffset = CGPointMake(scrollView.contentOffset.x, top);
      }
      completion:^(BOOL finished) {
        if (!self->_wantsRefreshing) {
          [control endRefreshing];
        }
      }];
}

- (void)valueChanged
{
  if (_onRefresh != nil) {
    _onRefresh();
  }
}

@end
