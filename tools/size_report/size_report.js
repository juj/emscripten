#!/usr/bin/env node
var fs = require('fs');
var assert = require('assert');

const { mergeKeyValues } = require('./utils');
const { extractBoolCmdLineInput, extractNumberCmdLineInput, extractStringCmdLineInput } = require('./cmd_line_parser');
const { createSymbolMapFromSourceMap, readSymbolMap } = require('./source_map_parser');
const javascript_code_size = require('./javascript_code_size');
const { extractJavaScriptCodeSize } = javascript_code_size;
const { extractWasmCodeSize } = require('./wasm_code_size');

function run(args, printOutput) {
  var outputJson = extractBoolCmdLineInput(args, '--json');
  var symbolMap = extractStringCmdLineInput(args, '--symbols');
  var sourceMap = extractStringCmdLineInput(args, '--createSymbolMapFromSourceMap');
  var dumpSymbol = extractStringCmdLineInput(args, '--dump');
  javascript_code_size.expandSymbolsLargerThanPercents = extractNumberCmdLineInput(args, '--expandLargerThanPercents', javascript_code_size.expandSymbolsLargerThanPercents);
  if (javascript_code_size.expandSymbolsLargerThanPercents > 1) javascript_code_size.expandSymbolsLargerThanPercents /= 100.0;
  javascript_code_size.expandSymbolsLargerThanBytes = extractNumberCmdLineInput(args, '--expandLargerThanBytes', javascript_code_size.expandSymbolsLargerThanBytes);
  while (dumpSymbol) {
    javascript_code_size.dumpJsTextContents.push(dumpSymbol);
    dumpSymbol = extractStringCmdLineInput(args, '--dump');
  }
  var sources = args;

  symbolMap = symbolMap ? readSymbolMap(symbolMap) : {};

  // Doing Source Map -> Symbol Map translation?
  if (sourceMap) {
    assert(sources.length == 1); // Must present exactly one input JS file when doing source map -> symbol map translation
    var symbolMap = createSymbolMapFromSourceMap(sourceMap, sources[0]);

    if (printOutput) {
      if (outputJson) {
        console.log(JSON.stringify(symbolMap));
      } else {
        for(var i in symbolMap) {
          console.log(i + ':' + symbolMap[i]);
        }
      }
    }
    return symbolMap;
  }
  // ... if not, then proceed with a regular JS size analysis run:

  var codeSizes = {};
  for(var s in sources) {
    var src = sources[s];
    if (src.toLowerCase().endsWith('.js')) {
      mergeKeyValues(codeSizes, extractJavaScriptCodeSize(src, symbolMap));
    } else if (src.endsWith('.wasm')) {
      mergeKeyValues(codeSizes, extractWasmCodeSize(src));
    }
  }

  codeSizes = Object.values(codeSizes).sort((a, b) => {
    return (a.ordinal|0) - (b.ordinal|0);
    //return b.size - a.size;
  })

  function demangleSymbol(node, symbolMap) {
    if (symbolMap) {
      var demangledName = symbolMap[node.name] || symbolMap[node.selfName];
      if (demangledName) return node.prefix + demangledName;
    }
    return node.name;
  }

  if (printOutput) {
    if (outputJson) {
      console.log(JSON.stringify(codeSizes));
    } else {
      console.log('--- Code sizes:');
      for(var i in codeSizes) {
        var node = codeSizes[i];
        if (['function','import','export'].includes(node.type))
        console.log(node.file + '/' + node.type + ' ' + demangleSymbol(node, symbolMap) + (node.desc ? ('=' + node.desc) : '') + ': ordinal ' + node.ordinal);
//        console.log(node.file + '/' + node.type + ' ' + demangleSymbol(node, symbolMap) + (node.desc ? ('=' + node.desc) : '') + ': ' + node.size);
      }
    }
  }
  return codeSizes;
}

function jsonContains(text, actual, expected) {
  console.log(text);
  console.log('---- actual result: ----');
  console.dir(actual);

  function findObjectByName(arr, obj) {
    for(var i in arr) {
      if (arr[i].name == obj.name) {
        return arr[i];
      }
    }
  }

  for(var i in expected) {
    var e = expected[i];
    var a = findObjectByName(actual, e);
    if (!a) {
      throw new Error('Could not find a key with name ' + e.name + '!');
    }
    for(var j in e) {
      if (!a[j]) {
        throw new Error('Actual result did not have a key by name ' + j + '! (searching for ' + e + ')');
      }
      if (a[j] != e[j]) {
        console.log('ACTUAL: ');
        console.dir(a);
        if (a.node) {
          console.dir(a.node);
        }
        console.log('EXPECTED: ');
        console.dir(e);
        throw new Error('Actual result did not agree on value of key "' + j + '"! (actual: ' + a[j] + ', expected: ' + e[j] + ')');
      }
    }
  }
  return true;
}

function runTests() {
  console.log('Running tests:');
  var testCases = [
    ['var foo;', [{ name: 'foo', type: 'var', size: 8}]],
    ['var foo=3;', [{ name: 'foo', type: 'var', size: 10}]],
    ['var foo=4,bar=2,baz=3;', [{ name: 'foo', type: 'var', size: 9}, { name: 'bar', type: 'var', size: 6}, { name: 'baz', type: 'var', size: 7}]],
    ['var foo = "var foo";', [{ name: 'foo', type: 'var', size: 20}]],
    ['function foo(){}', [{ name: 'foo', type: 'function', size: 16}]],
    ['function longFunctionName(){var x=4;}', [{ name: 'longFunctionName', type: 'function', size: 37}]],
    ['function foo(){var longVariableName=4;}', [{ name: 'foo', type: 'function', size: 16}, {name: 'foo/longVariableName', type: 'var', size: 23}]],
    ['var foo = {};', [{ name: 'foo', type: 'var', size: 13}]],
    ['var foo = {a:1};', [{ name: 'foo', type: 'var', size: 16}]],
    ['var foo = { bar: function(){"thisIsAFunctionAssignedToAMember";} };', [{ name: 'foo.bar', type: 'function', size: 52}]],
    ['var WebAssembly = { Instance: function(module, info) { var exports = (function() { function asmFunc() { function memcpy() {}} return asmFunc() })(asmLibraryArg, wasmMemory, wasmTable); } }',
      [{ name: 'WebAssembly.Instance/exports/asmFunc', type: 'function', size: 42}],
      [{ name: 'WebAssembly.Instance/exports', type: 'var', size: 87}],
      [{ name: 'WebAssembly.Instance', type: 'function', size: 37}],
      [{ name: 'WebAssembly', type: 'var', size: 22}]],
  ];
  for(var i in testCases) {
    [test, expected] = testCases[i];
    var testFile = 'generatedTestCase.js';
    fs.writeFileSync(testFile, test);
    assert(jsonContains(test, run([testFile]), expected));
  }
}

var args = process['argv'].slice(2);

if (extractBoolCmdLineInput(args, '--runTests')) {
  runTests();
} else if (extractBoolCmdLineInput(args, '--help')) {
  console.log(`${process['argv'][1]}: Break down size report of used code. Usage:`);
  console.log(`\n  node ${process['argv'][1]} [--options] file1.[js|wasm] [file2.[js|wasm]] ... [fileN.[js|wasm]]`);
  console.log(`\nwhere supported --options:\n`);
  console.log(`  --json: Print JSON instead of human-readable output`);
  console.log(`  --symbols <a.symbols>: Use the symbol map file a.symbols to unminify the symbol names`);
  console.log(`  --dump <symbolName>: Print the contents of the given symbol`);
  console.log(`  --expandLargerThanPercents <0.35>: Expand nested JavaScript blocks larger than given percentage of whole file size`);
  console.log(`  --expandLargerThanBytes <1000>: Expand nested JavaScript blocks larger than given number of bytes`);
  console.log('');
} else {
  run(args, /*printOutput=*/true);
}
