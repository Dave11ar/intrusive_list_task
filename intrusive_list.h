#pragma once

#include <type_traits>
#include <utility>
#include <iterator>

namespace intrusive {
/*
Тег по-умолчанию чтобы пользователям не нужно было
придумывать теги, если они используют лишь одну базу
list_element.
*/
struct default_tag;

template<typename Tag = default_tag>
struct list_element {
  list_element* next;
  list_element* prev;

  ~list_element() {
    //unlink();
  }

  /* Отвязывает элемент из списка в котором он находится. */
  void unlink() {
    next->prev = prev;
    prev->next = next;

    next = nullptr;
    prev = nullptr;
  };
};

template<typename T, typename Tag = default_tag>
struct list {
  template <typename E>
  struct list_iterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = std::remove_const_t<T>;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    list_iterator() = default;

    template <typename P>
    list_iterator(list_iterator<P> other,
        std::enable_if_t<std::is_same_v<std::remove_const_t<E>, std::remove_const_t<P>> &&
              (std::is_same_v<E, P> || std::is_const_v<E>)>* = nullptr) {
      current = other.current;
    }

    E& operator*() const noexcept {
      return *static_cast<E*>(current);
    }
    E* operator->() const noexcept {
      return static_cast<E*>(current);
    }

    list_iterator<E>& operator++() & noexcept {
      current = current->next;
      return *this;
    }
    list_iterator<E>& operator--() & noexcept {
      current = current->prev;
      return *this;
    }

    list_iterator<E> operator++(int) & noexcept {
      current = current->next;
      return list_iterator<E>(current->prev);
    }
    list_iterator<E> operator--(int) & noexcept {
      current = current->prev;
      return list_iterator<E>(current->next);
    }

    bool operator==(list_iterator const& rhs) const& noexcept {
      return current == rhs.current;
    }
    bool operator!=(list_iterator const& rhs) const& noexcept {
      return !(*this == rhs);
    }

   // lul private:
    /*
    Это важно иметь этот конструктор private, чтобы итератор нельзя было создать
    от nullptr.
    */
    explicit list_iterator(list_element<Tag>* cur) noexcept : current(cur){}

   //lul private:
    /*
    Хранить list_element*, а не T* важно.
    Иначе нельзя будет создать list_iterator для
    end().
    */
    list_element<Tag>* current;
  };


  using iterator = list_iterator<T>;
  using const_iterator = list_iterator<const T>;

  static_assert(std::is_convertible_v<T &, list_element<Tag> &>,
                "value type is not convertible to list_element");

  list() noexcept {
    fake_node = new list_element<Tag>();
    fake_node->next = fake_node;
    fake_node->prev = fake_node;
  }
  list(list const &) = delete;
  list(list&& cur_list) noexcept : fake_node(cur_list.fake_node) {
    cur_list.fake_node = new list_element<Tag>();
    cur_list.fake_node->next = cur_list.fake_node;
    cur_list.fake_node->prev = cur_list.fake_node;
  }

  ~list() {
    clear();
    delete fake_node;
  }

  list &operator=(list const &) = delete;
  list &operator=(list&& cur_list) noexcept {
    delete fake_node;

    fake_node = cur_list.fake_node;
    cur_list.fake_node = new list_element<Tag>();
    cur_list.fake_node->next = cur_list.fake_node;
    cur_list.fake_node->prev = cur_list.fake_node;

    return *this;
  }

  void clear() noexcept {
    //gline
    list_element<Tag>* node = fake_node->next;

    while (node != fake_node) {
      list_element<Tag>* tmp = node->next;
      node->unlink();
      node = tmp;
    }
  }

  /*
  Поскольку вставка изменяет данные в list_element
  мы принимаем неконстантный T&.
  */
  void push_back(T& node) noexcept {
    fake_node->prev->next = &node;
    static_cast<list_element<Tag>*>(&node)->prev = fake_node->prev;
    static_cast<list_element<Tag>*>(&node)->next = fake_node;
    fake_node->prev = &node;
  }

  void pop_back() noexcept {
    fake_node->prev->unlink();
  }
  T &back() noexcept {
    return *static_cast<T*>(fake_node->prev);
  }
  T const &back() const noexcept {
    return *static_cast<T*>(fake_node->prev);
  }

  void push_front(T& node) noexcept {
    fake_node->next->prev = &node;
    static_cast<list_element<Tag>*>(&node)->next = fake_node->next;
    static_cast<list_element<Tag>*>(&node)->prev = fake_node;
    fake_node->next = &node;

  }
  void pop_front() noexcept {
    fake_node->next->unlink();
  }

  T& front() noexcept {
    return *static_cast<T*>(fake_node->next);
  }
  T const &front() const noexcept {
    return *static_cast<T*>(fake_node->next);
  }

  bool empty() const noexcept {
    return fake_node->next == fake_node;
  }

  iterator begin() noexcept {
    return iterator(fake_node->next);
  }
  const_iterator begin() const noexcept {
    return const_iterator(fake_node->next);
  }

  iterator end() noexcept {
    return iterator(fake_node);
  }
  const_iterator end() const noexcept {
    return const_iterator(fake_node);
  }

  iterator insert(/*const_*/iterator pos, T& node) noexcept {
    pos->prev->next = static_cast<list_element<Tag>*>(&node);
    static_cast<list_element<Tag>*>(&node)->next = &(*pos);
    static_cast<list_element<Tag>*>(&node)->prev= pos->prev;
    pos->prev = static_cast<list_element<Tag>*>(&node);
    return iterator(&node);
  }
  iterator erase(/*const_*/iterator pos) noexcept {
    iterator cur = ++pos;
    pos->prev->unlink();
    return cur;
  }
  void splice(const_iterator pos, list& l, const_iterator first, const_iterator last) noexcept {
    iterator pos_non_const = non_const_transform(pos);
    iterator first_non_const = non_const_transform(first);
    iterator last_non_const = non_const_transform(last);

    if (first == last)
      return;

    pos_non_const->prev->next = &(*first_non_const);

    first_non_const->prev->next = &(*last_non_const);
    last_non_const->prev->next = &(*pos_non_const);

    list_element<Tag>* tmp = first_non_const->prev;
    first_non_const->prev = pos_non_const->prev;
    pos_non_const->prev = last_non_const->prev;
    last_non_const->prev = tmp;
  }

 private:
  iterator non_const_transform(const_iterator cur) {
    return iterator(cur->prev->next);
  }


  list_element<Tag>* fake_node;
};
}
