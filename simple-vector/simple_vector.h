#pragma once

#include <initializer_list>
#include <stdexcept>

#include "array_ptr.h"

class ReserveProxyObj {
public:
    ReserveProxyObj(size_t size) : size_(size) {
    }

    size_t GetSize() const noexcept {
        return size_;
    }

private:
    size_t size_;
};

ReserveProxyObj Reserve(size_t capacity_to_reserve) {
    return ReserveProxyObj(capacity_to_reserve);
}

template <typename Type>
class SimpleVector {
public:
    using Iterator = Type*;
    using ConstIterator = const Type*;

    SimpleVector() noexcept = default;

    // Создаёт вектор из size элементов, инициализированных значением по умолчанию
    explicit SimpleVector(size_t size) : data_{size}, size_{size}, capacity_{size} {
        std::fill(data_.Get(), data_.Get() + size_, Type{});
    }

    explicit SimpleVector(const ReserveProxyObj& proxy_for_res)
            : data_{proxy_for_res.GetSize()}, size_{0}, capacity_{proxy_for_res.GetSize()} {
    }

    // Создаёт вектор из size элементов, инициализированных значением value
    SimpleVector(size_t size, const Type& value) : data_{size}, size_{size}, capacity_{size} {
        std::fill(data_.Get(), data_.Get() + size_, value);
    }

    // Создаёт вектор из std::initializer_list
    SimpleVector(std::initializer_list<Type> init) :
            data_{init.size()}, size_{init.size()}, capacity_{init.size()} {
        std::copy(init.begin(), init.end(), data_.Get());
    }

    // Конструктор копирования
    SimpleVector(const SimpleVector& other) : data_{other.capacity_}, size_{other.size_}, capacity_{other.capacity_} {
        std::copy(other.data_.Get(), other.data_.Get() + other.size_, data_.Get());
    }

    // Оператор присваивания
    SimpleVector& operator=(const SimpleVector& rhs) {
        if (this != &rhs) {
            if(size_ > rhs.size_ || capacity_ >= rhs.capacity_) {
                std::copy(rhs.data_.Get(), rhs.data_.Get() + rhs.size_, data_.Get());
                size_ = rhs.size_;
                return *this;
            }

            Realloc(rhs.capacity_);
            size_ = rhs.size_;
            std::copy(rhs.data_.Get(), rhs.data_.Get() + rhs.size_, data_.Get());
        }
        return *this;
    }

    // Конструктор перемещения
    SimpleVector(SimpleVector&& other) noexcept
        : data_{std::move(other.data_)}, size_{std::exchange(other.size_,0)}, capacity_{std::exchange(other.capacity_,0)} {

    }

    // Оператор присваивания перемещением
    SimpleVector& operator=(SimpleVector&& rhs) {
        if (this != &rhs) {
            if(size_ > rhs.size_ || capacity_ >= rhs.capacity_) {
                std::copy(std::make_move_iterator(rhs.data_.Get()), std::make_move_iterator(rhs.data_.Get() + rhs.size_), data_.Get());
                size_ = std::exchange(rhs.size_,0);
                return *this;
            }

            Realloc(rhs.capacity_);
            std::copy(std::make_move_iterator(rhs.data_.Get()),std::make_move_iterator(rhs.data_.Get() + rhs.size_), data_.Get());
            size_ = std::exchange(rhs.size_,0);
        }
        return *this;
    }

    // Добавляет элемент в конец вектора
    // При нехватке места увеличивает вместимость вектора на размер capacity_multiplier
    void PushBack(const Type& item) {
        if(size_ < capacity_) {
            data_[size_] = item;
            ++size_;
            return;
        }

        ArrayPtr<Type> tmp{Realloc(capacity_multiplier * capacity_)};
        std::copy(tmp.Get(), tmp.Get() + size_, data_.Get());

        data_[size_] = item;
        ++size_;
    }

    // Перемещает элемент в конец вектора
    // При нехватке места увеличивает вместимость вектора на размер capacity_multiplier
    void PushBack(Type&& item) {
        if(size_ < capacity_) {
            data_[size_] = std::move(item);
            ++size_;
            return;
        }

        ArrayPtr<Type> tmp{Realloc(capacity_multiplier * capacity_)};
        std::copy(std::make_move_iterator(tmp.Get()), std::make_move_iterator(tmp.Get() + size_), data_.Get());
        data_[size_] = std::move(item);
        ++size_;
    }

    // Вставляет значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед вставкой значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, const Type& value) {
        size_t dist = static_cast<size_t>(pos - data_.Get());

        if(size_ < capacity_) {
            std::copy_backward(data_.Get() + dist, data_.Get() + size_, data_.Get() + size_ + 1);
            data_[dist] = value;
            ++size_;
            return data_.Get() + dist;
        }

        ArrayPtr<Type> tmp{Realloc(capacity_multiplier * capacity_)};
        std::copy(tmp.Get(), tmp.Get() + dist, data_.Get());
        std::copy_backward(tmp.Get() + dist, tmp.Get() + size_, data_.Get() + 1);
        data_[dist] = value;
        ++size_;

        return data_.Get() + dist;
    }

    // Переносит значение value в позицию pos.
    // Возвращает итератор на вставленное значение
    // Если перед перемещением значения вектор был заполнен полностью,
    // вместимость вектора должна увеличиться, а для вектора вместимостью 0 стать равной 1
    Iterator Insert(ConstIterator pos, Type&& value) {
        size_t dist = static_cast<size_t>(pos - data_.Get());

        if(size_ < capacity_) {
            std::copy_backward(std::make_move_iterator(data_.Get() + dist), std::make_move_iterator(data_.Get() + size_), data_.Get() + size_+1);
            data_[dist] = std::move(value);
            ++size_;
            return data_.Get() + dist;
        }

        ArrayPtr<Type> tmp{Realloc(capacity_multiplier * capacity_)};
        std::copy(std::make_move_iterator(tmp.Get()), std::make_move_iterator(tmp.Get() + dist), data_.Get());
        std::copy_backward(std::make_move_iterator(tmp.Get() + dist), std::make_move_iterator(tmp.Get() + size_), data_.Get() + size_ + 1);
        data_[dist] = std::move(value);
        ++size_;

        return data_.Get() + dist;
    }

    // "Удаляет" последний элемент вектора. Вектор не должен быть пустым
    void PopBack() noexcept {
        if( size_ > 0) {
            --size_;
        }
    }

    // Удаляет элемент вектора в указанной позиции
    Iterator Erase(ConstIterator pos) noexcept {
        if( size_ > 0) {
            ptrdiff_t dist = pos - data_.Get();
            std::copy(std::make_move_iterator(data_.Get() + dist + 1), std::make_move_iterator(data_.Get() + size_), data_.Get() + dist);
            --size_;
            return data_.Get() + dist;
        }
        return data_.Get();
    }

    // Изменяет размер массива.
    // При увеличении размера новые элементы получают значение по умолчанию для типа Type
    void Resize(size_t new_size) {
        if(new_size <= size_) {
            size_ = new_size;
            return;
        }

        if(new_size < capacity_) {
            std::fill(data_.Get() + size_, data_.Get() + new_size, Type{});
            size_ = new_size;
            return;
        }

        ArrayPtr<Type> tmp{Realloc(capacity_multiplier * new_size)};
        std::copy(std::make_move_iterator(tmp.Get()), std::make_move_iterator(tmp.Get() + size_), data_.Get());
        std::fill(data_.Get() + size_, data_.Get() + new_size, Type{});
        size_ = new_size;
    }

    // Резервирует заданный размер памяти
    void Reserve(size_t new_capacity) {
        if( capacity_ < new_capacity) {
            ArrayPtr<Type> tmp{Realloc(new_capacity)};
            std::copy(tmp.Get(), tmp.Get() + size_, data_.Get());
        }
    }

    // Обменивает значение с другим вектором
    void swap(SimpleVector& other) noexcept {
        data_.swap(other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
    }

    // Возвращает количество элементов в массиве
    size_t GetSize() const noexcept {
        return size_;
    }

    // Возвращает вместимость массива
    size_t GetCapacity() const noexcept {
        return capacity_;
    }

    // Сообщает, пустой ли массив
    bool IsEmpty() const noexcept {
        return size_ == 0;
    }

    // Возвращает ссылку на элемент с индексом index
    Type& operator[](size_t index) noexcept {
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    const Type& operator[](size_t index) const noexcept {
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    Type& At(size_t index) {
        if(index >= size_) {
            throw std::out_of_range("index out of range in SimpleVector::At");
        }
        return data_[index];
    }

    // Возвращает константную ссылку на элемент с индексом index
    // Выбрасывает исключение std::out_of_range, если index >= size
    const Type& At(size_t index) const {
        if(index >= size_) {
            throw std::out_of_range("index out of range in SimpleVector::At(const Type)");
        }
        return data_[index];
    }

    // Обнуляет размер массива, не изменяя его вместимость
    void Clear() noexcept {
        size_ = 0;
    }

    // Возвращает итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator begin() noexcept {
        return data_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    Iterator end() noexcept {
        return data_.Get() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator begin() const noexcept {
        return data_.Get();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator end() const noexcept {
        return data_.Get() + size_;
    }

    // Возвращает константный итератор на начало массива
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cbegin() const noexcept {
        return begin();
    }

    // Возвращает итератор на элемент, следующий за последним
    // Для пустого массива может быть равен (или не равен) nullptr
    ConstIterator cend() const noexcept {
        return end();
    }

private:
    ArrayPtr<Type> data_;
    size_t size_ = 0;
    size_t capacity_ = 0;
    const  size_t capacity_multiplier = 2;

    ArrayPtr<Type> Realloc(size_t new_capacity) {
        if( new_capacity == 0) {
            new_capacity = 1;
        }
        ArrayPtr<Type> tmp{new_capacity};
        data_.swap(tmp);
        capacity_ = new_capacity;
        return tmp;
   }
};

template <typename Type>
inline bool operator==(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    if(lhs.GetSize() != lhs.GetSize()) {
        return false;
    }
    return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
inline bool operator!=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs == rhs);
}

template <typename Type>
inline bool operator<(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

template <typename Type>
inline bool operator<=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return (lhs == rhs) || (lhs < rhs);
}

template <typename Type>
inline bool operator>(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs <= rhs);
}

template <typename Type>
inline bool operator>=(const SimpleVector<Type>& lhs, const SimpleVector<Type>& rhs) {
    return !(lhs < rhs);
}
