#pragma once

#include "database.hpp"
#include "renderer.hpp"
#include "terminal.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace cob {

class Application {
public:
    explicit Application(Database& database);
    int run();

private:
    Database& database_;
    Terminal terminal_;
    Renderer renderer_;
    std::vector<Node*> folderStack_;
    std::vector<std::size_t> parentSelections_;
    std::string message_;
    bool quitting_;
    std::size_t treeSelection_;

    void drawTree() const;
    void handleTreeKey(int key);
    void openRecord(Node& record);
    void drawRecord(const Node& record, std::size_t selected) const;
    void createPair(Node& record);
    bool editPairFields(KeyValuePair& pair, const std::string& title);
    void searchFoldersOrRecords(Node::Kind kind);
    void searchKeys(Node& record, std::size_t& selected);
    bool prompt(const std::string& label, std::string& value);
    bool confirm(const std::string& question);
    bool saveNow();
    Node* selectedNode();
    std::string currentFolderPath() const;
    void navigateTo(const std::vector<Node*>& path, Node* target);
};

} // namespace cob