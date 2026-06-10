#ifndef TICKET_SYSTEM_BPT_H
#define TICKET_SYSTEM_BPT_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <limits>
#include "vector.h"

const int MAX_KEYS = 40;
const int MIN_KEYS = MAX_KEYS / 2;
const size_t BLOCK_SIZE = 4096;          // 磁盘块大小

// 结点类型
enum NodeType : char {
    INTERNAL_NODE = 'I',
    LEAF_NODE = 'L'
};
template<typename T>
void writeKey(std::ostream& out, const T& key) {
    static_assert(sizeof(T) == 0, "writeKey not specialized for this type");
}
template<typename T>
T readKey(std::istream& in) {
    static_assert(sizeof(T) == 0, "readKey not specialized for this type");
}
template<typename T>
void writeValue(std::ostream& out, const T& val) {
    static_assert(sizeof(T) == 0, "writeValue not specialized for this type");
}
template<typename T>
T readValue(std::istream& in) {
    static_assert(sizeof(T) == 0, "readValue not specialized for this type");
}

// std::string 特化
template<>
inline void writeKey<std::string>(std::ostream& out, const std::string& s) {
    uint8_t len = static_cast<uint8_t>(s.size());
    out.write(reinterpret_cast<const char*>(&len), 1);
    out.write(s.c_str(), len);
}
template<>
inline std::string readKey<std::string>(std::istream& in) {
    uint8_t len; in.read(reinterpret_cast<char*>(&len), 1);
    std::string s(len, '\0'); in.read(&s[0], len);
    return s;
}
template<>
inline void writeValue<int>(std::ostream& out, const int& v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(int));
}
template<>
inline int readValue<int>(std::istream& in) {
    int v; in.read(reinterpret_cast<char*>(&v), sizeof(int)); return v;
}
// uint64_t 特化（用于文件偏移）
template<>
inline void writeValue<uint64_t>(std::ostream& out, const uint64_t& v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(uint64_t));
}
template<>
inline uint64_t readValue<uint64_t>(std::istream& in) {
    uint64_t v; in.read(reinterpret_cast<char*>(&v), sizeof(uint64_t)); return v;
}

template<typename KeyType, typename ValueType>
struct KeyValue {
    KeyType key;
    ValueType value;

    bool operator<(const KeyValue& other) const {
        if (key != other.key) return key < other.key;
        return value < other.value;
    }
    bool operator==(const KeyValue& other) const {
        return key == other.key && value == other.value;
    }
    bool operator<=(const KeyValue& other) const {
        return *this < other || *this == other;
    }
};

// 二分查找辅助函数
template<typename KeyType, typename ValueType>
int l_bound(const KeyValue<KeyType, ValueType>& kv,
            const sjtu::vector<KeyValue<KeyType, ValueType>>& vec) {
    int low = 0, high = vec.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (vec[mid] < kv) low = mid + 1;
        else high = mid;
    }
    return low;
}

template<typename KeyType, typename ValueType>
int u_bound(const KeyValue<KeyType, ValueType>& kv,
            const sjtu::vector<KeyValue<KeyType, ValueType>>& vec) {
    int low = 0, high = vec.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (vec[mid] <= kv) low = mid + 1;
        else high = mid;
    }
    return low;
}

class Node {
public:
    NodeType type;
    int count;
    Node(NodeType t, int c = 0) : type(t), count(c) {}
    virtual ~Node() = default;
};

template<typename KeyType, typename ValueType>
class LeafNode : public Node {
public:
    sjtu::vector<KeyValue<KeyType, ValueType>> entries;
    uint64_t fileOffset;       // 在叶子文件中的偏移（块号 * BLOCK_SIZE）
    uint64_t prevOffset;       // 前驱叶子节点偏移
    uint64_t nextOffset;       // 后继叶子节点偏移

    LeafNode() : Node(LEAF_NODE, 0), fileOffset(0), prevOffset(0), nextOffset(0) {}

    // 向叶子文件中写入叶子结点
    void writeLeaf_leaf(std::ostream& out) const {
        writeValue<int>(out, count);
        writeValue<uint64_t>(out, prevOffset);
        writeValue<uint64_t>(out, nextOffset);
        for (int i = 0; i < count; ++i) {
            writeKey<KeyType>(out, entries[i].key);
            writeValue<ValueType>(out, entries[i].value);
        }
    }

    // 向树文件内写入叶子结点（只存偏移）
    void writeLeaf_tree(std::ostream& out) const {
        char type = LEAF_NODE;
        out.write(&type, 1);
        writeValue<uint64_t>(out, fileOffset);
    }

    // 从叶子文件读入叶子结点
    void readLeaf_leaf(std::istream& in) {
        fileOffset = in.tellg();
        count = readValue<int>(in);
        prevOffset = readValue<uint64_t>(in);
        nextOffset = readValue<uint64_t>(in);
        entries.clear();
        for (int i = 0; i < count; ++i) {
            KeyValue<KeyType, ValueType> kv;
            kv.key = readKey<KeyType>(in);
            kv.value = readValue<ValueType>(in);
            entries.push_back(kv);
        }
    }

    // 插入，若重复返回 false
    bool insert(const KeyValue<KeyType, ValueType>& kv) {
        int pos = l_bound(kv, entries);
        if (pos < count && entries[pos] == kv) return false;
        entries.insert(entries.begin() + pos, kv);
        ++count;
        return true;
    }

    // 删除，若未找到返回 false
    bool remove(const KeyValue<KeyType, ValueType>& kv) {
        int pos = l_bound(kv, entries);
        if (pos >= count || !(entries[pos] == kv)) return false;
        entries.erase(entries.begin() + pos);
        --count;
        return true;
    }

    // 分裂，返回新结点
    LeafNode* split() {
        LeafNode* newNode = new LeafNode();
        int mid = count / 2;
        for (int i = mid; i < count; ++i) {
            newNode->entries.push_back(entries[i]);
        }
        newNode->count = newNode->entries.size();
        while ((int)entries.size() > mid)
            entries.pop_back();
        count = mid;
        return newNode;
    }

    KeyValue<KeyType, ValueType> getFirstEntry() const {
        if (count == 0) return KeyValue<KeyType, ValueType>{KeyType(), ValueType()};
        return entries[0];
    }
};

template<typename KeyType, typename ValueType>
class InternalNode : public Node {
public:
    sjtu::vector<KeyValue<KeyType, ValueType>> keys;  // 储存 key+value（value 仅占位）
    sjtu::vector<Node*> children;   // 孩子指针

    InternalNode() : Node(INTERNAL_NODE, 0) {}

    // 寻找可能存在 kv 的子树
    Node* findChild(const KeyValue<KeyType, ValueType>& kv) const {
        return children[u_bound(kv, keys)];
    }

    // 在 pos 位置插入 kv 和左右孩子
    void insertKey(const KeyValue<KeyType, ValueType>& kv,
                   Node* left, Node* right, int pos) {
        keys.insert(keys.begin() + pos, kv);
        children[pos] = left;
        children.insert(children.begin() + pos + 1, right);
        ++count;
    }

    // 分裂内部结点，升迁的键值存入 p_key
    InternalNode* split(KeyValue<KeyType, ValueType>& p_key) {
        InternalNode* newNode = new InternalNode();
        int mid = count / 2;
        p_key = keys[mid];
        for (int i = mid + 1; i < count; ++i)
            newNode->keys.push_back(keys[i]);
        for (int i = mid + 1; i <= count; ++i)
            newNode->children.push_back(children[i]);
        newNode->count = newNode->keys.size();
        while ((int)keys.size() > mid) keys.pop_back();
        while ((int)children.size() > mid + 1) children.pop_back();
        count = mid;
        return newNode;
    }
};

template<typename KeyType, typename ValueType>
class BPlusTree {
private:
    std::fstream leaf_file;          // 叶子结点文件
    std::fstream tree_file;          // 树文件
    std::string leaf_name;           // 叶子文件名
    std::string tree_name;           // 树文件名
    Node* root;                      // 树根
    uint64_t leafHead;               // 叶子链表头
    uint64_t leafTail;               // 叶子链表尾

    void writeLeafHeader() {
        leaf_file.seekp(0);
        writeValue<uint64_t>(leaf_file, leafHead);
        writeValue<uint64_t>(leaf_file, leafTail);
        leaf_file.seekp(4096);
    }
    void readLeafHeader() {
        leaf_file.seekg(0);
        leafHead = readValue<uint64_t>(leaf_file);
        leafTail = readValue<uint64_t>(leaf_file);
        leaf_file.seekg(4096);
    }

    LeafNode<KeyType, ValueType>* readLeaf_at(uint64_t offset) {
        leaf_file.seekg(offset);
        auto* leaf = new LeafNode<KeyType, ValueType>();
        leaf->readLeaf_leaf(leaf_file);
        return leaf;
    }
    LeafNode<KeyType, ValueType>* readLeaf_at2(uint64_t offset) {
        auto* leaf = new LeafNode<KeyType, ValueType>();
        leaf->fileOffset = offset;
        return leaf;
    }

    uint64_t allocateLeaf(LeafNode<KeyType, ValueType>* leaf) {
        uint64_t offset = 4096;
        leaf_file.seekp(offset);
        leaf->fileOffset = offset;
        leaf->prevOffset = 0;
        leaf->nextOffset = 0;
        leaf->writeLeaf_leaf(leaf_file);
        leaf_file.flush();
        leafHead = leafTail = offset;
        writeLeafHeader();
        return offset;
    }

    void updateLeaf(LeafNode<KeyType, ValueType>* leaf) {
        if (leaf->fileOffset == 0)
            allocateLeaf(leaf);
        else {
            leaf_file.seekp(leaf->fileOffset);
            leaf->writeLeaf_leaf(leaf_file);
            leaf_file.flush();
        }
    }

    void writeTree(Node* node, std::ostream& out) {
        if (node->type == LEAF_NODE) {
            auto* leaf = static_cast<LeafNode<KeyType, ValueType>*>(node);
            leaf->writeLeaf_tree(out);
            return;
        }
        auto* inode = static_cast<InternalNode<KeyType, ValueType>*>(node);
        out.write(reinterpret_cast<const char*>(&inode->type), 1);
        writeValue<int>(out, inode->count);
        for (int i = 0; i < inode->count; ++i) {
            writeKey<KeyType>(out, inode->keys[i].key);
            writeValue<ValueType>(out, inode->keys[i].value);
        }
        for (size_t i = 0; i < inode->children.size(); ++i) {
            writeTree(inode->children[i], out);
        }
    }

    Node* readTree(std::istream& in) {
        char type;
        in.read(&type, 1);
        if (type == LEAF_NODE) {
            uint64_t off = readValue<uint64_t>(in);
            return readLeaf_at2(off);
        } else if (type == INTERNAL_NODE) {
            auto* inode = new InternalNode<KeyType, ValueType>();
            inode->count = readValue<int>(in);
            for (int i = 0; i < inode->count; ++i) {
                KeyValue<KeyType, ValueType> kv;
                kv.key = readKey<KeyType>(in);
                kv.value = readValue<ValueType>(in);
                inode->keys.push_back(kv);
            }
            for (int i = 0; i <= inode->count; ++i) {
                inode->children.push_back(readTree(in));
            }
            return inode;
        }
        return nullptr;
    }

    void saveTree() {
        tree_file.open(tree_name, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!tree_file.is_open()) return;
        writeTree(root, tree_file);
        tree_file.close();
    }

    void loadTree() {
        tree_file.open(tree_name, std::ios::in | std::ios::binary);
        if (!tree_file.is_open()) return;
        root = readTree(tree_file);
        tree_file.close();
    }

    void deleteNode(Node* node) {
        if (node == nullptr) return;
        if (node->type == INTERNAL_NODE) {
            auto* inode = static_cast<InternalNode<KeyType, ValueType>*>(node);
            for (Node* child : inode->children) {
                deleteNode(child);
            }
            delete inode;
        } else if (node->type == LEAF_NODE) {
            delete static_cast<LeafNode<KeyType, ValueType>*>(node);
        }
    }

    bool insert(Node* node,
                const KeyValue<KeyType, ValueType>& kv,
                KeyValue<KeyType, ValueType>& promotedKey,
                Node*& promotedChild) {
        if (node->type == LEAF_NODE) {
            auto* leafPtr = static_cast<LeafNode<KeyType, ValueType>*>(node);
            auto* leaf = readLeaf_at(leafPtr->fileOffset);
            if (!leaf->insert(kv)) {
                delete leaf;
                return false;
            }
            if (leaf->count > MAX_KEYS) {
                auto* newLeaf = leaf->split();
                leaf_file.seekp(0, std::ios::end);
                uint64_t offset = leaf_file.tellp();
                if (offset % BLOCK_SIZE != 0) {
                    leaf_file.seekp((offset + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE);
                    offset = leaf_file.tellp();
                }
                promotedKey = newLeaf->getFirstEntry();
                newLeaf->fileOffset = offset;
                newLeaf->prevOffset = leaf->fileOffset;
                newLeaf->nextOffset = leaf->nextOffset;
                promotedChild = newLeaf;

                if (leaf->nextOffset) {
                    auto* oldNext = readLeaf_at(leaf->nextOffset);
                    oldNext->prevOffset = newLeaf->fileOffset;
                    leaf_file.seekp(leaf->nextOffset);
                    oldNext->writeLeaf_leaf(leaf_file);
                    delete oldNext;
                } else {
                    leafTail = newLeaf->fileOffset;
                }
                leaf->nextOffset = newLeaf->fileOffset;
                updateLeaf(leaf);
                updateLeaf(newLeaf);
                writeLeafHeader();
                delete leaf;
                return true;
            } else {
                updateLeaf(leaf);
                delete leaf;
                return false;
            }
        } else {
            auto* inode = static_cast<InternalNode<KeyType, ValueType>*>(node);
            Node* child = inode->findChild(kv);
            KeyValue<KeyType, ValueType> childPromotedKey;
            Node* childPromotedChild = nullptr;
            bool need = insert(child, kv, childPromotedKey, childPromotedChild);
            if (!need) return false;
            int pos = u_bound(childPromotedKey, inode->keys);
            inode->insertKey(childPromotedKey, child, childPromotedChild, pos);
            if (inode->count > MAX_KEYS) {
                KeyValue<KeyType, ValueType> newPromoted;
                auto* newInt = inode->split(newPromoted);
                promotedKey = newPromoted;
                promotedChild = newInt;
                return true;
            }
            return false;
        }
    }

public:
    BPlusTree(const std::string& leaf_name, const std::string& tree_name)
        : leaf_name(leaf_name), tree_name(tree_name), root(nullptr), leafHead(0), leafTail(0) {
        leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!leaf_file.is_open()) {
            leaf_file.open(leaf_name, std::ios::out | std::ios::binary);
            leaf_file.close();
            leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
            auto* rootLeaf = new LeafNode<KeyType, ValueType>();
            root = rootLeaf;
            allocateLeaf(rootLeaf);
            writeLeafHeader();
        } else {
            readLeafHeader();
            if (leafHead == 0) {
                auto* rootLeaf = new LeafNode<KeyType, ValueType>();
                root = rootLeaf;
                allocateLeaf(rootLeaf);
                writeLeafHeader();
            } else {
                loadTree();
            }
        }
    }

    ~BPlusTree() {
        if (root) {
            saveTree();
            deleteNode(root);
        }
        leaf_file.close();
    }

    sjtu::vector<ValueType> find(const KeyType& key) {
        sjtu::vector<ValueType> result;
        KeyValue<KeyType, ValueType> lower{key, std::numeric_limits<ValueType>::lowest()};
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            cur = static_cast<InternalNode<KeyType, ValueType>*>(cur)->findChild(lower);
        }
        auto* leafPtr = static_cast<LeafNode<KeyType, ValueType>*>(cur);
        uint64_t cur_off = leafPtr->fileOffset;
        while (cur_off != 0) {
            auto* node = readLeaf_at(cur_off);
            if (node->count > 0 && node->entries[0].key > key) {
                delete node;
                break;
            }
            for (const auto& e : node->entries) {
                if (e.key == key) result.push_back(e.value);
                else if (e.key > key) break;
            }
            cur_off = node->nextOffset;
            delete node;
        }
        return result;
    }

    void insert(const KeyType& key, const ValueType& value) {
        KeyValue<KeyType, ValueType> kv{key, value};
        KeyValue<KeyType, ValueType> promotedKey;
        Node* promotedChild = nullptr;
        if (insert(root, kv, promotedKey, promotedChild)) {
            auto* newRoot = new InternalNode<KeyType, ValueType>();
            newRoot->children.push_back(root);
            newRoot->keys.push_back(promotedKey);
            newRoot->children.push_back(promotedChild);
            newRoot->count = 1;
            root = newRoot;
        }
    }

    bool remove(const KeyType& key, const ValueType& value) {
        KeyValue<KeyType, ValueType> kv{key, value};
        if (root == nullptr) return false;

        // 根为叶子节点
        if (root->type == LEAF_NODE) {
            auto* leafPtr = static_cast<LeafNode<KeyType, ValueType>*>(root);
            auto* leaf = readLeaf_at(leafPtr->fileOffset);
            bool removed = leaf->remove(kv);
            if (removed) updateLeaf(leaf);
            delete leaf;
            return removed;
        }

        // 向下搜索并记录路径
        sjtu::vector<InternalNode<KeyType, ValueType>*> path;
        sjtu::vector<int> indices;
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            auto* in = static_cast<InternalNode<KeyType, ValueType>*>(cur);
            int idx = u_bound(kv, in->keys);
            path.push_back(in);
            indices.push_back(idx);
            cur = in->children[idx];
        }
        auto* leafPtr = static_cast<LeafNode<KeyType, ValueType>*>(cur);
        auto* leaf = readLeaf_at(leafPtr->fileOffset);
        if (!leaf->remove(kv)) {
            delete leaf;
            return false;
        }
        updateLeaf(leaf);

        Node* cur_node = leaf;
        bool is_leaf = true;
        bool leaf_deleted = false;

        while (true) {
            if (is_leaf) {
                auto* curLeaf = static_cast<LeafNode<KeyType, ValueType>*>(cur_node);
                if (curLeaf->count >= MIN_KEYS || path.empty()) break;

                auto* parent = path.back();
                int idx = indices.back();

                // 向左兄弟借
                if (idx > 0) {
                    auto* leftPtr = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    auto* leftLeaf = readLeaf_at(leftPtr->fileOffset);
                    if (leftLeaf->count > MIN_KEYS) {
                        KeyValue<KeyType, ValueType> borrowed = leftLeaf->entries.back();
                        leftLeaf->entries.pop_back();
                        leftLeaf->count--;
                        curLeaf->entries.insert(curLeaf->entries.begin(), borrowed);
                        curLeaf->count++;
                        parent->keys[idx-1] = curLeaf->entries[0];
                        updateLeaf(leftLeaf);
                        updateLeaf(curLeaf);
                        delete leftLeaf;
                        break;
                    }
                    delete leftLeaf;
                }

                // 向右兄弟借
                if (idx < parent->count) {
                    auto* rightPtr = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    auto* rightLeaf = readLeaf_at(rightPtr->fileOffset);
                    if (rightLeaf->count > MIN_KEYS) {
                        KeyValue<KeyType, ValueType> borrowed = rightLeaf->entries.front();
                        rightLeaf->entries.erase(rightLeaf->entries.begin());
                        rightLeaf->count--;
                        curLeaf->entries.push_back(borrowed);
                        curLeaf->count++;
                        parent->keys[idx] = rightLeaf->entries[0];
                        updateLeaf(rightLeaf);
                        updateLeaf(curLeaf);
                        delete rightLeaf;
                        break;
                    }
                    delete rightLeaf;
                }

                // 合并
                if (idx > 0) {   // 与左兄弟合并
                    auto* leftPtr = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    auto* leftLeaf = readLeaf_at(leftPtr->fileOffset);
                    for (auto& e : curLeaf->entries)
                        leftLeaf->entries.push_back(e);
                    leftLeaf->count += curLeaf->count;
                    leftLeaf->nextOffset = curLeaf->nextOffset;
                    if (curLeaf->nextOffset != 0) {
                        auto* nextLeaf = readLeaf_at(curLeaf->nextOffset);
                        nextLeaf->prevOffset = leftLeaf->fileOffset;
                        updateLeaf(nextLeaf);
                        delete nextLeaf;
                    } else {
                        leafTail = leftLeaf->fileOffset;
                        writeLeafHeader();
                    }
                    parent->keys.erase(parent->keys.begin() + idx - 1);
                    parent->children.erase(parent->children.begin() + idx);
                    parent->count--;
                    updateLeaf(leftLeaf);
                    delete leftLeaf;
                    delete curLeaf;
                    delete leafPtr;
                    leaf_deleted = true;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {        // 与右兄弟合并
                    auto* rightPtr = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    auto* rightLeaf = readLeaf_at(rightPtr->fileOffset);
                    for (auto& e : rightLeaf->entries)
                        curLeaf->entries.push_back(e);
                    curLeaf->count += rightLeaf->count;
                    curLeaf->nextOffset = rightLeaf->nextOffset;
                    if (rightLeaf->nextOffset != 0) {
                        auto* nextLeaf = readLeaf_at(rightLeaf->nextOffset);
                        nextLeaf->prevOffset = curLeaf->fileOffset;
                        updateLeaf(nextLeaf);
                        delete nextLeaf;
                    } else {
                        leafTail = curLeaf->fileOffset;
                        writeLeafHeader();
                    }
                    parent->keys.erase(parent->keys.begin() + idx);
                    parent->children.erase(parent->children.begin() + idx + 1);
                    parent->count--;
                    updateLeaf(curLeaf);
                    delete rightLeaf;
                    delete rightPtr;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                }
            } else {
                auto* curInt = static_cast<InternalNode<KeyType, ValueType>*>(cur_node);
                if (curInt->count >= MIN_KEYS || path.empty()) break;

                auto* parent = path.back();
                int idx = indices.back();

                // 向左兄弟借
                if (idx > 0) {
                    auto* leftSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    if (leftSib->count > MIN_KEYS) {
                        curInt->keys.insert(curInt->keys.begin(), parent->keys[idx-1]);
                        curInt->children.insert(curInt->children.begin(), leftSib->children.back());
                        leftSib->children.pop_back();
                        parent->keys[idx-1] = leftSib->keys.back();
                        leftSib->keys.pop_back();
                        curInt->count++;
                        leftSib->count--;
                        break;
                    }
                }

                // 向右兄弟借
                if (idx < parent->count) {
                    auto* rightSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    if (rightSib->count > MIN_KEYS) {
                        curInt->keys.push_back(parent->keys[idx]);
                        curInt->children.push_back(rightSib->children.front());
                        rightSib->children.erase(rightSib->children.begin());
                        parent->keys[idx] = rightSib->keys.front();
                        rightSib->keys.erase(rightSib->keys.begin());
                        curInt->count++;
                        rightSib->count--;
                        break;
                    }
                }

                // 合并
                if (idx > 0) {   // 与左兄弟合并
                    auto* leftSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    leftSib->keys.push_back(parent->keys[idx-1]);
                    for (auto& k : curInt->keys) leftSib->keys.push_back(k);
                    for (auto& c : curInt->children) leftSib->children.push_back(c);
                    leftSib->count += 1 + curInt->count;
                    parent->keys.erase(parent->keys.begin() + idx - 1);
                    parent->children.erase(parent->children.begin() + idx);
                    parent->count--;
                    delete curInt;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {          // 与右兄弟合并
                    auto* rightSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    curInt->keys.push_back(parent->keys[idx]);
                    for (auto& k : rightSib->keys) curInt->keys.push_back(k);
                    for (auto& c : rightSib->children) curInt->children.push_back(c);
                    curInt->count += 1 + rightSib->count;
                    parent->keys.erase(parent->keys.begin() + idx);
                    parent->children.erase(parent->children.begin() + idx + 1);
                    parent->count--;
                    delete rightSib;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                }
            }
        }

        if (!leaf_deleted) delete leaf;
        // 根节点收缩
        if (root->type == INTERNAL_NODE && static_cast<InternalNode<KeyType, ValueType>*>(root)->count == 0) {
            Node* newRoot = static_cast<InternalNode<KeyType, ValueType>*>(root)->children[0];
            delete root;
            root = newRoot;
        }
        return true;
    }
};

#endif // TICKET_SYSTEM_BPT_H