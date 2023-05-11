const fs = require('fs');
const assert = require('assert');
const { mergeKeyValues, countNodeSizes, recordSourceFile } = require('./utils');

function sectionIdToString(id) {
    return ['CUSTOM', 'TYPE', 'IMPORT', 'FUNCTION', 'TABLE', 'MEMORY', 'GLOBAL', 'EXPORT', 'START', 'ELEMENT', 'CODE', 'DATA'][id];
}

function readLEB128(file, cursor) {
    var n = 0;
    var shift = 0;
    while (1) {
        var byte = file.readUInt8(cursor++);
        n |= (byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80))
            return [n, cursor];
    }
}

function extractWasmCodeSection(wasm, cursor) {
    var numFunctions;
    [numFunctions, cursor] = readLEB128(wasm, cursor);
    var sizes = {};
    for (var i = 0; i < numFunctions; ++i) {
        var cursorStart = cursor;
        var functionSize;
        [functionSize, cursor] = readLEB128(wasm, cursor);
        cursor += functionSize;
        sizes[i] = {
            'type': 'function',
            'prefix': '',
            'name': i,
            'size': cursor - cursorStart
        };
    }
    return sizes;
}

function extractString(wasm, cursor) {
    var name = '';
    var nameLength;
    [nameLength, cursor] = readLEB128(wasm, cursor);
    for (var i = 0; i < nameLength; ++i) {
        var ch = wasm.readUInt8(cursor++);
        name += String.fromCharCode(ch);
    }
    return [name, cursor];
}

function extractWasmCustomSectionName(wasm, cursor) {
    return extractString(wasm, cursor)[0];
}

function extractWasmFunctionNames(wasm, cursor, sectionEndCursor) {
    var customSectionName;
    [customSectionName, cursor] = extractString(wasm, cursor);
    if (customSectionName != 'name') return [];

    var functionNames = {};

    while (cursor < sectionEndCursor) {
        var subsectionId = wasm.readUInt8(cursor++);
        var subsectionSize;
        [subsectionSize, cursor] = readLEB128(wasm, cursor);
        // Skip over other subsections than the function names subsection
        if (subsectionId != 1 /*function names subsection*/) {
            cursor += subsectionSize;
            continue;
        }
        var numNames;
        [numNames, cursor] = readLEB128(wasm, cursor);
        for (var i = 0; i < numNames; ++i) {
            var index, name;
            [index, cursor] = readLEB128(wasm, cursor);
            [name, cursor] = extractString(wasm, cursor);
            functionNames[index] = name;
        }
    }
    assert(cursor == sectionEndCursor);
    return Object.keys(functionNames).length > 0 ? functionNames : null;
}

function renumberFunctionNames(sizes, numImports) {
    var renumberedSymbols = {};
    for (var name in sizes) {
        var symbol = sizes[name];
        if (symbol.type == 'function' || symbol.type == 'export') {
            var renumberedName = (name | 0) + numImports;
            renumberedSymbols[renumberedName] = sizes[name];
            renumberedSymbols[renumberedName].name = renumberedName;
            renumberedSymbols[renumberedName].ordinal = renumberedName;
        } else {
            renumberedSymbols[name] = symbol;
        }
    }
    return renumberedSymbols;
}

function labelWasmFunctionsWithNames(sizes, wasmFunctionNames) {
    var labeledSymbols = {};
    for (var name in sizes) {
        var symbol = sizes[name];
        if (symbol.type == 'function' || symbol.type == 'export') {
            var functionNameIndex = (name | 0);
            var wasmName = wasmFunctionNames[functionNameIndex] || name;
            labeledSymbols[wasmName] = sizes[name];
            labeledSymbols[wasmName].name = wasmName;
        } else {
            labeledSymbols[name] = symbol;
        }
    }
    return labeledSymbols;
}

function extractWasmFunctionImportNames(wasm, cursor) {
    var numImports;
    [numImports, cursor] = readLEB128(wasm, cursor);
    var importNames = [];
    for (var i = 0; i < numImports; ++i) {
        var mod, name;
        [mod, cursor] = extractString(wasm, cursor);
        [name, cursor] = extractString(wasm, cursor);
        var importType = wasm.readUInt8(cursor++);
        switch (importType) {
            case 0x00: /*function import*/
                importNames.push(name);
                cursor = readLEB128(wasm, cursor)[1]; // skip typeidx
                break;
            case 0x01: /*table import*/
                assert(wasm.readUInt8(cursor++) == 0x70);
            // pass-through: table import ends with a limits value, which is what memory also contains
            case 0x02: /*memory import*/
                var limitType = wasm.readUInt8(cursor++);
                assert(limitType == 0x00 /*unshared, no max*/ ||
                    limitType == 0x01 /*unshared, min+max*/ ||
                    limitType == 0x03 /*shared, min+max*/);
                cursor = readLEB128(wasm, cursor)[1]; // limit min
                if (limitType != 0x00) cursor = readLEB128(wasm, cursor)[1]; // limit max
                break;
            case 0x03: /*global import*/
                cursor += 2; // type of the global, and mutability (const/var).. skip over them
                break;
        }
    }
    return importNames;
}

function extractWasmFunctionExportIndices(wasm, cursor) {
    var numExports;
    [numExports, cursor] = readLEB128(wasm, cursor);
    var exportIndices = [], exportIndex;
    for (var i = 0; i < numExports; ++i) {
        var name;
        [name, cursor] = extractString(wasm, cursor);
        var exportType = wasm.readUInt8(cursor++);
        [exportIndex, cursor] = readLEB128(wasm, cursor);
        if (exportType == 0x00 /*function export*/) exportIndices.push(exportIndex);
    }
    return exportIndices;
}

function extractWasmCodeSize(sourceFile) {
    var fileSizeInBytes = fs.statSync(sourceFile)['size'];
    var wasm = fs.readFileSync(sourceFile);
    var magic = wasm.readUInt32LE(0);
    assert(magic == 0x6D736100);
    var version = wasm.readUInt32LE(4);
    assert(version == 1);
    var cursor = 8;

    var sizes = {};

    var importNames = [];
    var exportIndices = [];
    var wasmFunctionNames;
    var unaccountedBytes = fileSizeInBytes;
    while (cursor < fileSizeInBytes) {
        var sectionStartCursor = cursor;
        var id = wasm.readInt8(cursor++);
        if (id === undefined) throw 'Failed to parse section ID in wasm file!';
        var name = sectionIdToString(id);
        [size, cursor] = readLEB128(wasm, cursor);
        var sectionEndCursor = cursor + size;
        var sectionSize = sectionEndCursor - sectionStartCursor;

        var functionSizes = null;
        if (id == 0 /*custom*/) {
            const customSectionName = extractWasmCustomSectionName(wasm, cursor);
            name = `CUSTOM/"${customSectionName}"`;

            switch (customSectionName) {
                case "name":
                    wasmFunctionNames = extractWasmFunctionNames(wasm, cursor, sectionEndCursor);
                    break;
            }
            //extractDwarfInformation(wasm, cursor, sectionEndCursor);
        } else if (id == 2 /*import*/) {
            importNames = extractWasmFunctionImportNames(wasm, cursor);
        } else if (id == 7 /*export*/) {
            exportIndices = extractWasmFunctionExportIndices(wasm, cursor);
        } else if (id == 10 /*code*/) {
            functionSizes = extractWasmCodeSection(wasm, cursor);
        }
        cursor = sectionEndCursor;
        if (functionSizes) {
            var funcSizes = countNodeSizes(functionSizes);
            sectionSize -= funcSizes;
            unaccountedBytes -= funcSizes;
            mergeKeyValues(sizes, functionSizes);
        }
        sizes[name] = {
            'type': 'section',
            'prefix': '',
            'name': name,
            'size': sectionSize
        };
        unaccountedBytes -= sectionSize;
    }
    if (cursor > fileSizeInBytes) {
        throw 'Failed to parse sections in Wasm file!';
    }

    if (importNames.length > 0) {
        // Reorder wasm function names list to account for imports
        sizes = renumberFunctionNames(sizes, importNames.length);

        // Record each function import as a zero-byte function size
        for(let i = 0; i < importNames.length; ++i) {
            sizes[name] = {
                'type': 'import',
                'prefix': '',
                'name': importNames[i],
                'ordinal': i,
                'size': 0 // Imports are functions from JavaScript side, so count them as zero-byte in wasm-side.
            };
        }
    }

    // Mark functions that are exports
    for(let i of exportIndices) {
        sizes[i].type = 'export';
    }

    if (wasmFunctionNames) {
        sizes = labelWasmFunctionsWithNames(sizes, wasmFunctionNames);
    }

    if (unaccountedBytes > 0) {
        var name = 'file header';
        sizes[name] = {
            'type': 'wasmHeader',
            'prefix': '',
            'name': name,
            'size': unaccountedBytes
        };
    }
    recordSourceFile(sizes, sourceFile);
    return sizes;
}

module.exports = {
    extractWasmCodeSize: extractWasmCodeSize
};
