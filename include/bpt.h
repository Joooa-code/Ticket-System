#ifndef TICKET_SYSTEM_BPT_H
#define TICKET_SYSTEM_BPT_H
#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>

const int MAX_KEYS = 40;
const int MIN_KEYS = MAX_KEYS / 2;
const size_t BLOCK_SIZE = 4096;          // 磁盘块大小

// 结点类型
enum NodeType : char {
    INTERNAL_NODE = 'I',
    LEAF_NODE = 'L'
};

// 辅助函数
void writeInt(std::ostream& out, int v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(int));
}
void writeUInt64(std::ostream& out, uint64_t v) {
    out.write(reinterpret_cast<const char*>(&v), sizeof(uint64_t));
}
// length + c_string
void writeString(std::ostream& out, const std::string& s) {
    uint8_t len = static_cast<uint8_t>(s.size());
    out.write(reinterpret_cast<const char*>(&len), 1);
    out.write(s.c_str(), len);
}
int readInt(std::istream& in) {
    int v; in.read(reinterpret_cast<char*>(&v), sizeof(int)); return v;
}
uint64_t readUInt64(std::istream& in) {
    uint64_t v; in.read(reinterpret_cast<char*>(&v), sizeof(uint64_t)); return v;
}
std::string readString(std::istream& in) {
    uint8_t len; in.read(reinterpret_cast<char*>(&len), 1);
    std::string s(len, '\0'); in.read(&s[0], len); return s;
}

// 键值对
struct KeyValue {
    std::string key;
    int value;
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

// 寻找第一个 >= kv的位置
int l_bound(const KeyValue& kv, const sjtu::vector<KeyValue>& vec) {
    int low = 0, high = vec.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (vec[mid] < kv) low = mid + 1;
        else high = mid;
    }
    return low;
}
// 寻找第一个 > kv的位置
int u_bound(const KeyValue& kv, const sjtu::vector<KeyValue>& vec) {
    int low = 0, high = vec.size();
    while (low < high) {
        int mid = low + (high - low) / 2;
        if (vec[mid] <= kv) low = mid + 1;
        else high = mid;
    }
    return low;
}
// 结点基类
class Node {
public:
    NodeType type;  // 结点类型
    int count;  // 条目数
    Node(NodeType t, int c = 0) : type(t), count(c) {}
    virtual ~Node() = default;
};

// 叶子结点
class LeafNode : public Node {
public:
    sjtu::vector<KeyValue> entries;
    uint64_t fileOffset;       // 在叶子文件中的偏移（块号 * BLOCK_SIZE）
    uint64_t prevOffset;       // 前驱叶子节点偏移
    uint64_t nextOffset;       // 后继叶子节点偏移
    LeafNode() : Node(LEAF_NODE, 0), fileOffset(0), prevOffset(0), nextOffset(0) {}

    // 向叶子文件中写入叶子结点
    void writeLeaf_leaf(std::ostream& out) const {
        writeInt(out, count);
        writeUInt64(out, prevOffset);
        writeUInt64(out, nextOffset);
        for (int i = 0; i < count; ++i) {
            writeString(out, entries[i].key);
            writeInt(out, entries[i].value);
        }
    }
    // 向树文件内写入叶子结点
    void writeLeaf_tree(std::ostream& out) const {
        char type = LEAF_NODE;
        out.write(&type, 1);
        writeUInt64(out, fileOffset);
    }
    // 从叶子文件读入叶子结点
    void readLeaf_leaf(std::istream& in) {
        fileOffset = in.tellg();
        count = readInt(in);
        prevOffset = readUInt64(in);
        nextOffset = readUInt64(in);
        entries.clear();
        for (int i = 0; i < count; ++i) {
            KeyValue kv;
            kv.key = readString(in);
            kv.value = readInt(in);
            entries.push_back(kv);
        }
    }
    // 只检查重复不检查溢出的插入
    bool insert(const KeyValue& kv) {
        int pos = l_bound(kv, entries);
        if (pos < count && entries[pos] == kv) return false;
        entries.insert(entries.begin() + pos, kv);
        ++count;
        return true;
    }
    // 删除，若失败(未找到)返回false
    bool remove(const KeyValue& kv) {
        int pos = l_bound(kv, entries);
        if (pos >= count || !(entries[pos] == kv)) return false;
        entries.erase(entries.begin() + pos);
        --count;
        return true;
    }
    // 不更新所有offset的分裂
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
    KeyValue getFirstEntry() const {
        if (count == 0) return KeyValue{"", 0};
        return entries[0];
    }
};

// 内部结点
class InternalNode : public Node {
public:
    sjtu::vector<KeyValue> keys;  // 储存key+value
    sjtu::vector<Node*> children;   // 孩子指针
    InternalNode() : Node(INTERNAL_NODE, 0) {}

    // 寻找可能存在kv的结点
    Node* findChild(const KeyValue& kv) const {
        return children[u_bound(kv, keys)];
    }
    // 在pos位置加入一个kv
    void insertKey(const KeyValue& kv, Node* left, Node* right, int pos) {
        keys.insert(keys.begin() + pos, kv);
        children[pos] = left;
        children.insert(children.begin() + pos + 1, right);
        ++count;
    }
    // 分裂内部结点，新的根存入p_key
    InternalNode* split(KeyValue& p_key) {
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
class BPlusTree {
private:
    std::fstream leaf_file;  // 叶子结点文件
    std::fstream tree_file;  // 树文件
    std::string leaf_name;  // 叶子文件名
    std::string tree_name;  // 树文件名
    Node* root;  // 树根
    uint64_t leafHead;  // 叶子链表头
    uint64_t leafTail;  // 叶子链表尾

    // 写入叶子文件头：leafhead+leaftail(4096字节)
    void writeLeafHeader() {
        leaf_file.seekp(0);
        writeUInt64(leaf_file, leafHead);
        writeUInt64(leaf_file, leafTail);
        leaf_file.seekp(4096);
    }
    // 读入叶子文件头
    void readLeafHeader() {
        leaf_file.seekg(0);
        leafHead = readUInt64(leaf_file);
        leafTail = readUInt64(leaf_file);
        leaf_file.seekg(4096);
    }
    // 从文件offset处读入LeafNode
    LeafNode* readLeaf_at(uint64_t offset) {
        leaf_file.seekg(offset);
        LeafNode* leaf = new LeafNode();
        leaf->readLeaf_leaf(leaf_file);
        return leaf;
    }  // 完整版
    LeafNode* readLeaf_at2(uint64_t offset) {
    	LeafNode* leaf = new LeafNode();
    	leaf->fileOffset = offset;
    	return leaf;
    }  // 内存中部分
    // 分配新叶子节点
    uint64_t allocateLeaf(LeafNode* leaf) {
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
    // 更新文件中的叶子节点
    void updateLeaf(LeafNode* leaf) {
        if (leaf->fileOffset == 0)
            allocateLeaf(leaf);
        else {
            leaf_file.seekp(leaf->fileOffset);
            leaf->writeLeaf_leaf(leaf_file);
            leaf_file.flush();
        }
    }
    // 递归将树写入树文件（前序遍历）
    void writeTree(Node* node, std::ostream& out) {
        if (node->type == LEAF_NODE) {
            LeafNode* leaf = static_cast<LeafNode*>(node);
            uint64_t leafOff = leaf->fileOffset;
            out.write(reinterpret_cast<const char*>(&leaf->type), 1);
            writeUInt64(out, leafOff);
            return;
        }
        InternalNode* inode = static_cast<InternalNode*>(node);
        out.write(reinterpret_cast<const char*>(&inode->type), 1);
        writeInt(out, inode->count);
        for (int i = 0; i < inode->count; ++i) {
            writeString(out, inode->keys[i].key);
            writeInt(out, inode->keys[i].value);
        }
        for (size_t i = 0; i < inode->children.size(); ++i) {
            writeTree(inode->children[i], out);
        }
    }
    // 保存树到内存
    void saveTree() {
        tree_file.open(tree_name, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!tree_file.is_open()) return;
        writeTree(root, tree_file);
        tree_file.close();
    }
    // 从树文件递归重建树
    Node* readTree(std::istream& in) {
        char type;
        in.read(&type, 1);
        if (type == LEAF_NODE) {
            uint64_t off = readUInt64(in);
            return readLeaf_at2(off);
        }
        else if (type == INTERNAL_NODE) {
            InternalNode* inode = new InternalNode();
            inode->count = readInt(in);
            for (int i = 0; i < inode->count; ++i) {
                KeyValue kv;
                kv.key = readString(in);
                kv.value = readInt(in);
                inode->keys.push_back(kv);
            }
            // 子节点
            for (int i = 0; i <= inode->count; ++i) {
                inode->children.push_back(readTree(in));
            }
            return inode;
        }
        return nullptr;
    }
    // 从外存读入树
    void loadTree() {
        tree_file.open(tree_name, std::ios::in | std::ios::binary);
        if (!tree_file.is_open()) return;
        root = readTree(tree_file);
        tree_file.close();
    }
    // 递归删除整棵树
    void deleteNode(Node* node) {
        if (node == nullptr) return;
        if (node->type == INTERNAL_NODE) {
            InternalNode* inode = static_cast<InternalNode*>(node);
            for (Node* child : inode->children) {
                deleteNode(child);
            }
            delete inode;
        }
        else if (node->type == LEAF_NODE) {
            delete static_cast<LeafNode*>(node);
        }
    }
    bool insert(Node* node, const KeyValue& kv, KeyValue& promotedKey, Node*& promotedChild) {
        if (node->type == LEAF_NODE) {
            LeafNode* leaf = static_cast<LeafNode*>(node);
        	LeafNode* fullLeaf = readLeaf_at(leaf->fileOffset);
        	if (!fullLeaf->insert(kv)) {
        		delete fullLeaf;
        		return false;
        	}
            if (fullLeaf->count > MAX_KEYS) {
                LeafNode* newLeaf = fullLeaf->split();
                leaf_file.seekp(0, std::ios::end);
                uint64_t offset = leaf_file.tellp();
                // 确保偏移是 BLOCK_SIZE 的整数倍
                if (offset % BLOCK_SIZE != 0) {
                    leaf_file.seekp((offset + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE);
                    offset = leaf_file.tellp();
                }
                promotedKey = newLeaf->getFirstEntry();
                // 维护双向链表
                newLeaf->fileOffset = offset;
                newLeaf->prevOffset = fullLeaf->fileOffset;
                newLeaf->nextOffset = fullLeaf->nextOffset;
                promotedChild = newLeaf;
                if (fullLeaf->nextOffset) {
                    LeafNode* oldNext = readLeaf_at(fullLeaf->nextOffset);
                    oldNext->prevOffset = newLeaf->fileOffset;
                    leaf_file.seekp(fullLeaf->nextOffset);
                    oldNext->writeLeaf_leaf(leaf_file);
                    delete oldNext;
                }
                else leafTail = newLeaf->fileOffset;
                fullLeaf->nextOffset = newLeaf->fileOffset;
                updateLeaf(fullLeaf);
                updateLeaf(newLeaf);
                writeLeafHeader();
            	delete fullLeaf;
                return true;
            }
            else {
                updateLeaf(fullLeaf);
            	delete fullLeaf;
                return false;
            }
        }
        else {
            InternalNode* inode = static_cast<InternalNode*>(node);
            Node* child = inode->findChild(kv);
            KeyValue childPromotedKey;
            Node* childPromotedChild = nullptr;
            bool need = insert(child, kv, childPromotedKey, childPromotedChild);
            if (!need) return false;
            int pos = u_bound(childPromotedKey, inode->keys);
            inode->insertKey(childPromotedKey, child, childPromotedChild, pos);
            if (inode->count > MAX_KEYS) {
                KeyValue newPromoted;
                InternalNode* newInt = inode->split(newPromoted);
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
        // 打开叶子文件
        leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!leaf_file.is_open()) {
            leaf_file.open(leaf_name, std::ios::out | std::ios::binary);
            leaf_file.close();
            leaf_file.open(leaf_name, std::ios::in | std::ios::out | std::ios::binary);
            // 初始化空树
            LeafNode* rootLeaf = new LeafNode();
            root = rootLeaf;
            allocateLeaf(rootLeaf);
            writeLeafHeader();
        }
        else {
            readLeafHeader();
            if (leafHead == 0) {
                LeafNode* rootLeaf = new LeafNode();
                root = rootLeaf;
                allocateLeaf(rootLeaf);
                writeLeafHeader();
            }
            else {
                loadTree();
            }
        }
    }
    ~BPlusTree() {
        if (root) {
            saveTree();   // 保存树
            deleteNode(root);
        }
        leaf_file.close();
    }
    sjtu::vector<int> find(const std::string& key) {
        sjtu::vector<int> result;
        KeyValue lower{key, -2147483648};
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            cur = static_cast<InternalNode*>(cur)->findChild(lower);
        }
        LeafNode* leaf = static_cast<LeafNode*>(cur);
        uint64_t cur_off = leaf->fileOffset;
        while (cur_off != 0) {
            LeafNode* node = readLeaf_at(cur_off);
            // 若当前节点的第一个键大于 key，停止
            if (node->count > 0 && node->entries[0].key > key) {
                delete node;
                break;
            }
            // 收集value
            for (const auto& e : node->entries) {
                if (e.key == key) result.push_back(e.value);
                else if (e.key > key) break;
            }
            cur_off = node->nextOffset;
            delete node;
        }
        return result;
    }
    void insert(const std::string& key, int value) {
        KeyValue kv{key, value};
        KeyValue promotedKey;
        Node* promotedChild = nullptr;
        if (insert(root, kv, promotedKey, promotedChild)) {
            InternalNode* newRoot = new InternalNode();
            newRoot->children.push_back(root);
            newRoot->keys.push_back(promotedKey);
            newRoot->children.push_back(promotedChild);
            newRoot->count = 1;
            root = newRoot;
        }
    }
    bool remove(const std::string& key, int value) {
        KeyValue kv{key, value};
        if (root == nullptr) return false;
        // 根为叶节点：直接删除
        if (root->type == LEAF_NODE) {
            LeafNode* leaf_ptr = static_cast<LeafNode*>(root);
            LeafNode* leaf = readLeaf_at(leaf_ptr->fileOffset);
            bool removed = leaf->remove(kv);  // 记录是否找到
            if (removed) updateLeaf(leaf);
            delete leaf;
            return removed;
        }
        // 向下搜索并记录路径
        sjtu::vector<InternalNode*> path;
        sjtu::vector<int> indices;
        Node* cur = root;
        while (cur->type == INTERNAL_NODE) {
            InternalNode* in = static_cast<InternalNode*>(cur);
            int idx = u_bound(kv, in->keys);
            path.push_back(in);
            indices.push_back(idx);
            cur = in->children[idx];
        }
        LeafNode* leafPtr = static_cast<LeafNode*>(cur);
        LeafNode* leaf = readLeaf_at(leafPtr->fileOffset);  // 完整叶节点
        if (!leaf->remove(kv)) {
            delete leaf;
            return false;
        }
        updateLeaf(leaf);
        Node* cur_node = leaf;
        bool is_leaf = true;
        // 用于内存清理：记录叶节点是否已被合并删除
        LeafNode* leaf_to_delete = leaf;
        bool leaf_deleted = false;
        while (true) {
            if (is_leaf) {
                LeafNode* curLeaf = static_cast<LeafNode*>(cur_node);
                if (curLeaf->count >= MIN_KEYS || path.empty()) break;
                InternalNode* parent = path.back();
                int idx = indices.back();
                // 1. 向左兄弟借
                if (idx > 0) {
                    LeafNode* leftPtr = dynamic_cast<LeafNode*>(parent->children[idx-1]);
                    LeafNode* leftLeaf = readLeaf_at(leftPtr->fileOffset);
                    if (leftLeaf->count > MIN_KEYS) {
                        KeyValue borrowed = leftLeaf->entries.back();
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
                // 2. 向右兄弟借
                if (idx < parent->count) {
                    LeafNode* rightPtr = dynamic_cast<LeafNode*>(parent->children[idx+1]);
                    LeafNode* rightLeaf = readLeaf_at(rightPtr->fileOffset);
                    if (rightLeaf->count > MIN_KEYS) {
                        KeyValue borrowed = rightLeaf->entries.front();
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
                // 3. 合并
                if (idx > 0) {   // 与左兄弟合并
                    LeafNode* leftPtr = dynamic_cast<LeafNode*>(parent->children[idx-1]);
                    LeafNode* leftLeaf = readLeaf_at(leftPtr->fileOffset);
                    for (auto& e : curLeaf->entries)
                        leftLeaf->entries.push_back(e);
                    leftLeaf->count += curLeaf->count;
                    leftLeaf->nextOffset = curLeaf->nextOffset;
                    if (curLeaf->nextOffset != 0) {
                        LeafNode* nextLeaf = readLeaf_at(curLeaf->nextOffset);
                        nextLeaf->prevOffset = leftLeaf->fileOffset;
                        updateLeaf(nextLeaf);
                        delete nextLeaf;
                    } else {
                        leafTail = leftLeaf->fileOffset;
                        writeLeafHeader();
                    }
                    // 从父节点删除当前叶节点
                    parent->keys.erase(parent->keys.begin() + idx - 1);
                    parent->children.erase(parent->children.begin() + idx);
                    parent->count--;
                    updateLeaf(leftLeaf);
                    delete leftLeaf;
                    delete curLeaf;          // 完整叶节点
                    delete leafPtr;
                    leaf_deleted = true;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {        // 与右兄弟合并
                    LeafNode* rightPtr = dynamic_cast<LeafNode*>(parent->children[idx+1]);
                    LeafNode* rightLeaf = readLeaf_at(rightPtr->fileOffset);
                    for (auto& e : rightLeaf->entries)
                        curLeaf->entries.push_back(e);
                    curLeaf->count += rightLeaf->count;
                    curLeaf->nextOffset = rightLeaf->nextOffset;
                    if (rightLeaf->nextOffset != 0) {
                        LeafNode* nextLeaf = readLeaf_at(rightLeaf->nextOffset);
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
                InternalNode* curInt = static_cast<InternalNode*>(cur_node);
                if (curInt->count >= MIN_KEYS || path.empty()) break;

                InternalNode* parent = path.back();
                int idx = indices.back();

                // 1. 向左兄弟借
                if (idx > 0) {
                    InternalNode* leftSib = dynamic_cast<InternalNode*>(parent->children[idx-1]);
                    if (leftSib->count > MIN_KEYS) {
                        // 旋转
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
                // 2. 向右兄弟借
                if (idx < parent->count) {
                    InternalNode* rightSib = dynamic_cast<InternalNode*>(parent->children[idx+1]);
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
                // 3. 合并
                if (idx > 0) {   // 与左兄弟合并
                    InternalNode* leftSib = dynamic_cast<InternalNode*>(parent->children[idx-1]);
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
                    InternalNode* rightSib = dynamic_cast<InternalNode*>(parent->children[idx+1]);
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
        // 如果叶节点没有被合并删除，需要手动清理
        if (!leaf_deleted) delete leaf_to_delete;
        // 删除根，树的高度变矮
        if (root->type == INTERNAL_NODE && static_cast<InternalNode*>(root)->count == 0) {
            Node* newRoot = static_cast<InternalNode*>(root)->children[0];
            delete root;
            root = newRoot;
        }
        return true;
    }
};
#endif //TICKET_SYSTEM_BPT_H