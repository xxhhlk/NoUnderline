# NoUnderline

> iOS jailbreak tweaks that remove the dashed sentence underline ("热门划线" / highlight marks)
> from apps that render their articles in a web view.

去掉 App 里"句子划线"的虚线，以及点击划线句子弹出的浮层菜单。目前支持知乎，微信在计划中。

这类划线是平台推的"热门划线"：别人划过的句子会在正文里留下一条虚线，点一下还会弹出
`复制 / 点赞 / 分享 / 微信好友 / 搜索` 之类的浮层。本项目把这些痕迹全部清掉，同时不影响
App 的其它功能（评论区、正常滚动、长按选中文字、图片查看等）。

## 支持目标

| 目标 | Bundle ID | 插件名 | 状态 |
| --- | --- | --- | --- |
| 知乎 | `com.zhihu.ios` | `NoUnderlineZhihu` | ✅ 可用（v1.3.x，全文视图 + 卡片视图） |
| 微信（公众号文章划线） | `com.weixin` | `NoUnderlineWeChat` | 🚧 规划中，见 `wechat/README.md` |

## 知乎有两种视图，必须分别处理

这是本项目最容易踩空的一点：知乎同一篇回答有两种渲染方式，**只处理其中一种会表现为"插件时灵时不灵"**。

| 视图 | 渲染方式 | 划线的真实形态 | 处理手段 |
| --- | --- | --- | --- |
| 全文视图 | `WKWebView` 载 `www.zhihu.com/appview/v2/answer/<id>` | DOM 里 `span.highlight-wrap[data-highlight-id]` 的 `border-bottom-style: dashed` | 注入 JS |
| 卡片视图 | 原生 `ZRichCoreUI`（`ZRichTextView` 是 `UITextView`） | attributed string 上的 `NSUnderline = 257` 属性 | hook 原生方法 |

两者互不相通：往 web view 里注入再多 JS 也碰不到卡片视图，反之亦然。

### 全文视图（注入 JS）

公共代码在 `common/UnderlineKiller.h`，各目标只负责自己的过滤规则（Bundle ID）。

注入方式：hook `WKUserContentController -addUserScript:`，在每个 web view 的
`WKUserScriptInjectionTimeAtDocumentStart` 追加脚本（每个 controller 只注入一次）。

划线在 DOM 里的形态是 **下边框，不是 text-decoration**：

```html
<span class="highlight-wrap other" data-highlight-id="2095181581161862678">
  一百万砸下去，能给你损耗60个，然后说正常。
</span>
```

```css
border-bottom-style: dashed;   /* 计算样式 */
text-decoration: none;
```

页面自己的样式表里带 `!important`，任何后注入的 `<style>` 都会被它压过（同优先级下后者胜，
而页面的规则总是在我们之后追加）。所以只能用 **内联样式 + `'important'` 优先级**
（`el.style.setProperty(prop, val, 'important')`），内联的最高优先级永远赢。

清掉的属性：`border-bottom` 系列 / `text-decoration` / `background-image` /
`background-color` / `box-shadow`；伪元素（`::before` / `::after`）用一段注入的
`<style id="nhl-style">` 置为 `display:none`，并定期重新追加到 `<head>` 末尾防止被页面样式表压过。

点击浮层则在 **capture 阶段**拦掉：对落在划线元素上的 `click` / `touchend` 调
`stopImmediatePropagation()`，且**不** `preventDefault`，同时跳过 `a/button/input/textarea/select/label`，
所以文字选择、拖选、系统复制菜单、划线里的链接都不受影响。

#### 两个必须注意的坑

1. **`kill()` 必须幂等。** 写内联样式本身就是对 `style` 属性的修改，如果 MutationObserver
   监听了 `attributes`，回调会再次触发 `kill()`，每次产生 7 条 mutation，形成自我反馈死循环
   （WebKit 进程单核 100%、页面卡死）。现在的做法是：先检查
   `getPropertyPriority('border-bottom-style') === 'important'` 和元素上的 `__nhl` 标记，
   已生效就直接 return，零 mutation 产生；页面若重写了样式则自动补回。
2. **observer 只监听 `childList`。** 新增子树用 `requestAnimationFrame` 合并成每帧一次处理，
   不做全文档扫描式的 attributes 监听。

### 卡片视图（hook 原生）

用 `ZRichTextView.setAttributedText:` 的属性转储实测出来的结论，被划线的句子带：

```
NSUnderline      = 257      /* = 1 | 256 = NSUnderlineStyleSingle | NSUnderlinePatternDash */
NSUnderlineColor = sRGB(0.3529, 0.3686, 0.4)
```

就是一个普通 TextKit 下划线属性，所以直接在 `setAttributedText:` 里剥掉
（先 `enumerateAttribute:` 收集 range 再 `removeAttribute:`，避免边枚举边改；字符串长度不变，
收集到的 range 复用安全）。

浮层是知乎自绘的（实测 `canPerformAction:withSender:` 完全没被调用，所以不是系统
`UIMenuController`）。两个并列入口：

| 类方法 | 用途 | 处理 |
| --- | --- | --- |
| `SCPopMenuSegLike` `+showPopMenuWithMark:sectionController:datas:actionAssembler:config:` | 点划线专用 | 吞掉，不调 `%orig` |
| `SCPopMenuSelection` `+showPopMenuWithTextView:selText:selRawRange:node:containsFormula:actionAssembler:config:` | 普通选中文字专用 | 保留 |

只改前者，所以长按选中普通文字的复制/搜索菜单照常。

## 关于"清空接口里的划线数据"（走过的弯路）

早期版本试过从数据层下手，`_zhihu/nozhihuline_0.0.1{5,7,8,9}*.deb` 里还能抠出当时的实现：
剥 `window.__INITIAL_STATE__ / __APOLLO_STATE__ / __DATA__` 里的 `segmentInfos`、把
`allowSegmentInteraction` 置 0、hook `JSON.parse`、重写 `<script id="js-initialData">` 的
`textContent`……**全部无效**：数据确实被删掉了，虚线照旧。换成直接操作 DOM（v0.0.22 unwrap 节点、
v0.0.25 起内联 `!important`）才第一次真正生效。

原因是那条数据链和渲染链不是同一条（SSR 脚本里是 camelCase `segmentInfos`，公开 API 是
snake_case `segment_infos`），而且这类方案要追着对方的字段名和时序跑，稳定性差。
**所以本项目只走 DOM / 属性层，不碰数据。**

## 构建

需要 [Theos](https://theos.dev) 和 iOS SDK（本仓库用 `iPhoneOS13.7.sdk` 验证）。

```bash
export THEOS=~/theos
make package FINALPACKAGE=1        # 根目录：递归构建所有目标
make package -C zhihu              # 只构建知乎
```

产物在各自的 `packages/` 下，例如
`zhihu/packages/com.xxhhlk.nounderline.zhihu_1.3.3_iphoneos-arm.deb`。

`TARGET := iphone:clang:13.7:11.0` 里的 `11.0` 不能省：deployment target 设成 13.0 会让编译器
生成 `LC_BUILD_VERSION` 加载命令，老版本的 substitute-loader 不认识它，插件会静默不注入。
设成 11.0 才会生成老式的 `LC_VERSION_MIN_IPHONEOS`。

## 安装

```bash
dpkg -i com.xxhhlk.nounderline.zhihu_1.3.3_iphoneos-arm.deb
killall -9 osee2unifiedRelease
```

`dpkg -l | grep nounderline` 必须是 `ii`（`install ok installed`），且**包名后面不带 `:架构` 后缀**。

### `Architecture:` 必须写 `iphoneos-arm`，不能写 `iphoneos-arm64`

iOS 13 上 `dpkg --print-architecture` 报的是 `iphoneos-arm`。如果 `control` 里写
`Architecture: iphoneos-arm64`，会连锁出三个问题：

1. 安装必须加 `--force-architecture`，否则 dpkg 直接拒绝；
2. 依赖在**包自己的架构语境**里解析，`Depends: firmware (>= 13.0)` 会去找
   `firmware:iphoneos-arm64`（实际装的是 `firmware:iphoneos-arm`）→ 永远不满足 →
   包停在 `iU` 不配置 → 插件不注入；
3. 包在 dpkg 里显示成 `com.xxhhlk.nounderline.zhihu:iphoneos-arm64`（带后缀的"外来架构"包），
   **APT/Sileo 会按架构过滤掉它，已安装列表里根本看不到**。

二进制是 arm64 和包声明成 `iphoneos-arm` 并不冲突：前者是 Mach-O 里的 CPU 类型，后者是包管理器的
平台标识，rootful 越狱上就应该写 `iphoneos-arm`。

## 改 tweak 名 / 改 dylib 文件名的坑

改 `TWEAK_NAME` 属于**高风险操作**，实测踩过一次：把 `NoZhiHuLine` 改名成 `NoUnderlineZhihu` 后，
无论 `killall -9 osee2unifiedRelease` 还是 respring，`/var/log/extensionloader.log` 里都**没有**
这条 dylib 的 `Injecting` 行 —— 插件完全不加载，看起来像"插件失效了"。

对照实验：把一个确认能注入的 `Dim.dylib` + `Dim.plist` 原样复制成 `ZZTest.dylib` + `ZZTest.plist`，
respring 后 SpringBoard 的注入名单里有 `Dim`、没有 `ZZTest`。所以**同一个开机周期内新增的 dylib
文件名不会被加载器接纳**。

不过后来同一个新名字又自己好了（没有重启），所以机制不是简单的"开机快照"，还有别的因素在
（可能和当时的包/目录状态有关）。结论就是：**改名字要留出验证成本，临时验证逻辑改动时别改
`TWEAK_NAME`，只改代码。** 真需要改名，装完后重启一次设备（unc0ver 半不完美越狱要重新越狱）
最稳妥。

### 排查注入问题的顺序

1. `dpkg -l | grep <包名>` 必须是 `ii`，且没有 `:架构` 后缀；
2. `ls -la /usr/lib/TweakInject/` 确认 dylib + plist 都在（`/Library/MobileSubstrate/DynamicLibraries` 是它的软链）；
3. `grep 'Injecting.*<名字>' /var/log/extensionloader.log | tail` —— 没有就是加载器没选它；
4. 用 `python3 -c "import plistlib;print(plistlib.load(open('<名字>.plist','rb')))"` 确认过滤规则，
   注意 `plistlib` 比 `plutil -p` 可靠（`plutil -p` 在这个场合经常只打印 `{`）；
5. `%ctor` 里写的日志文件（本插件是 `/tmp/nounderline.log`）出现 `loaded (pid N)` 才算真正注入。

## Choicy 用户请注意

实测结论：**在 unc0ver + Substitute 下，Choicy 的按 App 白/黑名单并不参与注入决策**。证据是
SpringBoard 的 `allowedTweaks` 是空数组（白名单模式）却仍然注入了 83 个 dylib —— 注入完全由各插件
自己的 `Filter` plist 决定。所以本插件不需要在 Choicy 里做任何配置；换到 libhooker
（Odyssey / Taurine）时才需要再看。

## 已知取舍

- **1.1.0 起不再用 `pointer-events: none`**（它会让划线文字不可选中，整段正文手感都不对），
  改成 capture 阶段拦截，细节见上文。
- 浮层是"收不到事件"所以不弹，并没有主动去 `display:none` 那个浮层节点。网页版脚本的做法是
  识别浮层里的三个 SVG 图标（`ZDI--Repeat24` / `ZDI--Heart24` / `ZDI--ChatBubble24`）后直接隐藏，
  App 内的 DOM 结构如果一致也可以照做。
- **"划线句子里含链接"这个场景实测不存在。** 卡片视图里对 21 组含链接的正文做过范围比对，
  链接范围与划线范围 **零重叠**（`underlineOverlap=0`）；全文视图刷了多篇幅也没出现过划线包住 `<a>`
  的 DOM。知乎应该本来就不允许划线覆盖链接（链接要能点、划线要能点，两者冲突）。
  所以这里不需要额外处理。
- 知乎版本差异：原生类名随版本变过（`UnderlineOverlayView` 在 10.46 上已经不存在，
  `ZRichAPI +reRenderSegUnderlinesWithTextView:node:` 这个 selector 也查不到了）。现在的代码只 hook
  确实存在的两个目标。换大版本时如果失效，先用 `strings` 扫一遍二进制确认类名。

## TODO

- [x] 把 `pointer-events: none` 换成 capture 阶段拦截，恢复划线文字的可选中性（1.1.0）
- [x] 补 `::before` / `::after` 的 `display: none`（1.1.0）
- [x] 卡片视图（原生 ZRichCoreUI）支持（1.3.0 / 1.3.1）
- [ ] 微信 `com.weixin` 公众号文章划线支持
- [ ] 插件内提供开关（PreferenceLoader），不用改代码就能关掉

## 目录结构

```
.
├── Makefile                  # 根构建入口，递归到各目标
├── common/
│   └── UnderlineKiller.h     # 共享注入 JS（两个目标复用）
├── zhihu/
│   ├── Makefile
│   ├── Tweak.xm              # web view 注入 + 原生 hook
│   ├── control
│   └── NoUnderlineZhihu.plist
└── wechat/
    └── README.md             # 规划中
```

调试用的辅助脚本放在工作区的 `_zhihu/`（不在本仓库）：
- `objc_methods.py`：从解密后的知乎二进制里导出指定类的 ObjC 方法表，用来找 hook 目标。
- 更新它需要 `Zhihu.dec`（flexdecrypt 产物）和 `Zhihu.pad`（补足 `__LINKEDIT` 的副本）。

## 参考

网页版同类需求的现成方案是 [知乎增强](https://greasyfork.org/zh-CN/scripts/589027) 这个
用户脚本，它的"屏蔽划线浮层"开关做的是同一件事，处理伪元素和保留文字选中这两点上比本项目
当前实现更细致，可以对照着看。
