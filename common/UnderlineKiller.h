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

#ifndef UNDERLINE_KILLER_H
#define UNDERLINE_KILLER_H

static NSString * const kUnderlineKillerJS =
    @"(function(){"
    "if(window.__nhlLoaded)return;window.__nhlLoaded=1;"
    "var n=0,q=[],pend=false;"
    // Idempotent: bail out when our declarations are still intact.
    "function kill(el){"
    "  try{"
    "    var s=el.style;if(!s)return false;"
    "    if(el.__nhl===1"
    "       &&s.getPropertyPriority('border-bottom-style')==='important'"
    "       &&s.getPropertyPriority('pointer-events')==='important')return false;"
    "    el.__nhl=1;"
    "    s.setProperty('border-bottom','none','important');"
    "    s.setProperty('border-bottom-style','none','important');"
    "    s.setProperty('border-bottom-width','0','important');"
    "    s.setProperty('text-decoration','none','important');"
    "    s.setProperty('background-image','none','important');"
    "    s.setProperty('box-shadow','none','important');"
    "    s.setProperty('pointer-events','none','important');"
    "    n++;"
    "    return true;"
    "  }catch(e){return false;}"
    "}"
    "var SEL='.highlight-wrap,[data-highlight-id]';"
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
    "function start(){"
    "  sweep(document);"
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
    // Safety net for the page rewriting an existing span's style. The
    // idempotent kill() makes this a no-op when nothing changed.
    "  setInterval(function(){"
    "    if(document.hidden)return;"
    "    sweep(document);"
    "  },2500);"
    "}"
    "if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',start);"
    "else start();"
    "})();";

#endif
