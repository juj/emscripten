var LibraryJsInput = {
  $JsInput: {
    onKeyPressFunc: null,
    onKeyDownFunc: null,
    onKeyUpFunc: null,
    onClickFunc: null,
    onMouseDownFunc: null,
    onMouseUpFunc: null,
    onDblClickFunc: null,

    // The following functions receive Javascript KeyboardEvent objects.
    // See https://developer.mozilla.org/en/DOM/Event/UIEvent/KeyEvent
    // The contents of the event objects are passed to a C callback function of form
    //    void onKeyboardEvent(int eventType, int charCode, int keyCode, int which, bool ctrlKey, bool shiftKey, bool altKey, bool metaKey, bool repeat);
    // where eventType identifies the type of the JavaScript event generated:
    //   0: KeyPress
    //   1: KeyDown
    //   2: KeyUp
    // and the other parameters come from the event object as-is.

    emscripten_keypress_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onKeyPressFunc](0 /* KeyPress */, e.charCode, e.keyCode, e.which, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.repeat);
    },

    emscripten_keydown_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onKeyDownFunc](1 /* KeyDown */, e.charCode, e.keyCode, e.which, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.repeat);
    },

    emscripten_keyup_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onKeyUpFunc](2 /* KeyUp */, e.charCode, e.keyCode, e.which, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.repeat);
    },

    // The following functions receive JavaScript MouseEvent objects.
    // See https://developer.mozilla.org/en/DOM/MouseEvent
    // The contents of these event objects are passed to a C callback function of form
    //    void onMouseEvent(int eventType, long screenX, long screenY, long clientX, long clientY, bool ctrlKey, bool shiftKey, bool altKey, 
    //                      bool metaKey, unsigned short button, unsigned short buttons);
    // where eventType identifies the type of the JavaScript event generated:
    //   3: Click
    //   4: MouseDown
    //   5: MouseUp
    //   6: DblClick
    // and the other parameters come from the event object as-is.

    emscripten_click_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onClickFunc](3 /* Click */, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.button, e.buttons);
    },

    emscripten_mousedown_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onClickFunc](4 /* MouseDown */, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.button, e.buttons);
    },

    emscripten_mouseup_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onClickFunc](5 /* MouseUp */, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.button, e.buttons);
    },

    emscripten_dblclick_handler: function(event) {
        var e = event || window.event;
        FUNCTION_TABLE[JsInput.onClickFunc](6 /* DblClick */, e.screenX, e.screenY, e.clientX, e.clientY, e.ctrlKey, e.shiftKey, e.altKey, e.metaKey, e.button, e.buttons);
    }
  },

  emscripten_set_keypress_callback: function(callbackfunc) {	
    JsInput.onKeyPressFunc = callbackfunc;
    window.addEventListener("keypress", JsInput.emscripten_keypress_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("keypress", callbackfunc, true); } });
  },

  emscripten_set_keydown_callback: function(callbackfunc) {	
    JsInput.onKeyDownFunc = callbackfunc;
    window.addEventListener("keydown", JsInput.emscripten_keydown_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("keydown", callbackfunc, true); } });
  },

  emscripten_set_keyup_callback: function(callbackfunc) {	
    JsInput.onKeyUpFunc = callbackfunc;
    window.addEventListener("keyup", JsInput.emscripten_keyup_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("keyup", callbackfunc, true); } });
  },

  emscripten_set_click_callback: function(callbackfunc) {	
    JsInput.onClickFunc = callbackfunc;
    window.addEventListener("click", JsInput.emscripten_click_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("click", callbackfunc, true); } });
  },

  emscripten_set_mousedown_callback: function(callbackfunc) {	
    JsInput.onMouseDownFunc = callbackfunc;
    window.addEventListener("mousedown", JsInput.emscripten_mousedown_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("mousedown", callbackfunc, true); } });
  },

  emscripten_set_mouseup_callback: function(callbackfunc) {	
    JsInput.onMouseUpFunc = callbackfunc;
    window.addEventListener("mouseup", JsInput.emscripten_mouseup_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("mouseup", callbackfunc, true); } });
  },

  emscripten_set_dblclick_callback: function(callbackfunc) {	
    JsInput.onDblClickFunc = callbackfunc;
    window.addEventListener("dblclick", JsInput.emscripten_dblclick_handler, true);
    __ATEXIT__.push({ func: function() { window.removeEventListener("dblclick", callbackfunc, true); } });
  },
};

autoAddDeps(LibraryJsInput, '$JsInput');
mergeInto(LibraryManager.library, LibraryJsInput);
