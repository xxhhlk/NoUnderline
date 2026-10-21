// NoUnderline / Zhihu
//
// Zhihu renders answers two ways and each needs a different fix:
//
//   1. 全文视图 (full-text view) -- a WKWebView loading
//      https://www.zhihu.com/appview/v2/answer/<id>. The dashed line is
//      border-bottom-style:dashed on span.highlight-wrap[data-highlight-id].
//      Handled by the injected payload in common/UnderlineKiller.h, which also
//      swallows the page's own tap handlers so the popup never opens.
//
//   2. 卡片视图 (card view) -- native ZRichCoreUI. Dumping the attributes
//      handed to ZRichTextView.setAttributedText: shows the marked sentence
//      carries
//          NSUnderline      = 257      // NSUnderlineStyleSingle | pattern dash
//          NSUnderlineColor = <rgb>    // the dash colour
//      i.e. a plain TextKit underline attribute. Strip it and the line goes.
//      Tapping a marked sentence calls SCPopMenuSegLike; plain text selection
//      goes through SCPopMenuSelection instead, so silencing only the first
//      keeps the normal copy menu working.
//
// The native classes are Swift (mangled names) but they are registered in the
// ObjC runtime, so %hook works on them.

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

#pragma mark - Web view path (全文视图)

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

#pragma mark - Native path (卡片视图)

// Strip the dashed-underline attributes. The string length never changes, so
// ranges collected from the source stay valid on the mutable copy.
static NSAttributedString *NUStripUnderline(NSAttributedString *src) {
    if (src.length == 0) return src;
    NSMutableArray<NSValue *> *ranges = [NSMutableArray array];
    [src enumerateAttribute:NSUnderlineStyleAttributeName
                    inRange:NSMakeRange(0, src.length)
                    options:0
                 usingBlock:^(id value, NSRange range, BOOL *stop) {
        if (value) [ranges addObject:[NSValue valueWithRange:range]];
    }];
    if (ranges.count == 0) return src;

    NSMutableAttributedString *m = [src mutableCopy];
    for (NSValue *v in ranges) {
        NSRange r = [v rangeValue];
        [m removeAttribute:NSUnderlineStyleAttributeName range:r];
        [m removeAttribute:NSUnderlineColorAttributeName range:r];
    }
    NZLog(@"stripped underline runs=%lu len=%lu",
          (unsigned long)ranges.count, (unsigned long)src.length);
    return m;
}

%hook _TtC11ZRichCoreUI13ZRichTextView

- (void)setAttributedText:(NSAttributedString *)text {
    %orig(NUStripUnderline(text));
}

%end

// Second layer: the popup for a tapped highlighted sentence. Plain selection
// uses SCPopMenuSelection, which is left alone.
%hook _TtC21ShortContainerFeature16SCPopMenuSegLike

+ (void)showPopMenuWithMark:(id)mark
          sectionController:(id)sectionController
                      datas:(id)datas
            actionAssembler:(id)actionAssembler
                     config:(id)config {
    // swallowed: the menu is only ever about the highlight marks
}

%end

%ctor {
    NZLog(@"=== NoUnderline/Zhihu loaded (pid %d) ===", getpid());
}
