# NoUnderline

> iOS jailbreak tweaks that remove the dashed sentence underline ("热门划线" / highlight marks)
> from apps that render their articles in a web view.

去掉 App 里"句子划线"的虚线，以及点击划线句子弹出的浮层菜单。目前支持知乎，微信在计划中。

这类划线是平台推的"热门划线"：别人划过的句子会在正文里留下一条虚线，点一下还会弹出
`复制 / 点赞 / 分享 / 微信好友 / 搜索` 之类的浮层。本项目把这些痕迹全部清掉，同时不影响
App 的其它功能（评论区、正常滚动、图片查看等）。

## 支持目标

| 目标 | Bundle ID | 插件名 | 状态 |
| --- | --- | --- | --- |
| 知乎 | `com.zhihu.ios` | `NoUnderlineZhihu` | ✅ 可用（v1.0.0） |
| 微信（公众号文章划线） | `com.weixin` | `NoUnderlineWeChat` | 🚧 规划中，见 `wechat/README.md` |

## 原理

两个目标都是 **WKWebView 渲染网页**，所以原生层的 `UIMenuController` / 富文本 hook 全部无效，
真正要做的是往页面里注入 JS。公共注入代码集中在 `common/UnderlineKiller.h`，各目标只负责
自己的过滤规则（Bundle ID）和必要的微调。

注入方式：hook `WKUserContentController -addUserScript:`，在每个 web view 的
`WKUserScriptInjectionTimeAtDocumentStart` 追加一段脚本（每个 controller 只注入一次）。

划线在 DOM 里的真实形态是 **下边框，不是 text-decoration**：

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

清掉的属性：`border-bottom` 系列 / `text-decoration` / `background-image` / `box-shadow`，
再对元素设 `pointer-events: none`，让点击落到外层没有 highlight id 的 `<p>` 上，浮层就不会弹。

### 两个必须注意的坑

1. **`kill()` 必须幂等。** 写内联样式本身就是对 `style` 属性的修改，如果 MutationObserver
   监听了 `attributes`，回调会再次触发 `kill()`，每次产生 7 条 mutation，形成自我反馈死循环
   （WebKit 进程单核 100%、页面卡死）。现在的做法是：先检查
   `getPropertyPriority('border-bottom-style') === 'important'` 和元素上的 `__nhl` 标记，
   已生效就直接 return，零 mutation 产生；页面若重写了样式则自动补回。
2. **observer 只监听 `childList`。** 新增子树用 `requestAnimationFrame` 合并成每帧一次处理，
   不做全文档扫描式的 attributes 监听。

## 构建

需要 [Theos](https://theos.dev) 和 iOS SDK（本仓库用 `iPhoneOS13.7.sdk` 验证）。

```bash
export THEOS=~/theos
make package FINALPACKAGE=1        # 根目录：递归构建所有目标
make package -C zhihu              # 只构建知乎
```

产物在各自的 `packages/` 下，例如
`zhihu/packages/com.xxhhlk.nounderline.zhihu_1.0.0_iphoneos-arm64.deb`。

`TARGET := iphone:clang:13.7:11.0` 里的 `11.0` 不能省：deployment target 设成 13.0 会让编译器
生成 `LC_BUILD_VERSION` 加载命令，老版本的 substitute-loader 不认识它，插件会静默不注入。
设成 11.0 才会生成老式的 `LC_VERSION_MIN_IPHONEOS`。

## 安装

```bash
# iOS 13 上 dpkg 把系统架构报成 iphoneos-arm，而包声明的是 iphoneos-arm64
dpkg --force-architecture -i com.xxhhlk.nounderline.zhihu_1.0.0_iphoneos-arm64.deb
dpkg --configure --force-depends com.xxhhlk.nounderline.zhihu:iphoneos-arm64
killall -9 osee2unifiedRelease
```

`dpkg -l | grep nounderline` 必须是 `ii`（`install ok installed`）。如果停在 `iU`
（unpacked），Substitute 会跳过这个插件不加载 —— 通常是因为依赖不满足，用上面的
`--configure --force-depends` 补一次。

## Choicy 用户请注意

如果装了 Choicy，插件是否注入取决于它的配置：

- **全局/该 App 是黑名单模式**：什么都不用做。
- **该 App 是白名单模式（`allowDenyMode = 2`）**：必须把对应的插件名
  （`NoUnderlineZhihu` / 后续的 `NoUnderlineWeChat`）加进这个 App 的 `allowedTweaks`，
  否则插件不注入。知乎默认就是白名单模式。

## 已知取舍

- `pointer-events: none` 是"让浮层不弹"的最省事做法，副作用是**划线的那句文字没法长按选中/复制**
  （拖选整段仍然可以）。更干净的做法是在 capture 阶段对划线元素 `stopImmediatePropagation()`
  且不 `preventDefault`，可以保留选中能力 —— 见下方 TODO。
- 浮层目前是靠"点不到"间接消除的，没有主动去 `display:none` 那个浮层节点。网页版脚本的做法是
  识别浮层里的三个 SVG 图标（`ZDI--Repeat24` / `ZDI--Heart24` / `ZDI--ChatBubble24`）后直接隐藏，
  App 内的 DOM 结构如果一致也可以照做。

## TODO

- [ ] 把 `pointer-events: none` 换成 capture 阶段拦截 click，恢复划线文字的可选中性
- [ ] 补 `::before` / `::after` 的 `display: none`（页面可能用伪元素画装饰线）
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
│   ├── Tweak.xm              # 只有 hook 和日志
│   ├── control
│   └── NoUnderlineZhihu.plist
└── wechat/
    └── README.md             # 规划中
```

## 参考

网页版同类需求的现成方案是 [知乎增强](https://greasyfork.org/zh-CN/scripts/589027) 这个
用户脚本，它的"屏蔽划线浮层"开关做的是同一件事，处理伪元素和保留文字选中这两点上比本项目
当前实现更细致，可以对照着看。
