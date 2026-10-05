#pragma once

#include "database.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace cob {
namespace FuzzySearch {

struct NodeMatch {
    Node* node;
    std::vector<Node*> path;
    double score;
    std::string displayPath;
};

struct PairMatch {
    std::size_t index;
    double score;
};

std::vector<NodeMatch> findNodes(Node& root, Node::Kind kind, const std::string& query);
std::vector<PairMatch> findPairs(const Node& record, const std::string& query);

} // namespace FuzzySearch
} // namespace cob