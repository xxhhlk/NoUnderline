// Shared web-view payload for every target in this repo.
//
// Include this header from a tweak's Tweak.xm and add it as a
// WKUserScript at WKUserScriptInjectionTimeAtDocumentStart.
//
// The dashed line under a highlighted sentence is border-bottom-style:dashed
// on span.highlight-wrap[data-highlight-id] (not text-decoration). The page's
// own stylesheet declares it !important and is appended after anything we
// inject, so a <style> tag loses the cascade. Inline style with "important"
// priority always wins, so we write inline declarations instead.
//
// Keep kill() idempotent: writing inline styles is itself a style-attribute
// mutation, so a non-idempotent kill() plus an attributes observer is an
// infinite feedback loop that pins a WebKit core.
//
// 1.1.0 drops pointer-events:none. That trick killed the tap popup but also
// made the highlighted text a non hit-test target, so it could not be
// selected or long-pressed -- and on Zhihu it made the whole paragraph feel
// dead. Instead we let the element receive input and swallow the page's own
// click/touchend handlers in the capture phase (no preventDefault, so native
// text selection, drag-select and the system copy menu keep working).
// Pseudo-elements are neutralised too: the page may draw the line with
// ::before / ::after rather than on the element itself.

#ifndef UNDERLINE_KILLER_H
#define UNDERLINE_KILLER_H

static NSString * const kUnderlineKillerJS =
    @"(function(){"
    "if(window.__nhlLoaded)return;window.__nhlLoaded=1;"
    "var n=0,q=[],pend=false;"
    "var SEL='.highlight-wrap,[data-highlight-id]';"
    "var PSEUDO='.highlight-wrap::before,.highlight-wrap::after,"
    "[data-highlight-id]::before,[data-highlight-id]::after"
    "{content:none !important;display:none !important;"
    "border:0 !important;background:none !important;}';"
    // Idempotent: bail out when our declarations are still intact.
    "function kill(el){"
    "  try{"
    "    var s=el.style;if(!s)return false;"
    "    if(el.__nhl===1"
    "       &&s.getPropertyPriority('border-bottom-style')==='important'"
    "       &&s.getPropertyPriority('background-color')==='important')return false;"
    "    el.__nhl=1;"
    "    s.setProperty('border-bottom','none','important');"
    "    s.setProperty('border-bottom-style','none','important');"
    "    s.setProperty('border-bottom-width','0','important');"
    "    s.setProperty('text-decoration','none','important');"
    "    s.setProperty('background-image','none','important');"
    "    s.setProperty('background-color','transparent','important');"
    "    s.setProperty('box-shadow','none','important');"
    "    n++;"
    "    return true;"
    "  }catch(e){return false;}"
    "}"
    "function sweep(root){"
    "  var list=[];"
    "  try{list=(root&&root.querySelectorAll)?root.querySelectorAll(SEL):[];}catch(e){return;}"
    "  for(var i=0;i<list.length;i++)kill(list[i]);"
    "}"
    // Coalesce bursts of DOM changes into one pass per frame.
    "function drain(){"
    "  pend=false;"
    "  var list=q;q=[];"
    "  for(var i=0;i<list.length;i++){"
    "    var a=list[i];"
    "    if(!a||a.nodeType!==1)continue;"
    "    try{"
    "      if(a.matches&&a.matches(SEL))kill(a);"
    "      else if(a.querySelectorAll)sweep(a);"
    "    }catch(e){}"
    "  }"
    "}"
    "function push(node){"
    "  if(!node||node.nodeType!==1)return;"
    "  q.push(node);"
    "  if(pend)return;"
    "  pend=true;"
    "  if(window.requestAnimationFrame)window.requestAnimationFrame(drain);"
    "  else setTimeout(drain,16);"
    "}"
    // Pseudo-element rules. Kept at the end of <head> so they outrank the
    // page's own sheet; re-appended below if the page drops them.
    "function ensureStyle(){"
    "  try{"
    "    var s=document.getElementById('nhl-style');"
    "    if(!s){"
    "      s=document.createElement('style');"
    "      s.id='nhl-style';"
    "      s.textContent=PSEUDO;"
    "    }"
    "    var head=document.head||document.documentElement;"
    "    if(head&&s.parentNode!==head)head.appendChild(s);"
    "  }catch(e){}"
    "}"
    // Capture-phase tap swallow. No preventDefault: selection stays native.
    "function blockTap(e){"
    "  var t=e.target;"
    "  if(!t||t.nodeType!==1)return;"
    "  var line=null;"
    "  try{line=t.closest?t.closest(SEL):null;}catch(x){return;}"
    "  if(!line)return;"
    "  try{if(t.closest('a,button,input,textarea,select,label'))return;}catch(x){}"
    "  e.stopImmediatePropagation();"
    "}"
    "function start(){"
    "  ensureStyle();"
    "  sweep(document);"
    "  document.addEventListener('click',blockTap,true);"
    "  document.addEventListener('touchend',blockTap,true);"
    "  try{"
    "    var mo=new MutationObserver(function(ms){"
    "      for(var i=0;i<ms.length;i++){"
    "        var ad=ms[i].addedNodes;"
    "        if(!ad)continue;"
    "        for(var j=0;j<ad.length;j++)push(ad[j]);"
    "      }"
    "    });"
    "    mo.observe(document.documentElement,{childList:true,subtree:true});"
    "  }catch(e){}"
    // Safety net for the page rewriting an existing span's style, and for a
    // stylesheet that got replaced. kill() is idempotent so this is cheap.
    "  setInterval(function(){"
    "    if(document.hidden)return;"
    "    ensureStyle();"
    "    sweep(document);"
    "  },2500);"
    "}"
    "if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',start);"
    "else start();"
    "})();";

#endif
