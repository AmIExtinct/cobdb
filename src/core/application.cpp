#include "application.hpp"
#include "fuzzy_search.hpp"

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace cob {
namespace {

const int kEscape = 27;
const int kEnter = Terminal::KeyEnter;
const int kBackspace = 127;

enum class TreeAction {
    Ignore, Quit, MoveDown, MoveUp, Back, NewFolder, NewRecord,
    Open, EnterFolder, Rename, Delete, SearchFolders, SearchRecords
};

enum class RecordAction { Ignore, Quit, Back, MoveDown, MoveUp, CreatePair, EditPair, DeletePair, SearchKeys };

TreeAction treeActionFor(int key) {
    static const std::unordered_map<int, TreeAction> actions = {
        {'q', TreeAction::Quit}, {'Q', TreeAction::Quit},
        {'j', TreeAction::MoveDown}, {Terminal::KeyDown, TreeAction::MoveDown},
        {'k', TreeAction::MoveUp}, {Terminal::KeyUp, TreeAction::MoveUp},
        {'h', TreeAction::Back}, {Terminal::KeyLeft, TreeAction::Back}, {kBackspace, TreeAction::Back},
        {'N', TreeAction::NewFolder}, {'n', TreeAction::NewRecord},
        {'o', TreeAction::Open}, {kEnter, TreeAction::Open},
        {'l', TreeAction::EnterFolder},
        {'R', TreeAction::Rename}, {'d', TreeAction::Delete}, {'D', TreeAction::Delete},
        {'f', TreeAction::SearchFolders}, {'r', TreeAction::SearchRecords}
    };
    std::unordered_map<int, TreeAction>::const_iterator action = actions.find(key);
    return action == actions.end() ? TreeAction::Ignore : action->second;
}

RecordAction recordActionFor(int key) {
    static const std::unordered_map<int, RecordAction> actions = {
        {'q', RecordAction::Quit}, {'Q', RecordAction::Quit},
        {'h', RecordAction::Back}, {Terminal::KeyLeft, RecordAction::Back},
        {kBackspace, RecordAction::Back}, {kEscape, RecordAction::Back},
        {'j', RecordAction::MoveDown}, {Terminal::KeyDown, RecordAction::MoveDown},
        {'k', RecordAction::MoveUp}, {Terminal::KeyUp, RecordAction::MoveUp},
        {'c', RecordAction::CreatePair}, {'e', RecordAction::EditPair},
        {'d', RecordAction::DeletePair}, {'D', RecordAction::DeletePair},
        {'s', RecordAction::SearchKeys}
    };
    std::unordered_map<int, RecordAction>::const_iterator action = actions.find(key);
    return action == actions.end() ? RecordAction::Ignore : action->second;
}

std::string safeText(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (std::string::const_iterator character = value.begin(); character != value.end(); ++character) {
        const unsigned char byte = static_cast<unsigned char>(*character);
        result += (byte < 32 || byte == 127) ? '?' : static_cast<char>(byte);
    }
    return result;
}

void eraseLastCharacter(std::string& value) {
    if (value.empty()) return;
    std::size_t start = value.size() - 1;
    while (start > 0 && (static_cast<unsigned char>(value[start]) & 0xc0) == 0x80) --start;
    value.erase(start);
}

std::size_t findChildIndex(const Node& folder, const Node* child) {
    for (std::size_t index = 0; index < folder.children.size(); ++index) {
        if (&folder.children[index] == child) return index;
    }
    return 0;
}

} // namespace

Application::Application(Database& database) : database_(database), quitting_(false), treeSelection_(0) {
    folderStack_.push_back(&database_.root());
}

int Application::run() {
    if (!terminal_.isInteractive()) {
        std::fprintf(stderr, "Opening a database requires an interactive terminal.\n");
        return 1;
    }
    terminal_.enterRawMode();
    terminal_.enterFullScreen();
    while (!quitting_) {
        drawTree();
        handleTreeKey(terminal_.readKey());
    }
    if (!saveNow()) {
        terminal_.restore();
        std::fprintf(stderr, "Database save failed: %s\n", message_.c_str());
        return 1;
    }
    terminal_.restore();
    std::puts("Database saved. Bye.");
    return 0;
}

void Application::drawTree() const {
    const Node& folder = *folderStack_.back();
    const std::size_t selected = folder.children.empty() ? 0 :
        std::min(folder.children.size() - 1, treeSelection_);
    std::vector<std::string> rows;
    for (std::vector<Node>::const_iterator child = folder.children.begin(); child != folder.children.end(); ++child) {
        rows.push_back(std::string(child->kind == Node::Kind::Folder ? "[FOLDER] " : "[RECORD] ") + safeText(child->name));
    }
    std::string controls = "j/k move  Enter/o open record  l enter folder  N folder  n record  R rename  d delete  f folders  r records  q quit";
    if (!message_.empty()) controls += "  |  " + message_;
    renderer_.draw("COB DATABASE", currentFolderPath(), rows, selected,
                   "This folder is empty. Press N to add a folder or n to add a record.", controls);
}

Node* Application::selectedNode() {
    Node& folder = *folderStack_.back();
    if (folder.children.empty()) return 0;
    return &folder.children[std::min(folder.children.size() - 1, treeSelection_)];
}

void Application::handleTreeKey(int key) {
    Node& folder = *folderStack_.back();
    switch (treeActionFor(key)) {
    case TreeAction::Quit:
        quitting_ = true;
        break;
    case TreeAction::MoveDown:
        if (!folder.children.empty() && treeSelection_ + 1 < folder.children.size()) ++treeSelection_;
        message_.clear();
        break;
    case TreeAction::MoveUp:
        if (treeSelection_ > 0) --treeSelection_;
        message_.clear();
        break;
    case TreeAction::Back:
        if (folderStack_.size() > 1) {
            folderStack_.pop_back();
            treeSelection_ = parentSelections_.back();
            parentSelections_.pop_back();
        }
        break;
    case TreeAction::NewFolder: {
        std::string name;
        if (prompt("New folder name", name) && !name.empty()) {
            folder.children.push_back(Node(Node::Kind::Folder, name));
            treeSelection_ = folder.children.size() - 1;
            saveNow();
        }
        break;
    }
    case TreeAction::NewRecord: {
        std::string name;
        if (prompt("New record name", name) && !name.empty()) {
            folder.children.push_back(Node(Node::Kind::Record, name));
            treeSelection_ = folder.children.size() - 1;
            saveNow();
        }
        break;
    }
    case TreeAction::Open:
    case TreeAction::EnterFolder: {
        Node* node = selectedNode();
        if (node && node->kind == Node::Kind::Folder) {
            parentSelections_.push_back(treeSelection_);
            folderStack_.push_back(node);
            treeSelection_ = 0;
        } else if (treeActionFor(key) == TreeAction::Open && node) {
            openRecord(*node);
        }
        break;
    }
    case TreeAction::Rename: {
        Node* node = selectedNode();
        if (node) {
            std::string name = node->name;
            if (prompt("Rename to", name) && !name.empty()) {
                node->name = name;
                saveNow();
            }
        }
        break;
    }
    case TreeAction::Delete: {
        Node* node = selectedNode();
        if (node) {
            const std::string type = node->kind == Node::Kind::Folder ? "folder" : "record";
            if (confirm("Delete " + type + " '" + node->name + "'? (y/N)")) {
                const std::size_t index = std::min(folder.children.size() - 1, treeSelection_);
                folder.children.erase(folder.children.begin() + static_cast<std::ptrdiff_t>(index));
                if (treeSelection_ > 0 && treeSelection_ >= folder.children.size()) --treeSelection_;
                saveNow();
            }
        }
        break;
    }
    case TreeAction::SearchFolders:
        searchFoldersOrRecords(Node::Kind::Folder);
        break;
    case TreeAction::SearchRecords:
        searchFoldersOrRecords(Node::Kind::Record);
        break;
    case TreeAction::Ignore:
        break;
    }
}

void Application::openRecord(Node& record) {
    std::size_t selected = 0;
    while (!quitting_) {
        drawRecord(record, selected);
        const RecordAction action = recordActionFor(terminal_.readKey());
        switch (action) {
        case RecordAction::Quit:
            quitting_ = true;
            return;
        case RecordAction::Back:
            return;
        case RecordAction::MoveDown:
            if (!record.pairs.empty() && selected + 1 < record.pairs.size()) ++selected;
            break;
        case RecordAction::MoveUp:
            if (selected > 0) --selected;
            break;
        case RecordAction::CreatePair:
            createPair(record);
            if (!record.pairs.empty()) selected = record.pairs.size() - 1;
            break;
        case RecordAction::EditPair:
            if (!record.pairs.empty()) {
                KeyValuePair editedPair = record.pairs[selected];
                if (editPairFields(editedPair, "EDIT KEY-VALUE PAIR")) {
                    record.pairs[selected] = editedPair;
                    saveNow();
                }
            }
            break;
        case RecordAction::DeletePair:
            if (!record.pairs.empty() && confirm("Delete key '" + record.pairs[selected].key + "'? (y/N)")) {
                record.pairs.erase(record.pairs.begin() + static_cast<std::ptrdiff_t>(selected));
                if (selected > 0 && selected >= record.pairs.size()) --selected;
                saveNow();
            }
            break;
        case RecordAction::SearchKeys:
            searchKeys(record, selected);
            break;
        case RecordAction::Ignore:
            break;
        }
    }
}

void Application::drawRecord(const Node& record, std::size_t selected) const {
    std::vector<std::string> rows;
    for (std::vector<KeyValuePair>::const_iterator pair = record.pairs.begin(); pair != record.pairs.end(); ++pair) {
        rows.push_back(safeText(pair->key) + "  =  " + safeText(pair->value));
    }
    std::string controls = "j/k move  c create  e edit  s search keys  d delete  h back  q quit";
    if (!message_.empty()) controls += "  |  " + message_;
    renderer_.draw("RECORD: " + safeText(record.name), currentFolderPath() + "  |  Key-value pairs", rows, selected,
                   "No pairs yet. Press c to add one.", controls);
}

void Application::createPair(Node& record) {
    KeyValuePair pair;
    if (!editPairFields(pair, "NEW KEY-VALUE PAIR")) return;
    record.pairs.push_back(pair);
    saveNow();
}

bool Application::editPairFields(KeyValuePair& pair, const std::string& title) {
    int active = 0;
    std::size_t keyCursor = pair.key.size();
    std::size_t valueCursor = pair.value.size();
    while (true) {
        const std::size_t cursor = active == 0 ? keyCursor : valueCursor;
        renderer_.drawPairInput(title, pair.key, pair.value, active, cursor,
                                "Left/Right move  Enter switches/saves  Tab switches  Esc cancels");
        const int key = terminal_.readKey();
        std::string& value = active == 0 ? pair.key : pair.value;
        std::size_t& position = active == 0 ? keyCursor : valueCursor;
        if (key == kEscape) return false;
        if (key == '\t') active = 1 - active;
        else if (key == kEnter || key == '\n') {
            if (active == 0) active = 1;
            else if (!pair.key.empty()) return true;
            else message_ = "A key cannot be empty.";
        } else if (key == Terminal::KeyLeft) {
            if (position > 0) {
                --position;
                while (position > 0 && (static_cast<unsigned char>(value[position]) & 0xc0) == 0x80) --position;
            }
        } else if (key == Terminal::KeyRight) {
            if (position < value.size()) {
                const unsigned char byte = static_cast<unsigned char>(value[position]);
                const std::size_t width = byte < 0x80 ? 1 :
                    (byte & 0xe0) == 0xc0 ? 2 : (byte & 0xf0) == 0xe0 ? 3 : (byte & 0xf8) == 0xf0 ? 4 : 1;
                position = std::min(value.size(), position + width);
            }
        } else if (key == kBackspace || key == 8) {
            if (position > 0) {
                std::size_t previous = position - 1;
                while (previous > 0 && (static_cast<unsigned char>(value[previous]) & 0xc0) == 0x80) --previous;
                value.erase(previous, position - previous);
                position = previous;
            }
        } else if (key >= 32 && key != 127) {
            value.insert(position, 1, static_cast<char>(key));
            ++position;
        }
    }
}

void Application::searchFoldersOrRecords(Node::Kind kind) {
    std::string query;
    const std::string type = kind == Node::Kind::Folder ? "folder" : "record";
    if (!prompt("Search " + type + " names", query) || query.empty()) return;
    std::vector<FuzzySearch::NodeMatch> matches = FuzzySearch::findNodes(database_.root(), kind, query);
    std::sort(matches.begin(), matches.end(), [](const FuzzySearch::NodeMatch& left,
                                                  const FuzzySearch::NodeMatch& right) {
        if (left.score != right.score) return left.score > right.score;
        return left.displayPath < right.displayPath;
    });
    std::size_t selected = 0;
    std::vector<std::string> rows;
    for (std::vector<FuzzySearch::NodeMatch>::const_iterator match = matches.begin(); match != matches.end(); ++match) {
        rows.push_back((kind == Node::Kind::Folder ? "[FOLDER] " : "[RECORD] ") + match->displayPath);
    }
    while (true) {
        const std::string heading = kind == Node::Kind::Folder ? "SEARCH FOLDERS" : "SEARCH RECORDS";
        renderer_.draw(heading, "Query: " + safeText(query), rows, selected,
                       "No matches. Try a shorter search.", "j/k move  Enter open  Esc/q return");
        const int key = terminal_.readKey();
        if (key == kEscape || key == 'q' || key == 'Q') return;
        if ((key == 'j' || key == Terminal::KeyDown) && !matches.empty() && selected + 1 < matches.size()) ++selected;
        else if ((key == 'k' || key == Terminal::KeyUp) && selected > 0) --selected;
        else if (key == kEnter && !matches.empty()) {
            FuzzySearch::NodeMatch& match = matches[selected];
            navigateTo(match.path, match.node);
            if (match.node->kind == Node::Kind::Record) openRecord(*match.node);
            return;
        }
    }
}

void Application::searchKeys(Node& record, std::size_t& selected) {
    std::string query;
    if (!prompt("Search keys in " + record.name, query) || query.empty()) return;
    std::vector<FuzzySearch::PairMatch> matches = FuzzySearch::findPairs(record, query);
    std::sort(matches.begin(), matches.end(), [](const FuzzySearch::PairMatch& left, const FuzzySearch::PairMatch& right) {
        return left.score != right.score ? left.score > right.score : left.index < right.index;
    });
    std::size_t resultIndex = 0;
    std::vector<std::string> rows;
    for (std::vector<FuzzySearch::PairMatch>::const_iterator match = matches.begin(); match != matches.end(); ++match) {
        const KeyValuePair& pair = record.pairs[match->index];
        rows.push_back(safeText(pair.key) + "  =  " + safeText(pair.value));
    }
    while (true) {
        renderer_.draw("SEARCH KEYS", "Record: " + safeText(record.name) + "  Query: " + safeText(query),
                       rows, resultIndex, "No matching keys. Try a shorter search.",
                       "j/k move  Enter select key  Esc/q return");
        const int key = terminal_.readKey();
        if (key == kEscape || key == 'q' || key == 'Q') return;
        if ((key == 'j' || key == Terminal::KeyDown) && !matches.empty() && resultIndex + 1 < matches.size()) ++resultIndex;
        else if ((key == 'k' || key == Terminal::KeyUp) && resultIndex > 0) --resultIndex;
        else if (key == kEnter && !matches.empty()) {
            selected = matches[resultIndex].index;
            return;
        }
    }
}

bool Application::prompt(const std::string& label, std::string& value) {
    while (true) {
        renderer_.drawInput("ENTER " + label, label, value,
                            "Enter accepts  Esc cancels  Backspace edits");
        const int key = terminal_.readKey();
        if (key == kEscape) return false;
        if (key == kEnter || key == '\n') return true;
        if (key == kBackspace || key == 8) {
            eraseLastCharacter(value);
        } else if (key >= 32 && key != 127) {
            value.push_back(static_cast<char>(key));
        }
    }
}

bool Application::confirm(const std::string& question) {
    renderer_.drawConfirmation(question, "y confirm  any other key cancel");
    const int key = terminal_.readKey();
    return key == 'y' || key == 'Y';
}

bool Application::saveNow() {
    std::string error;
    if (!database_.save(error)) {
        message_ = "Save failed: " + error;
        return false;
    }
    message_ = "Saved";
    return true;
}

std::string Application::currentFolderPath() const {
    std::string path = "/";
    for (std::size_t index = 1; index < folderStack_.size(); ++index) {
        path += safeText(folderStack_[index]->name) + "/";
    }
    return path;
}

void Application::navigateTo(const std::vector<Node*>& path, Node* target) {
    folderStack_.clear();
    parentSelections_.clear();
    folderStack_.push_back(&database_.root());
    const std::size_t limit = target->kind == Node::Kind::Folder ? path.size() : path.size() - 1;
    for (std::size_t index = 0; index < limit; ++index) {
        parentSelections_.push_back(treeSelection_);
        folderStack_.push_back(path[index]);
        treeSelection_ = 0;
    }
    if (target->kind == Node::Kind::Record) treeSelection_ = findChildIndex(*folderStack_.back(), target);
}

} // namespace cob