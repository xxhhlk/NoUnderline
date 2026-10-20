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
`zhihu/packages/com.xxhhlk.nounderline.zhihu_1.0.1_iphoneos-arm.deb`。

`TARGET := iphone:clang:13.7:11.0` 里的 `11.0` 不能省：deployment target 设成 13.0 会让编译器
生成 `LC_BUILD_VERSION` 加载命令，老版本的 substitute-loader 不认识它，插件会静默不注入。
设成 11.0 才会生成老式的 `LC_VERSION_MIN_IPHONEOS`。

## 安装

```bash
dpkg -i com.xxhhlk.nounderline.zhihu_1.0.1_iphoneos-arm.deb
killall -9 osee2unifiedRelease
```

`dpkg -l | grep nounderline` 必须是 `ii`（`install ok installed`），且**包名后面不带 `:架构` 后缀**。
如果停在 `iU`（unpacked），Substitute 会跳过这个插件不加载。

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

## 改名 / 改 dylib 文件名的坑（实测）

**Substitute 的插件清单是开机时建立的快照，运行时新增的 dylib 文件名不会被注入 —— respring 也不行，
必须重启（unc0ver 半不完美越狱要重新越狱）。**

验证方法：把一个确认能注入的 `Dim.dylib` + `Dim.plist` 复制成 `ZZTest.dylib` + `ZZTest.plist`，
respring 后看 SpringBoard 的注入日志 —— `Dim` 在名单里，`ZZTest` 不在。同理，把本插件从
`NoZhiHuLine` 改名成 `NoUnderlineZhihu` 后，它在任何进程里都不再被注入，日志里连 "Injecting" 都没有。

所以：改 tweak 名 = 需要重启一次。临时验证逻辑改动时，**别改 `TWEAK_NAME`**，只改代码。

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
