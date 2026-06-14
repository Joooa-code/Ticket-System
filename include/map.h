#ifndef TICKET_SYSTEM_MAP_H
#define TICKET_SYSTEM_MAP_H
#include <functional>
#include <cstddef>
#include "utility.h"
#include "exceptions.h"

namespace sjtu {

template<
    class Key,
    class T,
    class Compare = std::less <Key>
> class map {
public:
    typedef pair<const Key, T> value_type;
private:
    struct Node {
        value_type data;
        Node* left;
        Node* right;
        Node* parent;
        size_t height;
        Node(const value_type & element, Node* l, Node* r, size_t h, Node* p = nullptr)
            :data(element), left(l), right(r), height(h), parent(p){}
    };
    Node* root;
    size_t siz;
    friend class iterator;
    friend class const_iterator;
    static void Height(Node* &t) {
        size_t lh = (t->left ? t->left->height : 0);
        size_t rh = (t->right ? t->right->height : 0);
        t->height = std::max(lh, rh) + 1;
    }
    static size_t HeightOf(Node* p) {
        return p ? p->height : 0;
    }
    void LL(Node* &t) {
        Node* t1 = t->left;
        t->left = t1->right;
        if (t1->right) t1->right->parent = t;
        t1->right = t;
        t1->parent = t->parent;
        t->parent = t1;
        if (t->left) t->left->parent = t;
        Height(t);
        Height(t1);
        t = t1;
    }
    void RR(Node* &t) {
        Node* t1 = t->right;
        t->right = t1->left;
        if (t1->left) t1->left->parent = t;
        t1->left = t;
        t1->parent = t->parent;
        t->parent = t1;
        if (t->right) t->right->parent = t;
        Height(t);
        Height(t1);
        t = t1;
    }
    void LR(Node* &t) {
        RR(t->left);
        LL(t);
    }
    void RL(Node* &t) {
        LL(t->right);
        RR(t);
    }
    bool remove(const Key x, Node* &t) {
        if (t == nullptr) return true;
        if (!Compare()(x, t->data.first) && !Compare()(t->data.first, x)) {
            if (t->left == nullptr || t->right == nullptr) {
                Node* tmp = t;
                Node* p = t->parent;
                if (t->left) {
                    t->left->parent = t->parent;
                }
                if (t->right) {
                    t->right->parent = t->parent;
                }
                t = (t->left != nullptr)? t->left : t->right;
                delete tmp;
                return false;
            }
            else {
                Node* rep = t->right;
                while (rep->left != nullptr) rep = rep->left;
                Node* t1 = rep->parent;
                Node* t2 = rep->right;
                if (rep != t -> right) {
                    Node* rem = new Node(t->data, nullptr, t2, rep->height, t1);
                    rep->parent = t->parent;
                    rep->left = t->left;
                    rep->right = t->right;
                    rep->height = t->height;
                    rep->left->parent = rep;
                    rep->right->parent = rep;
                    t1->left = rem;
                    if (t2) t2->parent = rem;
                    Node* del = t;
                    t = rep;
                    delete del;
                    if (remove(x, t->right)) return true;
                    return adjust(t, 1);
                }
                else {
                    rep->parent = t->parent;
                    rep->left = t->left;
                    rep->height = t->height;
                    rep->left->parent = rep;
                    Node* del = t;
                    t = rep;
                    delete del;
                    return adjust(t, 1);
                }
            }
        }
        if (Compare()(x, t->data.first)) {
            if (remove(x, t->left)) return true;
            return adjust(t, 0);
        }
        else {
            if (remove(x, t->right)) return true;
            return adjust(t, 1);
        }
    }
    bool adjust(Node* &t, int sub) {
        if (sub == 1) {
            if (HeightOf(t->left) - HeightOf(t->right) == 1) return true;
            if (HeightOf(t->left) - HeightOf(t->right) == 0) {
                --t->height;
                return false;
            }
            if (t->left) {
                if (HeightOf(t->left->right) > HeightOf(t->left->left)) {
                    LR(t);
                    return false;
                }
            }
            LL(t);
            if (HeightOf(t->right) == HeightOf(t->left)) return false;
            return true;
        }
        else {
            if (HeightOf(t->right) - HeightOf(t->left) == 1) return true;
            if (HeightOf(t->right) - HeightOf(t->left) == 0) {
                --t->height;
                return false;
            }
            if (t->right) {
                if (HeightOf(t->right->left) > HeightOf(t->right->right)) {
                    RL(t);
                    return false;
                }
            }
            RR(t);
            if (HeightOf(t->right) == HeightOf(t->left)) return false;
            return true;
        }
    }
 public:
  /**
   * the internal type of data.
   * it should have a default constructor, a copy constructor.
   * You can use sjtu::map as value_type by typedef.
   */
  /**
   * see BidirectionalIterator at CppReference for help.
   *
   * if there is anything wrong throw invalid_iterator.
   *     like it = map.begin(); --it;
   *       or it = map.end(); ++end();
   */
  class const_iterator;
  class iterator {
   private:
    /**
     * TODO add data members
     *   just add whatever you want.
     */
      const map<Key, T, Compare>* Map;
      Node* ptr;
   public:
    iterator() {
      // TODO
        Map = nullptr;
        ptr = nullptr;
    }
    iterator(const map<Key, T,Compare>* p, Node* r) :
      Map(p), ptr(r){}

    iterator(const iterator &other) {
      // TODO
        Map = other.Map;
        ptr = other.ptr;
    }

    /**
     * TODO iter++
     */
    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    /**
     * TODO ++iter
     */
    iterator &operator++() {
        if (ptr == nullptr) throw invalid_iterator();
        if (ptr->right != nullptr) {
            ptr = ptr->right;
            while (ptr->left != nullptr) ptr = ptr->left;
        }
        else {
            Node* p = ptr->parent;
            while (p != nullptr && ptr == p->right) {
                ptr = p;
                p = p->parent;
            }
            ptr = p;  // maybe nullptr(end())
        }
        return *this;
    }

    /**
     * TODO iter--
     */
    iterator operator--(int) {
        iterator tmp = *this;
        --(*this);
        return tmp;
    }

    /**
     * TODO --iter
     */
    iterator &operator--() {
        if (ptr == nullptr) {   // end()
            if (Map->root == nullptr) throw invalid_iterator();
            ptr = Map->root;
            while (ptr->right != nullptr) ptr = ptr->right;
            return *this;
        }
        if (ptr->left != nullptr) {
            ptr = ptr->left;
            while (ptr->right != nullptr) ptr = ptr->right;
        } else {
            Node* p = ptr->parent;
            while (p != nullptr && ptr == p->left) {
                ptr = p;
                p = p->parent;
            }
            ptr = p;
            if (ptr == nullptr) throw invalid_iterator();   // --begin()
        }
        return *this;
    }

    /**
     * a operator to check whether two iterators are same (pointing to the same memory).
     */
    value_type &operator*() const {
        if (ptr == nullptr) throw invalid_iterator();
        return ptr->data;
    }

    bool operator==(const iterator &rhs) const {
        return ptr == rhs.ptr && Map == rhs.Map;
    }

    bool operator==(const const_iterator &rhs) const {
        return ptr == rhs.ptr && Map == rhs.Map;
    }

    /**
     * some other operator for iterator.
     */
    bool operator!=(const iterator &rhs) const {
        return !(*this == rhs);
    }

    bool operator!=(const const_iterator &rhs) const {
        return !(*this == rhs);
    }

    /**
     * for the support of it->first.
     * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
     */
    value_type *operator->() const noexcept {
        return &(ptr->data);
    }
      friend class map;
  };
  class const_iterator {
    // it should has similar member method as iterator.
    //  and it should be able to construct from an iterator.
   private:
    // data members.
      const map<Key, T, Compare>* Map;
      const Node* ptr;

   public:
    const_iterator() {
      // TODO
        Map = nullptr;
        ptr = nullptr;
    }

    const_iterator(const const_iterator &other) {
      // TODO
        Map = other.Map;
        ptr = other.ptr;
    }

    const_iterator(const map<Key, T,Compare>* p, Node* r) :
    Map(p), ptr(r){}

    const_iterator(const iterator &other) {
      // TODO
        Map = other.Map;
        ptr = other.ptr;
    }
      const_iterator operator++(int) {
        const_iterator tmp = *this;
        ++(*this);
        return tmp;
    }

      /**
       * TODO ++iter
       */
      const_iterator &operator++() {
        if (ptr == nullptr) throw invalid_iterator();
        if (ptr->right != nullptr) {
            ptr = ptr->right;
            while (ptr->left != nullptr) ptr = ptr->left;
        }
        else {
            const Node* p = ptr->parent;
            while (p != nullptr && ptr == p->right) {
                ptr = p;
                p = p->parent;
            }
            ptr = p;
        }
        return *this;
    }

      /**
       * TODO iter--
       */
      const_iterator operator--(int) {
        const_iterator tmp = *this;
        --(*this);
        return tmp;
    }

      /**
       * TODO --iter
       */
    const_iterator &operator--() {
        if (ptr == nullptr) {
            if (Map->root == nullptr) throw invalid_iterator();
            ptr = Map->root;
            while (ptr->right != nullptr) ptr = ptr->right;
            return *this;
        }
        if (ptr->left != nullptr) {
            ptr = ptr->left;
            while (ptr->right != nullptr) ptr = ptr->right;
        }
        else {
            const Node* p = ptr->parent;
            while (p != nullptr && ptr == p->left) {
                ptr = p;
                p = p->parent;
            }
            ptr = p;
            if (ptr == nullptr) throw invalid_iterator();
        }
        return *this;
    }

      /**
       * a operator to check whether two iterators are same (pointing to the same memory).
       */
      const value_type &operator*() const {
        if (ptr == nullptr) throw invalid_iterator();
        return ptr->data;
    }

      bool operator==(const iterator &rhs) const {
        return ptr == rhs.ptr && Map == rhs.Map;
    }

      bool operator==(const const_iterator &rhs) const {
        return ptr == rhs.ptr && Map == rhs.Map;
    }

      /**
       * some other operator for iterator.
       */
      bool operator!=(const iterator &rhs) const {
        return !(*this == rhs);
    }

      bool operator!=(const const_iterator &rhs) const {
        return !(*this == rhs);
    }

      /**
       * for the support of it->first.
       * See <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/> for help.
       */
      const value_type *operator->() const noexcept {
        return &(ptr->data);
    }
      friend class map;
  };

  /**
   * TODO two constructors
   */
  map() {
      root = nullptr;
      siz = 0;
  }

  Node* copyTree(Node* other) {
      if (other == nullptr) return nullptr;
      Node* node = new Node(other->data, copyTree(other->left), copyTree(other->right), other->height);
      if (node->left)  node->left->parent  = node;
      if (node->right) node->right->parent = node;
      return node;
  }
  map(const map &other) {
      root = copyTree(other.root);
      siz = other.siz;
  }

  /**
   * TODO assignment operator
   */
  map &operator=(const map &other) {
      if (this == &other) return *this;
      clear();
      root = copyTree(other.root);
      siz = other.siz;
      return *this;
  }

  /**
   * TODO Destructors
   */
  ~map() {
      clear();
  }

  /**
   * TODO
   * access specified element with bounds checking
   * Returns a reference to the mapped value of the element with key equivalent to key.
   * If no such element exists, an exception of type `index_out_of_bound'
   */
  T &at(const Key &key) {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) throw index_out_of_bound();
      return tmp->data.second;
  }

  const T &at(const Key &key) const {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) throw index_out_of_bound();
      return tmp->data.second;
  }

  /**
   * TODO
   * access specified element
   * Returns a reference to the value that is mapped to a key equivalent to key,
   *   performing an insertion if such key does not already exist.
   */
  T &operator[](const Key &key) {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr){
          value_type n(key, T());
          Node* inserted = nullptr;
          insert(n, root, nullptr, inserted);
          siz++;
          return inserted->data.second;
      }
      else {
          return tmp->data.second;
      }
  }

  /**
   * behave like at() throw index_out_of_bound if such key does not exist.
   */
  const T &operator[](const Key &key) const {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) throw index_out_of_bound();
      return tmp->data.second;
  }

  /**
   * return a iterator to the beginning
   */
  iterator begin() {
      if (root == nullptr) return end();
      Node* node = root;
      while (node->left != nullptr) {
          node = node->left;
      }
      return iterator(this, node);
  }

  const_iterator cbegin() const {
      if (root == nullptr) return cend();
      Node* node = root;
      while (node->left != nullptr) {
          node = node->left;
      }
      return const_iterator(this, node);
  }

  /**
   * return a iterator to the end
   * in fact, it returns past-the-end.
   */
  iterator end() {
      return iterator(this, nullptr);
  }

  const_iterator cend() const {
      return const_iterator(this, nullptr);
  }

  /**
   * checks whether the container is empty
   * return true if empty, otherwise false.
   */
  bool empty() const {
      return siz == 0;
  }

  /**
   * returns the number of elements.
   */
  size_t size() const {
      return siz;
  }

  /**
   * clears the contents
   */
  void clearTree(Node* node) {
      if (!node) return;
      clearTree(node->left);
      clearTree(node->right);
      delete node;
  }

  void clear() {
      clearTree(root);
      root = nullptr;
      siz = 0;
  }

  /**
   * insert an element.
   * return a pair, the first of the pair is
   *   the iterator to the new element (or the element that prevented the insertion),
   *   the second one is true if insert successfully, or false.
   */
  void insert(const value_type &value, Node* &t, Node* p, Node* &pos) {
      if (t == nullptr) {
          t = new Node(value, nullptr, nullptr, 1, p);
          pos = t;
      }
      else if (Compare()(value.first, t->data.first)) {
          insert(value, t->left, t, pos);
          if (HeightOf(t->left) - HeightOf(t->right) == 2) {
              if (Compare()(value.first, t->left->data.first)) {
                  LL(t);
              }
              else {
                  LR(t);
              }
          }
      }
      else {
          insert(value, t->right, t, pos);
          if (HeightOf(t->right) - HeightOf(t->left) == 2) {
              if (Compare()(value.first, t->right->data.first)) {
                  RL(t);
              }
              else {
                  RR(t);
              }
          }
      }
      Height(t);
  }

  pair<iterator, bool> insert(const value_type &value) {
      iterator tmp = find(value.first);
      if (tmp != end()) {
          return pair<iterator, bool>(tmp, false);
      }
      Node* p = nullptr;
      insert(value, root, nullptr, p);
      iterator it = iterator(this, p);
      siz++;
      return pair<iterator, bool>(it, true);
  }


  /**
   * erase the element at pos.
   *
   * throw if pos pointed to a bad element (pos == this->end() || pos points an element out of this)
   */
  void erase(iterator pos) {
      if (pos.Map != this || pos.ptr == nullptr || pos == end())
          throw invalid_iterator();
      Node* tmp = root;
      const Key& key = pos.ptr->data.first;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) ||Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          } else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr || tmp != pos.ptr) throw invalid_iterator();
      remove(key, root);
      siz--;
  }

  /**
   * Returns the number of elements with key
   *   that compares equivalent to the specified argument,
   *   which is either 1 or 0
   *     since this container does not allow duplicates.
   * The default method of check the equivalence is !(a < b || b > a)
   */
  size_t count(const Key &key) const {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) return 0;
      return 1;
  }

  /**
   * Finds an element with key equivalent to key.
   * key value of the element to search for.
   * Iterator to an element with key equivalent to key.
   *   If no such element is found, past-the-end (see end()) iterator is returned.
   */
  iterator find(const Key &key) {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) return end();
      return iterator(this, tmp);
  }

  const_iterator find(const Key &key) const {
      Node* tmp = root;
      while (tmp != nullptr && (Compare()(key, tmp->data.first) || Compare()(tmp->data.first, key))) {
          if (Compare()(key, tmp->data.first)) {
              tmp = tmp->left;
          }
          else {
              tmp = tmp->right;
          }
      }
      if (tmp == nullptr) return cend();
      return const_iterator(this, tmp);
  }
};

}
#endif //TICKET_SYSTEM_MAP_H