let COV_log = new Uint8Array(1);
let COV_blocksCalled = 0;
let COV_largestId = 0;

function COV_resize(largestId, canShrink) {
  let newSize = (largestId+8)>>3;
  if (!canShrink) newSize = Math.max(newSize, COV_log.length*2);
  let newLogArray = new Uint8Array(newSize);
  newLogArray.set(COV_log.subarray(0, Math.min(COV_log.length, newLogArray.length)));
  console.log(`Resized coverage log array from ${COV_log.length} to ${newLogArray.length}. COV_largestId=${COV_largestId}`);
  COV_log = newLogArray;
}

if (typeof document !== 'undefined') {
  let COV_div = document.createElement("div");
  COV_div.innerHTML = "<div style='border: 2px solid black; padding: 2px;'> <button style='color:red;font-size:23px' id=saveCoverageFileButton>Save Code Coverage Data</button> <span id='coverageCounter'>0%</span> blocks of code executed.</div>";
  document.body.appendChild(COV_div);
  document.getElementById("saveCoverageFileButton").onclick = () => {
    COV_resize(COV_largestId, /*canShrink=*/true);
    let a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([COV_log.buffer], { type: "text/csv" }));
    a.download = (location.pathname.substring(location.pathname.lastIndexOf('/')+1) || "index.html").replace('.html', '.cov');
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
  };
  setInterval(() => {
      document.getElementById("coverageCounter").innerHTML = `${COV_blocksCalled}/${COV_largestId} (${(COV_blocksCalled * 100 / COV_largestId).toFixed(2)}%)`;
    }, 500);
}

function COV_log_execution(id) {
  if (id >= COV_log.length << 3) COV_resize(id);
  let idx = id >> 3;
  let mask = 1 << (id & 7);
  if (!(COV_log[idx] & mask)) {
    ++COV_blocksCalled;
    COV_log[idx] |= mask;
  }
  if (id > COV_largestId) COV_largestId = id;
}
