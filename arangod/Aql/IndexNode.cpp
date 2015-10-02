////////////////////////////////////////////////////////////////////////////////
/// @brief Infrastructure for ExecutionPlans
///
/// @file arangod/Aql/ExecutionNode.cpp
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/IndexNode.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Ast.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                              methods of IndexNode
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief toJson, for IndexNode
////////////////////////////////////////////////////////////////////////////////

void IndexNode::toJsonHelper (triagens::basics::Json& nodes,
                              TRI_memory_zone_t* zone,
                              bool verbose) const {
  triagens::basics::Json json(ExecutionNode::toJsonHelperGeneric(nodes, zone, verbose));
  // call base class method

  if (json.isEmpty()) {
    return;
  }
 
  // Now put info about vocbase and cid in there
  json("database",    triagens::basics::Json(_vocbase->_name))
      ("collection",  triagens::basics::Json(_collection->getName()))
      ("outVariable", _outVariable->toJson());

  triagens::basics::Json indexes(triagens::basics::Json::Array, _indexes.size());
  for (auto& index : _indexes) {
    indexes.add(index->toJson());
  }
 
  json("indexes", indexes); 

  if (_condition != nullptr) {
    json("condition", _condition->toJson(TRI_UNKNOWN_MEM_ZONE)); 
  }
  else {
    json("condition", triagens::basics::Json(triagens::basics::Json::Object));
  }

  // And add it:
  nodes(json);
}

ExecutionNode* IndexNode::clone (ExecutionPlan* plan,
                                 bool withDependencies,
                                 bool withProperties) const {
  auto outVariable = _outVariable;

  if (withProperties) {
    outVariable = plan->getAst()->variables()->createVariable(outVariable);
  }

  auto c = new IndexNode(plan, _id, _vocbase, _collection, 
                         outVariable, _indexes, _condition);

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor for IndexNode from Json
////////////////////////////////////////////////////////////////////////////////

IndexNode::IndexNode (ExecutionPlan* plan,
                      triagens::basics::Json const& json)
  : ExecutionNode(plan, json),
    _vocbase(plan->getAst()->query()->vocbase()),
    _collection(plan->getAst()->query()->collections()->get(JsonHelper::checkAndGetStringValue(json.json(), "collection"))),
    _outVariable(varFromJson(plan->getAst(), json, "outVariable")),
    _indexes(),
    _condition(nullptr) { 

  auto indexes = JsonHelper::checkAndGetObjectValue(json.json(), "indexes");

  TRI_ASSERT(TRI_IsArrayJson(indexes));
  size_t length = TRI_LengthArrayJson(indexes);
  _indexes.reserve(length);

  for (size_t i = 0; i < length; ++i) {
    auto iid  = JsonHelper::checkAndGetStringValue(TRI_LookupArrayJson(indexes, i), "id");
    auto index = _collection->getIndex(iid);

    if (index == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "index not found");
    }

    _indexes.emplace_back(index);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the cost of an index range node is a multiple of the cost of
/// its unique dependency - TODO
////////////////////////////////////////////////////////////////////////////////
 
double IndexNode::estimateCost (size_t& nrItems) const { 
  nrItems = 1; // TODO FIXME
  return 1.0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, returning a vector
////////////////////////////////////////////////////////////////////////////////

std::vector<Variable const*> IndexNode::getVariablesUsedHere () const {
  std::unordered_set<Variable const*> s;
  // actual work is done by that method  
  getVariablesUsedHere(s); 

  // copy result into vector
  std::vector<Variable const*> v;
  v.reserve(s.size());

  for (auto const& vv : s) {
    v.emplace_back(const_cast<Variable*>(vv));
  }
  return v;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getVariablesUsedHere, modifying the set in-place - TODO
////////////////////////////////////////////////////////////////////////////////

void IndexNode::getVariablesUsedHere (std::unordered_set<Variable const*>& vars) const {
  // This is not yet clear how the variables will be stored
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
