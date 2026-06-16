#ifndef TICKET_SYSTEM_BPT_H
#define TICKET_SYSTEM_BPT_H

#include <iostream>
#include <fstream>
#include <cstdint>
#include "vector.h"

const int MAX_KEYS = 40;
const int MIN_KEYS = MAX_KEYS / 2;
const size_t BLOCK_SIZE = 4096;          // 磁盘块大小

enum NodeType : char {
    INTERNAL_NODE = 'I',
    LEAF_NODE = 'L'
};

// 键值对结构
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
int lower_bound(const KeyValue<KeyType, ValueType>& kv,
                const sjtu::vector<KeyValue<KeyType, ValueType>>& vec) {
    int lo = 0, hi = vec.size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (vec[mid] < kv) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

template<typename KeyType, typename ValueType>
int upper_bound(const KeyValue<KeyType, ValueType>& kv,
                const sjtu::vector<KeyValue<KeyType, ValueType>>& vec) {
    int lo = 0, hi = vec.size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (vec[mid] <= kv) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

class Node {
public:
    NodeType type;
    int count;
    Node(NodeType t, int c = 0) : type(t), count(c) {}
    virtual ~Node() = default;
};

// 叶子节点模板
template<typename KeyType, typename ValueType>
class LeafNode : public Node {
public:
    sjtu::vector<KeyValue<KeyType, ValueType>> entries;
    uint64_t fileOffset;       // 在叶子文件中的偏移
    uint64_t prevOffset;
    uint64_t nextOffset;

    LeafNode() : Node(LEAF_NODE, 0), fileOffset(0), prevOffset(0), nextOffset(0) {}

    // 写入叶子文件（完整存储）
    void writeLeaf(std::ostream& out) const {
        out.write(reinterpret_cast<const char*>(&count), sizeof(int));
        out.write(reinterpret_cast<const char*>(&prevOffset), sizeof(uint64_t));
        out.write(reinterpret_cast<const char*>(&nextOffset), sizeof(uint64_t));
        for (int i = 0; i < count; ++i) {
            out.write(reinterpret_cast<const char*>(&entries[i].key), sizeof(KeyType));
            out.write(reinterpret_cast<const char*>(&entries[i].value), sizeof(ValueType));
        }
    }

    // 写入树文件（仅类型和偏移）
    void writeTreeRef(std::ostream& out) const {
        char type = LEAF_NODE;
        out.write(&type, 1);
        out.write(reinterpret_cast<const char*>(&fileOffset), sizeof(uint64_t));
    }

    // 从叶子文件读取
    void readLeaf(std::istream& in) {
        fileOffset = in.tellg();
        in.read(reinterpret_cast<char*>(&count), sizeof(int));
        in.read(reinterpret_cast<char*>(&prevOffset), sizeof(uint64_t));
        in.read(reinterpret_cast<char*>(&nextOffset), sizeof(uint64_t));
        entries.clear();
        for (int i = 0; i < count; ++i) {
            KeyValue<KeyType, ValueType> kv;
            in.read(reinterpret_cast<char*>(&kv.key), sizeof(KeyType));
            in.read(reinterpret_cast<char*>(&kv.value), sizeof(ValueType));
            entries.push_back(kv);
        }
    }

    // 插入，返回是否成功（不重复）
    bool insert(const KeyValue<KeyType, ValueType>& kv) {
        int pos = lower_bound(kv, entries);
        if (pos < count && entries[pos] == kv) return false;
        entries.insert(entries.begin() + pos, kv);
        ++count;
        return true;
    }

    // 删除，返回是否成功
    bool remove(const KeyValue<KeyType, ValueType>& kv) {
        int pos = lower_bound(kv, entries);
        if (pos >= count || !(entries[pos] == kv)) return false;
        entries.erase(entries.begin() + pos);
        --count;
        return true;
    }

    // 分裂，返回新节点
    LeafNode* split() {
        LeafNode* newNode = new LeafNode();
        int mid = count / 2;
        for (int i = mid; i < count; ++i)
            newNode->entries.push_back(entries[i]);
        newNode->count = newNode->entries.size();
        while ((int)entries.size() > mid)
            entries.pop_back();
        count = mid;
        return newNode;
    }

    KeyValue<KeyType, ValueType> getFirstEntry() const {
        if (count == 0) return KeyValue<KeyType, ValueType>{};
        return entries[0];
    }
};

// 内部节点模板（仅存在于内存，树文件中递归存储）
template<typename KeyType, typename ValueType>
class InternalNode : public Node {
public:
    sjtu::vector<KeyValue<KeyType, ValueType>> keys;
    sjtu::vector<Node*> children;

    InternalNode() : Node(INTERNAL_NODE, 0) {}

    Node* findChild(const KeyValue<KeyType, ValueType>& kv) const {
        return children[upper_bound(kv, keys)];
    }

    void insertKey(const KeyValue<KeyType, ValueType>& kv,
                   Node* left, Node* right, int pos) {
        keys.insert(keys.begin() + pos, kv);
        children[pos] = left;
        children.insert(children.begin() + pos + 1, right);
        ++count;
    }

    // 分裂，提升的键存入 p_key
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

// B+树主模板
template<typename KeyType, typename ValueType>
class BPlusTree {
private:
    std::fstream leaf_file;
    std::fstream tree_file;
    std::string leaf_name;
    std::string tree_name;
    Node* root;
    uint64_t leafHead;
    uint64_t leafTail;

    // 叶子文件头操作
    void writeLeafHeader() {
        leaf_file.seekp(0);
        leaf_file.write(reinterpret_cast<const char*>(&leafHead), sizeof(uint64_t));
        leaf_file.write(reinterpret_cast<const char*>(&leafTail), sizeof(uint64_t));
        // 填充至块大小（可选）
        leaf_file.seekp(BLOCK_SIZE);
    }
    void readLeafHeader() {
        leaf_file.seekg(0);
        leaf_file.read(reinterpret_cast<char*>(&leafHead), sizeof(uint64_t));
        leaf_file.read(reinterpret_cast<char*>(&leafTail), sizeof(uint64_t));
        leaf_file.seekg(BLOCK_SIZE);
    }

    // 从文件读取叶子节点
    LeafNode<KeyType, ValueType>* readLeafNode(uint64_t offset) {
        leaf_file.seekg(offset);
        LeafNode<KeyType, ValueType>* leaf = new LeafNode<KeyType, ValueType>();
        leaf->readLeaf(leaf_file);
        return leaf;
    }

    // 创建一个仅知道偏移的叶子节点占位符（用于树节点中的叶子引用）
    LeafNode<KeyType, ValueType>* createLeafStub(uint64_t offset) {
        LeafNode<KeyType, ValueType>* leaf = new LeafNode<KeyType, ValueType>();
        leaf->fileOffset = offset;
        return leaf;
    }

    // 分配新叶子节点（追加到叶子文件末尾，对齐块边界）
    uint64_t allocateLeaf(LeafNode<KeyType, ValueType>* leaf) {
        leaf_file.seekp(0, std::ios::end);
        uint64_t offset = leaf_file.tellp();
        if (offset % BLOCK_SIZE != 0) {
            offset = (offset + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
            leaf_file.seekp(offset);
        }
        leaf->fileOffset = offset;
        leaf->prevOffset = leafTail;
        leaf->nextOffset = 0;
        leaf->writeLeaf(leaf_file);
        leaf_file.flush();

        if (leafHead == 0) {
            leafHead = leafTail = offset;
        } else {
            // 更新原尾节点的 nextOffset
            LeafNode<KeyType, ValueType>* tail = readLeafNode(leafTail);
            tail->nextOffset = offset;
            leaf_file.seekp(leafTail);
            tail->writeLeaf(leaf_file);
            delete tail;
            leafTail = offset;
        }
        writeLeafHeader();
        return offset;
    }

    // 更新叶子节点
    void updateLeaf(LeafNode<KeyType, ValueType>* leaf) {
        if (leaf->fileOffset == 0)
            allocateLeaf(leaf);
        else {
            leaf_file.seekp(leaf->fileOffset);
            leaf->writeLeaf(leaf_file);
            leaf_file.flush();
        }
    }

    // 递归写入树文件
    void writeTree(Node* node, std::ostream& out) {
        if (node->type == LEAF_NODE) {
            auto* leaf = static_cast<LeafNode<KeyType, ValueType>*>(node);
            leaf->writeTreeRef(out);
            return;
        }
        auto* inode = static_cast<InternalNode<KeyType, ValueType>*>(node);
        out.write(reinterpret_cast<const char*>(&inode->type), 1);
        out.write(reinterpret_cast<const char*>(&inode->count), sizeof(int));
        for (int i = 0; i < inode->count; ++i) {
            out.write(reinterpret_cast<const char*>(&inode->keys[i].key), sizeof(KeyType));
            out.write(reinterpret_cast<const char*>(&inode->keys[i].value), sizeof(ValueType));
        }
        for (size_t i = 0; i < inode->children.size(); ++i) {
            writeTree(inode->children[i], out);
        }
    }

    // 递归读取树文件
    Node* readTree(std::istream& in) {
        char type;
        in.read(&type, 1);
        if (type == LEAF_NODE) {
            uint64_t off;
            in.read(reinterpret_cast<char*>(&off), sizeof(uint64_t));
            return createLeafStub(off);
        } else if (type == INTERNAL_NODE) {
            auto* inode = new InternalNode<KeyType, ValueType>();
            in.read(reinterpret_cast<char*>(&inode->count), sizeof(int));
            for (int i = 0; i < inode->count; ++i) {
                KeyValue<KeyType, ValueType> kv;
                in.read(reinterpret_cast<char*>(&kv.key), sizeof(KeyType));
                in.read(reinterpret_cast<char*>(&kv.value), sizeof(ValueType));
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
            for (Node* child : inode->children) deleteNode(child);
            delete inode;
        } else {
            delete static_cast<LeafNode<KeyType, ValueType>*>(node);
        }
    }

    // 插入辅助函数，返回是否需要提升
    bool insertHelper(Node* node, const KeyValue<KeyType, ValueType>& kv,
                      KeyValue<KeyType, ValueType>& promotedKey, Node*& promotedChild) {
        if (node->type == LEAF_NODE) {
            auto* leafStub = static_cast<LeafNode<KeyType, ValueType>*>(node);
            auto* leaf = readLeafNode(leafStub->fileOffset);
            if (!leaf->insert(kv)) {
                delete leaf;
                return false;
            }
            if (leaf->count > MAX_KEYS) {
                auto* newLeaf = leaf->split();
                // 分配新叶子节点在文件中的位置
                leaf_file.seekp(0, std::ios::end);
                uint64_t offset = leaf_file.tellp();
                if (offset % BLOCK_SIZE != 0) {
                    offset = (offset + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
                    leaf_file.seekp(offset);
                }
                promotedKey = newLeaf->getFirstEntry();
                newLeaf->fileOffset = offset;
                newLeaf->prevOffset = leaf->fileOffset;
                newLeaf->nextOffset = leaf->nextOffset;
                promotedChild = newLeaf;  // 新节点在内存中

                // 更新链表
                if (leaf->nextOffset) {
                    auto* oldNext = readLeafNode(leaf->nextOffset);
                    oldNext->prevOffset = newLeaf->fileOffset;
                    leaf_file.seekp(leaf->nextOffset);
                    oldNext->writeLeaf(leaf_file);
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
            bool need = insertHelper(child, kv, childPromotedKey, childPromotedChild);
            if (!need) return false;
            int pos = upper_bound(childPromotedKey, inode->keys);
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
    BPlusTree(const char* leaf_file_name, const char* tree_file_name)
        : leaf_name(leaf_file_name), tree_name(tree_file_name), root(nullptr), leafHead(0), leafTail(0) {
        leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!leaf_file.is_open()) {
            leaf_file.open(leaf_name, std::ios::out | std::ios::binary);
            leaf_file.close();
            leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
            // 初始化空树
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

    // 查找单个 key，返回所有匹配的 value
    sjtu::vector<ValueType> find(const KeyType& key) {
        sjtu::vector<ValueType> result;
        KeyValue<KeyType, ValueType> lower{key, ValueType{}};
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            cur = static_cast<InternalNode<KeyType, ValueType>*>(cur)->findChild(lower);
        }
        auto* leafStub = static_cast<LeafNode<KeyType, ValueType>*>(cur);
        uint64_t curOff = leafStub->fileOffset;
        while (curOff != 0) {
            auto* leaf = readLeafNode(curOff);
            if (leaf->count > 0 && leaf->entries[0].key > key) {
                delete leaf;
                break;
            }
            for (const auto& e : leaf->entries) {
                if (e.key == key) result.push_back(e.value);
                else if (e.key > key) break;
            }
            curOff = leaf->nextOffset;
            delete leaf;
        }
        return result;
    }

    // 插入
    void insert(const KeyType& key, const ValueType& value) {
        KeyValue<KeyType, ValueType> kv{key, value};
        KeyValue<KeyType, ValueType> promotedKey;
        Node* promotedChild = nullptr;
        if (insertHelper(root, kv, promotedKey, promotedChild)) {
            auto* newRoot = new InternalNode<KeyType, ValueType>();
            newRoot->children.push_back(root);
            newRoot->keys.push_back(promotedKey);
            newRoot->children.push_back(promotedChild);
            newRoot->count = 1;
            root = newRoot;
        }
    }

    // 删除（完整实现，包含借用和合并）
    bool remove(const KeyType& key, const ValueType& value) {
        KeyValue<KeyType, ValueType> kv{key, value};
        if (root == nullptr) return false;

        // 根为叶子节点
        if (root->type == LEAF_NODE) {
            auto* leafStub = static_cast<LeafNode<KeyType, ValueType>*>(root);
            auto* leaf = readLeafNode(leafStub->fileOffset);
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
            int idx = upper_bound(kv, in->keys);
            path.push_back(in);
            indices.push_back(idx);
            cur = in->children[idx];
        }
        auto* leafStub = static_cast<LeafNode<KeyType, ValueType>*>(cur);
        auto* leaf = readLeafNode(leafStub->fileOffset);
        if (!leaf->remove(kv)) {
            delete leaf;
            return false;
        }
        updateLeaf(leaf);

        // 处理下溢
        bool isLeaf = true;
        Node* curNode = leaf;
        bool leafDeleted = false;

        while (true) {
            if (isLeaf) {
                auto* curLeaf = static_cast<LeafNode<KeyType, ValueType>*>(curNode);
                if (curLeaf->count >= MIN_KEYS || path.empty()) break;

                auto* parent = path.back();
                int idx = indices.back();

                // 尝试向左兄弟借用
                if (idx > 0) {
                    auto* leftStub = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    auto* leftLeaf = readLeafNode(leftStub->fileOffset);
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

                // 尝试向右兄弟借用
                if (idx < parent->count) {
                    auto* rightStub = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    auto* rightLeaf = readLeafNode(rightStub->fileOffset);
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
                if (idx > 0) {
                    // 与左兄弟合并
                    auto* leftStub = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    auto* leftLeaf = readLeafNode(leftStub->fileOffset);
                    for (const auto& e : curLeaf->entries)
                        leftLeaf->entries.push_back(e);
                    leftLeaf->count += curLeaf->count;
                    leftLeaf->nextOffset = curLeaf->nextOffset;
                    if (curLeaf->nextOffset != 0) {
                        auto* nextLeaf = readLeafNode(curLeaf->nextOffset);
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
                    delete leafStub;
                    leafDeleted = true;
                    curNode = parent;
                    isLeaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {
                    // 与右兄弟合并
                    auto* rightStub = dynamic_cast<LeafNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    auto* rightLeaf = readLeafNode(rightStub->fileOffset);
                    for (const auto& e : rightLeaf->entries)
                        curLeaf->entries.push_back(e);
                    curLeaf->count += rightLeaf->count;
                    curLeaf->nextOffset = rightLeaf->nextOffset;
                    if (rightLeaf->nextOffset != 0) {
                        auto* nextLeaf = readLeafNode(rightLeaf->nextOffset);
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
                    delete rightStub;
                    curNode = parent;
                    isLeaf = false;
                    path.pop_back();
                    indices.pop_back();
                }
            } else {
                auto* curInt = static_cast<InternalNode<KeyType, ValueType>*>(curNode);
                if (curInt->count >= MIN_KEYS || path.empty()) break;

                auto* parent = path.back();
                int idx = indices.back();

                // 尝试向左兄弟借用
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

                // 尝试向右兄弟借用
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
                if (idx > 0) {
                    // 与左兄弟合并
                    auto* leftSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx-1]);
                    leftSib->keys.push_back(parent->keys[idx-1]);
                    for (const auto& k : curInt->keys) leftSib->keys.push_back(k);
                    for (auto* c : curInt->children) leftSib->children.push_back(c);
                    leftSib->count += 1 + curInt->count;
                    parent->keys.erase(parent->keys.begin() + idx - 1);
                    parent->children.erase(parent->children.begin() + idx);
                    parent->count--;
                    delete curInt;
                    curNode = parent;
                    isLeaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {
                    // 与右兄弟合并
                    auto* rightSib = dynamic_cast<InternalNode<KeyType, ValueType>*>(parent->children[idx+1]);
                    curInt->keys.push_back(parent->keys[idx]);
                    for (const auto& k : rightSib->keys) curInt->keys.push_back(k);
                    for (auto* c : rightSib->children) curInt->children.push_back(c);
                    curInt->count += 1 + rightSib->count;
                    parent->keys.erase(parent->keys.begin() + idx);
                    parent->children.erase(parent->children.begin() + idx + 1);
                    parent->count--;
                    delete rightSib;
                    curNode = parent;
                    isLeaf = false;
                    path.pop_back();
                    indices.pop_back();
                }
            }
        }

        if (!leafDeleted) delete leaf;
        // 根节点收缩
        if (root->type == INTERNAL_NODE && static_cast<InternalNode<KeyType, ValueType>*>(root)->count == 0) {
            Node* newRoot = static_cast<InternalNode<KeyType, ValueType>*>(root)->children[0];
            delete root;
            root = newRoot;
        }
        return true;
    }

    // 范围查询
    void rangeQuery(const KeyType& startKey, const KeyType& endKey,
                    sjtu::vector<KeyValue<KeyType, ValueType>>& result) {
        KeyValue<KeyType, ValueType> lower{startKey, ValueType{}};
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            cur = static_cast<InternalNode<KeyType, ValueType>*>(cur)->findChild(lower);
        }
        auto* leafStub = static_cast<LeafNode<KeyType, ValueType>*>(cur);
        uint64_t curOff = leafStub->fileOffset;
        while (curOff != 0) {
            auto* leaf = readLeafNode(curOff);
            bool beyond = false;
            for (const auto& e : leaf->entries) {
                if (e.key < startKey) continue;
                if (e.key > endKey) { beyond = true; break; }
                result.push_back(e);
            }
            delete leaf;
            if (beyond) break;
            curOff = leaf->nextOffset;
        }
    }
    bool empty() const {
        // 打开叶子文件
        std::ifstream in(leaf_name, std::ios::binary);
        if (!in.is_open()) return true;      // 文件无法打开，视为空
        if (leafHead == 0) return true;      // 无叶子节点，视为空
        in.seekg(leafHead);                  // 定位到第一个叶子节点的起始位置
        int count;
        in.read(reinterpret_cast<char*>(&count), sizeof(int));
        return count == 0;
    }
};

#endif // TICKET_SYSTEM_BPT_H