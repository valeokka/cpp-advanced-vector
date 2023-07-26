#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = other.buffer_;
        capacity_ = other.capacity_;
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory &&rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
            rhs.Deallocate(rhs.buffer_);
            rhs.capacity_ = 0;
            rhs.buffer_ = nullptr;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept { 
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    [[nodiscard]] size_t Capacity() const {
        return capacity_;
    }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    //Инициализирует size элементов вектора значением по умолчанию. capacity_ и size_ равны size 
    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  {
        //std::uninitialized_fill_n(data_.GetAddress(), size_, T());
        std::uninitialized_value_construct_n(data_.GetAddress(), size_);
    }
    
    //Инициализирует size значением элементов other. Capacity_ и size_ равны other.size_.
    //Позволяет инициализировать память по мере ее необходимости
    //В данный момент используется лишь нужная для хранения всеъ элементов 
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    //Инициализирует other всеми элементами other. Capacity_ и size_ равны other.size_.
    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_))
        , size_(std::move(other.size_)) {
        RawMemory<T> empty;
        other.data_ = std::move(empty);
        other.size_ = 0;
    }
    //Инициализирует other всеми элементами other. Capacity_ и size_ равны other.size_.   
    Vector& operator=(Vector&& other) noexcept{
        if(this != &other){
            data_ = std::move(other.data_); // RawMemory& operator=(RawMemory &&rhs) разрушает объекты other.data_
            size_ = std::move(other.size_);

            other.size_ = 0;
        }
        return *this;
    }

    Vector& operator=(const Vector& other){
        if(this != &other){
          if(data_.Capacity() < other.size_){
                Vector<T> temp(other);
                Swap(temp);
            }else{
                if(size_ > other.size_){
                    std::destroy(data_ + other.size_, data_ + size_);
                    for(size_t i = 0; i < other.size_; ++i){ data_[i] = other.data_[i];}

                    size_ = other.size_;
                }else {
                    for(size_t i = 0; i < size_; ++i){ data_[i] = other.data_[i];}
                    std::uninitialized_copy_n(other.data_ + size_, other.size_ - size_, data_ + size_);

                    size_ = other.size_;
                }
            }
        }
        return *this;
    }

    ~Vector() {
        //Вызывает деструктор у всех существующих объектов вектора
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept{ return data_.GetAddress();}
    iterator end() noexcept{ return data_.GetAddress() + size_;}
    const_iterator begin() const noexcept{ return data_.GetAddress();}
    const_iterator end() const noexcept{ return data_.GetAddress() + size_;}
    const_iterator cbegin() const noexcept{ return data_.GetAddress();}
    const_iterator cend() const noexcept{ return data_.GetAddress() + size_;}

    void Swap(Vector<T>& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }   

    [[nodiscard]] size_t Size() const noexcept {
        return size_;
    }
    [[nodiscard]] size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) { return;}

        RawMemory<T> new_data{new_capacity};
        TransferAndSwap(new_data);
    }

    void Resize(size_t new_size){
        if(new_size < size_){
            std::destroy_n(data_ + new_size, size_ - new_size);
        }else{
            Reserve(new_size > Capacity() ? new_size : size_);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }
    
    void PushBack(const T& value){ EmplaceBack(value);}
    void PushBack(T&& value){ EmplaceBack(std::move(value));}
    iterator Insert(const_iterator pos, const T& value){ return Emplace(pos, value);}
    iterator Insert(const_iterator pos, T&& value){ return Emplace(pos, std::move(value));}

    //Добавляет элемент в конец вектора. Если size_ == capacity_ переаллоцирует память.
    //Чтобы избежать неопределенного поведения в случае если value ссылка на объект вектора
    //Сначала добавляет его в new_data, после перемещает старую data_
    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if(size_ == Capacity()){
            RawMemory<T> new_data{size_ == 0 ? 1 : size_ *2};
            new (new_data + size_) T(std::forward<Args>(args)...);
            try{
                TransferAndSwap(new_data);
            }catch(...){
                std::destroy_n(new_data.GetAddress() + size_, 1);
                throw;
            }
        }else{
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_-1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        size_t num_position = pos - begin();
        if(size_ == Capacity()){
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ *2);
            
            new(new_data + num_position) T(std::forward<Args>(args)...);
            
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), num_position, new_data.GetAddress());
                std::uninitialized_move_n(begin() + num_position, size_ - num_position, new_data + num_position + 1);

            } else {
                try{
                    std::uninitialized_copy_n(begin(), num_position, new_data.GetAddress());
                    std::uninitialized_copy_n(begin() + num_position, size_ - num_position, new_data + num_position + 1);
                }catch(...){
                    std::destroy_n(new_data.GetAddress(), num_position + 1);
                    throw;
                }
            }
        
        std::destroy_n(begin(),size_);
        data_.Swap(new_data);

        }else{
            
            if(pos == end()){
                new (end()) T(std::forward<Args>(args)...);
            }else{
                T temp(std::forward<Args>(args)...);
                
                new (end()) T(std::forward<T>(data_[size_ -1]));
                try{
                std::move_backward(begin() + num_position, end() - 1, end());
                *(begin() + num_position) = std::forward<T>(temp);
                }catch(...){
                    std::destroy_n(end(),1);
                    throw;
                }
            }
        }
        ++size_;
        return data_ + num_position;
    }
    //Удаляет последний элемент из вектора
    void PopBack() noexcept{
        assert(size_ > 0);
        std::destroy_n(data_ + size_ - 1, 1);
        --size_;
    }
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>){
        assert(begin() <= pos && pos < end());
        size_t num_position = pos - begin();
        if(pos == end()){
            PopBack();
            return end();
        }
        std::move(begin() + num_position + 1, end(), begin() + num_position);
        PopBack();
        return begin() + num_position;
    }
private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void TransferAndSwap(RawMemory<T>& new_data){
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(begin(), size_, new_data.GetAddress());
        }
        
        std::destroy_n(begin(),size_);

        data_.Swap(new_data);
    }
};