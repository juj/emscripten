mergeInto(LibraryManager.library, {
  $WebGLProfiler__deps: ['$GL'],
  $WebGLProfiler: {
    renderedFrameCount: 0,
    vertexDrawCount: 0,
    triangleDrawCount: 0,
    drawCount: 0,
    functionCount: {},

    logTimer: function() {
      console.log(WebGLProfiler.renderedFrameCount + ' frames rendered, '
        + WebGLProfiler.drawCount + ' draw calls ('
        + (WebGLProfiler.drawCount / WebGLProfiler.renderedFrameCount).toFixed(2) + ' draws/frame), '
        + WebGLProfiler.triangleDrawCount + ' triangles ('
        + (WebGLProfiler.triangleDrawCount/WebGLProfiler.renderedFrameCount).toFixed(2) + ' tris/frame, '
        + (WebGLProfiler.triangleDrawCount/WebGLProfiler.drawCount).toFixed(2) + ' tris/call), '
        + WebGLProfiler.vertexDrawCount + ' vertices ('
        + (WebGLProfiler.vertexDrawCount/WebGLProfiler.renderedFrameCount).toFixed(2) + ' verts/frame, '
        + (WebGLProfiler.vertexDrawCount/WebGLProfiler.drawCount).toFixed(2) + ' verts/call).');
      WebGLProfiler.renderedFrameCount = WebGLProfiler.drawCount = WebGLProfiler.triangleDrawCount = WebGLProfiler.vertexDrawCount = 0;
    },

    raf: function() {
      ++WebGLProfiler.renderedFrameCount;
      requestAnimationFrame(WebGLProfiler.raf);
    },

    createProfilerOnContext: function(contextHandle) {
      console.log('Started profiling WebGL context ' + contextHandle);
      WebGLProfiler.functionCount['drawArrays'] = 0;
      setInterval(WebGLProfiler.logTimer, 1000);
      requestAnimationFrame(WebGLProfiler.raf);
    },

    drawArrays: function(mode, first, count) {
      ++WebGLProfiler.functionCount['drawArrays'];
      ++WebGLProfiler.drawCount;
      WebGLProfiler.vertexDrawCount += count;
      switch(mode) {
        case 0 /*GL_POINTS*/:
          WebGLProfiler.triangleDrawCount += count; // Behave as if a single point == a single triangle
          break;
        case 1 /*GL_LINES*/:
          WebGLProfiler.triangleDrawCount += count/2; // One line == one triangle
          break;
        case 2 /*GL_LINE_LOOP*/:
          WebGLProfiler.triangleDrawCount += count/2 + 1; // One line == one triangle
          break;
        case 3 /*GL_LINE_STRIP*/:
          WebGLProfiler.triangleDrawCount += count - 1; // One line == one triangle
          break;
        case 4 /*GL_TRIANGLES*/:
          WebGLProfiler.triangleDrawCount += count / 3;
          break;
        case 5 /*GL_TRIANGLE_STRIP*/:
        case 6 /*GL_TRIANGLE_FAN*/:
          WebGLProfiler.triangleDrawCount += count - 2;
          break;
      }
    }
  }
});
