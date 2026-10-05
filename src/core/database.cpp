#include "database.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>

namespace cob {
namespace {

const char kMagic[] = {'C', 'O', 'B', 'D', 'B', '0', '0', '1'};
const unsigned int kMaxStringLength = 64u * 1024u * 1024u;
const unsigned int kMaxCollectionSize = 1000000u;
const unsigned int kMaxDepth = 1024u;

bool writeU32(std::ostream& output, unsigned int value) {
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value & 0xffu),
        static_cast<unsigned char>((value >> 8u) & 0xffu),
        static_cast<unsigned char>((value >> 16u) & 0xffu),
        static_cast<unsigned char>((value >> 24u) & 0xffu)
    };
    output.write(reinterpret_cast<const char*>(bytes), 4);
    return static_cast<bool>(output);
}

bool readU32(std::istream& input, unsigned int& value) {
    unsigned char bytes[4] = {};
    input.read(reinterpret_cast<char*>(bytes), 4);
    if (!input) return false;
    value = static_cast<unsigned int>(bytes[0]) |
        (static_cast<unsigned int>(bytes[1]) << 8u) |
        (static_cast<unsigned int>(bytes[2]) << 16u) |
        (static_cast<unsigned int>(bytes[3]) << 24u);
    return true;
}

bool writeString(std::ostream& output, const std::string& value) {
    if (value.size() > kMaxStringLength) return false;
    return writeU32(output, static_cast<unsigned int>(value.size())) &&
        (output.write(value.data(), static_cast<std::streamsize>(value.size())), static_cast<bool>(output));
}

bool readString(std::istream& input, std::string& value) {
    unsigned int length = 0;
    if (!readU32(input, length) || length > kMaxStringLength) return false;
    value.assign(length, '\0');
    if (length != 0) input.read(&value[0], static_cast<std::streamsize>(length));
    return static_cast<bool>(input);
}

bool writeNode(std::ostream& output, const Node& node, unsigned int depth) {
    if (depth > kMaxDepth || node.children.size() > kMaxCollectionSize ||
        node.pairs.size() > kMaxCollectionSize) return false;
    const unsigned char kind = static_cast<unsigned char>(node.kind);
    output.write(reinterpret_cast<const char*>(&kind), 1);
    if (!output || !writeString(output, node.name)) return false;

    if (node.kind == Node::Kind::Folder) {
        if (!node.pairs.empty() || !writeU32(output, static_cast<unsigned int>(node.children.size()))) return false;
        for (std::vector<Node>::const_iterator child = node.children.begin(); child != node.children.end(); ++child) {
            if (!writeNode(output, *child, depth + 1)) return false;
        }
        return true;
    }

    if (!node.children.empty() || !writeU32(output, static_cast<unsigned int>(node.pairs.size()))) return false;
    for (std::vector<KeyValuePair>::const_iterator entry = node.pairs.begin(); entry != node.pairs.end(); ++entry) {
        if (!writeString(output, entry->key) || !writeString(output, entry->value)) return false;
    }
    return true;
}

bool readNode(std::istream& input, Node& node, unsigned int depth) {
    if (depth > kMaxDepth) return false;
    unsigned char kind = 0;
    input.read(reinterpret_cast<char*>(&kind), 1);
    if (!input || kind > static_cast<unsigned char>(Node::Kind::Record)) return false;

    std::string name;
    unsigned int count = 0;
    if (!readString(input, name) || !readU32(input, count) || count > kMaxCollectionSize) return false;

    node = Node(static_cast<Node::Kind>(kind), name);
    if (node.kind == Node::Kind::Folder) {
        node.children.reserve(count);
        for (unsigned int index = 0; index < count; ++index) {
            Node child;
            if (!readNode(input, child, depth + 1)) return false;
            node.children.push_back(child);
        }
        return true;
    }

    node.pairs.reserve(count);
    for (unsigned int index = 0; index < count; ++index) {
        KeyValuePair entry;
        if (!readString(input, entry.key) || !readString(input, entry.value)) return false;
        node.pairs.push_back(entry);
    }
    return true;
}

} // namespace

Node::Node(Kind nodeKind, const std::string& nodeName) : kind(nodeKind), name(nodeName) {}

Database::Database(const std::string& filePath) : filePath_(filePath), root_(Node::Kind::Folder, std::string()) {}

bool Database::load(const std::string& filePath, Database& database, std::string& error) {
    std::ifstream input(filePath.c_str(), std::ios::binary);
    if (!input) {
        error = "Could not open database file.";
        return false;
    }

    char magic[sizeof(kMagic)] = {};
    input.read(magic, sizeof(magic));
    if (!input || !std::equal(magic, magic + sizeof(kMagic), kMagic)) {
        error = "This is not a supported COB database.";
        return false;
    }

    Node root;
    if (!readNode(input, root, 0) || root.kind != Node::Kind::Folder || !root.name.empty()) {
        error = "The database is damaged or has an unsupported structure.";
        return false;
    }
    if (input.peek() != std::char_traits<char>::eof()) {
        error = "The database contains unexpected trailing data.";
        return false;
    }

    database.filePath_ = filePath;
    database.root_ = root;
    return true;
}

bool Database::save(std::string& error) const {
    const std::string temporaryPath = filePath_ + ".tmp";
    std::ofstream output(temporaryPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not create a temporary database file.";
        return false;
    }

    output.write(kMagic, sizeof(kMagic));
    const bool written = static_cast<bool>(output) && writeNode(output, root_, 0);
    output.flush();
    const bool flushed = static_cast<bool>(output);
    output.close();
    if (!written || !flushed) {
        std::remove(temporaryPath.c_str());
        error = "Could not write the database. The file may be too large.";
        return false;
    }

#ifdef _WIN32
    std::remove(filePath_.c_str());
#endif
    if (std::rename(temporaryPath.c_str(), filePath_.c_str()) != 0) {
        std::remove(temporaryPath.c_str());
        error = "Could not replace the database file.";
        return false;
    }
    return true;
}

Node& Database::root() { return root_; }
const Node& Database::root() const { return root_; }
const std::string& Database::filePath() const { return filePath_; }

} // namespace cob