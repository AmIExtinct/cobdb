#pragma once

#include <string>
#include <vector>

namespace cob {

struct KeyValuePair {
    std::string key;
    std::string value;
};

struct Node {
    enum class Kind : unsigned char { Folder = 0, Record = 1 };

    Kind kind;
    std::string name;
    std::vector<Node> children;
    std::vector<KeyValuePair> pairs;

    Node(Kind nodeKind = Kind::Folder, const std::string& nodeName = std::string());
};

class Database {
public:
    explicit Database(const std::string& filePath = std::string());

    static bool load(const std::string& filePath, Database& database, std::string& error);

    bool save(std::string& error) const;
    Node& root();
    const Node& root() const;
    const std::string& filePath() const;

private:
    std::string filePath_;
    Node root_;
};

} // namespace cob