var path = require('path');

/**
 * Copies all fields from src to dst.
 * @param {Object} dst
 * @param {Object} src
 */
function mergeKeyValues(dst, src) {
    for (var i in src) {
        dst[i] = src[i];
    }
}

/**
 * Given an array of nodes, tally up their total contribution to size, including their children.
 */
 function countNodeSizes(nodeSizes) {
    var totalSize = 0;
    for (var i in nodeSizes) {
        totalSize += nodeSizes[i].size;
    }
    return totalSize;
}


/**
 * Marks all nodes in array 'nodeSizes' to have been sourced from file 'filename'.
 */
 function recordSourceFile(nodeSizes, filename) {
    filename = path.basename(filename);
    for (var n in nodeSizes) {
        nodeSizes[n].file = filename;
    }
}


module.exports = {
    mergeKeyValues: mergeKeyValues,
    countNodeSizes: countNodeSizes,
    recordSourceFile: recordSourceFile
};