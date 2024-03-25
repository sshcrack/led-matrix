'use strict';

var call = Function.prototype.call;
var $hasOwn = Object.prototype.hasOwnProperty;
var bind = require('function-bind');

/** @type {import('hasown')} */
module.exports = bind.call(call, $hasOwn);
