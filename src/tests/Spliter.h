#pragma once
#include "../query_processor/ExecutionPlan.h"

#include <vector>
#include <algorithm>
#include <sstream>

static const std::vector<query_processor::FilterMetaData::Comparison> comparisonTypes {
    query_processor::FilterMetaData::Comparison::Less,
    query_processor::FilterMetaData::Comparison::Greater,
    query_processor::FilterMetaData::Comparison::Equal
};

inline static bool isConstant(std::string& raw) { return raw.find('.') == std::string::npos; }


// parse a line into strings
void splitString(std::string& line, std::vector<std::string>& result, const char delimiter) {
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
    result.push_back(token);
    }
}

// split a line into predicate strings
void splitPredicates(std::string& line, std::vector<std::string>& result) {
    for (auto cT : comparisonTypes) {
    if (line.find(cT) != std::string::npos) {
        splitString(line, result, cT);
        break;
    }
    }
}