/*jshint globalstrict:false, strict:false, strict: false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, tenary operator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlTernaryTestSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator
////////////////////////////////////////////////////////////////////////////////
    
    testTernarySimple : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("RETURN 1 > 0 ? 2 : -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested1 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN 15 > 15 ? 1 : -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested2 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN 10 + 5 > 15 ? 1 : -1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested3 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("RETURN true ? true ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested4 : function () {
      var expected = [ 3 ];

      var actual = getQueryResults("RETURN false ? true ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested5 : function () {
      var expected = [ -2 ];

      var actual = getQueryResults("RETURN true ? false ? true ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNested6 : function () {
      var expected = [ -1 ];

      var actual = getQueryResults("RETURN true ? true ? false ? 1 : -1 : -2 : 3");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN true ? a : b");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet2 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN false ? a : b");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary operator precedence
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryLet3 : function () {
      var expected = [ 2 ];

      var actual = getQueryResults("LET a = 1, b = 2, c = 3 RETURN true ? false ? a : b : c");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ternary with non-boolean condition
////////////////////////////////////////////////////////////////////////////////
    
    testTernaryNonBoolean : function () {
      assertEqual([ 2 ], getQueryResults("RETURN 1 ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN 0 ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN null ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN false ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN true ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN (4) ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN (4 - 3) ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN \"true\" ? 2 : 3"));
      assertEqual([ 3 ], getQueryResults("RETURN \"\" ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN [ ] ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN [ 0 ] ? 2 : 3"));
      assertEqual([ 2 ], getQueryResults("RETURN { } ? 2 : 3"));
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlTernaryTestSuite);

return jsunity.done();

