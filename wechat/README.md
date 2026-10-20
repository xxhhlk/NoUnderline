# NoUnderline / WeChat（规划中）

微信 8.x 的公众号文章也有"划线"功能：别人划过或你自己划过的句子会留下下划线，点一下会弹出
浮层（复制 / 分享 / 搜一搜 等）。目标是把这个痕迹也去掉。

状态：**未开始**。下面是开工前需要确认的事情，确认完再动手。

## 已知

- 公众号文章在 `WKWebView` 里渲染，域名 `mp.weixin.qq.com`，所以技术路线和知乎完全一样：
  hook `WKUserContentController -addUserScript:` 注入 `common/UnderlineKiller.h` 里的载荷。
- Bundle ID 是 `com.weixin`，过滤 plist 直接改这一行即可。
- 如果划线节点同样是 `span` + 下边框（大概率），共享载荷可能**一行代码都不用改**。

## 开工前要确认

1. **划线节点的 DOM 形态。** 知乎是 `span.highlight-wrap[data-highlight-id]` + `border-bottom-style: dashed`。
   微信是否用了不同的类名/属性？是否用 `text-decoration` 或伪元素画线？如果不是 `highlight-wrap`
   也不是 `data-highlight-id`，需要给载荷加一个可配置的 selector 参数。
2. **浮层是页面内还是原生。** 知乎的浮层是页面里的 DOM 节点；如果微信的浮层是原生
   `UIViewController`（半屏 sheet），`pointer-events: none` 这招就无效，得在原生层拦截
   点击/长按事件，工作量大得多。
3. **越狱检测。** 微信对越狱环境的检测比知乎激进，注入后要确认不影响登录、支付、小程序。

## 探测方法

公众号文章页不能开 Safari 远程调试（release 包不可调试），所以沿用知乎那套做法：
先出一个只注入探测脚本的版本，把命中元素的 `outerHTML` 和计算样式通过
`window.webkit.messageHandlers.<channel>.postMessage` 回传到原生，写到
`/tmp/nounderline.log`，拿到真实结构后再写正式的清理逻辑。

注意微信的 JS bridge 频道名和知乎不同（知乎是 `zhihuNativeApp`），需要先确定可用的频道，
否则回传通道建不起来。

## 落地步骤

1. `wechat/` 下建 `Makefile` / `Tweak.xm` / `control` / `NoUnderlineWeChat.plist`
   （`TWEAK_NAME = NoUnderlineWeChat`，`Package: com.xxhhlk.nounderline.wechat`）
2. 根 `Makefile` 的 `TARGETS` 加上 `wechat`
3. Choicy 里把 `NoUnderlineWeChat` 加进微信的 `allowedTweaks`（微信若也是白名单模式）
4. 在公众号文章页验证：划线消失、点划线不弹浮层、文章正常滚动、图片/视频正常
