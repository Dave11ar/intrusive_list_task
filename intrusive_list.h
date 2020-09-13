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
        std::enable_if_t<std::is_same_v<E, P> || std::is_const_v<E>>* = nullptr) {
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

   private:
    /*
    Это важно иметь этот конструктор private, чтобы итератор нельзя было создать
    от nullptr.
    */
    explicit list_iterator(list_element<Tag>* cur) noexcept : current(cur){}
    friend list;
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
    ring();
  }
  list(list const &) = delete;
  list(list&& cur_list) noexcept : fake_node(cur_list.fake_node) {
    cur_list.ring();
  }

  ~list() {
    clear();
    delete fake_node;
  }

  list &operator=(list const &) = delete;
  list &operator=(list&& cur_list) noexcept {
    delete fake_node;
    fake_node = cur_list.fake_node;
    cur_list.ring();
    return *this;
  }

  void clear() noexcept {
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
    fake_node->prev->next = get_list(node);
    get_list(node)->prev = fake_node->prev;
    get_list(node)->next = fake_node;
    fake_node->prev = get_list(node);
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
    fake_node->next->prev = get_list(node);
    get_list(node)->next = fake_node->next;
    get_list(node)->prev = fake_node;
    fake_node->next = get_list(node);

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

  iterator insert(const_iterator pos, T& node) noexcept {
    iterator pos_non_const = non_const_transform(pos);

    pos_non_const->prev->next = get_list(node);
    get_list(node)->next = &*pos_non_const;
    get_list(node)->prev= pos_non_const->prev;
    pos_non_const->prev = get_list(node);
    return iterator(get_list(node));
  }

  iterator erase(const_iterator pos) noexcept {
    iterator pos_non_const = non_const_transform(pos);

    iterator cur = ++pos_non_const;
    pos_non_const->prev->unlink();
    return cur;
  }

  void splice(const_iterator pos, list& l, const_iterator first, const_iterator last) noexcept {
    if (first == last) {
      return;
    }
    iterator pos_non_const = non_const_transform(pos);
    iterator first_non_const = non_const_transform(first);
    iterator last_non_const = non_const_transform(last);

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

  void ring() {
    fake_node = new list_element<Tag>();
    fake_node->next = fake_node;
    fake_node->prev = fake_node;
  }

  static list_element<Tag>* get_list(T& node) {
    return static_cast<list_element<Tag>*>(&node);
  }

  list_element<Tag>* fake_node;
};
}
