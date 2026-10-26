#import "FLMImages.h"

#import <React/RCTUtils.h>

// RCTImageCache keeps no decoded image over 2 MB, which a full-width photo at 3x is, so cells keep their own.
static const NSUInteger kDecodedCacheBytes = 64 * 1024 * 1024;

static void FLMOnMain(void (^block)(void))
{
  if (NSThread.isMainThread) {
    block();
  } else {
    dispatch_async(dispatch_get_main_queue(), block);
  }
}

static NSString *FLMCacheKey(NSURL *url, CGSize size, RCTResizeMode mode)
{
  return [NSString stringWithFormat:@"%@|%gx%g|%ld", url.absoluteString, size.width, size.height, (long)mode];
}

@implementation FLMImages {
  NSCache<NSString *, UIImage *> *_cache;
}

+ (instancetype)shared
{
  static FLMImages *shared;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    shared = [FLMImages new];
  });
  return shared;
}

- (instancetype)init
{
  if (self = [super init]) {
    _cache = [NSCache new];
    _cache.totalCostLimit = kDecodedCacheBytes;
  }
  return self;
}

- (void)keep:(UIImage *)image forKey:(NSString *)key
{
  const NSUInteger cost = image.size.width * image.scale * image.size.height * image.scale * 4;
  [_cache setObject:image forKey:key cost:cost];
}

- (dispatch_block_t)load:(NSURLRequest *)request
                    size:(CGSize)size
              resizeMode:(RCTResizeMode)mode
              completion:(void (^)(UIImage *image))completion
{
  NSURL *url = request.URL;
  NSString *key = FLMCacheKey(url, size, mode);
  UIImage *cached = [_cache objectForKey:key];
  if (cached != nil) {
    FLMOnMain(^{
      completion(cached);
    });
    return ^{
    };
  }
  __weak FLMImages *weakSelf = self;
  void (^done)(UIImage *) = ^(UIImage *image) {
    if (image != nil) {
      [weakSelf keep:image forKey:key];
    }
    FLMOnMain(^{
      completion(image);
    });
  };
  id<RCTImageLoaderProtocol> loader = self.loader;
  if (loader != nil) {
    return [loader loadImageWithURLRequest:request
        size:size
        scale:RCTScreenScale()
        clipped:YES
        resizeMode:mode
        progressBlock:^(int64_t, int64_t) {
        }
        partialLoadBlock:^(UIImage *image) {
          FLMOnMain(^{
            completion(image);
          });
        }
        completionBlock:^(NSError *error, UIImage *image) {
          done(image);
        }];
  }
  NSURLSessionDataTask *task = [NSURLSession.sharedSession
      dataTaskWithRequest:request
        completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
          done(data != nil ? [UIImage imageWithData:data scale:RCTScreenScale()] : nil);
        }];
  [task resume];
  return ^{
    [task cancel];
  };
}

@end
