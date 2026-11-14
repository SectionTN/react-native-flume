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

// The bitmap, rounded at its own radii.
@interface FLMImageContentView : UIImageView <FLMShaped>
@property (nonatomic) std::array<float, 4> radii;
@property (nonatomic) BOOL opaqueShadow;
@end

@implementation FLMImageContentView

- (void)layoutSubviews
{
  [super layoutSubviews];
  FLMLayoutShape(self);
}

@end

@implementation FLMImageView {
  FLMImageContentView *_content;
  UIEdgeInsets _insets;
  NSString *_uri;
  CGSize _size;
  int _resizeMode;
  dispatch_block_t _cancel;
  uint64_t _request;
  BOOL _tinted;
}

// The bitmap sits in the content box, as in React Native's Image: the border draws beside it, not over it.
- (instancetype)initWithFrame:(CGRect)frame
{
  if (self = [super initWithFrame:frame]) {
    self.clipsToBounds = YES;
    self.userInteractionEnabled = NO;
    self.accessibilityTraits = UIAccessibilityTraitImage;
    _content = [[FLMImageContentView alloc] initWithFrame:self.bounds];
    _content.clipsToBounds = YES;
    _content.contentMode = UIViewContentModeScaleAspectFill;
    [self addSubview:_content];
  }
  return self;
}

- (void)setInsets:(UIEdgeInsets)insets
{
  _insets = insets;
  [self setNeedsLayout];
}

// The box's own radii come with its view style, the bitmap's with the image.
- (void)setSpec:(const flume::ImageSpec &)spec
{
  _content.contentMode = FLMContentMode(spec.resizeMode);
  _tinted = spec.tint.has_value();
  _content.tintColor = _tinted ? FLMColor(*spec.tint) : nil;
  FLMSetRadii(_content, spec.radii);
  NSString *uri = @(spec.uri.c_str());
  const CGSize size = CGSizeMake(spec.width, spec.height);
  if ([uri isEqualToString:_uri] && CGSizeEqualToSize(size, _size) && spec.resizeMode == _resizeMode) {
    if (_content.image != nil) {
      _content.image = [_content.image imageWithRenderingMode:_tinted ? UIImageRenderingModeAlwaysTemplate
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
  _content.image = nil;
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
                          view->_content.image = view->_tinted ? [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
                                                               : image;
                        }];
}

- (void)layoutSubviews
{
  [super layoutSubviews];
  _content.frame = UIEdgeInsetsInsetRect(self.bounds, _insets);
  FLMLayoutShape(self);
}

@end
