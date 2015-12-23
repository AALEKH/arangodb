////////////////////////////////////////////////////////////////////////////////
/// @brief Condition finder, used to build up the Condition object
///
/// @file 
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

#ifndef ARANGOD_AQL_CONDITION_FINDER_H
#define ARANGOD_AQL_CONDITION_FINDER 1

#include "Aql/Condition.h"
#include "Aql/ExecutionNode.h"
#include "Aql/WalkerWorker.h"

namespace triagens {
  namespace aql {

////////////////////////////////////////////////////////////////////////////////
/// @brief condition finder
////////////////////////////////////////////////////////////////////////////////

    class ConditionFinder : public WalkerWorker<ExecutionNode> {

      public:

        ConditionFinder (ExecutionPlan* plan,
                         std::unordered_map<size_t, ExecutionNode*>* changes,
                         bool* hasEmptyResult)
          : _plan(plan),
            _variableDefinitions(),
            _filters(),
            _sorts(),
            _changes(changes),
            _hasEmptyResult(hasEmptyResult) {
        };

        ~ConditionFinder () {
        }

        bool before (ExecutionNode*) override final;

        bool enterSubquery (ExecutionNode*, ExecutionNode*) override final;

      private:

        ExecutionPlan*                                 _plan;
        std::unordered_map<VariableId, AstNode const*> _variableDefinitions;
        std::unordered_set<VariableId>                 _filters;
        std::vector<std::pair<VariableId, bool>>       _sorts;
        // note: this class will never free the contents of this map
        std::unordered_map<size_t, ExecutionNode*>*    _changes;
        bool*                                          _hasEmptyResult;
    
    };
  }
}

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

