#pragma once
#include <cassert>

template<typename T, size_t N>
class StaticVector {
public:
    StaticVector() = default;

    ~StaticVector() = default;

    void push_back(T value) {
        if (size < N) {
            data[size] = value;
            ++size;
        }
    }

    void remove_at(size_t index) {
        if (index >= size) {
            return;
        }

        // Calculate bytes to move
        if (const size_t bytes_to_move = sizeof(T) * (size - index - 1); bytes_to_move > 0) {
            memcpy(&data[index], &data[index + 1], bytes_to_move);
        }

        --size;
    }

    template<size_t M>
     void push_back(const char (&value)[M]) {
        if (size < N) {
            T& dest = data[size];
            for (size_t i = 0; i < M; ++i) {
                dest[i] = value[i];
            }
            ++size;
        }
    }

    void pop_back() {
        if (size > 0) {
            --size;
        }
    }

    T& operator[](size_t index) {
        assert(index < size);
        return data[index];
    }

    const T& operator[](size_t index) const {
        assert(index < size);
        return data[index];
    }

    [[nodiscard]] size_t getSize() const {
        return size;
    }

    void clear() {
        size = 0;
    }

    [[nodiscard]] static constexpr size_t capacity() {
        return N;
    }

    [[nodiscard]] bool empty() const {
        return size == 0;
    }

    [[nodiscard]] bool full() const {
        return size == N;
    }

private:
    T data[N];
    size_t size = 0;
};
