#pragma once
#include "../database/Database.h"
#include "./Spliter.h"

#include <vector>
#include <algorithm>
#include <sstream>

struct Bound {
    uint64_t upperBound = std::numeric_limits<uint64_t>::max();
    uint64_t lowerBound = std::numeric_limits<uint64_t>::min();
};

struct Filter {
    std::string col;
    char compType;
    uint64_t constant;
};


template<uint32_t TABLE_PARTITION_SIZE>
class FilterRewriter {

    private:        
        database::Database<TABLE_PARTITION_SIZE>& _database;
        std::map<std::string, Bound> Col2Bound;
        std::map<std::string, std::string> _tabAlias;

        std::vector<Filter> _filters;

        std::map<std::string, int> _disjointCol;
        std::map<int, std::vector<std::string>> _disjointInt;

        std::map<int, Bound> _colBound;

        bool _impossible = false;
        int _idx = 0;

        Bound& getBound(std::string col) {
            if (Col2Bound.find(col) == Col2Bound.end()) {
                std::vector<std::string> column;
                splitString(col, column, '.');
                auto ptr = _database.retrieveTableByNameReadOnly("R" + _tabAlias[column[0]])->retrieveColumnByNameReadOnly(column[1]);
                Col2Bound.insert(std::make_pair(col, Bound{ptr->getLatestMaxValue(), ptr->getLatestMinValue()}));
            }
            return Col2Bound[col];
        }


    public:
        FilterRewriter(database::Database<TABLE_PARTITION_SIZE>& database, 
                std::map<std::string, std::string> tabAlias) : _database(database), _tabAlias(tabAlias) {

        }

        void disjointCols(std::string leftCol, std::string rightCol) {
            // TODO: implement disjoin set
            if (_disjointCol.find(leftCol) == _disjointCol.end() && _disjointCol.find(rightCol) == _disjointCol.end()) {
                _disjointCol.insert(std::make_pair(leftCol, _idx));
                _disjointCol.insert(std::make_pair(rightCol, _idx));
                _disjointInt.insert(std::make_pair(_idx, std::vector<std::string>{leftCol, rightCol}));
                _idx++;
            } else if (_disjointCol.find(leftCol) == _disjointCol.end()) {
                int curIdx = _disjointCol[rightCol];
                _disjointCol.insert(std::make_pair(leftCol, curIdx));
                _disjointInt[curIdx].emplace_back(leftCol);
            } else if (_disjointCol.find(rightCol) == _disjointCol.end()) {
                int curIdx = _disjointCol[leftCol];
                _disjointCol.insert(std::make_pair(rightCol, curIdx));
                _disjointInt[curIdx].emplace_back(rightCol);
            } else {
                int curLeftIdx = _disjointCol[leftCol];
                int curRightIdx = _disjointCol[rightCol];
                if (curLeftIdx != curRightIdx) {
                    if (_disjointInt[curLeftIdx].size() < _disjointInt[curRightIdx].size()) {
                        std::swap(curLeftIdx, curRightIdx);
                    }
                    for (auto& col : _disjointInt[curRightIdx]) {
                        _disjointCol[col] = curLeftIdx;
                    }
                    _disjointInt[curLeftIdx].insert(_disjointInt[curLeftIdx].end(), _disjointInt[curRightIdx].begin(), _disjointInt[curRightIdx].end());
                    _disjointInt.erase(curRightIdx);
                }
            }
        }

        void calJoinBound() {
            for (auto& disjoint : _disjointInt) {
                Bound curBound;
                for (auto& col : disjoint.second) {
                    curBound.upperBound = std::min(curBound.upperBound, getBound(col).upperBound);
                    curBound.lowerBound = std::max(curBound.lowerBound, getBound(col).lowerBound);
                }
                _colBound.insert(std::make_pair(disjoint.first, curBound));
            }
        }

        void calFilterBound() {
            if (_filters.size() > 0){
                // TODO: rewrite queries
                for (auto& filter : _filters) {
                    if (_disjointCol.find(filter.col) == _disjointCol.end()) {
                        _disjointCol.insert(std::make_pair(filter.col, _idx));
                        _disjointInt.insert(std::make_pair(_idx, std::vector<std::string>{filter.col}));
                        _colBound.insert(std::make_pair(_idx, Bound{getBound(filter.col).upperBound, getBound(filter.col).lowerBound}));
                        _idx++;
                    }
                    auto& curBound = _colBound[_disjointCol[filter.col]];
                    switch (filter.compType)
                    {
                    case '<':
                        curBound.upperBound = std::min(curBound.upperBound, filter.constant - 1);
                        break;
                    case '>':
                        curBound.lowerBound = std::max(curBound.lowerBound, filter.constant + 1);
                        break;
                    case '=':
                        if (filter.constant <= curBound.upperBound && filter.constant >= curBound.lowerBound) {
                            curBound.upperBound = filter.constant;
                            curBound.lowerBound = filter.constant;
                        } else {
                            _impossible = true;
                            return;
                        }
                        break;
                    default:
                        _impossible = true;
                        return;
                        break;
                    }
                }
            }
        }

        // parse _filters r1.a=r2.b&r1.b=r3.c...
        bool rewriteFilters(std::string& text, 
                std::vector<std::pair<std::string, std::string>>& joins, 
                std::vector<Filter>& filters) {
            std::vector<std::string> predicateStrings;
            splitString(text, predicateStrings, '&');
            for (auto& rawPredicate : predicateStrings) {
                // split predicate
                std::vector<std::string> relCols;
                splitPredicates(rawPredicate, relCols);
                // assert(relCols.size() == 2);
                // assert(!isConstant(relCols[0]) && "left side of a predicate is always a SelectInfo");
                // parse left side
                // check for filter
                if (isConstant(relCols[1])) {
                    // TODO: rewrite queries
                    uint64_t constant = stoul(relCols[1]);
                    char compType = (rawPredicate[relCols[0].size()]);
                    _filters.emplace_back(Filter{relCols[0], compType, constant});
                } else {
                    // parse right side
                    std::vector<std::string> rightSide;
                    splitString(relCols[1], rightSide, '.');
                    disjointCols(relCols[0], relCols[1]);

                    joins.emplace_back(std::make_pair(relCols[0], relCols[1]));
                }
            }
            
            calJoinBound();
            
            calFilterBound();

            if (_impossible) {
                return _impossible;
            }
            
            for (auto& col : _disjointCol) {
                auto& curBound = _colBound[col.second];
                if (curBound.upperBound > curBound.lowerBound) {
                    if (curBound.upperBound != getBound(col.first).upperBound) {
                        filters.emplace_back(Filter{col.first, '<', curBound.upperBound + 1});
                    }
                    if (curBound.lowerBound != getBound(col.first).lowerBound) {
                        filters.emplace_back(Filter{col.first, '>', curBound.lowerBound - 1});
                    }
                } else if (curBound.upperBound == curBound.lowerBound) {
                    filters.emplace_back(Filter{col.first, '=', curBound.upperBound});
                } else {
                    _impossible = true;
                    break;
                }
            }
            
            return _impossible;
        }
};
