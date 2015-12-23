/// @brief Implementation of Traversal Execution Node
///
/// @file arangod/Aql/TraversalNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"
#include "Aql/Index.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

static uint64_t checkTraversalDepthValue (AstNode const* node) {
  if (! node->isNumericValue()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid traversal depth");
  }
  double v = node->getDoubleValue();
  double intpart;
  if (modf(v, &intpart) != 0.0 ||
      v < 1.0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid traversal depth");
  }
  return static_cast<uint64_t>(v);
}


// -----------------------------------------------------------------------------
// --SECTION--                                  struct SimpleTraverserExpression
// -----------------------------------------------------------------------------

SimpleTraverserExpression::SimpleTraverserExpression (triagens::aql::Ast* ast, triagens::basics::Json j)
  : TraverserExpression(),
    expression(nullptr)
{  
  isEdgeAccess = basics::JsonHelper::checkAndGetBooleanValue(j.json(), "isEdgeAccess");

  comparisonType = static_cast<aql::AstNodeType>(basics::JsonHelper::checkAndGetNumericValue<uint32_t>(j.json(), "comparisonType"));

  varAccess = new AstNode(ast, j.get("varAccess"));
  compareToNode = new AstNode(ast, j.get("compareTo"));
}

SimpleTraverserExpression::~SimpleTraverserExpression () {
  if (expression != nullptr) {
    delete expression;
  }
}

void SimpleTraverserExpression::toJson (triagens::basics::Json& json,
                                        TRI_memory_zone_t* zone) const {
  auto op = triagens::aql::AstNode::Operators.find(comparisonType);
          
  if (op == triagens::aql::AstNode::Operators.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid operator for simpleTraverserExpression");
  }
  std::string const operatorStr = op->second;
  
  json("isEdgeAccess", triagens::basics::Json(isEdgeAccess))
    ("comparisonTypeStr", triagens::basics::Json(operatorStr))
    ("comparisonType", triagens::basics::Json(static_cast<int32_t>(comparisonType)))
    ("varAccess", varAccess->toJson(zone, true))
    ("compareTo", compareToNode->toJson(zone, true));
          
}

TraversalNode::TraversalNode (ExecutionPlan* plan,
               size_t id,
               TRI_vocbase_t* vocbase, 
               AstNode const* direction,
               AstNode const* start,
               AstNode const* graph)
  : ExecutionNode(plan, id), 
    _vocbase(vocbase),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _graphObj(nullptr),
    _condition(nullptr) {

  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(start != nullptr);
  TRI_ASSERT(graph != nullptr);
  auto resolver = std::make_unique<arango::CollectionNameResolver>(vocbase);
  
  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();
    _graphJson = triagens::basics::Json(triagens::basics::Json::Array, edgeCollectionCount);
    // List of edge collection names
    for (size_t i = 0; i <  edgeCollectionCount; ++i) {
      auto eColName = graph->getMember(i)->getStringValue();
      auto eColType = resolver->getCollectionTypeCluster(eColName);
      if (eColType != TRI_COL_TYPE_EDGE) {
        std::string msg("collection type invalid; Expecting Collection type 'Edge Collection'. Collection name: ");
        msg += eColName;
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID, msg);
      }
      _graphJson.add(triagens::basics::Json(eColName));
      _edgeColls.push_back(eColName);
    }
  } else {
    if (_edgeColls.empty()) {
      if (graph->isStringValue()) {
        std::string graphName = graph->getStringValue();
        _graphJson = triagens::basics::Json(graphName);
        _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);


        auto eColls = _graphObj->edgeCollections();
        for (const auto& n: eColls) {
          _edgeColls.push_back(n);
        }
      }
    }
  }

  // Parse start node
  switch (start->type) {
    case NODE_TYPE_REFERENCE:
      _inVariable = static_cast<Variable*>(start->getData());
      _vertexId = "";
      break;
    case NODE_TYPE_VALUE:
      if (start->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid start vertex. Must either be an _id string or an object with _id.");
      }
      _inVariable = nullptr;
      _vertexId = std::string(start->getStringValue(), start->getStringLength());;
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid start vertex. Must either be an _id string or an object with _id.");
  }

  // Parse Steps and direction
  TRI_ASSERT(direction->type == NODE_TYPE_DIRECTION);
  TRI_ASSERT(direction->numMembers() == 2);
  // Member 0 is the direction. Already the correct Integer.
  // Is not inserted by user but by enum.
  auto dir = direction->getMember(0);
  auto steps = direction->getMember(1);
  TRI_ASSERT(dir->isIntValue());
  auto dirNum = dir->getIntValue();

  switch (dirNum) {
    case 0:
      _direction = TRI_EDGE_ANY;
      break;
    case 1:
      _direction = TRI_EDGE_IN;
      break;
    case 2:
      _direction = TRI_EDGE_OUT;
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "direction can only be INBOUND, OUTBOUND or ANY");
      break;
  }

  if (steps->isNumericValue()) {
    // Check if a double value is integer
    _minDepth = checkTraversalDepthValue(steps);
    _maxDepth = _minDepth;
  } else if (steps->type == NODE_TYPE_RANGE) {
    // Range depth
    _minDepth = checkTraversalDepthValue(steps->getMember(0));
    _maxDepth = checkTraversalDepthValue(steps->getMember(1));

    if (_maxDepth < _minDepth) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid traversal depth");
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE, "invalid traversal depth");
  }

}


TraversalNode::TraversalNode (ExecutionPlan* plan,
                       size_t id,
                       TRI_vocbase_t* vocbase, 
                       std::vector<std::string> const& edgeColls,
                       Variable const* inVariable,
                       std::string const& vertexId,
                       TRI_edge_direction_e direction,
                       uint64_t minDepth,
                       uint64_t maxDepth)
  : ExecutionNode(plan, id),
    _vocbase(vocbase),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _inVariable(inVariable),
    _vertexId(vertexId),
    _minDepth(minDepth),
    _maxDepth(maxDepth),
    _direction(direction),
     _condition(nullptr) {
  for (auto& it : edgeColls) {
    _edgeColls.push_back(it);
  }
}

TraversalNode::TraversalNode (ExecutionPlan* plan,
                              triagens::basics::Json const& base)
  : ExecutionNode(plan, base),
    _vocbase(plan->getAst()->query()->vocbase()),
    _vertexOutVariable(nullptr),
    _edgeOutVariable(nullptr),
    _pathOutVariable(nullptr),
    _inVariable(nullptr),
    _condition(nullptr) {

  _minDepth = triagens::basics::JsonHelper::stringUInt64(base.json(), "minDepth");
  _maxDepth = triagens::basics::JsonHelper::stringUInt64(base.json(), "maxDepth");
  uint64_t dir = triagens::basics::JsonHelper::stringUInt64(base.json(), "direction");
  switch (dir) {
    case 0:
      _direction = TRI_EDGE_ANY;
      break;
    case 1:
      _direction = TRI_EDGE_IN;
      break;
    case 2:
      _direction = TRI_EDGE_OUT;
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "Invalid direction value");
      break;
  }

  // In Vertex
  if (base.has("inVariable")) {
    _inVariable = varFromJson(plan->getAst(), base, "inVariable");
  }
  else {
    _vertexId = triagens::basics::JsonHelper::getStringValue(base.json(), "vertexId", "");  
    if (_vertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "start vertex mustn't be empty.");
    }
  }

  if (base.has("condition")) {
    TRI_json_t const* condition = JsonHelper::checkAndGetObjectValue(base.json(), "condition");

    if (condition != nullptr) {
      triagens::basics::Json conditionJson(TRI_UNKNOWN_MEM_ZONE, condition, triagens::basics::Json::NOFREE);
      _condition = Condition::fromJson(plan, conditionJson);
    }
  }

  std::string graphName;
  if (base.has("graph") && (base.get("graph").isString())) {
    graphName = JsonHelper::checkAndGetStringValue(base.json(), "graph");
    if (base.has("graphDefinition")) {
      _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

      auto eColls = _graphObj->edgeCollections();
      for (const auto& n: eColls) {
        _edgeColls.push_back(n);
      }
    }
    else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "missing graphDefinition.");
    }
  }
  else {
    _graphJson = base.get("graph").copy(); 
    if (!_graphJson.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "graph has to be an array.");
    }
    size_t edgeCollectionCount = _graphJson.size();
    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto at = _graphJson.at(i);
      if (!at.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "graph has to be an array of strings.");
      }
      _edgeColls.push_back(at.json()->_value._string.data);
    }
    if (_edgeColls.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "graph has to be a non empty array of strings.");
    }
  }

  if (base.has("simpleExpressions")) {
    auto simpleExpSet = base.get("simpleExpressions");
    
    if (!simpleExpSet.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "simpleExpressions has to be an array.");
    }
    size_t nExpressionSets = simpleExpSet.members();
    // List of edge collection names
    for (size_t i = 0; i < nExpressionSets; i += 2) {
      auto key = static_cast<TRI_json_t const*>(TRI_AddressVector(&simpleExpSet.json()->_value._objects, i));

      if (! TRI_IsStringJson(key)) {
        // no string, should not happen
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "simpleExpressions object: key wrong.");
      }

      std::string const k(key->_value._string.data, key->_value._string.length - 1);

      auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&simpleExpSet.json()->_value._objects, i + 1));

      if (value == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "simpleExpressions object: value wrong.");
      }

      Json oneExpressionSetJson(TRI_UNKNOWN_MEM_ZONE, value);
      size_t oneSetLength = oneExpressionSetJson.size();
      if (oneSetLength == 0) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "simpleExpressions one expression set has to be an array.");
      }
      
      std::vector<triagens::arango::traverser::TraverserExpression*> oneExpressionSet;
      oneExpressionSet.reserve(oneSetLength);
      size_t n = std::stoull(k);
      _expressions.emplace(n, oneExpressionSet);
      auto it = _expressions.find(n);

      for (size_t j = 0; j < oneSetLength; j++) {
        auto sx = oneExpressionSetJson.at(j);
        std::unique_ptr<SimpleTraverserExpression> oneX(new SimpleTraverserExpression(plan->getAst(), sx));
        it->second.emplace_back(oneX.get());
        oneX.release();
      }
    }
    if (_expressions.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN, "simpleExpressions has to be non empty.");
    }
  }

  // Out variables
  if (base.has("vertexOutVariable")) {
    _vertexOutVariable = varFromJson(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.has("edgeOutVariable")) {
    _edgeOutVariable = varFromJson(plan->getAst(), base, "edgeOutVariable");
  }
  if (base.has("pathOutVariable")) {
    _pathOutVariable = varFromJson(plan->getAst(), base, "pathOutVariable");
  }
}

int TraversalNode::checkIsOutVariable (size_t variableId) const {
  if (_vertexOutVariable != nullptr && _vertexOutVariable->id == variableId) {
    return 0;
  }
  if (_edgeOutVariable != nullptr && _edgeOutVariable->id == variableId) {
    return 1;
  }
  if (_pathOutVariable != nullptr && _pathOutVariable->id == variableId) {
    return 2;
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for TraversalNode
////////////////////////////////////////////////////////////////////////////////

void TraversalNode::toJsonHelper (triagens::basics::Json& nodes,
                                  TRI_memory_zone_t* zone,
                                  bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));  // call base class method

  if (json.isEmpty()) {
    return;
  }

  json("database", triagens::basics::Json(_vocbase->_name))
      ("minDepth", triagens::basics::Json(static_cast<double>(_minDepth)))
      ("maxDepth", triagens::basics::Json(static_cast<double>(_maxDepth)))
      ("direction", triagens::basics::Json(static_cast<int32_t>(_direction)))
      ("graph" , _graphJson.copy());

  // In variable
  if (usesInVariable()) {
    json("inVariable", inVariable()->toJson());
  }
  else {
    json("vertexId", triagens::basics::Json(_vertexId));
  }

  if (_condition != nullptr) {
    json("condition", _condition->toJson(TRI_UNKNOWN_MEM_ZONE, verbose)); 
  }
  
  if (_graphObj != nullptr) {
    json("graphDefinition", _graphObj->toJson(TRI_UNKNOWN_MEM_ZONE, verbose)); 
  }

  // Out variables
  if (usesVertexOutVariable()) {
    json("vertexOutVariable", vertexOutVariable()->toJson());
  }
  if (usesEdgeOutVariable()) {
    json("edgeOutVariable", edgeOutVariable()->toJson());
  }
  if (usesPathOutVariable()) {
    json("pathOutVariable", pathOutVariable()->toJson());
  }

  if (! _expressions.empty()) {
    triagens::basics::Json expressionObject = triagens::basics::Json(triagens::basics::Json::Object,
                                                                     _expressions.size());
    for (auto const& map : _expressions) {
      triagens::basics::Json expressionArray = triagens::basics::Json(triagens::basics::Json::Array,
                                                                      map.second.size());
      for (auto const& x : map.second) {
        triagens::basics::Json exp(zone, triagens::basics::Json::Object);
        auto tmp = dynamic_cast<SimpleTraverserExpression*>(x);
        if (tmp != nullptr) {
          tmp->toJson(exp, zone);
        }
        expressionArray(exp);
      }
      expressionObject.set(std::to_string(map.first), expressionArray);
    }
    json("simpleExpressions", expressionObject);
  }
  // And add it:
  nodes(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone ExecutionNode recursively
////////////////////////////////////////////////////////////////////////////////

ExecutionNode* TraversalNode::clone (ExecutionPlan* plan,
                                     bool withDependencies,
                                     bool withProperties) const {
  auto c = new TraversalNode(plan, _id, _vocbase, _edgeColls, _inVariable,
                             _vertexId, _direction, _minDepth, _maxDepth);

  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable = plan->getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c->setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable = plan->getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c->setEdgeOutput(edgeOutVariable);
  }

  if (usesPathOutVariable()) {
    auto pathOutVariable = _pathOutVariable;
    if (withProperties) {
      pathOutVariable = plan->getAst()->variables()->createVariable(pathOutVariable);
    }
    TRI_ASSERT(pathOutVariable != nullptr);
    c->setPathOutput(pathOutVariable);
  }

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of a traversal node
////////////////////////////////////////////////////////////////////////////////
        
double TraversalNode::estimateCost (size_t& nrItems) const { 
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  double expectedEdgesPerDepth = 0;
  auto collections = _plan->getAst()->query()->collections();
  for (auto const& it : _edgeColls) {
    auto collection = collections->get(it);
    for (auto const& index : collection->getIndexes()) {
      if (index->type == triagens::arango::Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        // We can only use Edge Index
        if (index->hasSelectivityEstimate()) {
          expectedEdgesPerDepth += 1 / index->selectivityEstimate();
        }
        else {
          expectedEdgesPerDepth += 1000; // Hard-coded
        }
        break;
      }
    }
  }
  nrItems = static_cast<size_t>(incoming * pow(expectedEdgesPerDepth, _maxDepth));
  if (nrItems == 0 && incoming > 0) {
    nrItems = 1; // min value
  }
  return depCost + nrItems;
}

void TraversalNode::fillTraversalOptions (triagens::arango::traverser::TraverserOptions& opts) const {
  opts.direction = _direction;
  opts.minDepth = _minDepth;
  opts.maxDepth = _maxDepth;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief remember the condition to execute for early traversal abortion.
////////////////////////////////////////////////////////////////////////////////

void TraversalNode::setCondition(triagens::aql::Condition* condition){

  std::unordered_set<Variable const*> varsUsedByCondition;

  Ast::getReferencedVariables(condition->root(), varsUsedByCondition);

  for (auto const& oneVar : varsUsedByCondition) {
    if ((_vertexOutVariable != nullptr && oneVar->id != _vertexOutVariable->id) &&
        (_edgeOutVariable   != nullptr && oneVar->id != _edgeOutVariable->id) &&
        (_pathOutVariable   != nullptr && oneVar->id != _pathOutVariable->id) &&
        (_inVariable        != nullptr && oneVar->id != _inVariable->id)) {

      _conditionVariables.emplace_back(oneVar);
    }
  }

  _condition = condition;
}

void TraversalNode::storeSimpleExpression (bool isEdgeAccess,
                                           size_t indexAccess,
                                           AstNodeType comparisonType,
                                           AstNode const* varAccess,
                                           AstNode* compareToNode) {
  auto it = _expressions.find(indexAccess);

  if (it == _expressions.end()) {
    std::vector<triagens::arango::traverser::TraverserExpression* > sec;
    _expressions.emplace(indexAccess, sec);
    it = _expressions.find(indexAccess);
  }

  auto e = std::make_unique<SimpleTraverserExpression>(isEdgeAccess, comparisonType, varAccess, compareToNode);
  it->second.push_back(e.get());
  e.release();
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
