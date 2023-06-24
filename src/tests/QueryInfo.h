// SIGMOD Programming Contest 2018 Submission
// Copyright (C) 2018  Florian Wolf, Michael Brendle, Georgios Psaropoulos
//
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program; if not, see
// <http://www.gnu.org/licenses/>.

#include "../query_processor/ExecutionPlan.h"
#include "../query_processor/PipelineBreaker.h"
#include "./FilterRewriter.h"
#include "./Spliter.h"

#include <vector>
#include <algorithm>
#include <sstream>
// #include <cassert>

template<uint32_t TABLE_PARTITION_SIZE>
class QueryInfo {

    private:
        std::vector<query_processor::TableInput> _tables;
        std::vector<query_processor::JoinAttributeInput> _innerEquiJoins;
        std::vector<query_processor::ProjectionMetaData> _projections;

        std::map<std::string, std::string> _tabAlias;
        bool _impossible = false;

        // parse relation ids <r1> <r2> ...
        void parseRelationIds(std::string& rawRelations) {
            std::vector<std::string> relationIds;
            splitString(rawRelations, relationIds, ' ');
            for (uint32_t aliasId = 0; aliasId < relationIds.size(); ++aliasId) {
                _tables.emplace_back("R" + relationIds[aliasId], std::to_string(aliasId));
                _tabAlias.insert(std::make_pair(std::to_string(aliasId), relationIds[aliasId]));
            }
        }

        void parseJoinPredicates(std::vector<std::pair<std::string, std::string>>& joins) {
            for (auto& join : joins) {
                std::vector<std::string> leftSide, rightSide;
                splitString(join.first, leftSide, '.');
                splitString(join.second, rightSide, '.');
                // create join predicate
                query_processor::JoinAttributeInput joinAttribute(leftSide[0], leftSide[1], rightSide[0], rightSide[1]);
                // check if this join predicate was already added
                auto it = std::find(_innerEquiJoins.begin(), _innerEquiJoins.end(), joinAttribute);
                // add new join predicate
                if (it == _innerEquiJoins.end()) {
                    _innerEquiJoins.emplace_back(leftSide[0], leftSide[1], rightSide[0], rightSide[1]);
                    // add used column name to table(s)
                    _tables[std::stoul(leftSide[0])].addUsedColumn(leftSide[1]);
                    _tables[std::stoul(rightSide[0])].addUsedColumn(rightSide[1]);
                }
            }
        }

        void parseFilterPredicates(std::vector<Filter>& filters) {
            for (auto& filter : filters) {
                // add filter to table
                std::vector<std::string> column;
                splitString(filter.col, column, '.');
                query_processor::FilterMetaData filterPredicate(column[1], filter.constant, query_processor::FilterMetaData::Comparison(filter.compType));
                _tables[std::stoul(column[0])].addFilterPredicate(filterPredicate);
                // add used column name to table
                _tables[std::stoul(column[0])].addUsedColumn(column[1]);
            }
        }

        // parse selections r1.a r1.b r3.c...
        void parseProjections(std::string& rawProjections) {
            std::vector<std::string> projectionStrings;
            splitString(rawProjections, projectionStrings, ' ');
            for (auto& rawSelect : projectionStrings) {
                std::vector<std::string> projection;
                splitString(rawSelect, projection, '.');
                // assert(projection.size() == 2);
                _projections.emplace_back(projection[0], projection[1]);
                // add used column name to table
                _tables[std::stoul(projection[0])].addUsedColumn(projection[1], true); // boolean indicates that it is a projection column
            }
        }

        // parse selections [RELATIONS]|[PREDICATES]|[SELECTS]
        void parseQuery(std::string& rawQuery, database::Database<TABLE_PARTITION_SIZE>& database) {
            std::vector<std::string> queryParts;
            splitString(rawQuery, queryParts, '|');
            // assert(queryParts.size()==3);
            parseRelationIds(queryParts[0]);
            parseProjections(queryParts[2]);

            FilterRewriter<TABLE_PARTITION_SIZE> rewriter(database, _tabAlias);
            std::vector<std::pair<std::string, std::string>> joins;
            std::vector<Filter> filters;
            
            _impossible = rewriter.rewriteFilters(queryParts[1], joins, filters);
            if (_impossible) {
                return;
            }
            parseJoinPredicates(joins);
            parseFilterPredicates(filters);
        }

    public:
        // the constructor that parses a query
        QueryInfo(std::string rawQuery, database::Database<TABLE_PARTITION_SIZE>& database) {
            parseQuery(rawQuery, database);
        }

        bool isImpossible() {
          return _impossible;
        }

        // returns the table meta data
        std::vector<query_processor::TableInput>& getTables() {
            return _tables;
        }

        // returns the join attribute meta data
        std::vector<query_processor::JoinAttributeInput>& getInnerEquiJoins() {
            return _innerEquiJoins;
        }
        // returns the projection meta data
        std::vector<query_processor::ProjectionMetaData>& getProjections() {
            return _projections;
        }

};
