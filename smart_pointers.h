#include <bits/stdc++.h>

int cnt = 0;

template<typename T>
class SharedPtr {
public:
    template<typename U, typename... Args>
    friend auto makeShared(Args&&... args);

    template<typename U, typename... Args, typename Alloc>
    friend auto allocateShared(Args&&... args, const Alloc& alloc);

    template <typename U>
    friend class WeakPtr;

    template <typename U>
    friend class SharedPtr;

    struct BaseControlBlock {
        size_t shared_count = 0;
        size_t weak_count = 0;

        virtual T* get_ptr() = 0;
        virtual void destroy() = 0;
        virtual void deallocate() = 0;
        virtual ~BaseControlBlock() = default;
    };

    template<typename Deleter, typename Alloc>
    struct ControlBlockDirect : public BaseControlBlock {
        T* ptr = nullptr;
        Deleter deleter;
        Alloc alloc;

        ControlBlockDirect(T* object, const Deleter& deleter, const Alloc& alloc) :
                ptr(object), deleter(deleter), alloc(alloc) {}

        T* get_ptr() override {
            return ptr;
        }

        void destroy() override {
            deleter(ptr);
            ptr = nullptr;
        }

        void deallocate() override {
            using allocTraits = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockDirect<Deleter, Alloc> >;
            allocTraits(alloc).deallocate(this, 1);
            alloc.~Alloc();
            deleter.~Deleter();
        }
    };

    template<typename Alloc>
    struct ControlBlockMakeShared : public BaseControlBlock {
        Alloc alloc;
        T object;

        template<typename... Args>
        ControlBlockMakeShared(Alloc alloc, Args&&... args) :
                alloc(alloc), object(std::forward<Args>(args)...) {}

        T* get_ptr() override {
            return &object;
        }

        void destroy() override {
            std::allocator_traits<Alloc>::destroy(alloc, get_ptr());
        }

        void deallocate() override {
            using allocTraits = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockMakeShared<Alloc> >;
            allocTraits(alloc).deallocate(this, 1);
            alloc.~Alloc();
        }
    };

    BaseControlBlock* cb = nullptr;
    T* ptr = nullptr;

    SharedPtr(BaseControlBlock* cb) : cb(cb), ptr(cb->get_ptr()) {
        cb->shared_count++;
    }

public:
    SharedPtr() {}

    template<typename Deleter = std::default_delete<T>, typename Alloc = std::allocator<T> >
    SharedPtr(T* ptr, const Deleter& deleter = Deleter(), const Alloc& alloc = Alloc()) : ptr(reinterpret_cast<T*>(ptr)) {
        using AllocTraits = std::allocator_traits<Alloc>;
        using ControlBlockAllocator = typename AllocTraits::template rebind_alloc<ControlBlockDirect<Deleter, Alloc>>;
        using ControlBlockAllocatorTraits = typename AllocTraits::template rebind_traits<ControlBlockDirect<Deleter, Alloc>>;

        ControlBlockAllocator allocControlBlock = alloc;
        auto p = ControlBlockAllocatorTraits::allocate(allocControlBlock, 1);
        new(p) ControlBlockDirect<Deleter, Alloc>(ptr, deleter, allocControlBlock);
        cb = p;
        cb->shared_count = 1;
    }

    SharedPtr(const SharedPtr& other) noexcept : cb(other.cb), ptr(other.ptr) {
        if (cb != nullptr)
            cb->shared_count++;
    }

    template<typename U>
    SharedPtr(const SharedPtr<U>& other) noexcept : cb(reinterpret_cast<SharedPtr<T>::BaseControlBlock*>(other.cb)), ptr(other.ptr) {
        cb->shared_count++;
    }

    SharedPtr(SharedPtr&& other) noexcept : cb(other.cb), ptr(other.ptr) {
        other.ptr = nullptr;
        other.cb = nullptr;
    }

    template<typename U>
    SharedPtr(SharedPtr<U>&& other) noexcept : cb(other.cb), ptr(other.ptr) {
        other.ptr = nullptr;
        other.cb = nullptr;
    }

    template<typename U>
    void swap(SharedPtr<U>& other) {
        std::swap(ptr, other.ptr);
        std::swap(cb, other.cb);
    }

    const SharedPtr& operator=(const SharedPtr& other) {
        this->~SharedPtr();
        cb = reinterpret_cast<BaseControlBlock*>(other.cb);
        ptr = other.ptr;
        cb->shared_count++;
        return *this;
    }

    template<typename U>
    const SharedPtr& operator=(const SharedPtr<U>& other) {
        this->~SharedPtr();
        cb = reinterpret_cast<BaseControlBlock*>(other.cb);
        ptr = other.ptr;
        cb->shared_count++;
        return *this;
    }

    const SharedPtr& operator=(SharedPtr&& other) {
        this->~SharedPtr();
        cb = std::move(reinterpret_cast<BaseControlBlock*>(other.cb));
        ptr = std::move(other.ptr);
        other.ptr = nullptr;
        other.cb = nullptr;
        return *this;
    }

    template<typename U>
    SharedPtr& operator=(SharedPtr<U>&& other) {
        this->~SharedPtr();
        cb = std::move(reinterpret_cast<BaseControlBlock*>(other.cb));
        ptr = std::move(other.ptr);
        other.ptr = nullptr;
        other.cb = nullptr;
        return *this;
    }

    T& operator*() const {
        if (ptr)
            return *ptr;
        return *(cb->get_ptr());
    }

    size_t use_count() const noexcept {
        return cb->shared_count;
    }

    void reset() {
        *this = SharedPtr<T>();
    }

    template<typename U>
    void reset(U* ptr) {
        *this = ptr;
    }

    T* get() const {
        if (ptr == nullptr && cb == nullptr)
            return nullptr;
        if (ptr)
            return ptr;
        return cb->get_ptr();
    }

    T* operator->() const {
        return get();
    }

    ~SharedPtr() {
        if (cb == nullptr || cb->shared_count == 0)
            return;
        cb->shared_count--;
        if (cb->shared_count == 0) {
            cb->destroy();
            if (cb->weak_count == 0)
                cb->deallocate();
        }
    }
};


template <typename T>
class WeakPtr {
private:
    template<typename U>
    friend class WeakPtr;

    typename SharedPtr<T>::BaseControlBlock* cb = nullptr;

public:
    WeakPtr() {}

    WeakPtr(const WeakPtr<T>& other) : cb(reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb)) {
        cb->weak_count++;
    }

    template<typename U>
    WeakPtr(const WeakPtr<U>& other) : cb(reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb)) {
        cb->weak_count++;
    }

    template<typename U>
    WeakPtr(WeakPtr<U>&& other) : cb(reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb)) {
        other.cb = nullptr;
    }

    WeakPtr(const SharedPtr<T>& other) : cb(other.cb) {
        cb->weak_count++;
    }

    template<typename U>
    WeakPtr(const SharedPtr<U>& other) : cb(reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb)) {
        cb->weak_count++;
    }

    template<typename U>
    const WeakPtr<T>& operator=(const WeakPtr<U>& other) {
        this->~WeakPtr();
        cb = reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb);
        cb->weak_cnt++;
        return *this;
    }

    const WeakPtr<T>& operator=(const SharedPtr<T>& other) {
        this->~WeakPtr();
        cb = reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb);
        cb->weak_count++;
        return *this;
    }

    template<typename U>
    const WeakPtr<T>& operator=(const SharedPtr<U>& other) {
        this->~WeakPtr();
        cb = reinterpret_cast<typename SharedPtr<T>::BaseControlBlock*>(other.cb);
        cb->weak_count++;
        return *this;
    }

    bool expired() const {
        if (cb == nullptr)
            return true;
        return !(cb->shared_count);
    }

    auto lock() const {
        if (expired())
            return SharedPtr<T>();
        return SharedPtr<T>(cb);
    }

    size_t use_count() const {
        if (expired())
            return 0;
        return cb->shared_count;
    }

    ~WeakPtr() {
        if (cb == nullptr)
            return;
        if (cb->weak_count)
            cb->weak_count--;
        if (cb->weak_count == 0 && cb->shared_count == 0)
            cb->deallocate();
    }
};

template <typename V, typename Alloc, typename... Args>
SharedPtr<V> allocateShared(Alloc alloc, Args&&... args) {
    using AllocTraits = typename std::template allocator_traits<Alloc>;
    using SharedAlloc = typename AllocTraits::template rebind_alloc<typename SharedPtr<V>::template ControlBlockMakeShared<Alloc> >;
    using SharedAllocTraits = typename std::template allocator_traits<SharedAlloc>;
    SharedAlloc sharedAlloc = alloc;
    typename SharedPtr<V>:: template ControlBlockMakeShared<Alloc>* pt = SharedAllocTraits::allocate(sharedAlloc, 1);
    SharedAllocTraits::construct(sharedAlloc, pt, std::move(alloc), std::forward<Args>(args)...);
    SharedPtr<V> ptr = pt;
    return std::move(ptr);
}

template <typename T, typename... Args>
auto makeShared(Args&&... args) {
    return allocateShared<T>(std::allocator<T>(), std::forward<Args>(args)...);
}


