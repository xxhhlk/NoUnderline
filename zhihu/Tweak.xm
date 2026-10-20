// NoUnderline / Zhihu
//
// Answer pages are rendered by a WKWebView loading
// https://www.zhihu.com/appview/v2/answer/<id>, so native rich-text hooks
// (ZRichCoreUI, SegLike, UIMenuController) never see these highlights --
// everything happens inside the page. The payload lives in
// common/UnderlineKiller.h; this file only wires it into every web view.

#import <WebKit/WebKit.h>
#import <objc/runtime.h>
#import "UnderlineKiller.h"

static void NZLog(NSString *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    NSString *msg = [[NSString alloc] initWithFormat:fmt arguments:ap];
    va_end(ap);
    NSString *line = [NSString stringWithFormat:@"[%@] %@\n", [NSDate date], msg];
    FILE *f = fopen("/tmp/nounderline.log", "a");
    if (f) { fputs(line.UTF8String, f); fclose(f); }
}

static char kNZKey;

%hook WKUserContentController

- (void)addUserScript:(WKUserScript *)script {
    %orig(script);
    // One injection per controller. Zhihu calls addUserScript: several times
    // per configuration; without this guard every call stacks another copy of
    // the payload, i.e. N observers and N timers doing the same work.
    if (objc_getAssociatedObject(self, &kNZKey)) return;
    objc_setAssociatedObject(self, &kNZKey, @YES, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    WKUserScript *s = [[WKUserScript alloc] initWithSource:kUnderlineKillerJS
                                              injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                           forMainFrameOnly:NO];
    %orig(s);
}

%end

%ctor {
    NZLog(@"=== NoUnderline/Zhihu 1.0.0 loaded (pid %d) ===", getpid());
}
