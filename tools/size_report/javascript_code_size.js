const acorn = require('acorn');
const fs = require('fs');
const assert = require('assert');
const { mergeKeyValues, countNodeSizes, recordSourceFile } = require('./utils');

// Global state reused by various functions during a run:
var currentInputJsFile;
var currentInputJsFileSizeInBytes;
var runningCodeBlockCounter = 0;
var generatingSymbolMapFromSourceMap = false;

function printNodeContents(node) {
    console.log(currentInputJsFile.substring(node.start, node.end));
}

function getNodeType(node) {
    if (node.type == 'Property') node = node.value;

    if (['FunctionDeclaration', 'FunctionExpression'].indexOf(node.type) != -1) {
        return 'function';
    } else if (['VariableDeclaration', 'VariableDeclarator'].indexOf(node.type) != -1) {
        return 'var';
    } else {
        return 'code';
    }
}

// Given a node and a list of its childNodes, attempts to assign
// a unique name & type for that node by performing a lookahed into the descendants
// of the node. If there multiple ways to name the node, returns false.
// If the node cannot be named, returns null. Otherwise returns
// the name as a string.
function findUniqueNameAndTypeFromChildren(node, childNodes) {
    var oneChildName = null;
    var oneChildType = null;
    for (var i in childNodes) {
        var child = childNodes[i];
        var childNameNode = peekAheadGetNodeName(child);
        var childName = childNameNode && childNameNode.name;
        var childType = null;
        if (childName) {
            childType = getNodeType(child);
        } else if (child.type == 'FunctionExpression' || child.type == 'CallExpression') {
            return [null, null];
        } else {
            [childName, childType] = findUniqueNameAndTypeFromChildren(child, getChildNodesWithCode(child));
        }
        if (childName === false) {
            return [false, null];
        }
        if (childName) {
            if (oneChildName) {
                return [false, null];
            }
            oneChildName = childName;
            oneChildType = childType;
        }
    }
    return [oneChildName, oneChildType];
}

function hasAnyDescendantThatCanBeNamed(nodeArray, exceptNode) {
    for (var i in nodeArray) {
        var node = nodeArray[i];
        var nodeName = peekAheadGetNodeName(node);
        if (nodeName && nodeName != exceptNode) {
            return true;
        }
        var childNodes = getChildNodesWithCode(node);

        var [uniqueName, uniqueType] = findUniqueNameAndTypeFromChildren(node, childNodes);
        if (uniqueName || uniqueName === false) {
            return true;
        }

        if (hasAnyDescendantThatCanBeNamed(childNodes, exceptNode)) {
            return true;
        }
    }
}

// Heuristic choice to decide when a node should be expanded/split to its children components
// for size reporting.
function isLargeEnoughNodeToExpand(node) {
    var size = getSizeIncludingChildren(node) - getSizeExcludingChildren(node);
    return size > (module.exports.expandSymbolsLargerThanPercents * currentInputJsFileSizeInBytes) | 0 || size > module.exports.expandSymbolsLargerThanBytes;
}

// Given a node, returns an array of its child nodes.
// Or if passed an array of nodes, returns that directly.
function findListOfChildNodes(nodeOrArrayOfNodes) {
    while (!Array.isArray(nodeOrArrayOfNodes)) {
        if (nodeOrArrayOfNodes.type == 'BlockStatement') nodeOrArrayOfNodes = nodeOrArrayOfNodes.body;
        if (nodeOrArrayOfNodes.type == 'FunctionExpression') {
            parentPrefix += nodeOrArrayOfNodes.id.name + '/';
            nodeOrArrayOfNodes = nodeOrArrayOfNodes.body;
        }
        else break;
    }
    if (!Array.isArray(nodeOrArrayOfNodes)) {
        assert(false);
    }
    return nodeOrArrayOfNodes;
}

// Finds the list of child AST nodes of the given node, that can contain other code (and are relevant for size computation) 
function getChildNodesWithCode(node) {
    assert(node);
    assert(!Array.isArray(node));

    var children = [];
    function addChild(child) {
        assert(child);
        assert(!Array.isArray(child));
        children.push(child);
    }
    function maybeChild(child) {
        if (child) {
            assert(!Array.isArray(child));
            children.push(child);
        }
    }
    function addChildArray(childArray) {
        assert(Array.isArray(childArray));
        children = children.concat(childArray);
    }

    if (['BlockStatement', 'Program'].indexOf(node.type) != -1) {
        addChildArray(node.body);
    }
    else if (['IfStatement'].indexOf(node.type) != -1) {
        addChild(node.test);
        addChild(node.consequent);
        maybeChild(node.alternate);
    }
    else if (['BinaryStatement', 'BinaryExpression', 'LogicalExpression', 'AssignmentExpression'].indexOf(node.type) != -1) {
        addChild(node.left);
        addChild(node.right);
    }
    else if (['MemberExpression'].indexOf(node.type) != -1) {
        addChild(node.object);
        addChild(node.property);
    }
    else if (['Property'].indexOf(node.type) != -1) {
        addChild(node.key);
        addChild(node.value);
    }
    else if (['TryStatement'].indexOf(node.type) != -1) {
        addChild(node.block);
        addChild(node.handler);
    }
    else if (['CatchClause'].indexOf(node.type) != -1) {
        // Ignored node: addChild(node.param);
        addChild(node.body);
    }
    else if (['FunctionDeclaration'].indexOf(node.type) != -1) {
        // Ignored node: addChild(node.id);
        addChild(node.body);
        // Ignored node: addChildArray(node.params);
    }
    else if (['FunctionExpression'].indexOf(node.type) != -1) {
        // Ignored node: maybeChild(node.id);
        addChild(node.body);
        // Ignored node: addChildArray(node.params);
    }
    else if (['ThrowStatement', 'ReturnStatement'].indexOf(node.type) != -1) {
        maybeChild(node.argument);
    }
    else if (['UnaryExpression', 'UpdateExpression'].indexOf(node.type) != -1) {
        addChild(node.argument);
    }
    else if (['CallExpression', 'NewExpression'].indexOf(node.type) != -1) {
        addChild(node.callee);
        addChildArray(node.arguments);
    }
    else if (['VariableDeclaration'].indexOf(node.type) != -1) {
        // When we are creating a symbol map from a source map, process the whole
        // tree pedantically. When actually parsing code size, shortcut the AST
        // to account size in a nicer way
        if (node.declarations.length == 1 && !generatingSymbolMapFromSourceMap) {
            maybeChild(node.declarations[0].init);
        } else {
            addChildArray(node.declarations);
        }
    }
    else if (['ArrayExpression'].indexOf(node.type) != -1) {
        addChildArray(node.elements);
    }
    else if (['VariableDeclarator'].indexOf(node.type) != -1) {
        // Ignored node: addChild(node.id);
        maybeChild(node.init);
    }
    else if (['ObjectExpression'].indexOf(node.type) != -1) {
        addChildArray(node.properties);
    }
    else if (['ExpressionStatement'].indexOf(node.type) != -1) {
        addChild(node.expression);
    }
    else if (['BreakStatement'].indexOf(node.type) != -1) {
        // Ignored node: maybeChild(node.label);
    }
    else if (['LabeledStatement'].indexOf(node.type) != -1) {
        addChild(node.body);
        // Ignored node: addChild(node.label);
    }
    else if (['SwitchStatement'].indexOf(node.type) != -1) {
        addChild(node.discriminant);
        addChildArray(node.cases);
    }
    else if (['SwitchCase'].indexOf(node.type) != -1) {
        addChildArray(node.consequent);
        maybeChild(node.test);
    }
    else if (['SequenceExpression'].indexOf(node.type) != -1) {
        addChildArray(node.expressions);
    }
    else if (['ConditionalExpression'].indexOf(node.type) != -1) {
        addChild(node.test);
        addChild(node.consequent);
        addChild(node.alternate);
    }
    else if (['ForStatement'].indexOf(node.type) != -1) {
        maybeChild(node.init);
        maybeChild(node.test);
        maybeChild(node.update);
        addChild(node.body);
    }
    else if (['WhileStatement', 'DoWhileStatement'].indexOf(node.type) != -1) {
        addChild(node.test);
        addChild(node.body);
    }
    else if (['ForInStatement'].indexOf(node.type) != -1) {
        addChild(node.left);
        addChild(node.right);
        addChild(node.body);
    }
    else if (['Identifier', 'Literal', 'ThisExpression', 'EmptyStatement', 'DebuggerStatement', 'ContinueStatement'].indexOf(node.type) != -1) {
        // no children
    } else {
        console.error('----UNKNOWN NODE TYPE!----');
        console.error(node);
        console.error('----UNKNOWN NODE TYPE!----');
        assert(false);
    }
    return children;
}

function getSizeIncludingChildren(node, parentNode) {
    // Fix up acorn size computation to exactly take into account the size of variable initializers,
    // i.e. in "var a, b, c;", account size of "var " to "a", and size of ";" to "c".
    if (node.type == 'VariableDeclarator' && parentNode && parentNode.type == 'VariableDeclaration') {
        if (parentNode.declarations.length > 1) {
            var i = parentNode.declarations.indexOf(node);
            // Account "var " to "a".
            if (i == 0) {
                return node.end - parentNode.start;
            }
            // Account ";" to last declaration.
            if (i == parentNode.declarations.length - 1) {
                return node.end + 1 - (node.start - 1);
            }
            // Account "," to In-between declarations.
            return node.end - (node.start - 1);
        }
    }
    // Other nodes use the size returned by acorn.
    return node.end - node.start;
}

function getSizeExcludingChildren(node) {
    var totalSize = getSizeIncludingChildren(node);
    var childNodes = getChildNodesWithCode(node);
    for (var i in childNodes) {
        totalSize -= getSizeIncludingChildren(childNodes[i]);
    }
    return totalSize;
}

// Look into child nodes for name of this node.
function peekAheadGetNodeName(node) {
    if (node.type == 'ObjectExpression') return;

    if (node.type == 'ExpressionStatement' && node.expression.type == 'AssignmentExpression'
        && node.expression.left.type == 'Identifier') {
        return node.expression.left;
    }
    if (node.type == 'Property') {
        return node.key.name ? node.key : node.value;
    }
    if (node.type == 'FunctionExpression' || node.type == 'FunctionDeclaration') {
        return node.id;
    }
    if (node.type == 'VariableDeclarator') {
        return node.id;
    }
    if (node.type == 'VariableDeclaration') {
        if (node.declarations.length == 1) {
            return node.declarations[0].id;
        }
    }
}

// Unminifies the given name using a symbol map
function unminifyNameWithSymbolMap(name, symbolMap) {
    var unminifiedName = symbolMap[name];
    return unminifiedName || name;
}

function collectNodeSizes2(nodeArray, parentNode, parentPrefix, symbolMap) {
    var nodeSizes = {};
    for (var i in nodeArray) {
        var node = nodeArray[i];
        var nodeType = getNodeType(node);
        var childNodes = getChildNodesWithCode(node);

        var nodeSize = getSizeIncludingChildren(node, parentNode);
        var childNameNode = peekAheadGetNodeName(node);
        var nodeName = childNameNode && childNameNode.name;
        var shouldBreakDown = isLargeEnoughNodeToExpand(node) && hasAnyDescendantThatCanBeNamed(childNodes, childNameNode);

        if (!nodeName) {
            var [uniqueName, uniqueType] = findUniqueNameAndTypeFromChildren(node, childNodes);
            if (uniqueName !== false) {
                shouldBreakDown = false;
            }
            if (uniqueName) {
                nodeName = uniqueName;
                nodeType = uniqueType;
            }
        }
        if (['ObjectExpression', 'FunctionExpression', 'CallExpression', 'BlockStatement', 'MemberExpression', 'ExpressionStatement'].indexOf(node.type) != -1) {
            shouldBreakDown = true;
            nodeName = null;
        }
        var minifiedName = nodeName;
        nodeName = unminifyNameWithSymbolMap(nodeName, symbolMap);

        var childSizes = null;

        if (shouldBreakDown) {
            var delimiter = (node.type == 'ObjectExpression' || (childNodes && childNodes.length == 1 && childNodes[0].type == 'ObjectExpression')) ? '.' : '/';
            var childPrefix = nodeName ? parentPrefix + nodeName + delimiter : parentPrefix;
            childSizes = collectNodeSizes2(childNodes, node, childPrefix, symbolMap);
            mergeKeyValues(nodeSizes, childSizes);
            nodeSize -= countNodeSizes(childSizes);
        }

        var nodeDesc = null;
        if (!nodeName && nodeType == 'code') {
            var code = currentInputJsFile.substring(node.start, node.end).trim();
            nodeName = 'code#' + runningCodeBlockCounter++;
            if (code.length > 32) {
                nodeDesc = '"' + code.substring(0, 29).replace(/\n/g, ' ') + '..."';
            } else {
                nodeDesc = '"' + code.replace(/\n/g, ' ') + '"';
            }
        }

        if (node.type != 'ObjectExpression' && node.type != 'FunctionExpression') {
            if (nodeName && (nodeType != 'code' || !parentPrefix)) {
                var fullName = parentPrefix + nodeName;
                nodeSizes[fullName] = {
                    'type': nodeType,
                    'name': fullName,
                    'desc': nodeDesc,
                    'prefix': parentPrefix,
                    'selfName': nodeName,
                    'minifiedName': minifiedName,
                    'size': nodeSize,
                    'node': node
                };

                if (module.exports.dumpJsTextContents.indexOf(fullName) != -1) {
                    console.log('Contents of symbol ' + fullName + ':');
                    printNodeContents(node);
                    console.dir(node);
                }
            }
        }
    }
    return nodeSizes;
}

/**
 * Extract size of function from a javascript file
 * @param {string} sourceFile Path to the javascript source file
 * @param {Object} symbolMap A symbol map for minified javascript files
 * @returns 
 */
function extractJavaScriptCodeSize(sourceFile, symbolMap) {
    currentInputJsFileSizeInBytes = fs.statSync(sourceFile).size;
    currentInputJsFile = fs.readFileSync(sourceFile).toString();
    var ast = acorn.parse(currentInputJsFile, { ecmaVersion: 6 });

    var nodeSizes = collectNodeSizes2(ast.body, null, '', symbolMap);
    var totalAccountedFor = 0;
    for (var i in nodeSizes) {
        var node = nodeSizes[i];
        if (node.type) {
            totalAccountedFor += node.size;
        }
    }

    var whitespaceBytes = currentInputJsFileSizeInBytes - countNodeSizes(nodeSizes);

    if (whitespaceBytes > 0) {
        var name = 'unclassified';
        nodeSizes[name] = {
            'type': 'other',
            'prefix': '',
            'name': name,
            'size': whitespaceBytes
        };
    }

    recordSourceFile(nodeSizes, sourceFile);
    return nodeSizes;
}

module.exports = {
    // Configurable command line options:
    expandSymbolsLargerThanPercents: 0.35,
    expandSymbolsLargerThanBytes: 32*1024,
    // Global state reused by various functions during a run:
    dumpJsTextContents: [],
    getChildNodesWithCode: getChildNodesWithCode,
    extractJavaScriptCodeSize: extractJavaScriptCodeSize
};