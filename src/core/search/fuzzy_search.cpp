#include "fuzzy_search.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace cob {
namespace FuzzySearch {
namespace {

std::string lowerAscii(const std::string& value) {
    std::string result(value);
    for (std::string::iterator character = result.begin(); character != result.end(); ++character) {
        *character = static_cast<char>(std::tolower(static_cast<unsigned char>(*character)));
    }
    return result;
}

double scoreLowercase(const std::string& candidate, const std::string& query) {
    if (query.empty()) return 0.0;
    std::size_t position = 0;
    std::size_t previous = std::string::npos;
    double score = 0.0;
    for (std::string::const_iterator wanted = query.begin(); wanted != query.end(); ++wanted) {
        position = candidate.find(*wanted, position);
        if (position == std::string::npos) return -1.0;
        score += 1.0;
        if (position == 0 || candidate[position - 1] == '/' || candidate[position - 1] == '_' ||
            candidate[position - 1] == '-' || candidate[position - 1] == ' ') score += 1.4;
        if (previous != std::string::npos && position == previous + 1) score += 2.0;
        previous = position++;
    }
    score -= static_cast<double>(candidate.size() - query.size()) * 0.025;
    return score;
}

void collect(Node& folder, Node::Kind kind, const std::string& query,
             std::vector<Node*>& path, std::vector<NodeMatch>& matches) {
    for (std::vector<Node>::iterator child = folder.children.begin(); child != folder.children.end(); ++child) {
        path.push_back(&*child);
        if (child->kind == kind) {
            std::ostringstream fullPath;
            for (std::vector<Node*>::const_iterator part = path.begin(); part != path.end(); ++part) {
                fullPath << '/' << (*part)->name;
            }
            const std::string displayPath = fullPath.str();
            const double score = scoreLowercase(lowerAscii(displayPath), query);
            if (score >= 0.0) {
                NodeMatch match;
                match.node = &*child;
                match.path = path;
                match.score = score;
                match.displayPath = displayPath;
                matches.push_back(match);
            }
        }
        if (child->kind == Node::Kind::Folder) collect(*child, kind, query, path, matches);
        path.pop_back();
    }
}

} // namespace

std::vector<NodeMatch> findNodes(Node& root, Node::Kind kind, const std::string& query) {
    std::vector<NodeMatch> matches;
    std::vector<Node*> path;
    collect(root, kind, lowerAscii(query), path, matches);
    return matches;
}

std::vector<PairMatch> findPairs(const Node& record, const std::string& query) {
    const std::string normalizedQuery = lowerAscii(query);
    std::vector<PairMatch> matches;
    for (std::size_t index = 0; index < record.pairs.size(); ++index) {
        const double score = scoreLowercase(lowerAscii(record.pairs[index].key), normalizedQuery);
        if (score >= 0.0) {
            PairMatch match;
            match.index = index;
            match.score = score;
            matches.push_back(match);
        }
    }
    return matches;
}

} // namespace FuzzySearch
} // namespace cob