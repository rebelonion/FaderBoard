#pragma once


template <size_t QUEUE_SIZE, size_t ARRAY_SIZE>
class ByteArrayQueue {
public:
    [[nodiscard]] bool isEmpty() const {
        return (!full && (head == tail));
    }

    [[nodiscard]] bool isFull() const {
        return full;
    }

    [[nodiscard]] size_t size() const {
        if (full) return QUEUE_SIZE;
        if (head <= tail) return tail - head;
        return QUEUE_SIZE - (head - tail);
    }

    bool push(const uint8_t newData[ARRAY_SIZE]) {
        if (full) {
            return false;
        }

        memcpy(data[tail], newData, ARRAY_SIZE);
        tail = (tail + 1) % QUEUE_SIZE;
        full = (head == tail);

        return true;
    }

    bool pop(uint8_t output[ARRAY_SIZE]) {
        if (isEmpty()) {
            return false;
        }

        memcpy(output, data[head], ARRAY_SIZE);
        head = (head + 1) % QUEUE_SIZE;
        full = false;

        return true;
    }

    bool peek(uint8_t output[ARRAY_SIZE]) const {
        if (isEmpty()) {
            return false;
        }

        memcpy(output, data[head], ARRAY_SIZE);
        return true;
    }

    void clear() {
        head = 0;
        tail = 0;
        full = false;
    }

private:
    __attribute__((aligned(32))) uint8_t data[QUEUE_SIZE][ARRAY_SIZE] = {};

    volatile size_t head = 0;
    volatile size_t tail = 0;
    volatile bool full = false;
};