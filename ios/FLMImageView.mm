#import "FLMImageView.h"

#import "FLMColor.h"
#import "FLMImages.h"

static UIViewContentMode FLMContentMode(int resizeMode)
{
  switch (resizeMode) {
    case 1:
      return UIViewContentModeScaleAspectFit;
    case 2:
      return UIViewContentModeScaleToFill;
    case 3:
      return UIViewContentModeCenter;
    default:
      return UIViewContentModeScaleAspectFill;
  }
}

@implementation FLMImageView {
  NSString *_uri;
  CGSize _size;
  int _resizeMode;
  dispatch_block_t _cancel;
  uint64_t _request;
  BOOL _tinted;
}

- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.clipsToBounds = YES;
    self.contentMode = UIViewContentModeScaleAspectFill;
  }
  return self;
}

- (void)setSpec:(const flume::ImageSpec &)spec
{
  self.contentMode = FLMContentMode(spec.resizeMode);
  _tinted = spec.tint.has_value();
  self.tintColor = _tinted ? FLMColor(*spec.tint) : nil;
  FLMSetRadii(self, spec.radii);
  NSString *uri = @(spec.uri.c_str());
  const CGSize size = CGSizeMake(spec.width, spec.height);
  if ([uri isEqualToString:_uri] && CGSizeEqualToSize(size, _size) && spec.resizeMode == _resizeMode) {
    if (self.image != nil) {
      self.image = [self.image imageWithRenderingMode:_tinted ? UIImageRenderingModeAlwaysTemplate
                                                              : UIImageRenderingModeAutomatic];
    }
    return;
  }
  if (_cancel != nil) {
    _cancel();
    _cancel = nil;
  }
  _uri = uri;
  _size = size;
  _resizeMode = spec.resizeMode;
  self.image = nil;
  NSURL *url = [NSURL URLWithString:uri];
  if (url == nil) {
    return;
  }
  NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
  for (const auto &[name, value] : spec.headers) {
    [request setValue:@(value.c_str()) forHTTPHeaderField:@(name.c_str())];
  }
  const uint64_t token = ++_request;
  __weak FLMImageView *weakSelf = self;
  _cancel = [FLMImages.shared load:request
                              size:size
                        resizeMode:static_cast<RCTResizeMode>(FLMContentMode(spec.resizeMode))
                        completion:^(UIImage *image) {
                          FLMImageView *view = weakSelf;
                          if (view == nil || view->_request != token || image == nil) {
                            return;
                          }
                          view.image = view->_tinted ? [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
                                                     : image;
                        }];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  FLMLayoutShape(self);
}

@end
