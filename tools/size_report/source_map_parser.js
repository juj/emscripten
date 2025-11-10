const fs = require('fs');
const assert = require('assert');
const { mergeKeyValues } = require('./utils');
const { getChildNodesWithCode } = require('./javascript_code_size');

/**
 * Given a text file, outputs an array containing byte locations
 * to the start of each line. I.e. line N (zero-based) in the text file will start
 * at byte location specified by index N of the returned array.
 */
// 
function calculateLineStartPositions(filename) {
    var fileSizeInBytes = fs.statSync(filename)['size'];
    var f = fs.readFileSync(filename);
    var lineStartPositions = [0];
    for (var i = 0; i < fileSizeInBytes; ++i) {
        var byte = f.readUInt8(i);
        if (byte == 0x0A) {
            lineStartPositions.push(i + 1);
        }
    }
    return lineStartPositions;
}


/**
 * Given a line:col pair, returns the 1D byte location
 * to that position in an input file, as mapped by lineStartPositions.
 */
function mapLineColTo1DPos(line, col, lineStartPositions) {
    assert(line >= 0);
    if (line >= lineStartPositions.length) {
        console.error('Input file does not have a line:col ' + (line + 1) + ':' + (col + 1) + ', but only ' + lineStartPositions.length + ' lines!');
        line = lineStartPositions.length - 1;
    }

    if (line < lineStartPositions.length - 1) {
        var numColumnsOnThisLine = lineStartPositions[line + 1] - lineStartPositions[line];
        if (col >= numColumnsOnThisLine) {
            console.error('Input file does not have a line:col ' + (line + 1) + ':' + (col + 1) + ', but given line only has ' + numColumnsOnThisLine + ' columns!');
            col = numColumnsOnThisLine - 1;
        }
    }
    return lineStartPositions[line] + col;
}

/**
 * Read and decode a source map file
 * @param {string} sourceMapFile Path to the sourcemap file
 * @param {Array<number>} mangledFileLineStartPositions Byte offsets to the start of lines
 * @returns 
 */
function readSourceMap(sourceMapFile, mangledFileLineStartPositions) {
    var sourceMapJson = JSON.parse(fs.readFileSync(sourceMapFile).toString());
    var lines = sourceMapJson['mappings'].split(';');

    var base64DecodeMap = {};
    var chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=';
    for (var i = 0; i < chars.length; ++i) {
        base64DecodeMap[chars[i]] = i
    }

    function decodeVLQ(vlq) {
        var outputList = [];
        var val = 0, shift = 0;

        for (var i = 0; i < vlq.length; ++i) {
            var bits = base64DecodeMap[vlq[i]];
            val |= (bits & 0x1F) << shift;
            if (bits & 0x20) {
                shift += 5;
            } else {
                var sign = (val & 1) ? -1 : 1;
                val >>= 1;
                outputList.push(val * sign);
                val = shift = 0;
            }
        }
        return outputList;
    }

    var sourceMap = [];

    var sourceFileIndex = 0;
    var sourceLineNumber = 0;
    var sourceColumnNumber = 0;
    var sourceNameIndex = 0;
    var outputLine = 0;
    for (var l in lines) {
        var line = lines[l];
        var segments = line.split(',');

        var outputColumn = 0;
        for (var s in segments) {
            var segment = segments[s];
            var segmentArray = decodeVLQ(segment);

            if (segmentArray.length > 0) {
                outputColumn += segmentArray[0];
                if (segmentArray.length > 1) {
                    sourceFileIndex += segmentArray[1];
                    sourceLineNumber += segmentArray[2];
                    sourceColumnNumber += segmentArray[3];
                    if (segmentArray.length > 4) {
                        sourceNameIndex += segmentArray[4];
                        var sourceMapEntry = [
                            mapLineColTo1DPos(outputLine, outputColumn, mangledFileLineStartPositions),
                            sourceMapJson['sources'][sourceFileIndex],
                            sourceLineNumber,
                            sourceColumnNumber,
                            sourceMapJson['names'][sourceNameIndex]
                        ];
                        sourceMap.push(sourceMapEntry);
                    }
                }
            }
        }
        ++outputLine;
    }

    return sourceMap;
}

// Gets the name of given node, taking into account a few different node types that
// uniquely identify a name for the node.
function getNodeNameExact(node) {
    assert(node);
    assert(!Array.isArray(node));

    if (node.type == 'ExpressionStatement' && node.expression.type == 'AssignmentExpression'
        && node.expression.left.type == 'Identifier') {
        return node.expression.left.name;
    }
    if (node.type == 'Property') {
        return node.key && node.key.name;
    }
    if (node.type == 'FunctionExpression' || node.type == 'FunctionDeclaration') {
        return node.id && node.id.name;
    }
    if (node.type == 'VariableDeclarator') {
        return node.id && node.id.name;
    }
}

// Binary search to unminify the name of given node using a source map.
function unminifyNameWithSourceMap(node, sourceMap) {
    var nodeStart = node.start;
    var s = 0;
    var e = sourceMap.length-1;
  
    while(s < e) {
      var mid = ((s+e+1)/2)|0;
      if (sourceMap[mid][0] < nodeStart) {
        s = mid;
      } else if (sourceMap[mid][0] > nodeStart) {
        e = mid-1;
      } else if (sourceMap[mid][0] == nodeStart) {
        return sourceMap[mid][4];
      } else {
        assert(false);
      }
    }
    // Source map locations from Closure do not precisely match up with node locations from acorn,
    // so allow a bit of slack.
    var slack = 1;
    if (node.type == 'FunctionDeclaration') {
      slack = 'function '.length;
    }
    if (sourceMap[s][0] <= nodeStart && sourceMap[s][0] >= nodeStart - slack) {
      return sourceMap[mid][4];
    }
  }


function walkNodesForSymbolMap(nodeArray, minifiedParentPrefix, unminifiedParentPrefix, sourceMap) {
    var symbolMap = {};
    assert(Array.isArray(nodeArray));
    for (var i in nodeArray) {
        var node = nodeArray[i];
        var childNodes = getChildNodesWithCode(node);

        var minifiedName = getNodeNameExact(node);

        // Try to demangle the name with source map.
        var unminifiedName = unminifyNameWithSourceMap(node, sourceMap);

        if (['FunctionDeclaration', 'VariableDeclaration', 'VariableDeclarator'].indexOf(node.type) != -1) {
            if (minifiedName && unminifiedName && unminifiedName != minifiedName) {
                symbolMap[minifiedParentPrefix + minifiedName] = unminifiedParentPrefix + unminifiedName;
            }
        }

        if (childNodes) {
            var delimiter = (node.type == 'ObjectExpression' || (childNodes && childNodes.length == 1 && childNodes[0].type == 'ObjectExpression')) ? '.' : '/';
            var minifiedChildPrefix = minifiedName ? minifiedParentPrefix + minifiedName + delimiter : minifiedParentPrefix;
            var unminifiedChildPrefix = minifiedName && unminifiedName ? unminifiedParentPrefix + unminifiedName + delimiter : unminifiedParentPrefix;
            mergeKeyValues(symbolMap, walkNodesForSymbolMap(childNodes, minifiedChildPrefix, unminifiedChildPrefix, sourceMap));
        }
    }
    return symbolMap;
}

/**
 * Create a symbol map from a source map file
 * @param {Object} sourceMap an existing source map to extend
 * @param {*} sourceFile Path to the source map file
 * @returns {Object} A symbol map
 */
function createSymbolMapFromSourceMap(sourceMap, sourceFile) {
    generatingSymbolMapFromSourceMap = true;
    var lineStartPositions = calculateLineStartPositions(sourceFile);
    sourceFile = fs.readFileSync(sourceFile).toString();
    currentInputJsFile = sourceFile;
    var sourceMap = readSourceMap(sourceMap, lineStartPositions);
    var ast = acorn.parse(sourceFile, { ecmaVersion: 6 });
    var symbolMap = walkNodesForSymbolMap(ast.body, '', '', sourceMap);

    return symbolMap;
}

// Apply Wasm symbol name demangling (essentially UTF-8 decoding)
function demangleWasmSymbol(symbol) {
    var out = '';
    for (var i = 0; i < symbol.length; ++i) {
        if (symbol[i] != '\\') {
            out += symbol[i];
        } else {
            if (symbol[i + 1] == '\\') {
                out += '\\';
                ++i;
            } else {
                out += String.fromCharCode(parseInt(symbol.substring(i + 1, i + 3), 16));
                i += 2;
            }
        }
    }
    return out;
}

function readSymbolMap(filename) {
    var symbolMap = {};
  
    function splitInTwo(s, delim) {
      var idx = s.indexOf(delim);
      return [s.substr(0, idx), s.substr(idx+1)];
    }
    var symbolFile = fs.readFileSync(filename).toString();
    var symbols = symbolFile.split('\n');
    for(var i in symbols) {
      var [minified, unminified] = splitInTwo(symbols[i], ':');
      symbolMap[minified] = demangleWasmSymbol(unminified);
    }
    return symbolMap;
  }


module.exports = {
    createSymbolMapFromSourceMap: createSymbolMapFromSourceMap,
    readSymbolMap: readSymbolMap
};
