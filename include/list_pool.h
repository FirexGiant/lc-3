#pragma once

#include <cstdlib>
#include <iterator>
#include <vector>

namespace rks {

template <typename T, typename N = size_t>
// requires Regular(T) && Integer(N)
class list_pool {
public:
    using value_type = T;
    using list_type = N;

private:
    struct node_t {
        T value;
        N next;
    };

    std::vector<node_t> _pool;
    list_type _free_list = end();

    node_t& node(list_type x)
    {
        return _pool[N(x - 1)];
    }

    const node_t& node(list_type x) const
    {
        return _pool[N(x - 1)];
    }

    list_type new_list()
    {
        _pool.emplace_back();
        return _pool.size();
    }

public:
    using size_type = typename std::vector<node_t>::size_type;

    list_type end() const
    {
        return list_type(0);
    }

    bool is_end(list_type x) const
    {
        return x == end();
    }

    size_type size() const
    {
        return _pool.size();
    }

    void reserve(size_type n)
    {
        _pool.reserve(n);
    }

    T& value(list_type x)
    {
        return node(x).value;
    }

    const T& value(list_type x) const
    {
        return node(x).value;
    }

    list_type& next(list_type x)
    {
        return node(x).next;
    }

    const list_type& next(list_type x) const
    {
        return node(x).next;
    }

    list_type allocate(const T& val, list_type tail)
    {
        list_type head = _free_list;
        if (is_end(_free_list)) head = new_list();
        else _free_list = next(_free_list);
        value(head) = val;
        next(head) = tail;
        return head;
    }

    list_type free(list_type head)
    {
        list_type tail = next(head);
        next(head) = _free_list;
        _free_list = head;
        return tail;
    }

    struct iterator {
        using value_type = typename list_pool::value_type;
        using difference_type = typename list_pool::list_type;
        using iterator_category = std::forward_iterator_tag;
        using reference = value_type&;
        using pointer = value_type*;

        list_pool* pool;
        typename list_pool::list_type node;

        iterator() = default;

        explicit iterator(list_pool& p) :
            pool(&p), node(p.end()) { }

        iterator(list_pool& p, const typename list_pool::list_type& x) :
            pool(&p), node(x) { }

        friend
        bool operator==(const iterator& x, const iterator& y)
        {
            return x.node == y.node;
        }

        friend
        bool operator!=(const iterator& x, const iterator& y)
        {
            return !(x == y);
        }

        reference operator*() const
        {
            return pool->value(node);
        }

        pointer operator->() const
        {
            return &**this;
        }

        iterator& operator++()
        {
            node = pool->next(node);
            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        friend
        void set_successor(iterator x, iterator y)
        {
            x.pool->next(x.node) = y.node;
        }

        friend
        void push_front(iterator& x, const T& val)
        {
            x.node = x.pool->allocate(val, x.node);
        }

        friend
        void push_back(iterator& x, const T& val)
        {
            typename list_pool::list_type tmp = x.pool->allocate(val, x.pool->next(x.node));
            x.pool->next(x.node) = tmp;
        }

        friend
        void free(iterator& x)
        {
            x.node = x.pool->free(x.node);
        }
    };
};

template <typename T, typename N>
// requires Regular(T) && Integer(N)
void free_list(list_pool<T, N>& pool, typename list_pool<T, N>::list_type x)
{
    while (!pool.is_end(x)) x = pool.free(x);
}

} // namespace rks
