#ifndef TICKET_SYSTEM_BPT_H
#define TICKET_SYSTEM_BPT_H
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include "vector.h"

const size_t BLOCK_SIZE = 4096;          // 磁盘块大小
enum NodeType : char {
	INTERNAL_NODE = 'I',
	LEAF_NODE = 'L'
};
template<typename Key, typename Value>
class BPlusTree {
	struct KeyValue {
		Key key;
		Value value;
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
	// 计算叶子节点条目数
	static const int LEAF_MAX_ENTRIES =
		(BLOCK_SIZE - sizeof(char) - sizeof(int) - sizeof(uint64_t) * 3) / sizeof(KeyValue);


	struct LeafDisk {
		char type;
		int count;
		uint64_t fileOffset;
		uint64_t prev;
		uint64_t next;
		KeyValue entries[LEAF_MAX_ENTRIES];
	};
	class Node {
	public:
		NodeType type;
		int count;
		Node(NodeType t, int c = 0) : type(t), count(c) {}
		virtual ~Node() = default;
	};
	class LeafNode:public Node {
	public:
		uint64_t fileOffset;
		uint64_t prevOffset;             // 前驱叶子偏移
		uint64_t nextOffset;             // 后继叶子偏移
		KeyValue entries[LEAF_MAX_ENTRIES];
		int l_bound(const KeyValue& kv, const LeafNode* leaf) {
			int low = 0, high = leaf->count;
			while (low < high) {
				int mid = low + (high - low) / 2;
				if (leaf->entries[mid] < kv) low = mid + 1;
				else high = mid;
			}
			return low;
		}
		LeafNode() : Node(LEAF_NODE, 0), fileOffset(0), prevOffset(0), nextOffset(0) {}
		// 检查重复的插入，如果溢出，将溢出的值存入tem
		bool insert(const KeyValue& kv, KeyValue& tem) {
			int pos = l_bound(kv, this);
			if (pos < this->count && entries[pos] == kv) {
				return false;
			}
			// 节点未满，直接插入
			if (this->count < LEAF_MAX_ENTRIES) {
				// 将 [pos, count-1] 的元素后移一位
				for (int i = this->count; i > pos; --i) {
					entries[i] = entries[i - 1];
				}
				entries[pos] = kv;
				this->count++;
				return false;  // 未溢出
			}
			if (pos == this->count) {
				tem = kv;
				return true;
			}
			else {
				tem = entries[this->count - 1];
				for (int i = this->count - 1; i > pos; --i) {
					entries[i] = entries[i - 1];
				}
				entries[pos] = kv;
				return true;
			}
		}
		// 删除，若失败(未找到)返回false
		bool remove(const KeyValue& kv) {
			// 二分查找第一个 >= kv 的位置
			int pos = l_bound(kv, this);
			if (pos < this->count && entries[pos] == kv) {
				for (int i = pos; i < this->count - 1; ++i) {
					entries[i] = entries[i + 1];
				}
				this->count--;
				return true;
			}
			return false;
		}
		// 不更新所有offset的分裂
		LeafNode* split() {
			LeafNode* newNode = new LeafNode();
			int mid = this->count / 2;
			for (int i = mid; i < this->count; ++i) {
				newNode->entries[i - mid] = this->entries[i];
			}
			newNode->count = this->count - mid;   // 右节点数量
			this->count = mid;                   // 左节点数量
			return newNode;
		}
		KeyValue getFirstEntry() const {
			if (this->count == 0) return KeyValue{Key(), Value()};
			return entries[0];
		}
	};

	// 内部结点条目数
	static const int INTERNAL_MAX_KEYS =
		(BLOCK_SIZE - sizeof(char) - sizeof(int)) / (sizeof(Key) + sizeof(Value));

	class InternalNode:public Node {
	public:
		KeyValue keys[INTERNAL_MAX_KEYS];
		Node* children[INTERNAL_MAX_KEYS + 1];
		InternalNode() : Node(INTERNAL_NODE, 0) {}
		// 寻找第一个 > kv的位置
		int u_bound(const KeyValue& kv, const InternalNode* node) {
			int low = 0, high = node->count;
			while (low < high) {
				int mid = low + (high - low) / 2;
				if (node->keys[mid] <= kv) low = mid + 1;
				else high = mid;
			}
			return low;
		}
		// 寻找可能存在kv的结点
		Node* findChild(const KeyValue& kv) {
			return children[u_bound(kv, this)];
		}
		// 在pos位置加入一个kv,将溢出的键值对存入tem,溢出的子指针存入temp,返回是否溢出
		bool insertKey(const KeyValue& kv, Node* left, Node* right, int pos, KeyValue& tem, Node*& temp) {
			if (this->count >= INTERNAL_MAX_KEYS) {
				if (pos == INTERNAL_MAX_KEYS) {
					children[INTERNAL_MAX_KEYS] = left;
					tem = kv;
					temp = right;
					return true;
				}
				else {
					tem = keys[INTERNAL_MAX_KEYS - 1];
					for (int i = INTERNAL_MAX_KEYS - 2; i >= pos; --i) {
						keys[i + 1] = keys[i];
					}
					keys[pos] = kv;
					temp = children[INTERNAL_MAX_KEYS];
					children[pos] = left;
					for (int i = INTERNAL_MAX_KEYS - 1; i > pos; --i) {
						children[i + 1] = children[i];
					}
					children[pos + 1] = right;
					return true;
				}
			}
			for (int i = this->count - 1; i >= pos; --i) {
				keys[i + 1] = keys[i];
			}
			keys[pos] = kv;
			children[pos] = left;
			for (int i = this->count; i > pos; --i) {
				children[i + 1] = children[i];
			}
			children[pos + 1] = right;
			++this->count;
			return false;
		}
		// 分裂内部结点，新的根存入p_key
		InternalNode* split(KeyValue& p_key) {
			InternalNode* newNode = new InternalNode();
			int mid = this->count / 2;
			p_key = keys[mid];
			for (int i = mid + 1; i < this->count; ++i) {
				newNode->keys[i - mid - 1] = keys[i];
			}
			for (int i = mid + 1; i <= this->count; ++i) {
				newNode->children[i - mid - 1] = children[i];
			}
			newNode->count = this->count - mid - 1;
			this->count = mid;
			return newNode;
		}
	};

	std::fstream leaf_file;  // 叶子结点文件
	std::fstream tree_file;  // 树文件
	std::string leaf_name;  // 叶子文件名
	std::string tree_name;  // 树文件名
	Node* root;  // 树根
	uint64_t leafHead;  // 叶子链表头
	uint64_t leafTail;  // 叶子链表尾

	// 向叶子文件写入叶子结点
	void writeLeaf_leaf(const LeafNode* node) {
		LeafDisk disk;
		disk.type = LEAF_NODE;
		disk.count = node->count;
		disk.fileOffset = node->fileOffset;
		disk.prev = node->prevOffset;
		disk.next = node->nextOffset;

		for (int i = 0; i < node->count; ++i)
			disk.entries[i] = node->entries[i];

		leaf_file.write(reinterpret_cast<char*>(&disk), sizeof(disk));
	}
	// 向树文件写入叶子结点
	void writeLeaf_tree(const LeafNode* node) {
		char type = LEAF_NODE;
		tree_file.write(&type, 1);
		tree_file.write(reinterpret_cast<const char*>(&node->fileOffset), sizeof(uint64_t));
	}
	// 从叶子文件中读入叶子结点
	void readLeaf_leaf(LeafNode* node) {
		LeafDisk disk;
		leaf_file.read(reinterpret_cast<char*>(&disk), sizeof(disk));
		node->fileOffset = disk.fileOffset;
		node->count = disk.count;
		node->prevOffset = disk.prev;
		node->nextOffset = disk.next;
		for (int i = 0; i < disk.count; ++i)
			node->entries[i] = disk.entries[i];
	}
	// 写入叶子文件头：leafhead+leaftail(4096字节)
	void writeLeafHeader() {
		leaf_file.seekp(0);
		leaf_file.write(reinterpret_cast<const char*>(&leafHead), sizeof(uint64_t));
		leaf_file.write(reinterpret_cast<const char*>(&leafTail), sizeof(uint64_t));
		leaf_file.seekp(4096);
	}
	// 读入叶子文件头
	void readLeafHeader() {
		leaf_file.seekg(0);
		leaf_file.read(reinterpret_cast<char*>(&leafHead), sizeof(uint64_t));
		leaf_file.read(reinterpret_cast<char*>(&leafTail), sizeof(uint64_t));
		leaf_file.seekg(4096);
	}
	// 从文件offset处读入完整的LeafNode
	LeafNode* readLeaf_at(uint64_t offset) {
		leaf_file.seekg(offset);
		LeafNode* leaf = new LeafNode();
		readLeaf_leaf(leaf);
		return leaf;
	}
	// 读入内存版LeafNode
	LeafNode* readLeaf_at2(uint64_t offset) {
		LeafNode* leaf = new LeafNode();
		leaf->fileOffset = offset;
		return leaf;
	}
	// 分配新叶子节点
	uint64_t allocateLeaf(LeafNode* leaf) {
		uint64_t offset = 4096;
		leaf_file.seekp(offset);
		leaf->fileOffset = offset;
		leaf->prevOffset = 0;
		leaf->nextOffset = 0;
		writeLeaf_leaf(leaf);
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
			writeLeaf_leaf(leaf);
			leaf_file.flush();
		}
	}
	// 递归将树写入树文件（前序遍历）
	void writeTree(Node* node) {
		if (node->type == LEAF_NODE) {
			LeafNode* leaf = static_cast<LeafNode*>(node);
			uint64_t leafOff = leaf->fileOffset;
			tree_file.write(reinterpret_cast<const char*>(&leaf->type), 1);
			tree_file.write(reinterpret_cast<const char*>(&leafOff), sizeof(uint64_t));
			return;
		}
		InternalNode* inode = static_cast<InternalNode*>(node);
		tree_file.write(reinterpret_cast<const char*>(&inode->type), 1);
		tree_file.write(reinterpret_cast<const char*>(&inode->count), 4);
		tree_file.write(reinterpret_cast<const char*>(inode->keys), sizeof(inode->keys));
		for (size_t i = 0; i < inode->count + 1; ++i) {
			writeTree(inode->children[i]);
		}
	}
	// 从树文件递归重建树
	Node* readTree() {
		char type;
		tree_file.read(&type, 1);
		if (type == LEAF_NODE) {
			uint64_t off;
			tree_file.read(reinterpret_cast<char*>(&off), 8);
			return readLeaf_at2(off);
		}
		else if (type == INTERNAL_NODE) {
			InternalNode* inode = new InternalNode();
			tree_file.read(reinterpret_cast<char*>(&inode->count), 4);
			tree_file.read(reinterpret_cast<char*>(inode->keys), sizeof(inode->keys));
			// 子节点
			for (int i = 0; i <= inode->count; ++i) {
				inode->children[i] = readTree();
			}
			return inode;
		}
		return nullptr;
	}
	// 保存树到内存
	void saveTree() {
		tree_file.open(tree_name, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!tree_file.is_open()) return;
		writeTree(root);
		tree_file.close();
	}
	// 从外存读入树
	void loadTree() {
		tree_file.open(tree_name, std::ios::in | std::ios::binary);
		if (!tree_file.is_open()) return;
		root = readTree();
		tree_file.close();
	}
	// 递归删除整棵树
	void deleteNode(Node* node) {
		if (node == nullptr) return;
		if (node->type == INTERNAL_NODE) {
			InternalNode* inode = static_cast<InternalNode*>(node);
			for (int i = 0; i <= inode->count; ++i) {
				deleteNode(inode->children[i]);
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
			KeyValue tem;
			bool overflow = fullLeaf->insert(kv, tem);
			if (!overflow) {
				updateLeaf(fullLeaf);
				delete fullLeaf;
				return false;
			}
			LeafNode* newLeaf = fullLeaf->split();
			newLeaf->entries[newLeaf->count] = tem;
			newLeaf->count++;
			promotedKey = newLeaf->getFirstEntry();
			leaf_file.seekp(0, std::ios::end);
			uint64_t offset = leaf_file.tellp();
			if (offset % BLOCK_SIZE != 0) {
				leaf_file.seekp((offset + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE);
				offset = leaf_file.tellp();
			}
			newLeaf->fileOffset = offset;
			newLeaf->prevOffset = fullLeaf->fileOffset;
			newLeaf->nextOffset = fullLeaf->nextOffset;
			promotedChild = newLeaf;
			if (fullLeaf->nextOffset) {
				LeafNode* oldNext = readLeaf_at(fullLeaf->nextOffset);
				oldNext->prevOffset = newLeaf->fileOffset;
				leaf_file.seekp(fullLeaf->nextOffset);
				writeLeaf_leaf(oldNext);
				delete oldNext;
			} else {
				leafTail = newLeaf->fileOffset;
			}
			fullLeaf->nextOffset = newLeaf->fileOffset;
			updateLeaf(fullLeaf);
			updateLeaf(newLeaf);
			writeLeafHeader();
			delete fullLeaf;
			return true;
		}
        else {
            InternalNode* inode = static_cast<InternalNode*>(node);
            Node* child = inode->findChild(kv);
            KeyValue childPromotedKey;
            Node* childPromotedChild = nullptr;
            bool need = insert(child, kv, childPromotedKey, childPromotedChild);
            if (!need) return false;
            int pos = inode->u_bound(childPromotedKey, inode);
        	KeyValue tem;
        	Node* temp;
            if (inode->insertKey(childPromotedKey, child, childPromotedChild, pos, tem, temp)) {
            	KeyValue newPromoted;
            	InternalNode* newInt = inode->split(newPromoted);
            	newInt->keys[newInt->count] = tem;
            	newInt->children[newInt->count + 1] = temp;
            	newInt->count++;
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
	sjtu::vector<Value> find(const Key& key) {
		sjtu::vector<Value> result;
		KeyValue lower{key, Value()};
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
			for (int i = 0; i < node->count; ++i) {
				if (node->entries[i].key == key) result.push_back(node->entries[i].value);
				else if (node->entries[i].key > key) break;
			}
			cur_off = node->nextOffset;
			delete node;
		}
		return result;
	}
	void insert(const Key& key, const Value& value) {
		KeyValue kv{key, value};
		KeyValue promotedKey;
		Node* promotedChild = nullptr;
		if (insert(root, kv, promotedKey, promotedChild)) {
			InternalNode* newRoot = new InternalNode();
			newRoot->children[0] = root;
			newRoot->keys[0] = promotedKey;
			newRoot->children[1] = promotedChild;
			newRoot->count = 1;
			root = newRoot;
		}
	}
	bool remove(const Key& key, const Value& value) {
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
            int idx = in->u_bound(kv, in);
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
                if (curLeaf->count >= LEAF_MAX_ENTRIES / 2 || path.empty()) break;
                InternalNode* parent = path.back();
                int idx = indices.back();
                // 1. 向左兄弟借
                if (idx > 0) {
                    LeafNode* leftPtr = dynamic_cast<LeafNode*>(parent->children[idx-1]);
                    LeafNode* leftLeaf = readLeaf_at(leftPtr->fileOffset);
                    if (leftLeaf->count > LEAF_MAX_ENTRIES / 2) {
                        KeyValue borrowed = leftLeaf->entries[leftLeaf->count - 1];
                        leftLeaf->count--;
                    	for (int i = curLeaf->count; i > 0; --i) {
                    		curLeaf->entries[i] = curLeaf->entries[i - 1];
                    	}
                    	curLeaf->entries[0] = borrowed;
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
                    if (rightLeaf->count > LEAF_MAX_ENTRIES / 2) {
                        KeyValue borrowed = rightLeaf->entries[0];
                    	for (int i = 0; i < rightLeaf->count - 1; ++i) {
                    		rightLeaf->entries[i] = rightLeaf->entries[i + 1];
                    	}
                        rightLeaf->count--;
                    	curLeaf->entries[curLeaf->count] = borrowed;
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
                	for (int i = 0; i < curLeaf->count; ++i) {
                		leftLeaf->entries[leftLeaf->count] = curLeaf->entries[i];
                		leftLeaf->count++;
                	}
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
                	for (int i = idx - 1; i < parent->count - 1; ++i) {
                		parent->keys[i] = parent->keys[i + 1];
                	}
                	for (int i = idx; i < parent->count; ++i) {
                		parent->children[i] = parent->children[i + 1];
                	}
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
                	for (int i = 0; i < rightLeaf->count; ++i) {
                		curLeaf->entries[curLeaf->count] = rightLeaf->entries[i];
                		curLeaf->count++;
                	}
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
                	for (int i = idx; i < parent->count - 1; ++i) {
                		parent->keys[i] = parent->keys[i + 1];
                	}
                	for (int i = idx + 1; i < parent->count; ++i) {
                		parent->children[i] = parent->children[i + 1];
                	}
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
                if (curInt->count >= INTERNAL_MAX_KEYS / 2 || path.empty()) break;

                InternalNode* parent = path.back();
                int idx = indices.back();

                // 1. 向左兄弟借
                if (idx > 0) {
                    InternalNode* leftSib = dynamic_cast<InternalNode*>(parent->children[idx-1]);
                    if (leftSib->count > INTERNAL_MAX_KEYS / 2) {
                        // 旋转
                    	for (int i = curInt->count; i > 0; --i) curInt->keys[i] = curInt->keys[i-1];
                    	for (int i = curInt->count + 1; i > 0; --i) curInt->children[i] = curInt->children[i-1];
                    	curInt->keys[0] = parent->keys[idx-1];
                    	curInt->children[0] = leftSib->children[leftSib->count];
                    	curInt->count++;
                    	leftSib->count--;
                        parent->keys[idx-1] = leftSib->keys[leftSib->count];
                        break;
                    }
                }
                // 2. 向右兄弟借
                if (idx < parent->count) {
                    InternalNode* rightSib = dynamic_cast<InternalNode*>(parent->children[idx+1]);
                    if (rightSib->count > INTERNAL_MAX_KEYS / 2) {
                    	curInt->keys[curInt->count] = parent->keys[idx];
                    	curInt->children[curInt->count + 1] = rightSib->children[0];
                    	for (int i = 0; i < rightSib->count; ++i) {
                    		rightSib->children[i] = rightSib->children[i+1];
                    	}
                    	parent->keys[idx] = rightSib->keys[0];
                    	for (int i = 0; i < rightSib->count - 1; ++i) {
                    		rightSib->keys[i] = rightSib->keys[i+1];
                    	}
                        curInt->count++;
                        rightSib->count--;
                        break;
                    }
                }
                // 3. 合并
                if (idx > 0) {   // 与左兄弟合并
                    InternalNode* leftSib = dynamic_cast<InternalNode*>(parent->children[idx-1]);
                	leftSib->keys[leftSib->count] = parent->keys[idx-1];
                	for (int i = 0; i < curInt->count; ++i) {
                		leftSib->keys[leftSib->count + 1 + i] = curInt->keys[i];
                	}
                	for (int i = 0; i <= curInt->count; ++i) { // children 数量 = count+1
                		leftSib->children[leftSib->count + 1 + i] = curInt->children[i];
                	}
                    leftSib->count += 1 + curInt->count;
                	for (int i = idx - 1; i < parent->count - 1; ++i) {
                		parent->keys[i] = parent->keys[i+1];
                	}
                	for (int i = idx; i < parent->count; ++i) {
                		parent->children[i] = parent->children[i+1];
                	}
                    parent->count--;
                    delete curInt;
                    cur_node = parent;
                    is_leaf = false;
                    path.pop_back();
                    indices.pop_back();
                } else {          // 与右兄弟合并
                    InternalNode* rightSib = dynamic_cast<InternalNode*>(parent->children[idx+1]);
                	curInt->keys[curInt->count] = parent->keys[idx];
                	for (int i = 0; i < rightSib->count; ++i) {
                		curInt->keys[curInt->count + 1 + i] = rightSib->keys[i];
                	}
                	for (int i = 0; i <= rightSib->count; ++i) {
                		curInt->children[curInt->count + 1 + i] = rightSib->children[i];
                	}
                    curInt->count += 1 + rightSib->count;
                	for (int i = idx; i < parent->count - 1; ++i) {
                		parent->keys[i] = parent->keys[i+1];
                	}
                	for (int i = idx + 1; i < parent->count; ++i) { // 注意 children 数量
                		parent->children[i] = parent->children[i+1];
                	}
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
	// 范围查询
	void rangeQuery(const Key& startKey, const Key& endKey,sjtu::vector<Key>& keys, sjtu::vector<Value>& values) {
		KeyValue lower{startKey, Value{}};
		Node* cur = root;
		while (cur->type == INTERNAL_NODE) {
			cur = static_cast<InternalNode*>(cur)->findChild(lower);
		}
		auto* leafStub = static_cast<LeafNode*>(cur);
		uint64_t curOff = leafStub->fileOffset;
		while (curOff != 0) {
			auto* leaf = readLeaf_at(curOff);
			bool beyond = false;
			for (int i = 0; i < leaf->count; ++i) {
				if (leaf->entries[i].key < startKey) continue;
				if (leaf->entries[i].key > endKey){beyond = true; break;}
				keys.push_back(leaf->entries[i].key);
				values.push_back(leaf->entries[i].value);
			}
			delete leaf;
			if (beyond) break;
			curOff = leaf->nextOffset;
		}
	}
	bool empty() const {
		// 打开叶子文件
		std::ifstream in(leaf_name, std::ios::binary);
		if (!in.is_open()) return true;
		in.seekg(leafHead);
		char type;
		in.read(&type, 1);
		int count;
		in.read(reinterpret_cast<char*>(&count), sizeof(int));
		return count == 0;
	}


};
#endif // TICKET_SYSTEM_BPT_H