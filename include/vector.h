#ifndef TICKET_SYSTEM_VECTOR_H
#define TICKET_SYSTEM_VECTOR_H
#include "exceptions.hpp"
#include "utility.hpp"
#include <climits>
#include <cstddef>
constexpr size_t CAP = 128;
namespace sjtu
{
/**
 * a data container like std::vector
 * store data in a successive memory and support random access.
 */
template<typename T>
class vector
{
public:
	/**
	 * TODO
	 * a type for actions of the elements of a vector, and you should write
	 *   a class named const_iterator with same interfaces.
	 */
	T* data;
	size_t cap;  // capacity, n = 512
	size_t siz;  // current size
	/**
	 * you can see RandomAccessIterator at CppReference for help.
	 */
	class const_iterator;
	class iterator
	{
	// The following code is written for the C++ type_traits library.
	// Type traits is a C++ feature for describing certain properties of a type.
	// For instance, for an iterator, iterator::value_type is the type that the
	// iterator points to.
	// STL algorithms and containers may use these type_traits (e.g. the following
	// typedef) to work properly. In particular, without the following code,
	// @code{std::sort(iter, iter1);} would not compile.
	// See these websites for more information:
	// https://en.cppreference.com/w/cpp/header/type_traits
	// About value_type: https://blog.csdn.net/u014299153/article/details/72419713
	// About iterator_category: https://en.cppreference.com/w/cpp/iterator
	public:
		using difference_type = std::ptrdiff_t;  // 有符号整数类型，用于表示两个指针相减的结果
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;  // 用于标识输出迭代器的一个空标签类

	private:
		/**
		 * TODO add data members
		 *   just add whatever you want.
		 */
		T* ptr;  // point to the current element
		const vector<T>* vec;  //the vector it belongs to
	public:
		iterator(T* p = nullptr, const vector<T>* v = nullptr):
			ptr(p), vec(v){}
		iterator(const iterator& other) {
			ptr = other.ptr;
			vec = other.vec;
		}
		/**
		 * return a new iterator which pointer n-next elements
		 * as well as operator-
		 */
		iterator operator+(const int &n) const
		{
			//TODO
			return iterator(ptr + n, vec);
		}
		iterator operator-(const int &n) const
		{
			//TODO
			return iterator(ptr - n, vec);
		}
		// return the distance between two iterators,
		// if these two iterators point to different vectors, throw invaild_iterator.
		int operator-(const iterator &rhs) const
		{
			//TODO
			if (vec != rhs.vec) {throw invalid_iterator();}
			return ptr - rhs.ptr;
		}
		iterator& operator+=(const int &n)
		{
			//TODO
			ptr += n;
			return *this;
		}
		iterator& operator-=(const int &n)
		{
			//TODO
			ptr -= n;
			return *this;
		}
		/**
		 * TODO iter++
		 */
		iterator operator++(int) {
			iterator tmp = *this;
			ptr++;
			return tmp;
		}
		/**
		 * TODO ++iter
		 */
		iterator& operator++() {
			ptr++;
			return *this;
		}
		/**
		 * TODO iter--
		 */
		iterator operator--(int) {
			iterator tmp = *this;
			ptr--;
			return tmp;
		}
		/**
		 * TODO --iter
		 */
		iterator& operator--() {
			ptr--;
			return *this;
		}
		/**
		 * TODO *it
		 */
		T& operator*() const {
			if (ptr == nullptr) {throw invalid_iterator();}
			return *ptr;
		}
		/**
		 * a operator to check whether two iterators are same (pointing to the same memory address).
		 */
		bool operator==(const iterator &rhs) const {
			return ptr == rhs.ptr && vec == rhs.vec;
		}
		bool operator==(const const_iterator &rhs) const {
			return ptr == rhs.ptr && vec == rhs.vec;
		}
		/**
		 * some other operator for iterator.
		 */
		bool operator!=(const iterator &rhs) const {
			return !(ptr == rhs.ptr && vec == rhs.vec);
		}
		bool operator!=(const const_iterator &rhs) const {
			return !(ptr == rhs.ptr && vec == rhs.vec);
		}
	};
	/**
	 * TODO
	 * has same function as iterator, just for a const object.
	 */
	class const_iterator
	{
	public:
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using iterator_category = std::output_iterator_tag;

	private:
		/*TODO*/
		const T* ptr;
		const vector<T>* vec;
	public:
		const_iterator(T* p = nullptr, const vector<T>* v = nullptr):
			ptr(p), vec(v){}
		const_iterator(const iterator& other) {
			ptr = other.ptr;
			vec = other.vec;
		}

		const_iterator operator+(const int &n) const {
			return const_iterator(ptr + n, vec);
		}

		const_iterator operator-(const int &n) const {
			return const_iterator(ptr - n, vec);
		}

		int operator-(const const_iterator &rhs) const {
			if (vec != rhs.vec) {throw invalid_iterator();}
			return ptr - rhs.ptr;
		}

		const_iterator& operator+=(const int &n) {
			ptr += n;
			return *this;
		}

		const_iterator& operator-=(const int &n) {
			ptr -= n;
			return *this;
		}

		const_iterator operator++(int) {
			const_iterator tmp = *this;
			ptr++;
			return tmp;
		}

		const_iterator& operator++() {
			ptr++;
			return *this;
		}

		const_iterator operator--(int) {
			const_iterator tmp = *this;
			ptr--;
			return tmp;
		}

		const_iterator& operator--() {
			ptr--;
			return *this;
		}

		const T& operator*() const {
			if (ptr == nullptr) {throw invalid_iterator();}
			return *ptr;
		}

		bool operator==(const const_iterator &rhs) const {
			return ptr == rhs.ptr && vec == rhs.vec;
		}

		bool operator==(const iterator &rhs) const {
			return ptr == rhs.ptr && vec == rhs.vec;
		}

		bool operator!=(const const_iterator &rhs) const {
			return !(ptr == rhs.ptr && vec == rhs.vec);
		}

		bool operator!=(const iterator &rhs) const {
			return !(ptr == rhs.ptr && vec == rhs.vec);
		}
	};
	/**
	 * TODO Constructs
	 * At least two: default constructor, copy constructor
	 */
	vector(): data(static_cast<T*>(operator new(sizeof(T) * CAP))), cap(CAP), siz(0) {}
	vector(const vector &other) :cap(other.cap), siz(other.siz){
		data = static_cast<T*>(operator new(sizeof(T) * cap));
		for (size_t i = 0; i < siz; ++i) {
			new(data + i)T(other.data[i]);  // placement new:new(address)type(param)
		}
	}
	/**
	 * TODO Destructor
	 */
	~vector() {
		clear();
		operator delete(data);
	}
	/**
	 * TODO Assignment operator
	 */
	vector &operator=(const vector &other) {
		if (this == &other) {
			return *this;
		}
		clear();
		siz = other.siz;
		if (cap < other.siz) {
			operator delete(data);
			cap = other.cap;
			data = static_cast<T*>(operator new(sizeof(T) * cap));
		}
		for (size_t i = 0; i < siz; ++i) {
			new(data + i)T(other.data[i]);
		}
		return *this;
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 */
	T & at(const size_t &pos) {
		if (pos >= siz) {
			throw index_out_of_bound();
		}
		return data[pos];
	}
	const T & at(const size_t &pos) const {
		if (pos >= siz) {
			throw index_out_of_bound();
		}
		return data[pos];
	}
	/**
	 * assigns specified element with bounds checking
	 * throw index_out_of_bound if pos is not in [0, size)
	 * !!! Pay attentions
	 *   In STL this operator does not check the boundary but I want you to do.
	 */
	T & operator[](const size_t &pos) {
		if (pos >= siz) {
			throw index_out_of_bound();
		}
		return data[pos];
	}
	const T & operator[](const size_t &pos) const {
		if (pos >= siz) {
			throw index_out_of_bound();
		}
		return data[pos];
	}
	/**
	 * access the first element.
	 * throw container_is_empty if size == 0
	 */
	const T & front() const {
		if (siz == 0) {
			throw container_is_empty();
		}
		return data[0];
	}
	/**
	 * access the last element.
	 * throw container_is_empty if size == 0
	 */
	const T & back() const {
		if (siz == 0) {
			throw container_is_empty();
		}
		return data[siz - 1];
	}
	/**
	 * returns an iterator to the beginning.
	 */
	iterator begin() {
		return iterator(data, this);
	}
	const_iterator begin() const {
		return const_iterator(data, this);
	}
	const_iterator cbegin() const {
		return const_iterator(data, this);
	}
	/**
	 * returns an iterator to the end.
	 */
	iterator end() {
		return iterator(data + siz, this);
	}
	const_iterator end() const {
		return const_iterator(data + siz, this);
	}
	const_iterator cend() const {
		return const_iterator(data + siz, this);
	}
	/**
	 * checks whether the container is empty
	 */
	bool empty() const {
		return (siz == 0);
	}
	/**
	 * returns the number of elements
	 */
	size_t size() const {
		return siz;
	}
	/**
	 * clears the contents
	 */
	void clear() {
		for (size_t i = 0; i < siz; ++i) {
			data[i].~T();
		}
		siz = 0;
	}
	/**
	 * inserts value before pos
	 * returns an iterator pointing to the inserted value.
	 */
	iterator insert(iterator pos, const T &value) {
		int index = pos - begin();
		if (siz >= cap) {
			// reallocate memory
			size_t new_cap = cap * 2;
			T* new_data = static_cast<T*>(operator new(sizeof(T) * new_cap));

			// copy elements
			for (int i = 0; i < index; ++i) {
				new(new_data + i) T(data[i]);
			}

			// insert new element
			new(new_data + index) T(value);

			// copy elements
			for (size_t i = index; i < siz; ++i) {
				new(new_data + i + 1) T(data[i]);
			}

			// destroy old elements
			for (size_t i = 0; i < siz; ++i) {
				data[i].~T();
			}
			operator delete(data);

			data = new_data;
			cap = new_cap;
			siz++;

			return iterator(data + index, this);
		}

		if (index == static_cast<int>(siz)) {
			new(data + siz) T(value);
		} else {
			new(data + siz) T(data[siz - 1]);
			for (size_t i = siz - 1; i > static_cast<size_t>(index); --i) {
				data[i] = data[i - 1];
			}
			data[index].~T();
			new(data + index) T(value);
		}
		siz++;

		return iterator(data + index, this);
	}
	/**
	 * inserts value at index ind.
	 * after inserting, this->at(ind) == value
	 * returns an iterator pointing to the inserted value.
	 * throw index_out_of_bound if ind > size (in this situation ind can be size because after inserting the size will increase 1.)
	 */
	iterator insert(const size_t &ind, const T &value) {
		if (ind > siz) {
			throw index_out_of_bound();
		}
		return insert(begin() + ind, value);
	}
	/**
	 * removes the element at pos.
	 * return an iterator pointing to the following element.
	 * If the iterator pos refers the last element, the end() iterator is returned.
	 */
	iterator erase(iterator pos) {
		int index = pos - begin();

		// destroy the element at pos
		data[index].~T();

		// move elements
		for (size_t i = index; i < siz - 1; ++i) {
			new(data + i) T(data[i + 1]);
			data[i + 1].~T();
		}

		siz--;
		return iterator(data + index, this);
	}
	/**
	 * removes the element with index ind.
	 * return an iterator pointing to the following element.
	 * throw index_out_of_bound if ind >= size
	 */
	iterator erase(const size_t &ind) {
		if (ind >= siz) {
			throw index_out_of_bound();
		}
		return erase(begin() + ind);
	}
	/**
	 * adds an element to the end.
	 */
	void push_back(const T &value) {
		if (siz >= cap) {
			size_t new_cap = cap * 2;
			T* new_data = static_cast<T*>(operator new(sizeof(T) * new_cap));

			// copy elements
			for (size_t i = 0; i < siz; ++i) {
				new(new_data + i) T(data[i]);
			}

			// destroy old elements
			for (size_t i = 0; i < siz; ++i) {
				data[i].~T();
			}
			operator delete(data);

			data = new_data;
			cap = new_cap;
		}

		// add new element
		new(data + siz) T(value);
		siz++;
	}
	/**
	 * remove the last element from the end.
	 * throw container_is_empty if size() == 0
	 */
	void pop_back() {
		if (siz == 0) {
			throw container_is_empty();
		}

		data[siz - 1].~T();
		siz--;
	}
};
}
#endif //TICKET_SYSTEM_VECTOR_H