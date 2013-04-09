var g = void 0, h = !0, i = null, j = !1;
try {
  this.Module = Module;
} catch (aa) {
  this.Module = Module = {};
}
var k = "object" === typeof process && "function" === typeof require, l = "object" === typeof window, m = "function" === typeof importScripts, q = !l && !k && !m;
if (k) {
  Module.print = (function(a) {
    process.stdout.write(a + "\n");
  });
  Module.printErr = (function(a) {
    process.stderr.write(a + "\n");
  });
  var r = require("fs"), s = require("path");
  Module.read = (function(a, c) {
    var a = s.normalize(a), b = r.readFileSync(a);
    !b && a != s.resolve(a) && (a = path.join(__dirname, "..", "src", a), b = r.readFileSync(a));
    b && !c && (b = b.toString());
    return b;
  });
  Module.readBinary = (function(a) {
    return Module.read(a, h);
  });
  Module.load = (function(a) {
    t(read(a));
  });
  Module.arguments || (Module.arguments = process.argv.slice(2));
}
q && (Module.print = print, "undefined" != typeof printErr && (Module.printErr = printErr), Module.read = read, Module.readBinary = (function(a) {
  return read(a, "binary");
}), Module.arguments || ("undefined" != typeof scriptArgs ? Module.arguments = scriptArgs : "undefined" != typeof arguments && (Module.arguments = arguments)));
l && !m && (Module.print || (Module.print = (function(a) {
  console.log(a);
})), Module.printErr || (Module.printErr = (function(a) {
  console.log(a);
})));
if (l || m) {
  Module.read = (function(a) {
    var c = new XMLHttpRequest;
    c.open("GET", a, j);
    c.send(i);
    return c.responseText;
  }), Module.arguments || "undefined" != typeof arguments && (Module.arguments = arguments);
}
m && (Module.print || (Module.print = (function() {})), Module.load = importScripts);
if (!m && !l && !k && !q) {
  throw "Unknown runtime environment. Where are we?";
}
function t(a) {
  eval.call(i, a);
}
"undefined" == !Module.load && Module.read && (Module.load = (function(a) {
  t(Module.read(a));
}));
Module.print || (Module.print = (function() {}));
Module.printErr || (Module.printErr = Module.print);
Module.arguments || (Module.arguments = []);
Module.print = Module.print;
Module.a = Module.printErr;
Module.preRun || (Module.preRun = []);
Module.postRun || (Module.postRun = []);
function u() {
  var a = [], c = 0;
  this.i = (function(b) {
    b &= 255;
    c && (a.push(b), c--);
    if (0 == a.length) {
      if (128 > b) {
        return String.fromCharCode(b);
      }
      a.push(b);
      c = 191 < b && 224 > b ? 1 : 2;
      return "";
    }
    if (0 < c) {
      return "";
    }
    var b = a[0], d = a[1], e = a[2], b = 191 < b && 224 > b ? String.fromCharCode((b & 31) << 6 | d & 63) : String.fromCharCode((b & 15) << 12 | (d & 63) << 6 | e & 63);
    a.length = 0;
    return b;
  });
  this.j = (function(a) {
    for (var a = unescape(encodeURIComponent(a)), c = [], e = 0; e < a.length; e++) {
      c.push(a.charCodeAt(e));
    }
    return c;
  });
}
function v(a) {
  var c = w;
  w = w + a | 0;
  w = w + 3 >> 2 << 2;
  return c;
}
function x(a) {
  var c = y;
  y = y + a | 0;
  y = y + 3 >> 2 << 2;
  y >= z && A("Cannot enlarge memory arrays. Either (1) compile with -s TOTAL_MEMORY=X with X higher than the current value, (2) compile with ALLOW_MEMORY_GROWTH which adjusts the size at runtime but prevents some optimizations, or (3) set Module.TOTAL_MEMORY before the program runs.");
  return c;
}
var B = 4, ca = {}, C;
function A(a) {
  Module.print(a + ":\n" + Error().stack);
  throw "Assertion: " + a;
}
function D(a, c) {
  a || A("Assertion failed: " + c);
}
var da = this;
Module.ccall = (function(a, c, b, d) {
  return E(ea(a), c, b, d);
});
function ea(a) {
  try {
    var c = da.Module["_" + a];
    c || (c = eval("_" + a));
  } catch (b) {}
  D(c, "Cannot call unknown function " + a + " (perhaps LLVM optimizations or closure removed it?)");
  return c;
}
function E(a, c, b, d) {
  function e(a, c) {
    if ("string" == c) {
      if (a === i || a === g || 0 === a) {
        return 0;
      }
      f || (f = w);
      var b = v(a.length + 1);
      fa(a, b);
      return b;
    }
    return "array" == c ? (f || (f = w), b = v(a.length), ga(a, b), b) : a;
  }
  var f = 0, n = 0, d = d ? d.map((function(a) {
    return e(a, b[n++]);
  })) : [];
  a = a.apply(i, d);
  "string" == c ? c = ha(a) : (D("array" != c), c = a);
  f && (w = f);
  return c;
}
Module.cwrap = (function(a, c, b) {
  var d = ea(a);
  return (function() {
    return E(d, c, b, Array.prototype.slice.call(arguments));
  });
});
function ia(a, c, b) {
  b = b || "i8";
  "*" === b.charAt(b.length - 1) && (b = "i32");
  switch (b) {
   case "i1":
    F[a] = c;
    break;
   case "i8":
    F[a] = c;
    break;
   case "i16":
    G[a >> 1] = c;
    break;
   case "i32":
    I[a >> 2] = c;
    break;
   case "i64":
    C = [ c >>> 0, Math.min(Math.floor(c / 4294967296), 4294967295) >>> 0 ];
    I[a >> 2] = C[0];
    I[a + 4 >> 2] = C[1];
    break;
   case "float":
    J[a >> 2] = c;
    break;
   case "double":
    J[a >> 2] = c;
    break;
   default:
    A("invalid type for setValue: " + b);
  }
}
Module.setValue = ia;
Module.getValue = (function(a, c) {
  c = c || "i8";
  "*" === c.charAt(c.length - 1) && (c = "i32");
  switch (c) {
   case "i1":
    return F[a];
   case "i8":
    return F[a];
   case "i16":
    return G[a >> 1];
   case "i32":
    return I[a >> 2];
   case "i64":
    return I[a >> 2];
   case "float":
    return J[a >> 2];
   case "double":
    return J[a >> 2];
   default:
    A("invalid type for setValue: " + c);
  }
  return i;
});
var K = 2, L = 3;
Module.ALLOC_NORMAL = 0;
Module.ALLOC_STACK = 1;
Module.ALLOC_STATIC = K;
Module.ALLOC_NONE = L;
function M(a, c, b, d) {
  var e, f;
  "number" === typeof a ? (e = h, f = a) : (e = j, f = a.length);
  var n = "string" === typeof c ? c : i, b = b == L ? d : [ ja, v, x ][b === g ? K : b](Math.max(f, n ? 1 : c.length));
  if (e) {
    d = b;
    D(0 == (b & 3));
    for (a = b + (f & -4); d < a; d += 4) {
      I[d >> 2] = 0;
    }
    for (a = b + f; d < a; ) {
      F[d++ | 0] = 0;
    }
    return b;
  }
  if ("i8" === n) {
    return N.set(new Uint8Array(a), b), b;
  }
  for (var d = 0, p, ba; d < f; ) {
    var H = a[d];
    "function" === typeof H && (H = ca.o(H));
    e = n || c[d];
    0 === e ? d++ : ("i64" == e && (e = "i32"), ia(b + d, H, e), ba !== e && (1 == B ? p = 1 : (p = {
      "%i1": 1,
      "%i8": 1,
      "%i16": 2,
      "%i32": 4,
      "%i64": 8,
      "%float": 4,
      "%double": 8
    }["%" + e], p || ("*" == e.charAt(e.length - 1) ? p = B : "i" == e[0] && (p = parseInt(e.substr(1)), D(0 == p % 8), p /= 8))), ba = e), d += p);
  }
  return b;
}
Module.allocate = M;
function ha(a, c) {
  for (var b = j, d, e = 0; ; ) {
    d = N[a + e | 0];
    if (128 <= d) {
      b = h;
    } else {
      if (0 == d && !c) {
        break;
      }
    }
    e++;
    if (c && e == c) {
      break;
    }
  }
  c || (c = e);
  var f = "";
  if (!b) {
    for (; 0 < c; ) {
      d = String.fromCharCode.apply(String, N.subarray(a, a + Math.min(c, 1024))), f = f ? f + d : d, a += 1024, c -= 1024;
    }
    return f;
  }
  b = new u;
  for (e = 0; e < c; e++) {
    d = N[a + e | 0], f += b.i(d);
  }
  return f;
}
Module.Pointer_stringify = ha;
var F, N, G, ka, I, O, J, la, w, y, P = Module.TOTAL_STACK || 5242880, z = Module.TOTAL_MEMORY || 16777216;
D(!!Int32Array && !!Float64Array && !!(new Int32Array(1)).subarray && !!(new Int32Array(1)).set, "Cannot fallback to non-typed array case: Code is too specialized");
var Q = new ArrayBuffer(z);
F = new Int8Array(Q);
G = new Int16Array(Q);
I = new Int32Array(Q);
N = new Uint8Array(Q);
ka = new Uint16Array(Q);
O = new Uint32Array(Q);
J = new Float32Array(Q);
la = new Float64Array(Q);
I[0] = 255;
D(255 === N[0] && 0 === N[3], "Typed arrays 2 must be run on a little-endian system");
Module.HEAP = g;
Module.HEAP8 = F;
Module.HEAP16 = G;
Module.HEAP32 = I;
Module.HEAPU8 = N;
Module.HEAPU16 = ka;
Module.HEAPU32 = O;
Module.HEAPF32 = J;
Module.HEAPF64 = la;
w = 4 * Math.ceil(.25);
var ma = M(12, "i8", 1);
D(0 == 8 * Math.ceil(ma / 8) % 8);
y = P;
D(y < z);
M(R("(null)"), "i8", 1);
function S(a) {
  for (; 0 < a.length; ) {
    var c = a.shift(), b = c.n;
    if ("number" === typeof b) {
      if (c.b === g) {
        T[b]();
      } else {
        (c = [ c.b ]) && c.length ? T[b].apply(i, c) : T[b]();
      }
    } else {
      b(c.b === g ? i : c.b);
    }
  }
}
var na = [], oa = [], pa = [], U = j;
function R(a, c, b) {
  a = (new u).j(a);
  b && (a.length = b);
  c || a.push(0);
  return a;
}
Module.intArrayFromString = R;
Module.intArrayToString = (function(a) {
  for (var c = [], b = 0; b < a.length; b++) {
    var d = a[b];
    255 < d && (d &= 255);
    c.push(String.fromCharCode(d));
  }
  return c.join("");
});
function fa(a, c, b) {
  a = R(a, b);
  for (b = 0; b < a.length; ) {
    F[c + b | 0] = a[b], b += 1;
  }
}
Module.writeStringToMemory = fa;
function ga(a, c) {
  for (var b = 0; b < a.length; b++) {
    F[c + b | 0] = a[b];
  }
}
Module.writeArrayToMemory = ga;
Math.h || (Math.h = (function(a, c) {
  var b = a & 65535, d = c & 65535;
  return b * d + ((a >>> 16) * d + b * (c >>> 16) << 16) | 0;
}));
var V = 0, W = {}, qa = j, X = i;
Module.addRunDependency = (function(a) {
  V++;
  Module.monitorRunDependencies && Module.monitorRunDependencies(V);
  a ? (D(!W[a]), W[a] = 1, X === i && "undefined" !== typeof setInterval && (X = setInterval((function() {
    var a = j, b;
    for (b in W) {
      a || (a = h, Module.a("still waiting on run dependencies:")), Module.a("dependency: " + b);
    }
    a && Module.a("(end of list)");
  }), 6e3))) : Module.a("warning: run dependency added without ID");
});
Module.removeRunDependency = (function(a) {
  V--;
  Module.monitorRunDependencies && Module.monitorRunDependencies(V);
  a ? (D(W[a]), delete W[a]) : Module.a("warning: run dependency removed without ID");
  0 == V && (X !== i && (clearInterval(X), X = i), !qa && Y && Z());
});
Module.preloadedImages = {};
Module.preloadedAudios = {};
D(y == P);
D(P == P);
y += 4;
D(y < z);
M([ 0, 0, 0, 0 ], "i8", L, P);
function ja(a) {
  return x(a + 8) + 8 & 4294967288;
}
var ra = j, $ = j, sa = g, ta = g;
function ua(a, c) {
  function b() {
    $ = j;
    (document.webkitFullScreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.mozFullscreenElement || document.fullScreenElement || document.fullscreenElement) === d ? (d.c = document.cancelFullScreen || document.mozCancelFullScreen || document.webkitCancelFullScreen, d.c = d.c.bind(document), sa && d.p(), $ = h, ta && va()) : ta && wa();
    if (Module.onFullScreen) {
      Module.onFullScreen($);
    }
  }
  this.d = a;
  this.e = c;
  "undefined" === typeof this.d && (this.d = h);
  "undefined" === typeof this.e && (this.e = j);
  var d = Module.canvas;
  this.g || (this.g = h, document.addEventListener("fullscreenchange", b, j), document.addEventListener("mozfullscreenchange", b, j), document.addEventListener("webkitfullscreenchange", b, j));
  d.k = d.requestFullScreen || d.mozRequestFullScreen || (d.webkitRequestFullScreen ? (function() {
    d.webkitRequestFullScreen(Element.ALLOW_KEYBOARD_INPUT);
  }) : i);
  d.k();
}
var xa = [];
function ya() {
  var a = Module.canvas;
  xa.forEach((function(c) {
    c(a.width, a.height);
  }));
}
function va() {
  var a = Module.canvas;
  this.m = a.width;
  this.l = a.height;
  a.width = screen.width;
  a.height = screen.height;
  a = O[SDL.screen + 0 * B >> 2];
  I[SDL.screen + 0 * B >> 2] = a | 8388608;
  ya();
}
function wa() {
  var a = Module.canvas;
  a.width = this.m;
  a.height = this.l;
  a = O[SDL.screen + 0 * B >> 2];
  I[SDL.screen + 0 * B >> 2] = a & -8388609;
  ya();
}
Module.requestFullScreen = (function(a, c) {
  ua(a, c);
});
Module.requestAnimationFrame = (function(a) {
  window.requestAnimationFrame || (window.requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame || window.webkitRequestAnimationFrame || window.msRequestAnimationFrame || window.oRequestAnimationFrame || window.setTimeout);
  window.requestAnimationFrame(a);
});
Module.pauseMainLoop = (function() {});
Module.resumeMainLoop = (function() {
  ra && (ra = j, i());
});
var T = [ 0, 0 ];
Module._main = (function() {
  for (var a = 0; !(I[1310720] = I[1310720] + 1 | 0, a = a + 1 | 0, 1e6 == (a | 0)); ) {}
  return 0;
});
Module.f = (function(a) {
  function c() {
    for (var a = 0; 3 > a; a++) {
      d.push(0);
    }
  }
  D(0 == V, "cannot call main when async dependencies remain! (listen on __ATMAIN__)");
  a = a || [];
  U || (U = h, S(na));
  var b = a.length + 1, d = [ M(R("/bin/this.program"), "i8", K) ];
  c();
  for (var e = 0; e < b - 1; e += 1) {
    d.push(M(R(a[e]), "i8", K)), c();
  }
  d.push(0);
  var d = M(d, "i32", K), f, a = w;
  try {
    f = Module._main(b, d, 0);
  } catch (n) {
    if ("ExitStatus" == n.name) {
      return n.status;
    }
    if ("SimulateInfiniteLoop" == n) {
      Module.noExitRuntime = h;
    } else {
      throw n;
    }
  } finally {
    w = a;
  }
  return f;
});
function Z(a) {
  function c() {
    U || (U = h, S(na));
    var b = 0;
    qa = h;
    Module._main && (S(oa), b = Module.f(a), Module.noExitRuntime || S(pa));
    if (Module.postRun) {
      for ("function" == typeof Module.postRun && (Module.postRun = [ Module.postRun ]); 0 < Module.postRun.length; ) {
        Module.postRun.pop()();
      }
    }
    return b;
  }
  a = a || Module.arguments;
  if (0 < V) {
    return Module.a("run() called, but dependencies remain, so not running"), 0;
  }
  if (Module.preRun) {
    "function" == typeof Module.preRun && (Module.preRun = [ Module.preRun ]);
    var b = Module.preRun;
    Module.preRun = [];
    for (var d = b.length - 1; 0 <= d; d--) {
      b[d]();
    }
    if (0 < V) {
      return 0;
    }
  }
  return Module.setStatus ? (Module.setStatus("Running..."), setTimeout((function() {
    setTimeout((function() {
      Module.setStatus("");
    }), 1);
    c();
  }), 1), 0) : c();
}
Module.run = Module.q = Z;
if (Module.preInit) {
  for ("function" == typeof Module.preInit && (Module.preInit = [ Module.preInit ]); 0 < Module.preInit.length; ) {
    Module.preInit.pop()();
  }
}
var Y = h;
Module.noInitialRun && (Y = j);
Y && Z();
