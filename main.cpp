/*
 * Copyright (C) 2026 mcreatorLoginDanila also known as NotAndrey
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#define _WIN32_WINNT 0x0601
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shellapi.h>
#include <cmath>
extern "C" {
#include "lib/lua.h"
#include "lib/lauxlib.h"
#include "lib/lualib.h"
#pragma warning(push)
#pragma warning(disable: 4002 4003)
#include "lib/wasm3/wasm3.h"
#include "lib/wasm3/m3_env.h"
#pragma warning(pop)
}
#include "lume_plugin.h"
#include <objidl.h>
#include <wincrypt.h>
#include <algorithm>
namespace Gdiplus {
    using std::min;
    using std::max;
}
#include <cstdio>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#include <shlwapi.h>
#include <GL/gl.h>
#include <GL/glu.h>
template<typename T>
class LumeVector {
private:
    T* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
public:
    using iterator = T*;
    using const_iterator = const T*;
    LumeVector() = default;
    explicit LumeVector(size_t count) {resize(count);}
    LumeVector(size_t count, const T& val) {
        reserve(count);
        for (size_t i = 0; i < count; ++i) new (&m_data[i]) T(val);
        m_size = count;
    }
    LumeVector(const LumeVector& other) {
        reserve(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i) new (&m_data[i]) T(other.m_data[i]);
        m_size = other.m_size;
    }
    LumeVector(LumeVector&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }
    LumeVector& operator=(const LumeVector& other) {
        if (this != &other) {
            clear(); reserve(other.m_size);
            for (size_t i = 0;i < other.m_size; ++i) new (&m_data[i]) T(other.m_data[i]);
            m_size = other.m_size;
        }
        return *this;
    }
    LumeVector& operator=(LumeVector&& other) noexcept {
        if (this != &other) {
            clear();
            if (m_data) HeapFree(GetProcessHeap(), 0, m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }
    ~LumeVector() {
        clear();
        if (m_data) HeapFree(GetProcessHeap(), 0, m_data);
    }
    void reserve(size_t newCap) {
        if (newCap <= m_capacity) return;
        HANDLE hHeap = GetProcessHeap();
        T* newData = (T*)HeapAlloc(hHeap, 0, newCap * sizeof(T));
        if (m_data) {
            for (size_t i = 0; i < m_size; ++i) {
                new (&newData[i]) T(static_cast<T&&>(m_data[i]));
                m_data[i].~T();
            }
            HeapFree(hHeap, 0, m_data);
        }
        m_data = newData;
        m_capacity = newCap;
    }
    void resize(size_t newSize) {
        if (newSize > m_capacity) reserve(newSize);
        if (newSize > m_size) {
            for (size_t i = m_size; i < newSize; ++i) new (&m_data[i]) T();
        }
        else if (newSize < m_size) {
            for (size_t i = newSize; i < m_size; ++i) m_data[i].~T();
        }
        m_size = newSize;
    }
    void push_back(const T& val) {
        if (m_size == m_capacity) reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        new (&m_data[m_size]) T(val);
        m_size++;
    }
    void push_back(T&& val) {
        if (m_size == m_capacity) reserve(m_capacity == 0 ? 8 : m_capacity * 2);
        new (&m_data[m_size]) T(static_cast<T&&>(val));
        m_size++;
    }
    iterator erase(iterator it) {
        if (it >= begin() && it < end()) {
            it->~T();
            size_t idx = it - begin();
            for (size_t i = idx; i < m_size - 1; ++i) {
                new (&m_data[i]) T(static_cast<T&&>(m_data[i + 1]));
                m_data[i + 1].~T();
            }
            m_size--;
            return begin() + idx;
        }
        return end();
    }
    void clear() {for (size_t i = 0; i < m_size; ++i) m_data[i].~T(); m_size = 0;}
    size_t size() const {return m_size;}
    bool empty() const {return m_size == 0;}
    size_t capacity() const {return m_capacity;}
    T* data() {return m_data;}
    const T* data() const {return m_data;}
    T& operator[](size_t idx) {return m_data[idx];}
    const T& operator[](size_t idx) const {return m_data[idx];}
    T& front() {return m_data[0];}
    T& back() {return m_data[m_size - 1];}
    iterator begin() {return m_data;}
    iterator end() {return m_data + m_size;}
    const_iterator begin() const {return m_data;}
    const_iterator end() const {return m_data + m_size;}
};
template<typename T1, typename T2>
struct LumePair {
    T1 first;
    T2 second;
    LumePair() = default;
    LumePair(const T1& a, const T2& b) : first(a), second(b) {}
};
template<typename K, typename V>
class LumeMap {
private:
    LumeVector<LumePair<K, V>> m_data;
public:
    using iterator = LumePair<K, V>*;
    LumeMap() = default;
    V& operator[](const K& key) {
        for (auto& p : m_data) if (p.first == key) return p.second;
        m_data.push_back(LumePair<K, V>(key, V()));
        return m_data.back().second;
    }
    iterator find(const K& key) {
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if (it->first == key) return it;
        }
        return m_data.end();
    }
    void erase(iterator it) {
        if (it >= m_data.begin() && it < m_data.end()) m_data.erase(it);
    }
    void erase(const K& key) {
        auto it = find(key);
        if (it != end()) m_data.erase(it);
    }
    void clear() { m_data.clear(); }
    iterator begin() { return m_data.begin(); }
    iterator end() { return m_data.end(); }
};
template<typename Iter, typename Compare>
void LumeStableSort(Iter first, Iter last, Compare comp) {
    if (first == last) return;
    for (Iter i = first + 1; i != last; ++i) {
        auto val = *i;
        Iter j = i;
        while (j > first && comp(val, *(j - 1))) {
            *j = *(j - 1);
            --j;
        }
        *j = val;
    }
}
template<typename T>
class LumeBasicString {
private:
    T* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
    void grow(size_t minCap) {
        size_t newCap = m_capacity == 0 ? 15 : m_capacity * 2;
        if (newCap < minCap) newCap = minCap;
        HANDLE hHeap = GetProcessHeap();
        if (!m_data) m_data = (T*)HeapAlloc(hHeap, 0, (newCap + 1) * sizeof(T));
        else m_data = (T*)HeapReAlloc(hHeap, 0, m_data, (newCap + 1) * sizeof(T));
        m_capacity = newCap;
        m_data[m_size] = 0;
    }
public:
    static const size_t npos = (size_t)-1;
    using iterator = T*;
    using const_iterator = const T*;
    LumeBasicString() { grow(15); }
    LumeBasicString(const T* str) {
        size_t len = 0; while (str && str[len]) len++;
        if (len > 0) {
            grow(len);
            append(str, len);
        }
        else grow(15);
    }
    LumeBasicString(const T* str, size_t len) {
        if (len > 0) {
            grow(len);
            append(str, len);
        }
        else grow(15);
    }
    LumeBasicString(size_t count, T ch) {
        grow(count);
        for (size_t i = 0; i < count; ++i) m_data[i] = ch;
        m_size = count;
        m_data[m_size] = 0;
    }
    LumeBasicString(const LumeBasicString& o) {
        if (o.m_size > 0) {
            grow(o.m_size);
            append(o.m_data, o.m_size);
        }
        else grow(15);
    }
    LumeBasicString(LumeBasicString&& o) noexcept : m_data(o.m_data), m_size(o.m_size), m_capacity(o.m_capacity) {
        o.m_data = nullptr; o.m_size = 0; o.m_capacity = 0;
    }
    ~LumeBasicString() {
        if (m_data) HeapFree(GetProcessHeap(), 0, m_data);
    }
    LumeBasicString& operator=(const LumeBasicString& o) {
        if (this != &o) {
            clear();
            append(o.m_data, o.m_size);
        }
        return *this;
    }
    LumeBasicString& operator=(LumeBasicString&& o) noexcept {
        if (this != &o) {
            if (m_data) HeapFree(GetProcessHeap(), 0, m_data);
            m_data = o.m_data; m_size = o.m_size; m_capacity = o.m_capacity;
            o.m_data = nullptr; o.m_size = 0; o.m_capacity = 0;
        }
        return *this;
    }
    LumeBasicString& operator=(const T* str) {
        clear();
        size_t len = 0; while (str && str[len]) len++;
        if (len > 0) append(str, len);
        return *this;
    }
    void append(const T* str, size_t len) {
        if (len == 0) return;
        if (m_size + len > m_capacity) grow(m_size + len);
        for (size_t i = 0; i < len; ++i) m_data[m_size + i] = str[i];
        m_size += len;
        m_data[m_size] = 0;
    }
    LumeBasicString& operator+=(const LumeBasicString& o) { append(o.m_data, o.size()); return *this; }
    LumeBasicString& operator+=(const T* str) {
        size_t len = 0; while (str && str[len]) len++;
        append(str, len); return *this;
    }
    LumeBasicString& operator+=(T ch) {append(&ch, 1); return *this;}
    friend LumeBasicString operator+(const LumeBasicString& lhs, const LumeBasicString& rhs) {
        LumeBasicString res = lhs; res += rhs; return res;
    }
    friend LumeBasicString operator+(const LumeBasicString& lhs, const T* rhs) {
        LumeBasicString res = lhs;
        res += rhs;
        return res;
    }
    friend LumeBasicString operator+(const T* lhs, const LumeBasicString& rhs) {
        LumeBasicString res(lhs);
        res += rhs;
        return res;
    }
    friend LumeBasicString operator+(const LumeBasicString& lhs, T rhs) {
        LumeBasicString res = lhs;
        res += rhs;
        return res;
    }
    bool operator==(const LumeBasicString& o) const {
        if (m_size != o.m_size) return false;
        if (!m_data || !o.m_data) return m_size == o.m_size;
        for (size_t i = 0; i < m_size; ++i) if (m_data[i] != o.m_data[i]) return false;
        return true;
    }
    bool operator==(const T* str) const {
        size_t i = 0;
        if (!m_data) return str == nullptr || str[0] == 0;
        while (i < m_size && str[i]) {
            if (m_data[i] != str[i]) return false;
            i++;
        }
        return i == m_size && str[i] == 0;
    }
    bool operator!=(const LumeBasicString& o) const {return !(*this == o);}
    bool operator!=(const T* str) const {return !(*this == str);}
    friend bool operator==(const T* lhs, const LumeBasicString& rhs) {return rhs == lhs;}
    friend bool operator!=(const T* lhs, const LumeBasicString& rhs) {return rhs != lhs;}
    bool operator<(const LumeBasicString& o) const {
        size_t minLen = m_size < o.m_size ? m_size : o.m_size;
        for (size_t i = 0; i < minLen; ++i) {
            if (m_data[i] < o.m_data[i]) return true;
            if (m_data[i] > o.m_data[i]) return false;
        }
        return m_size < o.m_size;
    }
    const T* c_str() const {return m_data ? m_data : (const T*)"\0\0";}
    T* data() {return m_data;}
    const T* data() const {return m_data;}
    size_t size() const {return m_size;}
    size_t length() const {return m_size;}
    bool empty() const {return m_size == 0;}
    iterator begin() {return m_data;}
    iterator end() {return m_data + m_size;}
    const_iterator begin() const {return m_data;}
    const_iterator end() const {return m_data + m_size;}
    size_t rfind(const T* str, size_t pos = npos) const {
        size_t len = 0; while (str && str[len]) len++;
        if (len == 0) return pos < m_size ? pos : m_size;
        if (m_size < len || !m_data) return npos;
        size_t start = (pos < m_size - len) ? pos : m_size - len;
        for (size_t i = start + 1; i > 0; --i) {
            bool match = true;
            for (size_t j = 0; j < len; ++j) {
                if (m_data[i - 1 + j] != str[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return i - 1;
        }
        return npos;
    }
    size_t find_last_of(T ch) const {
        if (!m_data) return npos;
        for (size_t i = m_size; i > 0; --i) if (m_data[i - 1] == ch) return i - 1;
        return npos;
    }
    iterator erase(iterator it) {
        if (m_data && it >= m_data && it < m_data + m_size) {
            size_t pos = it - m_data;
            erase(pos, 1);
            return m_data + pos;
        }
        return end();
    }
    void clear() { m_size = 0; if (m_data) m_data[0] = 0; }
    void resize(size_t n) {
        if (n > m_capacity) grow(n);
        if (n > m_size && m_data) {for (size_t i = m_size; i < n; ++i) m_data[i] = 0;}
        m_size = n;
        if (m_data) m_data[m_size] = 0;
    }
    void reserve(size_t n) {if (n > m_capacity) grow(n);}
    T& operator[](size_t i) {return m_data[i];}
    const T& operator[](size_t i) const {return m_data[i];}
    T& front() {return m_data[0];}
    T& back() {return m_data[m_size - 1];}
    void push_back(T ch) {append(&ch, 1);}
    void pop_back() {
        if (m_size > 0 && m_data) {
            m_size--;
            m_data[m_size] = 0;
        }
    }
    LumeBasicString substr(size_t pos = 0, size_t count = npos) const {
        if (pos >= m_size || !m_data) return LumeBasicString();
        size_t len = m_size - pos;
        if (count < len) len = count;
        return LumeBasicString(m_data + pos, len);
    }
    size_t find(T ch, size_t pos = 0) const {
        if (!m_data) return npos;
        for (size_t i = pos; i < m_size; ++i) if (m_data[i] == ch) return i;
        return npos;
    }
    size_t find(const LumeBasicString& str, size_t pos = 0) const {return find(str.c_str(), pos);}
    size_t find(const T* str, size_t pos = 0) const {
        size_t len = 0; while (str && str[len]) len++;
        if (len == 0) return pos <= m_size ? pos : npos;
        if (pos + len > m_size || !m_data) return npos;
        for (size_t i = pos; i <= m_size - len; ++i) {
            bool match = true;
            for (size_t j = 0; j < len; ++j) {
                if (m_data[i + j] != str[j]) { match = false; break; }
            }
            if (match) return i;
        }
        return npos;
    }
    size_t find_last_of(const T* chars) const {
        if (!m_data || !chars) return npos;
        for (size_t i = m_size; i > 0; --i) {
            size_t j = 0;
            while (chars[j]) {
                if (m_data[i - 1] == chars[j]) return i - 1;
                j++;
            }
        }
        return npos;
    }
    void insert(size_t pos, const LumeBasicString& str) {
        if (pos > m_size) pos = m_size;
        size_t len = str.size();
        if (len == 0) return;
        if (m_size + len > m_capacity) grow(m_size + len);
        if (!m_data) return;
        for (size_t i = m_size; i > pos; --i) m_data[i + len - 1] = m_data[i - 1];
        for (size_t i = 0; i < len; ++i) m_data[pos + i] = str[i];
        m_size += len;
        m_data[m_size] = 0;
    }
    void insert(size_t pos, const T* str) {
        size_t len = 0; while (str && str[len]) len++;
        if (len > 0) insert(pos, LumeBasicString(str, len));
    }
    void erase(size_t pos = 0, size_t count = npos) {
        if (pos >= m_size || !m_data) return;
        if (count > m_size - pos) count = m_size - pos;
        for (size_t i = pos + count; i < m_size; ++i) m_data[i - count] = m_data[i];
        m_size -= count;
        m_data[m_size] = 0;
    }
    void remove_char(T ch) {
        if (!m_data) return;
        size_t write_idx = 0;
        for (size_t i = 0; i < m_size; ++i) {
            if (m_data[i] != ch) m_data[write_idx++] = m_data[i];
        }
        m_size = write_idx;
        m_data[m_size] = 0;
    }
};
struct LumeMutex {
    SRWLOCK lock = SRWLOCK_INIT;
    void lock_mutex() { AcquireSRWLockExclusive(&lock); }
    void unlock_mutex() { ReleaseSRWLockExclusive(&lock); }
};
template<typename Mutex>
struct LumeLockGuard {
    Mutex& m;
    LumeLockGuard(Mutex& mut) : m(mut) { m.lock_mutex(); }
    ~LumeLockGuard() { m.unlock_mutex(); }
};
template<typename T>
class LumeSharedPtr {
private:
    struct ControlBlock {
        T* ptr;
        volatile LONG ref_count;
    };
    ControlBlock* cb;
    void add_ref() {
        if (cb) InterlockedIncrement(&cb->ref_count);
    }
    void release() {
        if (cb) {
            if (InterlockedDecrement(&cb->ref_count) == 0) {
                if (cb->ptr) delete cb->ptr;
                HeapFree(GetProcessHeap(), 0, cb);
            }
        }
    }
public:
    LumeSharedPtr() : cb(nullptr) {}
    LumeSharedPtr(std::nullptr_t) : cb(nullptr) {}
    explicit LumeSharedPtr(T* p) {
        if (p) {
            cb = (ControlBlock*)HeapAlloc(GetProcessHeap(), 0, sizeof(ControlBlock));
            cb->ptr = p;
            cb->ref_count = 1;
        }
        else {
            cb = nullptr;
        }
    }
    LumeSharedPtr(const LumeSharedPtr& o) : cb(o.cb) {add_ref();}
    LumeSharedPtr(LumeSharedPtr&& o) noexcept : cb(o.cb) {o.cb = nullptr;}
    ~LumeSharedPtr() {release();}
    LumeSharedPtr& operator=(const LumeSharedPtr& o) {
        if (this != &o) { release(); cb = o.cb; add_ref(); }
        return *this;
    }
    LumeSharedPtr& operator=(LumeSharedPtr&& o) noexcept {
        if (this != &o) {release(); cb = o.cb; o.cb = nullptr;}
        return *this;
    }
    LumeSharedPtr& operator=(std::nullptr_t) {
        reset();
        return *this;
    }
    T* get() const {return cb ? cb->ptr : nullptr;}
    T* operator->() const {return get();}
    T& operator*() const {return *get();}
    explicit operator bool() const {return get() != nullptr;}
    void reset() {release(); cb = nullptr;}
};
template<typename T, typename... Args>
LumeSharedPtr<T> lume_make_shared(Args&&... args) {
    T* ptr = new T(static_cast<Args&&>(args)...);
    return LumeSharedPtr<T>(ptr);
}
static volatile LONG g_isShuttingDown = 0;
class LumeThread {
private:
    HANDLE m_handle = nullptr;
    template <typename F>
    struct ThreadData {F func;};
    template <typename F>
    static DWORD WINAPI ThreadProc(LPVOID lpParam) {
        auto* data = static_cast<ThreadData<F>*>(lpParam);
        if (InterlockedCompareExchange(&g_isShuttingDown, 0, 0) == 0) {
            data->func();
        }
        data->~ThreadData();
        HeapFree(GetProcessHeap(), 0, data);
        return 0;
    }
public:
    LumeThread() = default;
    template <typename F>
    explicit LumeThread(F&& f) {
        HANDLE hHeap = GetProcessHeap();
        auto* data = (ThreadData<F>*)HeapAlloc(hHeap, 0, sizeof(ThreadData<F>));
        if (data) {
            new (data) ThreadData<F>{ static_cast<F&&>(f) };
            m_handle = CreateThread(nullptr, 0, ThreadProc<F>, data, 0, nullptr);
            if (!m_handle) {
                data->~ThreadData();
                HeapFree(hHeap, 0, data);
            }
        }
    }
    ~LumeThread() { detach(); }
    LumeThread(const LumeThread&) = delete;
    LumeThread& operator=(const LumeThread&) = delete;
    LumeThread(LumeThread&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    LumeThread& operator=(LumeThread&& other) noexcept {
        if (this != &other) {
            detach();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    bool joinable() const {return m_handle != nullptr;}
    void join() {
        if (m_handle) {
            WaitForSingleObject(m_handle, INFINITE);
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
    void detach() {
        if (m_handle) {
            CloseHandle(m_handle);
            m_handle = nullptr;
        }
    }
    template <typename F>
    static void RunAsync(F&& f) {
        HANDLE hHeap = GetProcessHeap();
        auto* data = (ThreadData<F>*)HeapAlloc(hHeap, 0, sizeof(ThreadData<F>));
        if (data) {
            new (data) ThreadData<F>{static_cast<F&&>(f)};
            if (!QueueUserWorkItem(ThreadProc<F>, data, WT_EXECUTEDEFAULT)) {
                data->~ThreadData();
                HeapFree(hHeap, 0, data);
            }
        }
    }
};
template<typename T> T lume_min(T a, T b) {return a < b ? a : b;}
template<typename T> T lume_max(T a, T b) {return a > b ? a : b;}
class LumeAction {
private:
    struct Concept {
        virtual ~Concept() {}
        virtual void invoke() = 0;
        virtual Concept* clone() const = 0;
    };
    template<typename F>
    struct Model : Concept {
        F func;
        Model(F f) : func(static_cast<F&&>(f)) {}
        void invoke() override { func(); }
        Concept* clone() const override { return new Model<F>(func); }
    };
    Concept* ptr = nullptr;
public:
    LumeAction() = default;
    template<typename F>
    LumeAction(F f) { ptr = new Model<F>(static_cast<F&&>(f)); }
    LumeAction(const LumeAction& o) { if (o.ptr) ptr = o.ptr->clone(); }
    LumeAction(LumeAction&& o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }
    LumeAction& operator=(const LumeAction& o) {
        if (this != &o) { if (ptr) delete ptr; ptr = o.ptr ? o.ptr->clone() : nullptr; }
        return *this;
    }
    LumeAction& operator=(LumeAction&& o) noexcept {
        if (this != &o) { if (ptr) delete ptr; ptr = o.ptr; o.ptr = nullptr; }
        return *this;
    }
    ~LumeAction() { if (ptr) delete ptr; }
    void operator()() const { if (ptr) ptr->invoke(); }
    explicit operator bool() const { return ptr != nullptr; }
};
using LumeString = LumeBasicString<char>;
using LumeWString = LumeBasicString<wchar_t>;
inline LumeString lume_to_string(int val) {
    char buf[32];
    wsprintfA(buf, "%d", val);
    return LumeString(buf);
}
inline LumeString lume_to_string(unsigned int val) {
    char buf[32];
    wsprintfA(buf, "%u", val);
    return LumeString(buf);
}
inline LumeString lume_to_string(uint64_t val) {
    char buf[64];
    wsprintfA(buf, "%I64u", val);
    return LumeString(buf);
}
LumeWString utf8_to_wstring(const LumeString& str) {
    if (str.empty()) return L"";
    int safeSize = (int)(str.size() > 0x7FFFFFFF ? 0x7FFFFFFF : str.size());
    if (str.size() < 1024) {
        wchar_t buf[1024];
        int len = MultiByteToWideChar(CP_UTF8, 0, str.data(), safeSize, buf, 1023);
        buf[len] = L'\0';
        return LumeWString(buf, len);
    }
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), safeSize, NULL, 0);
    LumeWString wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), safeSize, &wstrTo[0], size_needed);
    return wstrTo;
}
LumeString wstring_to_utf8(const LumeWString& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    LumeString res(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &res[0], size, NULL, NULL);
    return res;
}
LumeString fastReadFile(const LumeString& path) {
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath, MAX_PATH);
    HANDLE hFile = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";
    DWORD size = GetFileSize(hFile, NULL);
    if (size == 0 || size == INVALID_FILE_SIZE) { CloseHandle(hFile); return ""; }
    LumeString res;
    res.resize(size);
    DWORD bytesRead = 0;
    ReadFile(hFile, res.data(), size, &bytesRead, NULL);
    CloseHandle(hFile);
    return res;
}
int DrawTextU(HDC hdc, const char* str, int len, RECT* lprc, UINT format) {
    wchar_t buf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, buf, 4095);
    buf[wlen] = L'\0';
    return DrawTextW(hdc, buf, wlen, lprc, format);
}
BOOL GetTextExtentPoint32U(HDC hdc, const char* str, int len, LPSIZE psizl) {
    wchar_t buf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, buf, 4095);
    buf[wlen] = L'\0';
    return GetTextExtentPoint32W(hdc, buf, wlen, psizl);
}
BOOL TextOutU(HDC hdc, int x, int y, const char* str, int len) {
    wchar_t buf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, len, buf, 4095);
    buf[wlen] = L'\0';
    return TextOutW(hdc, x, y, buf, wlen);
}
BOOL SetWindowTextU(HWND hwnd, const char* str) {
    wchar_t buf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, 4095);
    if (wlen == 0) {wlen = 4095;}
    buf[wlen] = L'\0';
    return SetWindowTextW(hwnd, buf);
}
int MessageBoxU(HWND hWnd, const char* text, const char* caption, UINT type) {
    wchar_t wtext[2048]; MultiByteToWideChar(CP_UTF8, 0, text ? text : "", -1, wtext, 2048);
    wchar_t wcap[512]; MultiByteToWideChar(CP_UTF8, 0, caption ? caption : "", -1, wcap, 512);
    return MessageBoxW(hWnd, wtext, wcap, type);
}
typedef HGLRC(WINAPI* PFN_wglCreateContext)(HDC);
typedef BOOL(WINAPI* PFN_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL(WINAPI* PFN_wglDeleteContext)(HGLRC);
static double g_perfFreqInv = 0.0;
static LARGE_INTEGER g_perfStart = {};
void initHighResTimer() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    g_perfFreqInv = 1.0 / (double)freq.QuadPart;
    QueryPerformanceCounter(&g_perfStart);
}
double getHighResTime() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - g_perfStart.QuadPart) * g_perfFreqInv;
}
namespace GLLoader {
static HMODULE hGL = nullptr;
static PFN_wglCreateContext pfn_wglCreateContext = nullptr;
static PFN_wglMakeCurrent pfn_wglMakeCurrent = nullptr;
static PFN_wglDeleteContext pfn_wglDeleteContext = nullptr;
static bool g_loaded = false;
bool load() {
    if (g_loaded) return hGL != nullptr;
    g_loaded = true;
    char sysDir[MAX_PATH] = {};
    GetSystemDirectoryA(sysDir, MAX_PATH);
    LumeString glPath = LumeString(sysDir) + "\\opengl32.dll";
    hGL = LoadLibraryA(glPath.c_str());
    if (!hGL) return false;
    pfn_wglCreateContext = (PFN_wglCreateContext)GetProcAddress(hGL, "wglCreateContext");
    pfn_wglMakeCurrent = (PFN_wglMakeCurrent)GetProcAddress(hGL, "wglMakeCurrent");
    pfn_wglDeleteContext = (PFN_wglDeleteContext)GetProcAddress(hGL, "wglDeleteContext");
    if (!pfn_wglCreateContext || !pfn_wglMakeCurrent || !pfn_wglDeleteContext) {
        FreeLibrary(hGL);
        hGL = nullptr;
        return false;
    }
    return true;
}
void unload() {
    if (hGL) {
        FreeLibrary(hGL);
        hGL = nullptr;
    }
    g_loaded = false;
}
bool available() {return hGL != nullptr;}
}
void navigateTo(const LumeString& url);
void invalidateContent();
HWND g_mainWnd = nullptr;
HWND g_addressBar = nullptr;
HWND g_statusBar = nullptr;
HWND g_menuBtn = nullptr;
const int TOOLBAR_H = 40;
const int STATUS_H = 24;
#define ID_ADDR 1001
#define ID_GO   1002
#define ID_BACK 1003
#define ID_FWD  1004
#define ID_REF  1005
#define ID_MENU_BTN 1006
#define IDM_GPU_TOGGLE 1100
#define IDM_LUA_HTTP 1101
#define IDM_LUA_MOUSE 1102
#define IDM_HIST_START 2000
#define IDM_PLUGIN_START 3000
#define WM_UPDATE_CONTENT (WM_USER + 1)
#define WM_PAGE_LOADED (WM_USER + 2)
#define WM_NAVIGATE_DEFERRED (WM_USER + 3)
extern bool g_opt_gpu;
bool g_opt_gpu = true;
bool g_opt_lua_http = true;
bool g_opt_lua_mouse = true;
bool g_opt_plugins = true;
ULONG_PTR g_gdiplusToken;
static HDC g_backDC = nullptr;
static HBITMAP g_backBmp = nullptr;
static HBITMAP g_backOld = nullptr;
static int g_backW = 0, g_backH = 0;
static bool g_contentDirty = true;
static bool g_domDirty = true;
static bool g_mouseCaptured = false;
static int g_captureMode = 0;
static bool g_ignoreWarpMouse = false;
static char g_capturedCanvasId[128] = { 0 };
static int g_capturedCanvasX = 0, g_capturedCanvasY = 0;
static int g_capturedCanvasW = 0, g_capturedCanvasH = 0;
static int g_mouseDeltaX = 0, g_mouseDeltaY = 0;
static int g_mouseWheelDelta = 0;
static bool g_fullscreenCanvas = false;
static char g_fullscreenCanvasId[128] = { 0 };
static RECT g_preFullscreenRect = {};
static LONG_PTR g_preFullscreenStyle = 0;
void ensureBackbuffer(HDC hdc, int w, int h) {
    if (g_backDC && g_backW == w && g_backH == h) return;
    if (g_backDC) {
        SelectObject(g_backDC, g_backOld);
        DeleteDC(g_backDC);
        g_backDC = nullptr;
    }
    if (g_backBmp) {
        DeleteObject(g_backBmp);
        g_backBmp = nullptr;
    }
    g_backDC = CreateCompatibleDC(hdc);
    g_backBmp = CreateCompatibleBitmap(hdc, w, h);
    g_backOld = (HBITMAP)SelectObject(g_backDC, g_backBmp);
    g_backW = w;
    g_backH = h;
    g_contentDirty = true;
}
void cleanupBackbuffer() {
    if (g_backDC) {
        SelectObject(g_backDC, g_backOld);
        DeleteDC(g_backDC);
        g_backDC = nullptr;
    }
    if (g_backBmp) {
        DeleteObject(g_backBmp);
        g_backBmp = nullptr;
    }
    g_backOld = nullptr;
    g_backW = g_backH = 0;
}
void invalidateDOM() {
    if (!g_mainWnd) return;
    g_domDirty = true;
    g_contentDirty = true;
    RECT cr; GetClientRect(g_mainWnd, &cr);
    RECT contentRect = {0, TOOLBAR_H, cr.right, cr.bottom - STATUS_H};
    InvalidateRect(g_mainWnd, &contentRect, FALSE);
}
void invalidateContent() {
    if (!g_mainWnd) return;
    g_contentDirty = true;
    g_domDirty = true;
    RECT cr;
    GetClientRect(g_mainWnd, &cr);
    RECT contentRect = { 0, TOOLBAR_H, cr.right, cr.bottom - STATUS_H };
    InvalidateRect(g_mainWnd, &contentRect, FALSE);
}
void invalidateRect(int x, int y, int w, int h) {
    if (!g_mainWnd) return;
    RECT r = { x, TOOLBAR_H + y, x + w, TOOLBAR_H + h };
    InvalidateRect(g_mainWnd, &r, FALSE);
}
void invalidateGLCanvasRect(int x, int y, int w, int h) {
    if (!g_mainWnd) return;
    RECT r = { x, TOOLBAR_H + y, x + w, TOOLBAR_H + y + h };
    InvalidateRect(g_mainWnd, &r, FALSE);
}
void setStatus(const LumeString& t) {
    if (g_statusBar) SetWindowTextU(g_statusBar, t.c_str());
}
void captureCanvasMouse(const LumeString& canvasId, int mode = 0);
void releaseMouse() {
    if (g_mouseCaptured) {
        g_mouseCaptured = false;
        g_ignoreWarpMouse = false;
        g_capturedCanvasId[0] = '\0';
        ReleaseCapture();
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
}
namespace Script {
    void fireCanvasClick(const LumeString& canvasId, int x, int y, int button);
}
namespace GLCanvas {
    struct GLView;
    LumeSharedPtr<GLView> find(const LumeString& id);
    bool registerClass(HINSTANCE hInst);
    void beginLayoutPass();
    void endLayoutPass();
    void place(const LumeString& id, int x, int y, int w, int h, int scrollY = 0, int toolbarH = 0);
    bool beginRender(const LumeString& id, int w, int h);
    void endRender(const LumeString& id);
    void refresh(const LumeString& id);
    void moveToFullscreen(const LumeString& id);
    void restoreFromFullscreen(const LumeString& id);
    void destroyAll();
    bool getContext(const LumeString& id, HDC* out_hdc, HGLRC* out_hglrc);
}
void exitFullscreenCanvas() {
    if (!g_fullscreenCanvas) return;
    LumeString prevId = g_fullscreenCanvasId;
    g_fullscreenCanvas = false;
    g_fullscreenCanvasId[0] = '\0';
    SetWindowLongPtrA(g_mainWnd, GWL_STYLE, g_preFullscreenStyle);
    SetWindowPos(g_mainWnd, HWND_NOTOPMOST,
        g_preFullscreenRect.left, g_preFullscreenRect.top,
        g_preFullscreenRect.right - g_preFullscreenRect.left,
        g_preFullscreenRect.bottom - g_preFullscreenRect.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(g_addressBar, SW_SHOW);
    ShowWindow(g_statusBar, SW_SHOW);
    HWND hBack = GetDlgItem(g_mainWnd, ID_BACK);
    HWND hFwd = GetDlgItem(g_mainWnd, ID_FWD);
    HWND hRef = GetDlgItem(g_mainWnd, ID_REF);
    HWND hGo = GetDlgItem(g_mainWnd, ID_GO);
    if (hBack) ShowWindow(hBack, SW_SHOW);
    if (hFwd)  ShowWindow(hFwd, SW_SHOW);
    if (hRef)  ShowWindow(hRef, SW_SHOW);
    if (hGo)   ShowWindow(hGo, SW_SHOW);
    if (!prevId.empty()) {
        GLCanvas::restoreFromFullscreen(prevId);
    }
    g_contentDirty = true;
    InvalidateRect(g_mainWnd, nullptr, FALSE);
    invalidateDOM();
}
void enterFullscreenCanvas(const LumeString& canvasId) {
    if (g_fullscreenCanvas) return;
    g_fullscreenCanvas = true;
    lstrcpynA(g_fullscreenCanvasId, canvasId.c_str(), 128);
    GetWindowRect(g_mainWnd, &g_preFullscreenRect);
    g_preFullscreenStyle = GetWindowLongPtrA(g_mainWnd, GWL_STYLE);
    HMONITOR hMon = MonitorFromWindow(g_mainWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoA(hMon, &mi);
    SetWindowLongPtrA(g_mainWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(g_mainWnd, HWND_TOPMOST,
        mi.rcMonitor.left, mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    ShowWindow(g_addressBar, SW_HIDE);
    ShowWindow(g_statusBar, SW_HIDE);
    HWND hBack = GetDlgItem(g_mainWnd, ID_BACK);
    HWND hFwd = GetDlgItem(g_mainWnd, ID_FWD);
    HWND hRef = GetDlgItem(g_mainWnd, ID_REF);
    HWND hGo = GetDlgItem(g_mainWnd, ID_GO);
    if (hBack) ShowWindow(hBack, SW_HIDE);
    if (hFwd)  ShowWindow(hFwd, SW_HIDE);
    if (hRef)  ShowWindow(hRef, SW_HIDE);
    if (hGo)   ShowWindow(hGo, SW_HIDE);
    GLCanvas::moveToFullscreen(canvasId);
    InvalidateRect(g_mainWnd, nullptr, FALSE);
}
namespace FontCache {
    struct Key {
        int size; bool bold, italic; uint32_t face_hash;
        bool operator==(const Key& o) const {
            return size == o.size && bold == o.bold && italic == o.italic && face_hash == o.face_hash;
        }
    };
    struct Entry {Key k; HFONT v; bool occ;};
    static Entry* g_tab = nullptr;
    static size_t g_cap = 0, g_sz = 0;
    static LumeVector<HANDLE> g_memFonts;
    HFONT get(int sz, bool b = false, bool i = false, const char* f = "Segoe UI") {
        uint32_t hash = 2166136261u;
        for (const char* p = f; *p; ++p) {hash ^= (uint8_t)*p; hash *= 16777619u;}
        Key k{ sz, b, i, hash };
        uint32_t kh = hash ^ ((uint32_t)sz << 16) ^ (b ? 1 : 0) ^ (i ? 2 : 0);
        if (g_sz >= g_cap / 2) {
            size_t oCap = g_cap; Entry* oTab = g_tab;
            g_cap = oCap == 0 ? 16 : oCap * 2;
            g_tab = (Entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, g_cap * sizeof(Entry));
            g_sz = 0;
            if (oTab) {
                for (size_t j = 0; j < oCap; ++j) {
                    if (oTab[j].occ) {
                        uint32_t h = oTab[j].k.face_hash ^ ((uint32_t)oTab[j].k.size << 16) ^ (oTab[j].k.bold ? 1 : 0) ^ (oTab[j].k.italic ? 2 : 0);
                        size_t idx = h & (g_cap - 1);
                        while (g_tab[idx].occ) idx = (idx + 1) & (g_cap - 1);
                        g_tab[idx] = oTab[j];
                        g_sz++;
                    }
                }
                HeapFree(GetProcessHeap(), 0, oTab);
            }
        }
        size_t idx = kh & (g_cap - 1);
        while (g_tab[idx].occ) {
            if (g_tab[idx].k == k) return g_tab[idx].v;
            idx = (idx + 1) & (g_cap - 1);
        }
        LumeWString wface = utf8_to_wstring(f);
        HFONT font = CreateFontW(-sz, 0, 0, 0, b ? FW_BOLD : FW_NORMAL, i, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, wface.c_str());
        g_tab[idx] = {k, font, true};
        g_sz++;
        return font;
    }
    void clear() {
        if (g_tab) {
            for (size_t j = 0; j < g_cap; ++j) if (g_tab[j].occ) DeleteObject(g_tab[j].v);
            HeapFree(GetProcessHeap(), 0, g_tab);
            g_tab = nullptr; g_cap = g_sz = 0;
        }
        for (HANDLE h : g_memFonts) RemoveFontMemResourceEx(h);
        g_memFonts.clear();
    }
}
inline bool fast_streq(const char* a, const char* b) {
    while (*a && *a == *b) {++a; ++b;}
    return *a == *b;
}
namespace HTP {
struct Color {
    int r = 255, g = 255, b = 255, a = 255;
    static Color fromHex(const char* s) {
        Color c;
        if (!s || !*s) return c;
        if (*s == '#') s++;
        auto to_hex = [](char ch) -> int {
            int val = ch - '0';
            if (val > 9) val = (ch & ~0x20) - 'A' + 10;
            return val & 0xF;
            };
        if (s[0] && s[1]) {
            c.r = (to_hex(s[0]) << 4) | to_hex(s[1]);
            if (s[2] && s[3]) {
                c.g = (to_hex(s[2]) << 4) | to_hex(s[3]);
                if (s[4] && s[5]) {
                    c.b = (to_hex(s[4]) << 4) | to_hex(s[5]);
                    if (s[6] && s[7]) c.a = (to_hex(s[6]) << 4) | to_hex(s[7]);
                }
            }
        }
        return c;
    }
    COLORREF cr() const { return RGB(r, g, b); }
};
enum class EType {
    PAGE, BLOCK, TEXT, IMAGE, LINK, BUTTON, INPUT_FIELD, DIVIDER, LIST, ITEM, BR, SCRIPT, CANVAS, ROW, COLUMN, GL_CANVAS, UNKNOWN
};
struct Props {
    LumeVector<LumePair<LumeString, LumeString>> d;
    const char* get(const char* k, const char* def = "") const {
        for (const auto& pair : d) {
            if (fast_streq(pair.first.c_str(), k)) return pair.second.c_str();
        }
        return def;
    }
    int getInt(const char* k, int def = 0) const {
        for (const auto& pair : d) {
            if (fast_streq(pair.first.c_str(), k)) {
                const char* s = pair.second.c_str();
                int val = 0;
                bool neg = false;
                if (*s == '-') { neg = true; s++; }
                while (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    s++;
                }
                return neg ? -val : val;
            }
        }
        return def;
    }
    bool getBool(const char* k, bool def = false) const {
        for (const auto& pair : d) {
            if (fast_streq(pair.first.c_str(), k)) {
                const char* s = pair.second.c_str();
                return (s[0] == 't' || s[0] == '1');
            }
        }
        return def;
    }
    Color getColor(const char* k, Color def = { 255,255,255 }) const {
        for (const auto& pair : d) {
            if (fast_streq(pair.first.c_str(), k)) {
                return Color::fromHex(pair.second.c_str());
            }
        }
        return def;
    }
};
struct Elem {
    EType type = EType::UNKNOWN;
    LumeString tag;
    Props props;
    LumeString scriptCode;
    LumeVector<LumeSharedPtr<Elem>> children;
};
struct Doc {
    LumeSharedPtr<Elem> root;
    LumeString title;
    Color bg = { 26,26,46 };
};
LumeSharedPtr<Elem> findById(LumeSharedPtr<Elem> node, const LumeString& id) {
    if (!node) return nullptr;
    const char* nodeId = node->props.get("id");
    if (nodeId[0] != '\0' && fast_streq(nodeId, id.c_str())) return node;
    for (auto& c : node->children) {
        auto r = findById(c, id);
        if (r) return r;
    }
    return nullptr;
}
class Parser {
    LumeString src;
    int pos = 0, len = 0;
    void skipWS() {
        while (pos < len) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') {
                while (pos < len && src[pos] != '\n') pos++;
                continue;
            }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') {
                pos += 2;
                while (pos + 1 < len && !(src[pos] == '*' && src[pos + 1] == '/')) pos++;
                if (pos + 1 < len) pos += 2;
                continue;
            }
            break;
        }
    }
    LumeString readIdent() {
        int start = pos;
        const char* p = src.data();
        while (pos < len) {
            char c = p[pos];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-') {
                pos++;
            }
            else {
                break;
            }
        }
        return LumeString(p + start, pos - start);
    }
    LumeString readString() {
        if (pos >= len || src[pos] != '"') return "";
        pos++;
        LumeString r;
        r.reserve(64);
        while (pos < len && src[pos] != '"') {
            if (src[pos] == '\\' && pos + 1 < len) {
                pos++;
                switch (src[pos]) {
                case 'n': r += '\n'; break;
                case 't': r += '\t'; break;
                case '"': r += '"'; break;
                case '\\': r += '\\'; break;
                default: r += src[pos]; break;
                }
            }
            else {
                r += src[pos];
            }
            pos++;
        }
        if (pos < len) pos++;
        return r;
    }
    LumeString readValue() {
        skipWS();
        if (pos < len && src[pos] == '"') return readString();
        int start = pos;
        const char* p = src.data();
        while (pos < len && p[pos] != ';' && p[pos] != '}') pos++;
        int end = pos;
        while (end > start && (p[end - 1] == ' ' || p[end - 1] == '\t' || p[end - 1] == '\r' || p[end - 1] == '\n')) {
            end--;
        }
        return LumeString(p + start, end - start);
    }
    LumeString readRawBlock() {
        int depth = 1;
        int start = pos;
        bool inStr = false, inLC = false, inBC = false;
        while (pos < len && depth > 0) {
            char c = src[pos];
            if (inLC) { if (c == '\n') inLC = false; pos++; continue; }
            if (inBC) {
                if (c == '*' && pos + 1 < len && src[pos + 1] == '/') { inBC = false; pos += 2; continue; }
                pos++; continue;
            }
            if (inStr) {
                if (c == '\\') { pos += 2; continue; }
                if (c == '"') inStr = false;
                pos++; continue;
            }
            if (c == '[') {
                int eqCount = 0;
                int tempPos = pos + 1;
                while (tempPos < len && src[tempPos] == '=') {
                    eqCount++;
                    tempPos++;
                }
                if (tempPos < len && src[tempPos] == '[') {
                    pos = tempPos + 1;
                    while (pos < len) {
                        if (src[pos] == ']') {
                            int closeEq = 0;
                            int checkPos = pos + 1;
                            while (checkPos < len && src[checkPos] == '=') {
                                closeEq++;
                                checkPos++;
                            }
                            if (checkPos < len && src[checkPos] == ']' && closeEq == eqCount) {
                                pos = checkPos + 1;
                                break;
                            }
                        }
                        pos++;
                    }
                    continue;
                }
            }
            if (c == '"') { inStr = true; pos++; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '/') { inLC = true; pos += 2; continue; }
            if (c == '/' && pos + 1 < len && src[pos + 1] == '*') { inBC = true; pos += 2; continue; }
            if (c == '{') depth++;
            else if (c == '}') { depth--; if (depth == 0) break; }
            pos++;
        }
        LumeString code = src.substr(start, pos - start);
        if (pos < len && src[pos] == '}') pos++;
        return code;
    }
    void parseInlineAttrs(LumeSharedPtr<Elem>& e) {
        skipWS();
        if (pos >= len || src[pos] != '(') return;
        pos++;
        while (pos < len && src[pos] != ')') {
            skipWS();
            LumeString key = readIdent();
            skipWS();
            if (pos < len && src[pos] == '=') {
                pos++;
                skipWS();
                e->props.d.push_back({ key, readString() });
            }
            skipWS();
            if (pos < len && src[pos] == ',') pos++;
        }
        if (pos < len) pos++;
    }
    EType tagToType(const LumeString& t) {
        if (t == "page") return EType::PAGE;
        if (t == "block") return EType::BLOCK;
        if (t == "text") return EType::TEXT;
        if (t == "image") return EType::IMAGE;
        if (t == "link") return EType::LINK;
        if (t == "button") return EType::BUTTON;
        if (t == "input") return EType::INPUT_FIELD;
        if (t == "divider") return EType::DIVIDER;
        if (t == "list") return EType::LIST;
        if (t == "item") return EType::ITEM;
        if (t == "br") return EType::BR;
        if (t == "script") return EType::SCRIPT;
        if (t == "canvas") return EType::CANVAS;
        if (t == "row") return EType::ROW;
        if (t == "column") return EType::COLUMN;
        if (t == "glcanvas") return EType::GL_CANVAS;
        return EType::UNKNOWN;
    }
    LumeSharedPtr<Elem> parseElement() {
        skipWS();
        if (pos >= len || src[pos] != '@') return nullptr;
        pos++;
        auto e = lume_make_shared<Elem>();
        e->tag = readIdent();
        e->type = tagToType(e->tag);
        parseInlineAttrs(e);
        skipWS();
        if (pos >= len || src[pos] != '{') return e;
        pos++;
        if (e->type == EType::SCRIPT) {
            e->scriptCode = readRawBlock();
            return e;
        }
        while (pos < len) {
            skipWS();
            if (pos >= len) break;
            if (src[pos] == '}') { pos++; break; }
            if (src[pos] == '@') {
                auto ch = parseElement();
                if (ch) e->children.push_back(ch);
            }
            else if (IsCharAlphaA(src[pos]) || src[pos] == '_') {
                LumeString key = readIdent();
                skipWS();
                if (pos < len && src[pos] == ':') {
                    pos++;
                    e->props.d.push_back({ key, readValue() });
                    skipWS();
                    if (pos < len && src[pos] == ';') pos++;
                }
            }
            else pos++;
        }
        return e;
    }
public:
    Doc parse(const LumeString& source) {
        Doc doc;
        src = source;
        pos = 0;
        len = (int)src.length();
        auto root = lume_make_shared<Elem>();
        root->type = EType::PAGE;
        root->tag = "root";
        while (pos < len) {
            skipWS();
            if (pos >= len) break;
            if (src[pos] == '@') {
                auto e = parseElement();
                if (e) {
                    if (e->type == EType::PAGE) {
                        doc.title = e->props.get("title", "Lume");
                        doc.bg = e->props.getColor("background", { 26,26,46 });
                        for (auto& c : e->children) root->children.push_back(c);
                    }
                    else root->children.push_back(e);
                }
            }
            else pos++;
        }
        doc.root = root;
        return doc;
    }
};
}
namespace Net {
    struct URL {
        LumeString proto, host, path;
        int port = 80;
        static URL parse(const LumeString& s) {
            URL u;
            LumeWString wurl = utf8_to_wstring(s);
            URL_COMPONENTSW uc = { sizeof(URL_COMPONENTSW) };
            uc.dwSchemeLength = (DWORD)-1;
            uc.dwHostNameLength = (DWORD)-1;
            uc.dwUrlPathLength = (DWORD)-1;
            if (WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &uc)) {
                if (uc.dwSchemeLength > 0)
                    u.proto = wstring_to_utf8(LumeWString(uc.lpszScheme, uc.dwSchemeLength));
                else
                    u.proto = "http";
                if (uc.dwHostNameLength > 0)
                    u.host = wstring_to_utf8(LumeWString(uc.lpszHostName, uc.dwHostNameLength));

                if (uc.dwUrlPathLength > 0)
                    u.path = wstring_to_utf8(LumeWString(uc.lpszUrlPath, uc.dwUrlPathLength));
                else
                    u.path = "/";
                u.port = uc.nPort;
            }
            else {
                u.proto = "http";
                u.path = "/";
                u.host = s;
                u.port = 80;
            }
            return u;
        }
    };
struct Resp {
    int code = 0;
    LumeString body, error;
    bool ok = false;
};
Resp fetch(const URL& url, bool verifyCert = false) {
    Resp resp;
    wchar_t whost[1024]; MultiByteToWideChar(CP_UTF8, 0, url.host.c_str(), -1, whost, 1024);
    wchar_t wpath[2048]; MultiByteToWideChar(CP_UTF8, 0, url.path.c_str(), -1, wpath, 2048);
    HINTERNET hSession = WinHttpOpen(L"Lume Browser Engine", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { resp.error = "WinHttpOpen failed"; return resp; }
    HINTERNET hConnect = WinHttpConnect(hSession, whost, url.port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); resp.error = "WinHttpConnect failed"; return resp; }
    DWORD flags = (url.proto == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        resp.error = "WinHttpOpenRequest failed"; return resp;
    }
    if (flags & WINHTTP_FLAG_SECURE) {
        if (!verifyCert) {
            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
        }
    }
    bool bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (bResults) {
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&dwStatusCode,&dwSize,WINHTTP_NO_HEADER_INDEX);
        resp.code = dwStatusCode;
        resp.ok = (resp.code >= 200 && resp.code < 400);
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            LumeVector<char> buffer(dwSize);
            if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                resp.body.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }
    else {
        resp.error = "HTTP Connection failed";
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return resp;
}
Resp fetchUrl(const LumeString& u) {return fetch(URL::parse(u));}
}
namespace AsyncNet {
    struct DLState {
        int status = 0;
        LumeString localPath;
        size_t downloadedBytes = 0;
        size_t totalBytes = 0;
    };
    LumeMap<LumeString, DLState> g_downloads;
    LumeMutex g_dlMutex;
    LumeString getTempDir() {
        char ep[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, ep, MAX_PATH);
        LumeString dir = ep;
        auto ls = dir.find_last_of("\\/");
        dir = (ls != LumeString::npos) ? dir.substr(0, ls + 1) : ".\\";
        LumeString td = dir + "temp\\";
        CreateDirectoryA(td.c_str(), nullptr);
        return td;
    }
    void startDownload(const LumeString& urlStr, const LumeString& filename) {
        if (filename.find("..") != LumeString::npos || filename.find('/') != LumeString::npos || filename.find('\\') != LumeString::npos) {
            return;
        }
        {
            LumeLockGuard<LumeMutex> lock(g_dlMutex);
            g_downloads[urlStr] = { 0, "", 0, 0 };
        }
        LumeThread::RunAsync([urlStr, filename]() {
            Net::URL url = Net::URL::parse(urlStr);
            LumeString outPath = getTempDir() + filename;
            auto setError = [&]() {
                LumeLockGuard<LumeMutex> lock(g_dlMutex);
                g_downloads[urlStr].status = -1;
                };
            HINTERNET hSession = WinHttpOpen(L"Lume Download Agent", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) {
                setError();
                return;
            }
            HINTERNET hConnect = WinHttpConnect(hSession, utf8_to_wstring(url.host).c_str(), url.port, 0);
            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                setError();
                return;
            }
            DWORD flags = (url.proto == "https") ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", utf8_to_wstring(url.path).c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                setError();
                return;
            }
            if (flags & WINHTTP_FLAG_SECURE) {
                DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
                WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));
            }
            bool bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            if (bResults) bResults = WinHttpReceiveResponse(hRequest, NULL);
            if (bResults) {
                DWORD statusCode = 0, sz = sizeof(statusCode);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &sz, WINHTTP_NO_HEADER_INDEX);
                if (statusCode >= 400) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    setError();
                    return;
                }
                DWORD contentLength = 0;
                sz = sizeof(contentLength);
                WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &sz, WINHTTP_NO_HEADER_INDEX);
                {
                    LumeLockGuard<LumeMutex> lock(g_dlMutex);
                    g_downloads[urlStr].totalBytes = contentLength;
                }
                LumeWString wOutPath = utf8_to_wstring(outPath);
                HANDLE hFile = CreateFileW(wOutPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile == INVALID_HANDLE_VALUE) {
                    WinHttpCloseHandle(hRequest);
                    WinHttpCloseHandle(hConnect);
                    WinHttpCloseHandle(hSession);
                    setError();
                    return;
                }
                DWORD dwSize = 0, dwDownloaded = 0;
                do {
                    if (InterlockedCompareExchange(&g_isShuttingDown, 0, 0) != 0) break;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                    if (dwSize == 0) break;
                    LumeVector<char> buffer(dwSize);
                    if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                        DWORD bytesWritten = 0;
                        WriteFile(hFile, buffer.data(), dwDownloaded, &bytesWritten, NULL);
                        LumeLockGuard<LumeMutex> lock(g_dlMutex);
                        g_downloads[urlStr].downloadedBytes += dwDownloaded;
                    }
                } while (dwSize > 0);
                CloseHandle(hFile);
                {
                    LumeLockGuard<LumeMutex> lock(g_dlMutex);
                    g_downloads[urlStr].status = 1;
                    g_downloads[urlStr].localPath = outPath;
                }
            }
            else {
                setError();
            }
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            });
    }
}
namespace WasmEngine {
    int load(const LumeString& wasmBytes);
}
namespace Bindings {
    extern LumeMap<LumeString, LumeString> g_texts;
    void alert(const char* msg);
    void navigate(const char* url);
    void refresh();
    bool is_fullscreen();
    void fullscreen_canvas(const char* id, int enter);
    void set_prop(const char* id, const char* prop, const char* val);
    const char* get_node_prop(const char* id, const char* prop, const char* def);
    void set_inner_htp(const char* id, const char* htp);
    void set_text(const char* id, const char* t);
    const char* get_text(const char* id);
    void set_offset_y(const char* id, int o);
    int get_offset_y(const char* id);
    void set_shimmer(const char* id, float s);
    void on_click(const char* id, LumeAction action);
    void on_right_click(const char* id, LumeAction action);
    void fire_click(const char* id);
    void fire_right_click(const char* id);
    bool is_key_pressed(int vk);
    bool is_key_released(int vk);
    bool key_down(int vk);
    void get_mouse(int* x, int* y);
    void get_mouse_delta(int* dx, int* dy);
    int get_mouse_wheel();
    bool capture_mouse(const char* id, int mode);
    void get_canvas_mouse(const char* id, int* x, int* y);
    void release_mouse();
    bool is_mouse_captured();
    bool window_active();
    const char* get_input(const char* id);
    void set_input(const char* id, const char* t);
    void http_request(const char* url, LumeString& out_body, int& out_code);
    void cv_clear(const char* id, const char* color_hex);
    void cv_rect(const char* id, int x, int y, int w, int h, const char* color_hex);
    void cv_circle(const char* id, int cx, int cy, int r, const char* color_hex);
    void cv_line(const char* id, int x1, int y1, int x2, int y2, const char* color_hex, int t);
    void cv_text(const char* id, int x, int y, const char* txt, int sz, const char* color_hex);
    void reset();
}
namespace Script {
    extern lua_State* g_L;
}
extern HTP::Doc g_doc;
namespace Plugins {
    LumeMap<LumeString, CustomProtocolHandler> g_protocols;
    CustomProtocolHandler* g_activeProtocol = nullptr;
    void hostRegisterProtocolEngine(CustomProtocolHandler handler) {
        g_protocols[handler.scheme] = handler;
    }
    LumeMap<LumeString, void(*)(const char*)> g_scriptEngines;
    void hostRegisterScriptEngine(const char* lang, void (*exec_fn)(const char*)) {
        if (lang && exec_fn) {
            g_scriptEngines[lang] = exec_fn;
        }
    }
    struct LoadedPlugin {
        LumeString name, path;
        HMODULE hModule = nullptr;
        lume_plugin_init_fn initFn = nullptr;
        bool enabled = true;
    };
    struct DynamicPlugin {
        LumeString url, localPath, originHost;
        bool isUnsafe;
        HMODULE hModule = nullptr;
        bool enabled = true;
    };
    static LumeVector<LoadedPlugin> g_plugins;
    static LumeVector<DynamicPlugin> g_dynPlugins;
    static LumeHostAPI g_hostAPI = {};
    static HWND hostGetMainHwnd() {return g_mainWnd;}
    static LumeString g_currentPluginNamespace;
    static LumeString extractPluginNamespace(const LumeString& filename) {
        LumeString id = filename;
        size_t dot = id.find_last_of('.');
        if (dot != LumeString::npos) id = id.substr(0, dot);
        for (size_t i = 0; i < id.length(); ++i) {
            char c = id[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
                id[i] = '_';
            }
        }
        if (id.empty() || (id[0] >= '0' && id[0] <= '9')) id = "plugin_" + id;
        return id;
    }
    static void hostInvalidateContent() {invalidateContent();}
    static void hostSetStatus(const char* t) {setStatus(t ? t : "");}
    static void hostNavigateTo(const char* u) { if (u) navigateTo(u); }

    LumeMap<LumeString, CustomTagHandler> g_customTags;
    LumeMap<LumeString, CustomPageHandler> g_customPages;
    CustomPageHandler* g_activeCustomEngine = nullptr;
    void* g_activeCustomPageContext = nullptr;

    const char* hostGetNodeProp(HTP_NodeHandle node, const char* key, const char* def) {
        if (!node) return def;
        auto e = (HTP::Elem*)node;
        return e->props.get(key, def);
    }
    int hostGetNodePropInt(HTP_NodeHandle node, const char* key, int def) {
        if (!node) return def;
        auto e = (HTP::Elem*)node;
        return e->props.getInt(key, def);
    }
    void hostRegisterTag(CustomTagHandler handler) {
        g_customTags[handler.tag_name] = handler;
    }
    void hostRegisterPageEngine(CustomPageHandler handler) {
        g_customPages[handler.extension] = handler;
    }
    static void hostAlert(const char* msg) {
        MessageBoxU(g_mainWnd, msg ? msg : "", "Lume Plugin", MB_OK | MB_ICONINFORMATION);
    }
    static int hostGlBegin(const char* id, int w, int h) {
        return GLCanvas::beginRender(id, w, h) ? 1 : 0;
    }
    static void hostGlEnd(const char* id) {
        GLCanvas::endRender(id);
    }
    static void hostGlPlace(const char* id, int x, int y, int w, int h, int scroll_y) {
        GLCanvas::place(id, x, TOOLBAR_H + y, w, h, scroll_y, TOOLBAR_H);
    }
    static int hostWasmLoad(const char* bytes, size_t len) {
        if (!bytes || len == 0) return -1;
        return WasmEngine::load(LumeString(bytes, len));
    }
    static int hostGlGetContext(const char* id, HDC* out_hdc, HGLRC* out_hglrc) {
        if (!id) return 0;
        return GLCanvas::getContext(id, out_hdc, out_hglrc) ? 1 : 0;
    }
    static LumeVector<void(*)()> g_onResetCallbacks;
    static void hostRegisterOnReset(void (*callback)()) {
        if (callback) g_onResetCallbacks.push_back(callback);
    }
    void reboot() {
        if (g_activeCustomEngine && g_activeCustomPageContext) {
            g_activeCustomEngine->free_page(g_activeCustomPageContext);
        }
        if (g_activeProtocol && g_activeCustomPageContext) {
            g_activeProtocol->free_page(g_activeCustomPageContext);
        }
        g_scriptEngines.clear();
        g_activeCustomEngine = nullptr;
        g_activeProtocol = nullptr;
        g_activeCustomPageContext = nullptr;
        g_onResetCallbacks.clear();
        for (auto& p : g_plugins) {
            if (p.hModule) {
                auto sf = (void(*)())GetProcAddress(p.hModule, "lume_plugin_shutdown");
                if (sf) sf();
                FreeLibrary(p.hModule);
                p.hModule = nullptr;
                p.initFn = nullptr;
            }
        }
        for (auto& dp : g_dynPlugins) {
            if (dp.hModule) {
                auto sf = (void(*)())GetProcAddress(dp.hModule, "lume_plugin_shutdown");
                if (sf) sf();
                FreeLibrary(dp.hModule);
                dp.hModule = nullptr;
            }
        }
        g_protocols.clear();
        g_customTags.clear();
        g_customPages.clear();
        for (auto& p : g_plugins) {
            if (!p.enabled) continue;
            p.hModule = LoadLibraryW(utf8_to_wstring(p.path).c_str());
        }
        for (auto& dp : g_dynPlugins) {
            if (!dp.enabled) continue;
            dp.hModule = LoadLibraryA(dp.localPath.c_str());
        }
    }
    static HTP_NodeHandle hostGetDomRoot() {return g_doc.root.get();}
    static int hostGetNodeChildrenCount(HTP_NodeHandle node) {
        if (!node) return 0;
        return (int)((HTP::Elem*)node)->children.size();
    }
    static HTP_NodeHandle hostGetNodeChild(HTP_NodeHandle node, int index) {
        if (!node) return nullptr;
        auto e = (HTP::Elem*)node;
        if (index >= 0 && index < e->children.size()) return e->children[index].get();
        return nullptr;
    }
    static void hostSetNodeProp(HTP_NodeHandle node, const char* key, const char* val) {
        if (!node) return;
        auto e = (HTP::Elem*)node;
        bool found = false;
        for (auto& pair : e->props.d) {
            if (pair.first == key) {
                pair.second = val;
                found = true;
                break;
            }
        }
        if (!found) e->props.d.push_back({ key, val });
        if (fast_streq(key, "content")) {
            const char* id = e->props.get("id", "");
            if (id[0] != '\0') {
                Bindings::g_texts[id] = val;
            }
        }
    }
    /* ---------- Безопасные обертки для C API (JS/Python/C плагины) ---------- */
    static void host_http_request(const char* url, char** out_body, int* out_code) {
        LumeString body;
        Bindings::http_request(url, body, *out_code);
        if (!body.empty()) {
            *out_body = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, body.length() + 1);
            lstrcpyA(*out_body, body.c_str());
        }
        else {
            *out_body = nullptr;
        }
    }
    static void host_free_string(char* s) {if (s) HeapFree(GetProcessHeap(), 0, s);}
    static int host_is_key_pressed(int vk) {return Bindings::is_key_pressed(vk) ? 1 : 0;}
    static int host_is_key_released(int vk) {return Bindings::is_key_released(vk) ? 1 : 0;}
    static int host_key_down(int vk) {return Bindings::key_down(vk) ? 1 : 0;}
    static int host_capture_mouse(const char* id, int mode) {return Bindings::capture_mouse(id, mode) ? 1 : 0;}
    static int host_is_mouse_captured() {return Bindings::is_mouse_captured() ? 1 : 0;}
    static int host_is_fullscreen() {return Bindings::is_fullscreen() ? 1 : 0;}
    static int host_window_active() {return Bindings::window_active() ? 1 : 0;}
    static void host_register_click_handler(const char* id, void(*cb)(void*), void* ctx) {
        LumeString sid = id;
        Bindings::on_click(id, LumeAction([sid, cb, ctx]() {if (cb) cb(ctx);}));
    }
    static void host_register_right_click_handler(const char* id, void(*cb)(void*), void* ctx) {
        LumeString sid = id;
        Bindings::on_right_click(id, LumeAction([sid, cb, ctx]() { if (cb) cb(ctx); }));
    }
    static void host_lua_setglobal(lua_State* L, const char* name) {
        if (!g_currentPluginNamespace.empty()) {
            const char* ns = g_currentPluginNamespace.c_str();
            lua_getglobal(L, ns);
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                lua_newtable(L);
                lua_pushvalue(L, -1);
                lua_setglobal(L, ns);
            }
            lua_pushvalue(L, -2);
            lua_setfield(L, -2, name);
            lua_pop(L, 1);
        }
        lua_setglobal(L, name);
    }
    void initHostAPI() {
        g_hostAPI.get_main_hwnd = hostGetMainHwnd;
        g_hostAPI.invalidate_content = Bindings::refresh;
        g_hostAPI.set_status = hostSetStatus;
        g_hostAPI.navigate_to = Bindings::navigate;
        g_hostAPI.alert = Bindings::alert;
        g_hostAPI.api_version = 3;
        g_hostAPI.p_luaL_checknumber = luaL_checknumber;
        g_hostAPI.p_luaL_checkinteger = luaL_checkinteger;
        g_hostAPI.p_luaL_optinteger = luaL_optinteger;
        g_hostAPI.p_luaL_optnumber = luaL_optnumber;
        g_hostAPI.p_luaL_optlstring = luaL_optlstring;
        g_hostAPI.p_luaL_checklstring = luaL_checklstring;
        g_hostAPI.p_lua_pushnumber = lua_pushnumber;
        g_hostAPI.p_lua_pushboolean = lua_pushboolean;
        g_hostAPI.p_lua_pushstring = lua_pushstring;
        g_hostAPI.p_lua_pushcclosure = lua_pushcclosure;
        g_hostAPI.p_lua_pushnil = lua_pushnil;
        g_hostAPI.p_lua_pushvalue = lua_pushvalue;
        g_hostAPI.p_lua_pushlightuserdata = lua_pushlightuserdata;
        g_hostAPI.p_lua_setglobal = host_lua_setglobal;
        g_hostAPI.p_lua_type = lua_type;
        g_hostAPI.p_lua_rawgeti = lua_rawgeti;
        g_hostAPI.p_lua_tonumberx = lua_tonumberx;
        g_hostAPI.p_lua_toboolean = lua_toboolean;
        g_hostAPI.p_lua_touserdata = lua_touserdata;
        g_hostAPI.p_lua_settop = lua_settop;
        g_hostAPI.p_lua_gettop = lua_gettop;
        g_hostAPI.p_lua_createtable = lua_createtable;
        g_hostAPI.p_lua_setfield = lua_setfield;
        g_hostAPI.get_node_prop = hostGetNodeProp;
        g_hostAPI.get_node_prop_int = hostGetNodePropInt;
        g_hostAPI.set_node_prop = hostSetNodeProp;
        g_hostAPI.get_dom_root = hostGetDomRoot;
        g_hostAPI.get_node_children_count = hostGetNodeChildrenCount;
        g_hostAPI.get_node_child = hostGetNodeChild;
        g_hostAPI.register_tag = hostRegisterTag;
        g_hostAPI.register_page_engine = hostRegisterPageEngine;
        g_hostAPI.register_protocol_engine = hostRegisterProtocolEngine;
        g_hostAPI.register_on_reset = hostRegisterOnReset;
        g_hostAPI.register_script_engine = hostRegisterScriptEngine;
        g_hostAPI.gl_begin_render = hostGlBegin;
        g_hostAPI.gl_end_render = hostGlEnd;
        g_hostAPI.gl_place = hostGlPlace;
        g_hostAPI.gl_get_context = hostGlGetContext;
        g_hostAPI.wasm_load = hostWasmLoad;
        g_hostAPI.b_set_prop = Bindings::set_prop;
        g_hostAPI.b_set_inner_htp = Bindings::set_inner_htp;
        g_hostAPI.b_set_text = Bindings::set_text;
        g_hostAPI.b_get_text = Bindings::get_text;
        g_hostAPI.b_set_offset_y = Bindings::set_offset_y;
        g_hostAPI.b_get_offset_y = Bindings::get_offset_y;
        g_hostAPI.b_set_shimmer = Bindings::set_shimmer;
        g_hostAPI.b_fire_click = Bindings::fire_click;
        g_hostAPI.b_fire_right_click = Bindings::fire_right_click;
        g_hostAPI.b_register_click_handler = host_register_click_handler;
        g_hostAPI.b_register_right_click_handler = host_register_right_click_handler;
        g_hostAPI.b_is_key_pressed = host_is_key_pressed;
        g_hostAPI.b_is_key_released = host_is_key_released;
        g_hostAPI.b_key_down = host_key_down;
        g_hostAPI.b_get_mouse = Bindings::get_mouse;
        g_hostAPI.b_get_mouse_delta = Bindings::get_mouse_delta;
        g_hostAPI.b_get_mouse_wheel = Bindings::get_mouse_wheel;
        g_hostAPI.b_capture_mouse = host_capture_mouse;
        g_hostAPI.b_get_canvas_mouse = Bindings::get_canvas_mouse;
        g_hostAPI.b_release_mouse = Bindings::release_mouse;
        g_hostAPI.b_is_mouse_captured = host_is_mouse_captured;
        g_hostAPI.b_fullscreen_canvas = Bindings::fullscreen_canvas;
        g_hostAPI.b_is_fullscreen = host_is_fullscreen;
        g_hostAPI.b_window_active = host_window_active;
        g_hostAPI.b_get_input = Bindings::get_input;
        g_hostAPI.b_set_input = Bindings::set_input;
        g_hostAPI.b_http_request = host_http_request;
        g_hostAPI.b_free_string = host_free_string;
        g_hostAPI.b_cv_clear = Bindings::cv_clear;
        g_hostAPI.b_cv_rect = Bindings::cv_rect;
        g_hostAPI.b_cv_circle = Bindings::cv_circle;
        g_hostAPI.b_cv_line = Bindings::cv_line;
        g_hostAPI.b_cv_text = Bindings::cv_text;
    }
    void discoverPlugins() {
        wchar_t ep[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, ep, MAX_PATH);
        LumeWString dir = ep;
        auto ls = dir.find_last_of(L"\\/");
        dir = (ls != LumeWString::npos) ? dir.substr(0, ls + 1) : L".\\";
        LumeWString pd = dir + L"plugins\\";
        CreateDirectoryW(pd.c_str(), nullptr);
        WIN32_FIND_DATAW fd = {};
        HANDLE hf = FindFirstFileW((pd + L"*.dll").c_str(), &fd);
        if (hf == INVALID_HANDLE_VALUE) return;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            LoadedPlugin p;
            p.name = wstring_to_utf8(fd.cFileName);
            LumeWString fullWPath = pd + fd.cFileName;
            p.path = wstring_to_utf8(fullWPath);
            p.hModule = LoadLibraryW(fullWPath.c_str());
            if (!p.hModule) continue;
            p.initFn = (lume_plugin_init_fn)GetProcAddress(p.hModule, "lume_plugin_init");
            if (!p.initFn) {
                FreeLibrary(p.hModule);
                continue;
            }
            g_plugins.push_back(p);
        } while (FindNextFileW(hf, &fd));
        FindClose(hf);
    }
    void initAllPlugins(lua_State* L) {
        if (!L) return;
        for (auto& p : g_plugins) {
            if (!p.enabled) continue;
            if (p.hModule) {
                p.initFn = (lume_plugin_init_fn)GetProcAddress(p.hModule, "lume_plugin_init");
                if (p.initFn) {
                    g_currentPluginNamespace = extractPluginNamespace(p.name);
                    p.initFn(L, &g_hostAPI);
                    g_currentPluginNamespace.clear();
                }
            }
        }
        for (auto& dp : g_dynPlugins) {
            if (!dp.enabled) continue;
            if (dp.hModule) {
                auto initFn = (lume_plugin_init_fn)GetProcAddress(dp.hModule, "lume_plugin_init");
                if (initFn) {
                    LumeString shortName = dp.url.substr(dp.url.find_last_of('/') + 1);
                    g_currentPluginNamespace = extractPluginNamespace(shortName);
                    initFn(L, &g_hostAPI);
                    g_currentPluginNamespace.clear();
                }
            }
        }
    }
    void checkDynamicUnloads(const LumeString& newUrl) {
        LumeString newHost = Net::URL::parse(newUrl).host;
        for (auto it = g_dynPlugins.begin(); it != g_dynPlugins.end(); ) {
            if (it->isUnsafe && it->originHost != newHost) {
                if (it->hModule) {
                    auto sf = (void(*)())GetProcAddress(it->hModule, "lume_plugin_shutdown");
                    if (sf) sf();
                    FreeLibrary(it->hModule);
                }
                DeleteFileA(it->localPath.c_str());
                it = g_dynPlugins.erase(it);
            }
            else {
                ++it;
            }
        }
    }
    bool loadFromNet(const LumeString& url, const LumeString& originUrl, lua_State* L) {
        LumeString expectedHash = "";
        LumeString fetchUrl = url;
        size_t hashPos = url.find("#sha256=");
        if (hashPos != LumeString::npos) {
            expectedHash = url.substr(hashPos + 8);
            fetchUrl = url.substr(0, hashPos);
        }

        for (const auto& dp : g_dynPlugins) {
            if (dp.url == fetchUrl) return true;
        }

        int trust = 3;
        if (fetchUrl.find("..") != LumeString::npos ||
            fetchUrl.find("%2e") != LumeString::npos ||
            fetchUrl.find("%2E") != LumeString::npos) {
            trust = 3;
        }
        else {
            if (fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/main/plugins/aaOfficial/") == 0 ||
                fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/refs/heads/main/plugins/aaOfficial/") == 0 ||
                fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/main/plugins/Community/") == 0 ||
                fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/refs/heads/main/plugins/Community/") == 0 ||
                fetchUrl.find("https://github.com/Lume-corp/LumeSources/raw/refs/heads/main/plugins/aaOfficial/") == 0 ||
                fetchUrl.find("https://github.com/Lume-corp/LumeSources/raw/refs/heads/main/plugins/Community/") == 0) {
                trust = 0;
            }
            else if (fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/main/plugins/aaOfficialUnsafe/") == 0 ||
                fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/refs/heads/main/plugins/aaOfficialUnsafe/") == 0 ||
                fetchUrl.find("https://github.com/Lume-corp/LumeSources/raw/refs/heads/main/plugins/aaOfficialUnsafe/") == 0) {
                trust = 1;
            }
            else if (fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/main/plugins/CommunityUnsafe/") == 0 ||
                fetchUrl.find("https://raw.githubusercontent.com/Lume-corp/LumeSources/refs/heads/main/plugins/CommunityUnsafe/") == 0 ||
                fetchUrl.find("https://github.com/Lume-corp/LumeSources/raw/refs/heads/main/plugins/CommunityUnsafe/") == 0) {
                trust = 2;
            }
        }
        bool isUnsafe = (trust > 0);
        if (trust == 1 || trust == 2) {
            LumeString msg = "Do you want to download and load this UNSAFE plugin?\n\nURL: " + fetchUrl + "\n\nIt requires elevated privileges.";
            if (MessageBoxA(g_mainWnd, msg.c_str(), "Unsafe Plugin Warning", MB_YESNO | MB_ICONWARNING) != IDYES) return false;
        }
        else if (trust == 3) {
            LumeString msg = "WARNING: This plugin is NOT from the official Lume repository!\n\nURL: " + fetchUrl;
            if (MessageBoxA(g_mainWnd, msg.c_str(), "Unknown Origin Plugin", MB_YESNO | MB_ICONERROR) != IDYES) return false;
        }
        bool strictSSL = (trust == 0);
        auto resp = Net::fetch(Net::URL::parse(fetchUrl), strictSSL);
        if (!resp.ok || resp.body.empty()) {
            MessageBoxA(g_mainWnd, "Failed to download plugin.", "Download Error", MB_OK | MB_ICONERROR);
            return false;
        }
        if (trust == 0 && !expectedHash.empty()) {
            HCRYPTPROV hProv = 0;
            HCRYPTHASH hHash = 0;
            bool hashOk = false;
            if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
                if (CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
                    if (CryptHashData(hHash, (const BYTE*)resp.body.data(), (DWORD)resp.body.size(), 0)) {
                        BYTE hashData[32];
                        DWORD hashLen = 32;
                        if (CryptGetHashParam(hHash, HP_HASHVAL, hashData, &hashLen, 0)) {
                            char hexStr[65];
                            for (int i = 0; i < 32; i++) wsprintfA(&hexStr[i * 2], "%02x", hashData[i]);
                            hashOk = true;
                            for (int i = 0; i < 64 && expectedHash[i] != '\0'; i++) {
                                char c1 = hexStr[i];
                                char c2 = expectedHash[i];
                                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                                if (c1 != c2) { hashOk = false; break; }
                            }
                        }
                    }
                    CryptDestroyHash(hHash);
                }
                CryptReleaseContext(hProv, 0);
            }
            if (!hashOk) {
                MessageBoxA(g_mainWnd, "SECURITY ERROR: SHA256 integrity mismatch for automatic plugin!\n\nConnection might be compromised.", "Integrity Error", MB_OK | MB_ICONERROR);
                return false;
            }
        }
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        static volatile LONG pluginCounter = 0;
        LumeString fileName = "lume_dyn_" + lume_to_string(GetTickCount64()) + "_" + lume_to_string(InterlockedIncrement(&pluginCounter)) + ".dll";
        LumeString fullPath = LumeString(tempPath) + fileName;
        LumeWString wFullPath = utf8_to_wstring(fullPath);
        HANDLE hFile = CreateFileW(wFullPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        DWORD bytesWritten = 0;
        WriteFile(hFile, resp.body.data(), (DWORD)resp.body.size(), &bytesWritten, NULL);
        CloseHandle(hFile);
        HMODULE hMod = LoadLibraryA(fullPath.c_str());
        if (!hMod) {
            DeleteFileA(fullPath.c_str());
            MessageBoxA(g_mainWnd, "Failed to load the downloaded DLL.", "Load Error", MB_OK | MB_ICONERROR);
            return false;
        }
        auto initFn = (lume_plugin_init_fn)GetProcAddress(hMod, "lume_plugin_init");
        if (!initFn) {
            FreeLibrary(hMod);
            DeleteFileA(fullPath.c_str());
            MessageBoxA(g_mainWnd, "Invalid Lume Plugin (missing lume_plugin_init).", "Plugin Error", MB_OK | MB_ICONERROR);
            return false;
        }
        initFn(L, &g_hostAPI);
        DynamicPlugin dp;
        dp.url = fetchUrl;
        dp.localPath = fullPath;
        dp.originHost = Net::URL::parse(originUrl).host;
        dp.isUnsafe = isUnsafe;
        dp.hModule = hMod;
        g_dynPlugins.push_back(dp);
        return true;
    }
    void unloadAll() {
        for (auto& p : g_plugins) {
            if (!p.hModule) continue;
            auto sf = (void(*)())GetProcAddress(p.hModule, "lume_plugin_shutdown");
            if (sf) sf();
            FreeLibrary(p.hModule);
            p.hModule = nullptr;
        }
        g_plugins.clear();
        for (auto& dp : g_dynPlugins) {
            if (dp.hModule) {
                auto sf = (void(*)())GetProcAddress(dp.hModule, "lume_plugin_shutdown");
                if (sf) sf();
                FreeLibrary(dp.hModule);
            }
            DeleteFileA(dp.localPath.c_str());
        }
        g_dynPlugins.clear();
    }
}
namespace ImageCache {
    LumeMap<LumeString, LumeSharedPtr<Gdiplus::Image>> cache;
    LumeMap<LumeString, bool> loading;
    LumeVector<LumeString> history;
    LumeMutex mtx;
    const size_t MAX_CACHE_SIZE = 50;
    LumeSharedPtr<Gdiplus::Image> get(const LumeString& url) {
        if (url.empty()) return nullptr;
        {
            LumeLockGuard<LumeMutex> lock(mtx);
            auto it = cache.find(url);
            if (it != cache.end()) return it->second;
            if (loading[url]) return nullptr;
            loading[url] = true;
        }
        LumeThread::RunAsync([url]() {
            LumeSharedPtr<Gdiplus::Image> img;
            if (url.substr(0, 4) == "http") {
                auto r = Net::fetch(Net::URL::parse(url));
                if (r.ok && !r.body.empty()) {
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, r.body.size());
                    if (hMem) {
                        void* pData = GlobalLock(hMem);
                        CopyMemory(pData, r.body.data(), r.body.size());
                        GlobalUnlock(hMem);
                        IStream* pStream = nullptr;
                        if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {
                            img = LumeSharedPtr<Gdiplus::Image>(Gdiplus::Image::FromStream(pStream));
                            pStream->Release();
                        }
                        else {
                            GlobalFree(hMem);
                        }
                    }
                }
            }
            else {
                LumeString path = url;
                if (path.length() > 7 && path.substr(0, 7) == "file://") path = path.substr(7);
                int len = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, NULL, 0);
                LumeWString wpath(len, 0);
                MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], len);
                img = LumeSharedPtr<Gdiplus::Image>(Gdiplus::Image::FromFile(wpath.c_str()));
            }
            if (img && img->GetLastStatus() != Gdiplus::Ok) {
                img = nullptr;
            }
            {
                LumeLockGuard<LumeMutex> lock(mtx);
                cache[url] = img;
                loading[url] = false;
                if (img) {
                    history.push_back(url);
                    if (history.size() > MAX_CACHE_SIZE) {
                        LumeString oldest = history.front();
                        history.erase(history.begin());
                        cache.erase(oldest);
                    }
                }
            }
            PostMessage(g_mainWnd, WM_UPDATE_CONTENT, 0, 0);
            });
        return nullptr;
    }
    void clear() {
        LumeLockGuard<LumeMutex> lock(mtx);
        cache.clear();
        loading.clear();
        history.clear();
    }
}
namespace Canvas {
    struct Buf {
        int w = 0, h = 0;
        HDC dc = 0;
        HBITMAP bmp = 0, old = 0;
        void create(HDC ref, int ww, int hh) {
            cleanup(); w = ww; h = hh;
            dc = CreateCompatibleDC(ref);
            bmp = CreateCompatibleBitmap(ref, w, h);
            old = (HBITMAP)SelectObject(dc, bmp);
            HBRUSH b = CreateSolidBrush(0);
            RECT r = {0,0,w,h};
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
        void cleanup() {
            if (dc) {
                SelectObject(dc, old);
                DeleteDC(dc); dc = 0;
            }
            if (bmp) {
                DeleteObject(bmp);
                bmp = 0;
            }
        }
        ~Buf() {cleanup();}
    };
    LumeMap<LumeString, LumeSharedPtr<Buf>> g_bufs;
    LumeSharedPtr<Buf> get(const LumeString& id, HDC ref, int w, int h) {
        auto i = g_bufs.find(id);
        if (i != g_bufs.end() && i->second->w == w && i->second->h == h) return i->second;
        auto b = lume_make_shared<Buf>();
        b->create(ref, w, h);
        g_bufs[id] = b;
        return b;
    }
}
extern HTP::Doc g_doc;
namespace GLCanvas {
    static const wchar_t* kClassName = L"LumeGLCanvasClass";
    static bool g_classRegistered = false;
    static HINSTANCE g_hInst = nullptr;
    struct GLView {
        LumeString id;
        HWND hwnd = nullptr;
        HDC hdc = nullptr;
        HGLRC hglrc = nullptr;
        int x = 0, y = 0, w = 0, h = 0;
        bool visible = false;
        bool valid = false;
        bool touchedThisLayout = false;
        bool fullscreen = false;
    };
    LumeMap<LumeString, LumeSharedPtr<GLView>> g_views;
    LumeString findIdByHwnd(HWND hwnd) {
        for (auto& kv : g_views) {
            if (kv.second && kv.second->hwnd == hwnd)
                return kv.first;
        }
        return "";
    }
    LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            LumeString canvasId = findIdByHwnd(hwnd);
            int localX = GET_X_LPARAM(lp);
            int localY = GET_Y_LPARAM(lp);
            auto e = HTP::findById(::g_doc.root, canvasId);
            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, localX, localY, 1);
            }
            if (!e || e->props.getBool("capture", true)) {
                int mode = e ? e->props.getInt("capture-mode", 0) : 0;
                captureCanvasMouse(canvasId, mode);
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_RBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            LumeString canvasId = findIdByHwnd(hwnd);
            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 2);
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_MBUTTONDOWN: {
            if (!g_mainWnd) return 0;
            LumeString canvasId = findIdByHwnd(hwnd);
            if (!canvasId.empty()) {
                Script::fireCanvasClick(canvasId, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 3);
            }
            SetFocus(g_mainWnd);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_mouseCaptured && g_mainWnd) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ClientToScreen(hwnd, &pt);
                ScreenToClient(g_mainWnd, &pt);
                SendMessage(g_mainWnd, WM_MOUSEMOVE, wp, MAKELPARAM(pt.x, pt.y));
                return 0;
            }
            break;
        }
        case WM_SETFOCUS:
            if (g_mainWnd) SetFocus(g_mainWnd);
            return 0;
        case WM_NCHITTEST:
            return HTCLIENT;

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    bool registerClass(HINSTANCE hInst) {
        if (g_classRegistered) return true;
        g_hInst = hInst;
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CanvasWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = kClassName;
        wc.hbrBackground = nullptr;

        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
        }
        g_classRegistered = true;
        return true;
    }
    bool setupPixelFormat(HDC hdc) {
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        int pf = ChoosePixelFormat(hdc, &pfd);
        if (!pf) return false;
        if (!SetPixelFormat(hdc, pf, &pfd)) return false;
        return true;
    }
    LumeSharedPtr<GLView> createView(const LumeString& id, int w, int h) {
        if (!g_mainWnd || !g_hInst) return nullptr;
        auto v = lume_make_shared<GLView>();
        v->id = id;
        v->w = w;
        v->h = h;
        v->hwnd = CreateWindowExW(
            0,
            kClassName,
            L"",
            WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
            0, 0, w, h,
            g_mainWnd,
            nullptr,
            g_hInst,
            nullptr
        );
        if (!v->hwnd) return nullptr;
        v->hdc = GetDC(v->hwnd);
        if (!v->hdc) {
            DestroyWindow(v->hwnd);
            v->hwnd = nullptr;
            return nullptr;
        }
        if (!setupPixelFormat(v->hdc)) {
            ReleaseDC(v->hwnd, v->hdc);
            DestroyWindow(v->hwnd);
            v->hdc = nullptr;
            v->hwnd = nullptr;
            return nullptr;
        }
        v->hglrc = wglCreateContext(v->hdc);
        if (!v->hglrc) {
            ReleaseDC(v->hwnd, v->hdc);
            DestroyWindow(v->hwnd);
            v->hdc = nullptr;
            v->hwnd = nullptr;
            return nullptr;
        }
        v->valid = true;
        v->visible = true;
        ShowWindow(v->hwnd, SW_SHOW);
        UpdateWindow(v->hwnd);
        return v;
    }
    void destroyView(LumeSharedPtr<GLView>& v) {
        if (!v) return;
        if (v->hglrc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(v->hglrc);
            v->hglrc = nullptr;
        }
        if (v->hdc && v->hwnd) {
            ReleaseDC(v->hwnd, v->hdc);
            v->hdc = nullptr;
        }
        if (v->hwnd) {
            DestroyWindow(v->hwnd);
            v->hwnd = nullptr;
        }
        v->valid = false;
    }
    LumeSharedPtr<GLView> find(const LumeString& id) {
        auto it = g_views.find(id);
        if (it == g_views.end()) return nullptr;
        return it->second;
    }
    bool getContext(const LumeString& id, HDC* out_hdc, HGLRC* out_hglrc) {
        auto v = find(id);
        if (v && v->valid) {
            if (out_hdc) *out_hdc = v->hdc;
            if (out_hglrc) *out_hglrc = v->hglrc;
            return true;
        }
        return false;
    }
    LumeSharedPtr<GLView> ensure(const LumeString& id, int w, int h) {
        if (!g_opt_gpu) return nullptr;
        auto it = g_views.find(id);
        if (it != g_views.end()) {
            auto v = it->second;
            if (v && v->valid) {
                if (!v->fullscreen && (v->w != w || v->h != h)) {
                    v->w = w;
                    v->h = h;
                    MoveWindow(v->hwnd, v->x, v->y, w, h, TRUE);
                }
                return v;
            }
        }
        auto v = createView(id, w, h);
        if (!v) return nullptr;
        g_views[id] = v;
        return v;
    }
    void beginLayoutPass() {
        for (auto& kv : g_views) {
            if (kv.second) kv.second->touchedThisLayout = false;
        }
    }
    void place(const LumeString& id, int x, int y, int w, int h, int scrollY, int toolbarH) {
        auto v = ensure(id, w, h);
        if (!v || !v->valid) return;
        v->touchedThisLayout = true;
        if (g_fullscreenCanvas && g_fullscreenCanvasId == id) {
            moveToFullscreen(id);
            return;
        }
        bool changed = (v->x != x || v->y != y || v->w != w || v->h != h);
        v->x = x;
        v->y = y;
        v->w = w;
        v->h = h;
        v->visible = true;
        RECT cr;
        GetClientRect(g_mainWnd, &cr);
        int contentTop = toolbarH;
        int contentBot = cr.bottom - STATUS_H;
        bool inView = (y + h > contentTop) && (y < contentBot);

        if (inView) {
            if (changed) {
                MoveWindow(v->hwnd, x, y, w, h, FALSE);
            }
            if (!IsWindowVisible(v->hwnd)) {
                ShowWindow(v->hwnd, SW_SHOWNA);
            }
        }
        else {
            if (IsWindowVisible(v->hwnd)) {
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    void endLayoutPass() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (!v || !v->valid) continue;
            if (!v->touchedThisLayout && !g_fullscreenCanvas) {
                v->visible = false;
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    bool beginRender(const LumeString& id, int w, int h) {
        auto v = ensure(id, w, h);
        if (!v || !v->valid) return false;
        return wglMakeCurrent(v->hdc, v->hglrc) == TRUE;
    }
    void endRender(const LumeString& id) {
        auto v = find(id);
        if (!v || !v->valid) return;
        glFlush();
        SwapBuffers(v->hdc);
        wglMakeCurrent(nullptr, nullptr);
    }
    void refresh(const LumeString& id) {
        (void)id;
    }
    void setVisible(const LumeString& id, bool vis) {
        auto v = find(id);
        if (!v || !v->valid) return;
        v->visible = vis;
        ShowWindow(v->hwnd, vis ? SW_SHOW : SW_HIDE);
    }
    void moveToFullscreen(const LumeString& id) {
        auto v = find(id);
        if (!v || !v->valid || !g_mainWnd) return;
        RECT cr;
        GetClientRect(g_mainWnd, &cr);
        v->fullscreen = true;
        v->visible = true;
        MoveWindow(v->hwnd, 0, 0, cr.right, cr.bottom, TRUE);
        ShowWindow(v->hwnd, SW_SHOW);
        BringWindowToTop(v->hwnd);
    }
    void restoreFromFullscreen(const LumeString& id) {
        auto v = find(id);
        if (!v || !v->valid) return;
        v->fullscreen = false;
        MoveWindow(v->hwnd, v->x, v->y, v->w, v->h, TRUE);
        if (v->visible) ShowWindow(v->hwnd, SW_SHOW);
    }
    void hideAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (v && v->valid) {
                v->visible = false;
                ShowWindow(v->hwnd, SW_HIDE);
            }
        }
    }
    void showAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            if (v && v->valid && v->visible) {
                ShowWindow(v->hwnd, SW_SHOW);
            }
        }
    }
    void destroyAll() {
        for (auto& kv : g_views) {
            auto& v = kv.second;
            destroyView(v);
        }
        g_views.clear();
    }
}
void captureCanvasMouse(const LumeString& canvasId, int mode) {
    if (g_mouseCaptured) return;
    auto v = GLCanvas::find(canvasId);
    if (v && v->valid) {
        g_mouseCaptured = true;
        g_captureMode = mode;
        g_ignoreWarpMouse = false;
        lstrcpynA(g_capturedCanvasId, canvasId.c_str(), 128);
        g_capturedCanvasW = v->w;
        g_capturedCanvasH = v->h;
        g_capturedCanvasX = v->x;
        g_capturedCanvasY = v->y - TOOLBAR_H;
        SetCapture(g_mainWnd);
        if (mode == 0 || mode == 1) ShowCursor(FALSE);
        else ShowCursor(TRUE);
        RECT wr;
        GetWindowRect(v->hwnd, &wr);
        ClipCursor(&wr);
    }
}
namespace GradientText {
    struct HSV {float h, s, v;};
    static HSV rgb2hsv(HTP::Color c) {
        float r = c.r * 0.003921569f;
        float g = c.g * 0.003921569f;
        float b = c.b * 0.003921569f;
        float max_val = r; if (g > max_val) max_val = g; if (b > max_val) max_val = b;
        float min_val = r; if (g < min_val) min_val = g; if (b < min_val) min_val = b;
        float d = max_val - min_val;
        float h = 0.0f;
        if (d != 0.0f) {
            if (max_val == r) h = (g - b) / d + (g < b ? 6.0f : 0.0f);
            else if (max_val == g) h = (b - r) / d + 2.0f;
            else h = (r - g) / d + 4.0f;
            h *= 60.0f;
        }
        return { h, (max_val == 0.0f) ? 0.0f : (d / max_val), max_val };
    }
    struct CacheBuffer {
        HDC dc = nullptr;
        HBITMAP bmp = nullptr;
        HBITMAP oldBmp = nullptr;
        BYTE* bits = nullptr;
        int w = 0, h = 0;
        void ensure(HDC ref, int reqW, int reqH) {
            if (w >= reqW && h >= reqH) return;
            if (dc) {
                SelectObject(dc, oldBmp);
                DeleteObject(bmp);
                DeleteDC(dc);
            }
            w = reqW + 256; 
            h = reqH + 128;
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = w;
            bmi.bmiHeader.biHeight = -h;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            dc = CreateCompatibleDC(ref);
            bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void**)&bits, NULL, 0);
            oldBmp = (HBITMAP)SelectObject(dc, bmp);
        }
    };
    static CacheBuffer g_maskBuf;
    static CacheBuffer g_outBuf;
    void cleanupBuffers() {
        if (g_maskBuf.dc) {
            SelectObject(g_maskBuf.dc, g_maskBuf.oldBmp);
            DeleteObject(g_maskBuf.bmp);
            DeleteDC(g_maskBuf.dc);
            g_maskBuf.dc = nullptr; g_maskBuf.w = 0; g_maskBuf.h = 0;
        }
        if (g_outBuf.dc) {
            SelectObject(g_outBuf.dc, g_outBuf.oldBmp);
            DeleteObject(g_outBuf.bmp);
            DeleteDC(g_outBuf.dc);
            g_outBuf.dc = nullptr; g_outBuf.w = 0; g_outBuf.h = 0;
        }
    }
    static LumeVector<HTP::Color> g_gradLine;
    void draw(HDC dc, const LumeString& text, HFONT font, int x, int y, HTP::Color c1, HTP::Color c2, float shimmer_offset = 0.0f) {
        if (text.empty()) return;
        HFONT oldFont = (HFONT)SelectObject(dc, font);
        SIZE totalSz;
        GetTextExtentPoint32U(dc, text.c_str(), (int)text.length(), &totalSz);
        int W = totalSz.cx;
        int H = totalSz.cy;
        if (W <= 0 || H <= 0) {
            SelectObject(dc, oldFont);
            return;
        }
        g_maskBuf.ensure(dc, W, H);
        g_outBuf.ensure(dc, W, H);
        if (g_gradLine.capacity() < (size_t)W) g_gradLine.reserve(W + 256);
        if (g_gradLine.size() < (size_t)W) g_gradLine.resize(W);
        BitBlt(g_maskBuf.dc, 0, 0, W, H, NULL, 0, 0, BLACKNESS);
        HFONT oldMaskFont = (HFONT)SelectObject(g_maskBuf.dc, font);
        SetBkMode(g_maskBuf.dc, TRANSPARENT);
        SetTextColor(g_maskBuf.dc, RGB(255, 255, 255));
        TextOutU(g_maskBuf.dc, 0, 0, text.c_str(), lstrlenA(text.c_str()));
        GdiFlush();
        BitBlt(g_outBuf.dc, 0, 0, W, H, dc, x, y, SRCCOPY);
        GdiFlush();
        HSV hsv1 = rgb2hsv(c1);
        HSV hsv2 = rgb2hsv(c2);
        float diffH = hsv2.h - hsv1.h;
        if (diffH > 180.0f) diffH -= 360.0f; else if (diffH < -180.0f) diffH += 360.0f;
        float diffS = hsv2.s - hsv1.s;
        float diffV = hsv2.v - hsv1.v;
        float inv_W = (W > 1) ? 1.0f / (W - 1) : 0.0f;
        HTP::Color* gradArr = g_gradLine.data();
        for (int px = 0; px < W; px++) {
            float t = px * inv_W + shimmer_offset;
            int t_int = (int)(t * 0.5f);
            if (t < 0.0f) t_int--;
            t = t - t_int * 2.0f;
            if (t > 1.0f) t = 2.0f - t;
            float h_val = hsv1.h + diffH * t;
            if (h_val >= 360.0f) h_val -= 360.0f;
            else if (h_val < 0.0f) h_val += 360.0f;
            float s = hsv1.s + diffS * t;
            float v = hsv1.v + diffV * t;
            float c = v * s;
            float h_prime = h_val * 0.016666667f;
            int hp = (int)h_prime;
            float x_val = c * (1.0f - std::abs(h_prime - (hp & ~1) - 1.0f));
            float m = v - c;
            float r = 0, g = 0, b = 0;
            switch (hp) {
                case 0:
                    r = c;
                    g = x_val;
                    break;
                case 1:
                    r = x_val;
                    g = c;
                    break;
                case 2:
                    g = c;
                    b = x_val;
                    break;
                case 3:
                    g = x_val;
                    b = c;
                    break;
                case 4:
                    r = x_val;
                    b = c;
                    break;
                default:
                    r = c;
                    b = x_val;
                    break;
            }
            gradArr[px] = {
                (int)((r + m) * 255.0f), (int)((g + m) * 255.0f), (int)((b + m) * 255.0f), 255
            };
        }
        int mPad = (g_maskBuf.w - W) * 4;
        int oPad = (g_outBuf.w - W) * 4;
        BYTE* mPtr = g_maskBuf.bits;
        BYTE* oPtr = g_outBuf.bits;
        for (int py = 0; py < H; py++) {
            for (int px = 0; px < W; px++) {
                BYTE alpha = mPtr[0];
                if (alpha) {
                    HTP::Color& gc = gradArr[px];
                    int bgB = oPtr[0];
                    int bgG = oPtr[1];
                    int bgR = oPtr[2];
                    int pB = (gc.b - bgB) * alpha + 128;
                    int pG = (gc.g - bgG) * alpha + 128;
                    int pR = (gc.r - bgR) * alpha + 128;
                    oPtr[0] = (BYTE)(bgB + ((pB + (pB >> 8)) >> 8));
                    oPtr[1] = (BYTE)(bgG + ((pG + (pG >> 8)) >> 8));
                    oPtr[2] = (BYTE)(bgR + ((pR + (pR >> 8)) >> 8));
                }
                mPtr += 4;
                oPtr += 4;
            }
            mPtr += mPad;
            oPtr += oPad;
        }
        BitBlt(dc, x, y, W, H, g_outBuf.dc, 0, 0, SRCCOPY);
        SelectObject(g_maskBuf.dc, oldMaskFont);
        SelectObject(dc, oldFont);
    }
}
static void gluPerspective_impl(double fovY, double aspect, double zNear, double zFar) {
    double f = 1.0 / tan(fovY * 3.14159265358979323846 / 360.0);
    double m[16] = {};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.0;
    m[14] = (2.0 * zFar * zNear) / (zNear - zFar);
    glMultMatrixd(m);
}
static void gluLookAt_impl(double eyeX, double eyeY, double eyeZ,
    double centerX, double centerY, double centerZ,
    double upX, double upY, double upZ) {
    double fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
    double flen = sqrt(fx * fx + fy * fy + fz * fz);
    if (flen > 0) { fx /= flen; fy /= flen; fz /= flen;}
    double sx = fy * upZ - fz * upY, sy = fz * upX - fx * upZ, sz = fx * upY - fy * upX;
    double slen = sqrt(sx * sx + sy * sy + sz * sz);
    if (slen > 0) { sx /= slen; sy /= slen; sz /= slen;}
    double ux = sy * fz - sz * fy, uy = sz * fx - sx * fz, uz = sx * fy - sy * fx;
    double m[16] = {
        sx, ux, -fx, 0,
        sy, uy, -fy, 0,
        sz, uz, -fz, 0,
        0,  0,   0,  1
    };
    glMultMatrixd(m);
    glTranslated(-eyeX, -eyeY, -eyeZ);
}
LumeString resolveUrl(const LumeString& url, const LumeString& baseUrl);
extern LumeString g_curUrl;
namespace WasmEngine {
    static IM3Environment env = nullptr;
    struct Instance {IM3Runtime runtime = nullptr; IM3Module module = nullptr; LumeString bytes;};
    static LumeMap<int, Instance> instances;
    static int nextId = 1;
    void init() {if (!env) env = m3_NewEnvironment();}
    int load(const LumeString& wasmBytes) {
        if (!env) init();
        IM3Runtime runtime = m3_NewRuntime(env, 64 * 1024, nullptr);
        if (!runtime) return -1;
        int id = nextId++;
        Instance& inst = instances[id];
        inst.bytes = wasmBytes;
        IM3Module module;
        M3Result result = m3_ParseModule(env, &module, (const uint8_t*)inst.bytes.data(), inst.bytes.size());
        if (result) {
            m3_FreeRuntime(runtime);
            instances.erase(id);
            return -1;
        }
        result = m3_LoadModule(runtime, module);
        if (result) {
            m3_FreeRuntime(runtime);
            instances.erase(id);
            return -1;
        }
        inst.runtime = runtime;
        inst.module = module;
        return id;
    }
    void clear() {
        for (auto& kv : instances) m3_FreeRuntime(kv.second.runtime);
        instances.clear();
        if (env) {
            m3_FreeEnvironment(env);
            env = nullptr;
        }
    }
}
extern HTP::Doc g_doc;
namespace GLBuffers {
    LumeMap<int, LumeVector<float>> buffers;
    int nextId = 1;
    void clear() { buffers.clear(); }
}
namespace GLFont {
    struct Key {
        HGLRC ctx; uint64_t hash;
        bool operator==(const Key& o) const {return ctx == o.ctx && hash == o.hash;}
    };
    struct Entry {Key k; GLuint v; bool occ;};
    static Entry* g_tab = nullptr;
    static size_t g_cap = 0, g_sz = 0;
    GLuint get(HDC hdc, HGLRC ctx, HFONT font, uint64_t hash) {
        Key k{ctx, hash};
        uint64_t kh = hash ^ (uint64_t)ctx;
        if (g_sz >= g_cap / 2) {
            size_t oCap = g_cap; Entry* oTab = g_tab;
            g_cap = oCap == 0 ? 16 : oCap * 2;
            g_tab = (Entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, g_cap * sizeof(Entry));
            g_sz = 0;
            if (oTab) {
                for (size_t j = 0; j < oCap; ++j) {
                    if (oTab[j].occ) {
                        uint64_t h = oTab[j].k.hash ^ (uint64_t)oTab[j].k.ctx;
                        size_t idx = h & (g_cap - 1);
                        while (g_tab[idx].occ) idx = (idx + 1) & (g_cap - 1);
                        g_tab[idx] = oTab[j];
                        g_sz++;
                    }
                }
                HeapFree(GetProcessHeap(), 0, oTab);
            }
        }
        size_t idx = kh & (g_cap - 1);
        while (g_tab[idx].occ) {
            if (g_tab[idx].k == k) return g_tab[idx].v;
            idx = (idx + 1) & (g_cap - 1);
        }
        GLuint base = glGenLists(2048);
        HFONT oldFont = (HFONT)SelectObject(hdc, font);
        wglUseFontBitmapsW(hdc, 0, 2048, base);
        SelectObject(hdc, oldFont);
        g_tab[idx] = {k, base, true};
        g_sz++;
        return base;
    }
    void clear() {
        if (g_tab) {
            for (size_t j = 0; j < g_cap; ++j) if (g_tab[j].occ) glDeleteLists(g_tab[j].v, 2048);
            HeapFree(GetProcessHeap(), 0, g_tab);
            g_tab = nullptr; g_cap = g_sz = 0;
        }
    }
}
namespace GLModelCache {
    LumeVector<GLuint> lists;
    void add(GLuint id) { lists.push_back(id); }
    void clear() {
        for (GLuint id : lists) glDeleteLists(id, 1);
        lists.clear();
    }
}
namespace GLBatcher {
    static LumeVector<float> vertices;
    static LumeVector<float> colors;
    static LumeVector<float> texCoords;
    static LumeVector<float> normals;
    static GLenum currentMode = 0;
    static float curR = 1, curG = 1, curB = 1, curA = 1;
    static float curU = 0, curV = 0;
    static float curNx = 0, curNy = 0, curNz = 1;
    static bool useColor = false;
    static bool useTex = false;
    static bool useNormal = false;
    void begin(GLenum mode) {
        currentMode = mode;
        vertices.clear();
        colors.clear();
        texCoords.clear();
        normals.clear();
        useColor = false;
        useTex = false;
        useNormal = false;
    }
    void color(float r, float g, float b, float a) {
        curR = r;
        curG = g;
        curB = b;
        curA = a;
        useColor = true;
    }
    void texCoord(float u, float v) {
        curU = u; curV = v;
        useTex = true;
    }
    void normal(float x, float y, float z) {
        curNx = x; curNy = y; curNz = z;
        useNormal = true;
    }
    void vertex(float x, float y, float z) {
        vertices.push_back(x); vertices.push_back(y); vertices.push_back(z);
        if (useColor) {
            colors.push_back(curR);
            colors.push_back(curG);
            colors.push_back(curB);
            colors.push_back(curA);
        }
        if (useTex) {
            texCoords.push_back(curU);
            texCoords.push_back(curV);
        }
        if (useNormal) {
            normals.push_back(curNx);
            normals.push_back(curNy);
            normals.push_back(curNz);
        }
    }
    void end() {
        if (vertices.empty()) return;
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.data());
        if (useColor && colors.size() == (vertices.size() / 3) * 4) {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_FLOAT, 0, colors.data());
        }
        if (useTex && texCoords.size() == (vertices.size() / 3) * 2) {
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, texCoords.data());
        }
        if (useNormal && normals.size() == vertices.size()) {
            glEnableClientState(GL_NORMAL_ARRAY);
            glNormalPointer(GL_FLOAT, 0, normals.data());
        }
        glDrawArrays(currentMode, 0, (GLsizei)(vertices.size() / 3));
        glDisableClientState(GL_VERTEX_ARRAY);
        if (useColor) glDisableClientState(GL_COLOR_ARRAY);
        if (useTex) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        if (useNormal) glDisableClientState(GL_NORMAL_ARRAY);
    }
}
namespace Bindings {
    struct InputState {
        LumeString text, placeholder;
        int x = 0, y = 0, w = 250, h = 28;
        bool focused = false;
        int cursor = 0;
    };
    LumeMap<LumeString, InputState> g_inputs;
    LumeMap<LumeString, LumeString> g_texts;
    LumeMap<LumeString, int> g_offsets_y;
    LumeMap<LumeString, float> g_shimmer_offsets;
    LumeMap<LumeString, LumeAction> g_clicks;
    LumeMap<LumeString, LumeAction> g_rightClicks;
    bool g_keyPressed[256] = {false};
    bool g_keyReleased[256] = {false};
    LumeString g_focusId;
    void reset() {
        g_clicks.clear();
        g_rightClicks.clear();
        memset(g_keyPressed, 0, sizeof(g_keyPressed));
        memset(g_keyReleased, 0, sizeof(g_keyReleased));
        g_texts.clear();
        g_offsets_y.clear();
        g_shimmer_offsets.clear();
        g_inputs.clear();
        g_focusId.clear();
    }
    void set_prop(const char* id, const char* prop, const char* val) {
        auto e = HTP::findById(g_doc.root, id);
        if (e) {
            bool found = false;
            for (auto& pair : e->props.d) {
                if (pair.first == prop) { pair.second = val; found = true; break; }
            }
            if (!found) e->props.d.push_back({ prop, val });
            invalidateDOM();
        }
    }
    const char* get_node_prop(const char* id, const char* prop, const char* def) {
        auto e = HTP::findById(g_doc.root, id);
        if (e) return e->props.get(prop, def);
        return def;
    }
    void set_inner_htp(const char* id, const char* htp) {
        auto e = HTP::findById(g_doc.root, id);
        if (e) {
            HTP::Parser p;
            HTP::Doc temp = p.parse(htp);
            e->children = temp.root->children;
            invalidateDOM();
        }
    }
    void set_text(const char* id, const char* t) {
        LumeString nv = t;
        auto& c = g_texts[id];
        if (c != nv) {
            c = nv;
            auto i = g_inputs.find(id);
            if (i != g_inputs.end()) i->second.text = nv;
            invalidateDOM();
        }
    }
    const char* get_text(const char* id) {
        auto i = g_texts.find(id);
        if (i != g_texts.end()) return i->second.c_str();
        auto j = g_inputs.find(id);
        if (j != g_inputs.end()) return j->second.text.c_str();
        return "";
    }
    void set_offset_y(const char* id, int o) {
        auto& c = g_offsets_y[id];
        if (c != o) {
            c = o;
            invalidateDOM();
        }
    }
    int get_offset_y(const char* id) {
        auto i = g_offsets_y.find(id);
        return i != g_offsets_y.end() ? i->second : 0;
    }
    void set_shimmer(const char* id, float s) {
        g_shimmer_offsets[id] = s;
        invalidateDOM();
    }
    void on_click(const char* id, LumeAction action) {
        g_clicks[id] = static_cast<LumeAction&&>(action);
    }
    void on_right_click(const char* id, LumeAction action) {
        g_rightClicks[id] = static_cast<LumeAction&&>(action);
    }
    void fire_click(const char* id) {
        auto i = g_clicks.find(id);
        if (i != g_clicks.end()) i->second();
    }
    void fire_right_click(const char* id) {
        auto i = g_rightClicks.find(id);
        if (i != g_rightClicks.end()) i->second();
    }
    bool is_key_pressed(int vk) {
        int key = vk & 0xFF;
        bool res = g_keyPressed[key];
        g_keyPressed[key] = false;
        return res;
    }
    bool is_key_released(int vk) {
        int key = vk & 0xFF;
        bool res = g_keyReleased[key];
        g_keyReleased[key] = false;
        return res;
    }
    bool key_down(int vk) {
        return (GetAsyncKeyState(vk) & 0x8000) != 0;
    }
    void get_mouse(int* x, int* y) {
        if (!g_opt_lua_mouse) {
            *x = 0;
            *y = 0;
            return;
        }
        POINT p; GetCursorPos(&p);
        if (g_mainWnd) ScreenToClient(g_mainWnd, &p);
        *x = p.x; *y = p.y - TOOLBAR_H;
    }
    void get_mouse_delta(int* dx, int* dy) {
        if (!g_opt_lua_mouse) {*dx = 0; *dy = 0; return;}
        *dx = g_mouseDeltaX;
        *dy = g_mouseDeltaY;
        g_mouseDeltaX = 0;
        g_mouseDeltaY = 0;
    }
    int get_mouse_wheel() {
        if (!g_opt_lua_mouse) return 0;
        int res = g_mouseWheelDelta;
        g_mouseWheelDelta = 0;
        return res;
    }
    bool capture_mouse(const char* id, int mode) {
        captureCanvasMouse(id, mode);
        return g_mouseCaptured;
    }
    void get_canvas_mouse(const char* id, int* x, int* y) {
        auto v = GLCanvas::find(id);
        if (!v || !v->valid) { *x = 0; *y = 0; return; }
        POINT p; GetCursorPos(&p);
        ScreenToClient(v->hwnd, &p);
        *x = p.x; *y = p.y;
    }
    void release_mouse() {::releaseMouse();}
    bool is_mouse_captured() {return g_mouseCaptured;}
    void fullscreen_canvas(const char* id, int enter) {
        if (enter) enterFullscreenCanvas(id);
        else exitFullscreenCanvas();
    }
    bool is_fullscreen() {return g_fullscreenCanvas;}
    bool window_active() {
        HWND fg = GetForegroundWindow();
        return (fg == g_mainWnd) || (GetParent(fg) == g_mainWnd);
    }
    void navigate(const char* url) {
        LumeString* u = new LumeString(url);
        PostMessage(g_mainWnd, WM_NAVIGATE_DEFERRED, (WPARAM)u, 0);
    }
    void alert(const char* msg) {
        MessageBoxU(g_mainWnd, msg, "Lume", MB_OK);
    }
    void refresh() {invalidateContent();}
    const char* get_input(const char* id) {
        auto i = g_inputs.find(id);
        return i != g_inputs.end() ? i->second.text.c_str() : "";
    }
    void set_input(const char* id, const char* t) {
        g_inputs[id].text = t;
        g_inputs[id].cursor = lstrlenA(t);
        invalidateContent();
    }
    bool isLocalAccessAllowed() {
        return (g_curUrl.find("http://") != 0 && g_curUrl.find("https://") != 0);
    }
    void http_request(const char* url, LumeString& out_body, int& out_code) {
        if (!g_opt_lua_http) {
            out_body = "SECURITY_ERROR: HTTP requests are disabled by user.";
            out_code = 403; return;
        }
        LumeString reqUrl = url;
        if (reqUrl.find("file://") == 0 && !isLocalAccessAllowed()) {
            out_body = "SECURITY_ERROR: Remote pages cannot access local files.";
            out_code = 403; return;
        }
        auto r = Net::fetchUrl(reqUrl);
        out_body = r.body;
        out_code = r.code;
    }
    void cv_clear(const char* id, const char* color_hex) {
        auto i = Canvas::g_bufs.find(id);
        if (i != Canvas::g_bufs.end() && i->second->dc) {
            auto c = HTP::Color::fromHex(color_hex);
            HBRUSH b = CreateSolidBrush(c.cr());
            RECT r = { 0,0,i->second->w,i->second->h };
            FillRect(i->second->dc, &r, b);
            DeleteObject(b);
        }
    }
    void cv_rect(const char* id, int x, int y, int w, int h, const char* color_hex) {
        auto i = Canvas::g_bufs.find(id);
        if (i != Canvas::g_bufs.end() && i->second->dc) {
            auto c = HTP::Color::fromHex(color_hex);
            HBRUSH b = CreateSolidBrush(c.cr());
            RECT r = { x,y,x + w,y + h };
            FillRect(i->second->dc, &r, b);
            DeleteObject(b);
        }
    }
    void cv_circle(const char* id, int cx, int cy, int r, const char* color_hex) {
        auto i = Canvas::g_bufs.find(id);
        if (i != Canvas::g_bufs.end() && i->second->dc) {
            auto c = HTP::Color::fromHex(color_hex);
            HBRUSH b = CreateSolidBrush(c.cr());
            HPEN p = CreatePen(PS_SOLID, 1, c.cr());
            auto ob = SelectObject(i->second->dc, b);
            auto op = SelectObject(i->second->dc, p);
            Ellipse(i->second->dc, cx - r, cy - r, cx + r, cy + r);
            SelectObject(i->second->dc, ob); SelectObject(i->second->dc, op);
            DeleteObject(b); DeleteObject(p);
        }
    }
    void cv_line(const char* id, int x1, int y1, int x2, int y2, const char* color_hex, int t) {
        auto i = Canvas::g_bufs.find(id);
        if (i != Canvas::g_bufs.end() && i->second->dc) {
            auto c = HTP::Color::fromHex(color_hex);
            HPEN p = CreatePen(PS_SOLID, t, c.cr());
            auto op = SelectObject(i->second->dc, p);
            MoveToEx(i->second->dc, x1, y1, 0);
            LineTo(i->second->dc, x2, y2);
            SelectObject(i->second->dc, op); DeleteObject(p);
        }
    }
    void cv_text(const char* id, int x, int y, const char* txt, int sz, const char* color_hex) {
        auto i = Canvas::g_bufs.find(id);
        if (i != Canvas::g_bufs.end() && i->second->dc) {
            auto c = HTP::Color::fromHex(color_hex);
            HFONT f = FontCache::get(sz);
            auto of = SelectObject(i->second->dc, f);
            SetTextColor(i->second->dc, c.cr());
            SetBkMode(i->second->dc, TRANSPARENT);
            TextOutU(i->second->dc, x, y, txt, lstrlenA(txt));
            SelectObject(i->second->dc, of);
        }
    }
}
namespace Script {
    LumeMap<int, int> g_timerRefs;
    LumeMap<LumeString, int> g_canvasClickRefs;
    int g_timerN = 9000;
    int g_keyDownRef = LUA_NOREF;
    lua_State* g_L = nullptr;
static int l_set_prop(lua_State* L) {
    Bindings::set_prop(luaL_checkstring(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3));
    return 0;
}
static int l_get_node_prop(lua_State* L) {
    lua_pushstring(L, Bindings::get_node_prop(luaL_checkstring(L, 1), luaL_checkstring(L, 2), luaL_optstring(L, 3, "")));
    return 1;
}
static int l_set_inner_htp(lua_State* L) {
    Bindings::set_inner_htp(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}
static int l_set_text(lua_State* L) {
    Bindings::set_text(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}
static int l_get_text(lua_State* L) {
    lua_pushstring(L, Bindings::get_text(luaL_checkstring(L, 1)));
    return 1;
}
static int l_set_offset_y(lua_State* L) {
    Bindings::set_offset_y(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}
static int l_get_offset_y(lua_State* L) {
    lua_pushinteger(L, Bindings::get_offset_y(luaL_checkstring(L, 1)));
    return 1;
}
static int l_set_shimmer(lua_State* L) {
    Bindings::set_shimmer(luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2));
    return 0;
}
static int l_on_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    LumeString sid = id;
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    Bindings::on_click(id, LumeAction([sid, ref]() {
        if (!g_L) return;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(g_L, 0, 0, 0) != 0) {
            OutputDebugStringA(lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
        }));
    return 0;
}
static int l_on_right_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    LumeString sid = id;
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    Bindings::on_right_click(id, LumeAction([sid, ref]() {
        if (!g_L) return;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, ref);
        if (lua_pcall(g_L, 0, 0, 0) != 0) {
            OutputDebugStringA(lua_tostring(g_L, -1));
            lua_pop(g_L, 1);
        }
        }));
    return 0;
}
static int l_is_key_pressed(lua_State* L) {
    lua_pushboolean(L, Bindings::is_key_pressed((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}
static int l_is_key_released(lua_State* L) {
    lua_pushboolean(L, Bindings::is_key_released((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}
static int l_get_mouse(lua_State* L) {
    int x, y; Bindings::get_mouse(&x, &y);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
}
static int l_get_mouse_delta(lua_State* L) {
    int dx, dy; Bindings::get_mouse_delta(&dx, &dy);
    lua_pushinteger(L, dx);
    lua_pushinteger(L, dy);
    return 2;
}
static int l_get_mouse_wheel(lua_State* L) {
    lua_pushinteger(L, Bindings::get_mouse_wheel());
    return 1;
}
static int l_capture_mouse(lua_State* L) {
    lua_pushboolean(L, Bindings::capture_mouse(luaL_checkstring(L, 1), (int)luaL_optinteger(L, 2, 0)) ? 1 : 0);
    return 1;
}
static int l_get_canvas_mouse(lua_State* L) {
    int x, y; Bindings::get_canvas_mouse(luaL_checkstring(L, 1), &x, &y); lua_pushinteger(L, x); lua_pushinteger(L, y);
    return 2;
}
static int l_release_mouse(lua_State* L) {
    Bindings::release_mouse();
    return 0;
}
static int l_is_mouse_captured(lua_State* L) {
    lua_pushboolean(L, Bindings::is_mouse_captured() ? 1 : 0);
    return 1;
}
static int l_fullscreen_canvas(lua_State* L) {
    Bindings::fullscreen_canvas(luaL_checkstring(L, 1), lua_toboolean(L, 2));
    return 0;
}
static int l_is_fullscreen(lua_State* L) {
    lua_pushboolean(L, Bindings::is_fullscreen() ? 1 : 0);
    return 1;
}
static int l_window_active(lua_State* L) {
    lua_pushboolean(L, Bindings::window_active() ? 1 : 0);
    return 1;
}
static bool isLocalAccessAllowed() {
    return (g_curUrl.find("http://") != 0 && g_curUrl.find("https://") != 0);
}
static int l_http(lua_State* L) {
    LumeString body; int code;
    Bindings::http_request(luaL_checkstring(L, 1), body, code);
    lua_pushstring(L, body.c_str()); lua_pushinteger(L, code);
    return 2;
}
static int l_navigate(lua_State* L) {
    Bindings::navigate(luaL_checkstring(L, 1));
    return 0;
}
static int l_alert(lua_State* L) {
    Bindings::alert(luaL_checkstring(L, 1));
    return 0;
}
static int l_refresh(lua_State* L) {
    Bindings::refresh();
    return 0;
}
static int l_get_input(lua_State* L) {
    lua_pushstring(L, Bindings::get_input(luaL_checkstring(L, 1)));
    return 1;
}
static int l_set_input(lua_State* L) {
    Bindings::set_input(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}
static int l_key_down(lua_State* L) {
    lua_pushboolean(L, Bindings::key_down((int)luaL_checkinteger(L, 1)) ? 1 : 0);
    return 1;
}
static int l_cv_clear(lua_State* L) {
    Bindings::cv_clear(luaL_checkstring(L, 1), luaL_optstring(L, 2, "#000000"));
    return 0;
}
static int l_cv_rect(lua_State* L) {
    Bindings::cv_rect(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4), (int)luaL_checkinteger(L, 5), luaL_optstring(L, 6, "#ffffff"));
    return 0;
}
static int l_cv_circle(lua_State* L) {
    Bindings::cv_circle(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4), luaL_optstring(L, 5, "#ffffff"));
    return 0;
}
static int l_cv_line(lua_State* L) {
    Bindings::cv_line(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4), (int)luaL_checkinteger(L, 5), luaL_optstring(L, 6, "#ffffff"), (int)luaL_optinteger(L, 7, 1));
    return 0;
}
static int l_cv_text(lua_State* L) {
    Bindings::cv_text(luaL_checkstring(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), luaL_checkstring(L, 4), (int)luaL_optinteger(L, 5, 16), luaL_optstring(L, 6, "#ffffff"));
    return 0;
}
static int l_gl_available(lua_State* L) {
    lua_pushboolean(L, (g_opt_gpu && GLLoader::available()) ? 1 : 0);
    return 1;
}
static int l_gl_begin(lua_State* L) {
    if (!g_opt_gpu) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const char* id = luaL_checkstring(L, 1);
    int w = (int)luaL_optinteger(L, 2, 0);
    int h = (int)luaL_optinteger(L, 3, 0);
    lua_pushboolean(L, GLCanvas::beginRender(id, w, h) ? 1 : 0);
    return 1;
}
static int l_gl_end(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    GLCanvas::endRender(id);
    return 0;
}
static int l_gl_refresh(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    GLCanvas::refresh(id);
    return 0;
}
static int l_gl_clear(lua_State* L) {
    glClearColor((float)luaL_optnumber(L, 1, 0), (float)luaL_optnumber(L, 2, 0), (float)luaL_optnumber(L, 3, 0), (float)luaL_optnumber(L, 4, 1));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    return 0;
}
static int l_gl_viewport(lua_State* L) {
    glViewport((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3), (int)luaL_checkinteger(L, 4));
    return 0;
}
static int l_gl_color(lua_State* L) {
    GLBatcher::color((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 1.0f));
    return 0;
}
static int l_gl_begin_prim(lua_State* L) {
    GLBatcher::begin((GLenum)luaL_checkinteger(L, 1));
    return 0;
}
static int l_gl_end_prim(lua_State* L) {
    (void)L;
    GLBatcher::end();
    return 0;
}
static int l_gl_vertex2f(lua_State* L) {
    GLBatcher::vertex((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), 0.0f);
    return 0;
}
static int l_gl_vertex3f(lua_State* L) {
    GLBatcher::vertex((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    return 0;
}
static int l_gl_ortho(lua_State* L) {
    glOrtho(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6));
    return 0;
}
static int l_gl_frustum(lua_State* L) {
    glFrustum(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6));
    return 0;
}
static int l_gl_load_identity(lua_State* L) {
    (void)L; glLoadIdentity(); return 0; }
static int l_gl_matrix_mode(lua_State* L) {
    glMatrixMode((GLenum)luaL_checkinteger(L, 1));
    return 0;
}
static int l_gl_push_matrix(lua_State* L) {
    (void)L; glPushMatrix();
    return 0;
}
static int l_gl_pop_matrix(lua_State* L) {
    (void)L; glPopMatrix();
    return 0;
}
static int l_gl_translatef(lua_State* L) {
    glTranslatef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    return 0;
}
static int l_gl_rotatef(lua_State* L) {
    glRotatef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
    return 0;
}
static int l_gl_scalef(lua_State* L) {
    glScalef((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    return 0;
}
static int l_gl_enable(lua_State* L) {
    glEnable((GLenum)luaL_checkinteger(L, 1));
    return 0;
}
static int l_gl_disable(lua_State* L) {
    glDisable((GLenum)luaL_checkinteger(L, 1));
    return 0;
}
static int l_gl_line_width(lua_State* L) {
    glLineWidth((float)luaL_checknumber(L, 1));
    return 0;
}
static int l_gl_point_size(lua_State* L) {
    glPointSize((float)luaL_checknumber(L, 1));
    return 0;
}

static int l_gl_normal3f(lua_State* L) {
    GLBatcher::normal((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    return 0;
}
static int l_glu_perspective(lua_State* L) {
    gluPerspective_impl(luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3), luaL_checknumber(L, 4));
    return 0;
}
static int l_glu_look_at(lua_State* L) {
    gluLookAt_impl(
        luaL_checknumber(L, 1), luaL_checknumber(L, 2), luaL_checknumber(L, 3),
        luaL_checknumber(L, 4), luaL_checknumber(L, 5), luaL_checknumber(L, 6),
        luaL_checknumber(L, 7), luaL_checknumber(L, 8), luaL_checknumber(L, 9)
    );
    return 0;
}
static int l_gl_gen_list(lua_State* L) {
    lua_pushinteger(L, (int)glGenLists(1));
    return 1;
}
static int l_gl_new_list(lua_State* L) {
    glNewList((GLuint)luaL_checkinteger(L, 1), GL_COMPILE);
    return 0;
}
static int l_gl_end_list(lua_State* L) {
    (void)L; glEndList();
    return 0;
}
static int l_gl_call_list(lua_State* L) {
    glCallList((GLuint)luaL_checkinteger(L, 1));
    return 0;
}
static int l_gl_delete_list(lua_State* L) {
    glDeleteLists((GLuint)luaL_checkinteger(L, 1), 1);
    return 0;
}
static int l_set_timer(lua_State* L) {
    int ms = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    int tid = ++g_timerN;
    g_timerRefs[tid] = ref;
    SetTimer(g_mainWnd, tid, ms, [](HWND, UINT, UINT_PTR id, DWORD) {
        if (!g_mainWnd || IsIconic(g_mainWnd)) return;
        auto i = g_timerRefs.find((int)id);
        if (i == g_timerRefs.end() || !g_L) return;
        lua_rawgeti(g_L, LUA_REGISTRYINDEX, i->second);
        if (lua_pcall(g_L, 0, 0, 0) != 0) {
            const char* err = lua_tostring(g_L, -1);
            OutputDebugStringA(err ? err : "Lua timer error\n");
            KillTimer(g_mainWnd, id);
            MessageBoxU(g_mainWnd, err ? err : "Unknown error", "Lua Timer Error", MB_OK | MB_ICONERROR);
            lua_pop(g_L, 1);
            luaL_unref(g_L, LUA_REGISTRYINDEX, i->second);
            g_timerRefs.erase(i);
        }
        });
    lua_pushinteger(L, tid); return 1;
}
static int l_kill_timer(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    KillTimer(g_mainWnd, id);
    auto it = g_timerRefs.find(id);
    if (it != g_timerRefs.end()) { if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, it->second); g_timerRefs.erase(it); }
    return 0;
}
static int l_on_key_down(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    if (g_keyDownRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, g_keyDownRef);
    lua_pushvalue(L, 1);
    g_keyDownRef = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static int l_get_time(lua_State* L) {
    lua_pushnumber(L, getHighResTime());
    return 1;
}
static int l_get_window_size(lua_State* L) {
    RECT cr;
    GetClientRect(g_mainWnd, &cr);
    if (g_fullscreenCanvas) {
        lua_pushinteger(L, cr.right);
        lua_pushinteger(L, cr.bottom);
    }
    else {
        lua_pushinteger(L, cr.right);
        lua_pushinteger(L, cr.bottom - TOOLBAR_H - STATUS_H);
    }
    return 2;
}
void fireCanvasClick(const LumeString& canvasId, int x, int y, int button) {
    auto it = g_canvasClickRefs.find(canvasId);
    if (it == g_canvasClickRefs.end() || !g_L) return;
    lua_rawgeti(g_L, LUA_REGISTRYINDEX, it->second);
    lua_pushinteger(g_L, x);
    lua_pushinteger(g_L, y);
    lua_pushinteger(g_L, button);
    if (lua_pcall(g_L, 3, 0, 0) != 0) { OutputDebugStringA(lua_tostring(g_L, -1)); lua_pop(g_L, 1); }
}
static int l_on_canvas_click(lua_State* L) {
    const char* id = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    LumeString sid = id;
    auto it = g_canvasClickRefs.find(sid);
    if (it != g_canvasClickRefs.end()) luaL_unref(L, LUA_REGISTRYINDEX, it->second);
    lua_pushvalue(L, 2);
    g_canvasClickRefs[sid] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}
static int l_gl_blend_func(lua_State* L) {
    glBlendFunc((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2));
    return 0;
}
static int l_glu_sphere(lua_State* L) {
    double radius = luaL_checknumber(L, 1);
    int slices = (int)luaL_checkinteger(L, 2);
    int stacks = (int)luaL_checkinteger(L, 3);
    GLUquadric* q = gluNewQuadric();
    gluSphere(q, radius, slices, stacks);
    gluDeleteQuadric(q);
    return 0;
}
static int l_gl_bind_texture(lua_State* L) {
    GLenum target = (GLenum)luaL_checkinteger(L, 1);
    GLuint texID = (GLuint)luaL_checkinteger(L, 2);
    glBindTexture(target, texID);
    return 0;
}
static int l_gl_text_coord2f(lua_State* L) {
    GLBatcher::texCoord((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2));
    return 0;
}
static int l_gl_load_texture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
    Gdiplus::Bitmap bmp(wpath);
    if (bmp.GetLastStatus() != Gdiplus::Ok) {
        lua_pushnil(L);
        return 1;
    }
    int w = bmp.GetWidth();
    int h = bmp.GetHeight();
    LumeVector<unsigned char> pixels;
    pixels.resize(w * h * 4);
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData);
    const unsigned char* src = (const unsigned char*)bmpData.Scan0;
    int stride = bmpData.Stride;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * stride + x * 4;
            int p = (y * w + x) * 4;
            pixels[p + 0] = src[i + 2];
            pixels[p + 1] = src[i + 1];
            pixels[p + 2] = src[i + 0];
            pixels[p + 3] = src[i + 3];
        }
    }
    bmp.UnlockBits(&bmpData);
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    lua_pushinteger(L, texID);
    return 1;
}
namespace FastJson {
    enum Type {TYPE_NULL = 0, TYPE_BOOL, TYPE_NUMBER, TYPE_STRING, TYPE_ARRAY, TYPE_OBJECT};
    struct Node {
        Type type; char* key; Node* next;
        union {
            char* str_val;
            double num_val;
            bool bool_val;
            Node* child;
        };
    };
    struct ArenaChunk {
        ArenaChunk* next;
        size_t used;
        size_t cap;
        char data[1];
    };
    struct Doc {
        ArenaChunk* head = nullptr;
        Node* root = nullptr;
    };
    static void* Alloc(Doc* doc, size_t size) {
        size = (size + 7) & ~7;
        if (!doc->head || doc->head->used + size > doc->head->cap) {
            size_t cap = size > 65536 ? size : 65536;
            ArenaChunk* chunk = (ArenaChunk*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ArenaChunk) + cap);
            if (!chunk) return nullptr;
            chunk->cap = cap;
            chunk->next = doc->head;
            doc->head = chunk;
        }
        void* ptr = doc->head->data + doc->head->used;
        doc->head->used += size;
        return ptr;
    }
    static void Free(Doc* doc) {
        while (doc->head) {
            ArenaChunk* next = doc->head->next;
            HeapFree(GetProcessHeap(), 0, doc->head);
            doc->head = next;
        }
    }
    static void SkipWS(const char** p) {while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') (*p)++;}
    static bool FastStrToD(const char** str, double* out) {
        const char* p = *str; double sign = 1.0;
        if (*p == '-') {
            sign = -1.0;
            p++;
        }
        double val = 0.0; bool has_digits = false;
        while (*p >= '0' && *p <= '9') {
            val = val * 10.0 + (*p - '0');
            p++;
            has_digits = true;
        }
        if (!has_digits) return false;
        if (*p == '.') {
            p++; double frac = 1.0;
            while (*p >= '0' && *p <= '9') {
                frac *= 0.1;
                val += (*p - '0') * frac;
                p++;
            }
        }
        if (*p == 'e' || *p == 'E') {
            p++;
            double exp_sign = 1.0;
            if (*p == '-') {
                exp_sign = -1.0;
                p++;
            }
            else if (*p == '+') p++;
            int exp = 0;
            while (*p >= '0' && *p <= '9') {
                exp = exp * 10 + (*p - '0');
                p++;
            }
            double mult = 1.0, base = 10.0;
            while (exp > 0) {
                if (exp & 1) mult *= base;
                base *= base;
                exp >>= 1;
            }
            if (exp_sign < 0) val /= mult;
            else val *= mult;
        }
        *str = p;
        *out = sign * val;
        return true;
    }
    static char* ParseStr(Doc* doc, const char** p) {
        (*p)++;
        const char* start = *p;
        while (**p && **p != '"') {
            if (**p == '\\') (*p)++;
            (*p)++;
        }
        if (**p != '"') return nullptr;
        size_t max_len = *p - start;
        char* str = (char*)Alloc(doc, max_len + 1);
        if (!str) return nullptr;
        const char* src = start; char* dst = str;
        while (src < *p) {
            if (*src == '\\') {
                src++;
                switch (*src) {
                case '"': *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                case '/': *dst++ = '/'; break;
                case 'b': *dst++ = '\b'; break;
                case 'f': *dst++ = '\f'; break;
                case 'n': *dst++ = '\n'; break;
                case 'r': *dst++ = '\r'; break;
                case 't': *dst++ = '\t'; break;
                default: *dst++ = *src;break;
                }
                src++;
            }
            else *dst++ = *src++;
        }
        *dst = '\0';
        (*p)++;
        return str;
    }
    static Node* ParseVal(Doc* doc, const char** p, int depth);
    static Node* ParseArr(Doc* doc, const char** p, int depth) {
        if (depth > 512) return nullptr;
        Node* node = (Node*)Alloc(doc, sizeof(Node));
        node->type = TYPE_ARRAY;
        (*p)++;
        SkipWS(p);
        if (**p == ']') {
            (*p)++;
            return node;
        }
        Node* current = nullptr;
        while (**p) {
            Node* child = ParseVal(doc, p, depth + 1);
            if (!child) return nullptr;
            if (!node->child) {
                node->child = child;
                current = child;
            }
            else {
                current->next = child;
                current = child;
            }
            SkipWS(p);
            if (**p == ']') {
                (*p)++;
                return node;
            }
            if (**p != ',') return nullptr;
            (*p)++; SkipWS(p);
        }
        return nullptr;
    }
    static Node* ParseObj(Doc* doc, const char** p, int depth) {
        if (depth > 512) return nullptr;
        Node* node = (Node*)Alloc(doc, sizeof(Node));
        node->type = TYPE_OBJECT;
        (*p)++;
        SkipWS(p);
        if (**p == '}') {
            (*p)++;
            return node;
        }
        Node* current = nullptr;
        while (**p) {
            if (**p != '"') return nullptr;
            char* key = ParseStr(doc, p);
            if (!key) return nullptr;
            SkipWS(p);
            if (**p != ':') return nullptr;
            (*p)++;
            SkipWS(p);
            Node* child = ParseVal(doc, p, depth + 1);
            if (!child) return nullptr;
            child->key = key;
            if (!node->child) {
                node->child = child;
                current = child;
            }
            else {
                current->next = child;
                current = child;
            }
            SkipWS(p); if (**p == '}') {
                (*p)++;
                return node;
            }
            if (**p != ',') return nullptr;
            (*p)++;
            SkipWS(p);
        }
        return nullptr;
    }
    static Node* ParseVal(Doc* doc, const char** p, int depth) {
        SkipWS(p);
        char c = **p;
        if (!c) return nullptr;
        if (c == '"') {
            Node* n = (Node*)Alloc(doc, sizeof(Node));
            n->type = TYPE_STRING;
            n->str_val = ParseStr(doc, p);
            return n->str_val ? n : nullptr;
        }
        if (c == '{') return ParseObj(doc, p, depth);
        if (c == '[') return ParseArr(doc, p, depth);
        if ((c >= '0' && c <= '9') || c == '-') {
            double val;
            if (!FastStrToD(p, &val)) return nullptr;
            Node* n = (Node*)Alloc(doc, sizeof(Node));
            n->type = TYPE_NUMBER;
            n->num_val = val;
            return n;
        }
        auto match = [](const char* s, const char* t, int l) {
            for (int i = 0; i < l; i++) if (s[i] != t[i]) return false;
            return true;
        };
        if (match(*p, "true", 4)) {
            Node* n = (Node*)Alloc(doc, sizeof(Node));
            n->type = TYPE_BOOL;
            n->bool_val = true;
            (*p) += 4;
            return n;
        }
        if (match(*p, "false", 5)) {
            Node* n = (Node*)Alloc(doc, sizeof(Node));
            n->type = TYPE_BOOL;
            n->bool_val = false;
            (*p) += 5;
            return n;
        }
        if (match(*p, "null", 4)) {
            Node* n = (Node*)Alloc(doc, sizeof(Node));
            n->type = TYPE_NULL;
            (*p) += 4;
            return n;
        }
        return nullptr;
    }
    static bool Parse(const char* json, Doc* out) {
        const char* p = json; out->head = nullptr; out->root = ParseVal(out, &p, 0);
        if (!out->root) {
            Free(out);
            return false;
        }
        return true;
    }
    static Node* GetObj(Node* obj, const char* key) {
        if (!obj || obj->type != TYPE_OBJECT) return nullptr;
        for (Node* c = obj->child; c; c = c->next) if (c->key && fast_streq(c->key, key)) return c;
        return nullptr;
    }
}
static void PushJsonToLua(lua_State* L, FastJson::Node* node) {
    if (!node) { lua_pushnil(L); return; }
    switch (node->type) {
        case FastJson::TYPE_NULL: lua_pushnil(L); break;
        case FastJson::TYPE_BOOL: lua_pushboolean(L, node->bool_val); break;
        case FastJson::TYPE_NUMBER: lua_pushnumber(L, node->num_val); break;
        case FastJson::TYPE_STRING: lua_pushstring(L, node->str_val); break;
        case FastJson::TYPE_ARRAY: {
            lua_newtable(L); int idx = 1;
            for (FastJson::Node* c = node->child; c; c = c->next) {
                PushJsonToLua(L, c);
                lua_rawseti(L, -2, idx++);
            }
            break;
        }
        case FastJson::TYPE_OBJECT: {
            lua_newtable(L);
            for (FastJson::Node* c = node->child; c; c = c->next) {
                if (c->key) {
                    PushJsonToLua(L, c);
                    lua_setfield(L, -2, c->key);
                }
            }
            break;
        }
    }
}
static int l_json_parse(lua_State* L) {
    const char* json_str = luaL_checkstring(L, 1);
    FastJson::Doc doc;
    if (FastJson::Parse(json_str, &doc)) {
        PushJsonToLua(L, doc.root);
        FastJson::Free(&doc);
        return 1;
    }
    lua_pushnil(L);
    lua_pushstring(L, "Invalid JSON format or depth limit exceeded");
    return 2;
}
static LumeString fetch_resource(const LumeString& fullUrl) {
    if (fullUrl.find("http://") == 0 || fullUrl.find("https://") == 0) {
        auto r = Net::fetch(Net::URL::parse(fullUrl));
        return r.ok ? r.body : "";
    }
    LumeString path = fullUrl;
    if (path.find("file://") == 0) path = path.substr(7);
    return fastReadFile(path);
}
static int l_gl_load_bbmodel(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    LumeString fullUrl = resolveUrl(path, g_curUrl);
    LumeString fileData = fetch_resource(fullUrl);
    if (fileData.empty()) {
        lua_pushnil(L);
        return 1;
    }
    FastJson::Doc doc;
    if (!FastJson::Parse(fileData.c_str(), &doc)) {
        lua_pushnil(L);
        return 1;
    }
    GLuint listId = glGenLists(1);
    GLModelCache::add(listId);
    glNewList(listId, GL_COMPILE);
    FastJson::Node* resNode = FastJson::GetObj(doc.root, "resolution");
    float resW = 16.0f, resH = 16.0f;
    if (resNode) {
        FastJson::Node* rw = FastJson::GetObj(resNode, "width");
        if (rw && rw->type == FastJson::TYPE_NUMBER) resW = (float)rw->num_val;
        FastJson::Node* rh = FastJson::GetObj(resNode, "height");
        if (rh && rh->type == FastJson::TYPE_NUMBER) resH = (float)rh->num_val;
    }
    auto get_vec3 = [](FastJson::Node* obj, const char* name, float* out, float def) {
        out[0] = out[1] = out[2] = def;
        FastJson::Node* arr = FastJson::GetObj(obj, name);
        if (arr && arr->type == FastJson::TYPE_ARRAY && arr->child) {
            FastJson::Node* c = arr->child; out[0] = (float)c->num_val; c = c->next;
            if (c) { out[1] = (float)c->num_val; c = c->next; if (c) out[2] = (float)c->num_val; }
        }
    };
    auto get_uv = [](FastJson::Node* obj, const char* dir, float* uv, float rw, float rh) {
        uv[0] = 0; uv[1] = 0; uv[2] = 1; uv[3] = 1;
        FastJson::Node* faces = FastJson::GetObj(obj, "faces");
        if (!faces) return;
        FastJson::Node* face = FastJson::GetObj(faces, dir);
        if (!face) return;
        FastJson::Node* uv_arr = FastJson::GetObj(face, "uv");
        if (uv_arr && uv_arr->type == FastJson::TYPE_ARRAY && uv_arr->child) {
            FastJson::Node* c = uv_arr->child;
            uv[0] = (float)c->num_val / rw;
            c = c->next;
            if (c) {
                uv[1] = (float)c->num_val / rh;
                c = c->next;
                if (c) {
                    uv[2] = (float)c->num_val / rw;
                    c = c->next;
                    if (c) uv[3] = (float)c->num_val / rh;
                }
            }
        }
    };
    FastJson::Node* elements = FastJson::GetObj(doc.root, "elements");
    if (elements && elements->type == FastJson::TYPE_ARRAY) {
        for (FastJson::Node* el = elements->child;
            el;
            el = el->next) {
            if (el->type != FastJson::TYPE_OBJECT) continue;
            float from[3], to[3], origin[3];
            get_vec3(el, "from", from, 0.0f);
            get_vec3(el, "to", to, 0.0f);
            get_vec3(el, "origin", origin, 0.0f);
            for (int j = 0; j < 3; j++) {
                from[j] /= 16.0f;
                to[j] /= 16.0f;
                origin[j] /= 16.0f;
            }
            float rotAngle = 0.0f;
            const char* rotAxis = "x";
            FastJson::Node* rot = FastJson::GetObj(el, "rotation");
            if (rot) {
                FastJson::Node* ra = FastJson::GetObj(rot, "angle");
                if (ra) rotAngle = (float)ra->num_val;
                FastJson::Node* rx = FastJson::GetObj(rot, "axis"); 
                if (rx && rx->str_val) rotAxis = rx->str_val;
            }
            glPushMatrix();
            glTranslatef(origin[0], origin[1], origin[2]);
            if (rotAngle != 0.0f) {
                if (rotAxis[0] == 'x') glRotatef(rotAngle, 1, 0, 0);
                else if (rotAxis[0] == 'y') glRotatef(rotAngle, 0, 1, 0);
                else if (rotAxis[0] == 'z') glRotatef(rotAngle, 0, 0, 1);
            }
            glTranslatef(-origin[0], -origin[1], -origin[2]);
            glBegin(GL_QUADS);
            float uv[4];
            glNormal3f(0.0f, 0.0f, -1.0f); get_uv(el, "north", uv, resW, resH);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(from[0], from[1], from[2]);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(to[0], from[1], from[2]);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(to[0], to[1], from[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(from[0], to[1], from[2]);

            glNormal3f(0.0f, 0.0f, 1.0f); get_uv(el, "south", uv, resW, resH);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(from[0], from[1], to[2]);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(to[0], from[1], to[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(to[0], to[1], to[2]);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(from[0], to[1], to[2]);

            glNormal3f(0.0f, 1.0f, 0.0f); get_uv(el, "up", uv, resW, resH);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(from[0], to[1], from[2]);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(to[0], to[1], from[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(to[0], to[1], to[2]);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(from[0], to[1], to[2]);

            glNormal3f(0.0f, -1.0f, 0.0f); get_uv(el, "down", uv, resW, resH);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(from[0], from[1], from[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(to[0], from[1], from[2]);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(to[0], from[1], to[2]);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(from[0], from[1], to[2]);

            glNormal3f(1.0f, 0.0f, 0.0f); get_uv(el, "east", uv, resW, resH);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(to[0], from[1], from[2]);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(to[0], from[1], to[2]);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(to[0], to[1], to[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(to[0], to[1], from[2]);

            glNormal3f(-1.0f, 0.0f, 0.0f); get_uv(el, "west", uv, resW, resH);
            glTexCoord2f(uv[0], uv[3]); glVertex3f(from[0], from[1], from[2]);
            glTexCoord2f(uv[2], uv[3]); glVertex3f(from[0], from[1], to[2]);
            glTexCoord2f(uv[2], uv[1]); glVertex3f(from[0], to[1], to[2]);
            glTexCoord2f(uv[0], uv[1]); glVertex3f(from[0], to[1], from[2]);
            glEnd();
            glPopMatrix();
        }
    }
    FastJson::Free(&doc);
    glEndList();
    lua_pushinteger(L, listId);
    return 1;
}
static int l_gl_load_obj(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    LumeString fileData = fastReadFile(path);
    if (fileData.empty()) {
        lua_pushnil(L);
        return 1;
    }
    LumeVector<float> verts;
    GLuint listId = glGenLists(1);
    GLModelCache::add(listId);
    glNewList(listId, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    auto fast_atof = [](const char** ptr) -> float {
        const char* p = *ptr;
        while (*p == ' ') p++;
        bool neg = (*p == '-');
        if (neg || *p == '+') p++;
        float res = 0.0f;
        while (*p >= '0' && *p <= '9') { res = res * 10.0f + (*p - '0'); p++; }
        if (*p == '.') {
            p++;
            float frac = 1.0f;
            while (*p >= '0' && *p <= '9') { frac *= 0.1f; res += (*p - '0') * frac; p++; }
        }
        *ptr = p;
        return neg ? -res : res;
        };
    size_t pos = 0;
    while (pos < fileData.length()) {
        size_t next = fileData.find('\n', pos);
        LumeString line = (next == LumeString::npos) ? fileData.substr(pos) : fileData.substr(pos, next - pos);
        pos = (next == LumeString::npos) ? fileData.length() : next + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.length() >= 2 && line[0] == 'v' && line[1] == ' ') {
            const char* p = line.c_str() + 2;
            float x = fast_atof(&p);
            float y = fast_atof(&p);
            float z = fast_atof(&p);
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
        }
        else if (line.length() >= 2 && line[0] == 'f' && line[1] == ' ') {
            LumeVector<int> face_verts;
            const char* p = line.c_str() + 2;
            int total_verts = (int)(verts.size() / 3);
            while (*p) {
                while (*p == ' ') p++;
                if (!*p) break;
                int v = StrToIntA(p);
                if (v != 0) {
                    int idx = (v > 0) ? (v - 1) : (total_verts + v);
                    if (idx >= 0 && idx < total_verts) {
                        face_verts.push_back(idx);
                    }
                }
                while (*p && *p != ' ') p++;
            }
            if (face_verts.size() >= 3) {
                glVertex3f(verts[face_verts[0] * 3], verts[face_verts[0] * 3 + 1], verts[face_verts[0] * 3 + 2]);
                glVertex3f(verts[face_verts[1] * 3], verts[face_verts[1] * 3 + 1], verts[face_verts[1] * 3 + 2]);
                glVertex3f(verts[face_verts[2] * 3], verts[face_verts[2] * 3 + 1], verts[face_verts[2] * 3 + 2]);
                if (face_verts.size() >= 4) {
                    glVertex3f(verts[face_verts[0] * 3], verts[face_verts[0] * 3 + 1], verts[face_verts[0] * 3 + 2]);
                    glVertex3f(verts[face_verts[2] * 3], verts[face_verts[2] * 3 + 1], verts[face_verts[2] * 3 + 2]);
                    glVertex3f(verts[face_verts[3] * 3], verts[face_verts[3] * 3 + 1], verts[face_verts[3] * 3 + 2]);
                }
            }
        }
    }
    glEnd();
    glEndList();
    lua_pushinteger(L, listId);
    return 1;
}
static int l_load_ttf(lua_State* L) {
    const char* urlStr = luaL_checkstring(L, 1);
    LumeString fullUrl = resolveUrl(urlStr, g_curUrl);
    LumeString fontBytes = fetch_resource(fullUrl);
    if (fontBytes.empty()) {
        lua_pushboolean(L, 0);
        return 1;
    }
    DWORD numFonts = 0;
    HANDLE hFont = AddFontMemResourceEx((void*)fontBytes.data(), (DWORD)fontBytes.size(), 0, &numFonts);
    if (hFont) {
        FontCache::g_memFonts.push_back(hFont);
        lua_pushboolean(L, 1);
    }
    else lua_pushboolean(L, 0);
    return 1;
}
static int l_gl_delete_texture(lua_State* L) {
    GLuint texID = (GLuint)luaL_checkinteger(L, 1);
    glDeleteTextures(1, &texID);
    return 0;
}
static int l_gl_tex_parameteri(lua_State* L) {
    glTexParameteri((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2), (GLint)luaL_checkinteger(L, 3));
    return 0;
}
static int l_gl_lightfv(lua_State* L) {
    GLenum light = (GLenum)luaL_checkinteger(L, 1);
    GLenum pname = (GLenum)luaL_checkinteger(L, 2);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_checknumber(L, 4),
        (GLfloat)luaL_checknumber(L, 5),
        (GLfloat)luaL_optnumber(L, 6, 1.0)
    };
    glLightfv(light, pname, params);
    return 0;
}
static int l_gl_materialfv(lua_State* L) {
    GLenum face = (GLenum)luaL_checkinteger(L, 1);
    GLenum pname = (GLenum)luaL_checkinteger(L, 2);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_optnumber(L, 4, 0.0),
        (GLfloat)luaL_optnumber(L, 5, 0.0),
        (GLfloat)luaL_optnumber(L, 6, 1.0)
    };
    glMaterialfv(face, pname, params);
    return 0;
}
static int l_gl_create_buffer(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    int len = (int)luaL_len(L, 1);
    LumeVector<float> buf;
    buf.reserve(len);
    for (int i = 1; i <= len; ++i) {
        lua_rawgeti(L, 1, i);
        buf.push_back((float)lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    int id = GLBuffers::nextId++;
    GLBuffers::buffers[id] = static_cast<LumeVector<float>&&>(buf);
    lua_pushinteger(L, id);
    return 1;
}
static int l_gl_draw_buffer(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    int mode = (int)luaL_checkinteger(L, 2);
    auto it = GLBuffers::buffers.find(id);
    if (it != GLBuffers::buffers.end() && !it->second.empty()) {
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, it->second.data());
        glDrawArrays(mode, 0, (int)(it->second.size() / 3));
        glDisableClientState(GL_VERTEX_ARRAY);
    }
    return 0;
}
static int l_gl_fogf(lua_State* L) {glFogf((GLenum)luaL_checkinteger(L, 1), (GLfloat)luaL_checknumber(L, 2)); return 0;}
static int l_gl_fogi(lua_State* L) {glFogi((GLenum)luaL_checkinteger(L, 1), (GLint)luaL_checkinteger(L, 2)); return 0;}
static int l_gl_fogfv(lua_State* L) {
    GLenum pname = (GLenum)luaL_checkinteger(L, 1);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 2),
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_checknumber(L, 4),
        (GLfloat)luaL_optnumber(L, 5, 1.0f)
    };
    glFogfv(pname, params);
    return 0;
}
static int l_gl_render_mode(lua_State* L) {
    lua_pushinteger(L, glRenderMode((GLenum)luaL_checkinteger(L, 1)));
    return 1;
}
static int l_gl_tex_genf(lua_State* L) {glTexGenf((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2), (GLfloat)luaL_checknumber(L, 3)); return 0;}
static int l_gl_tex_geni(lua_State* L) {glTexGeni((GLenum)luaL_checkinteger(L, 1), (GLenum)luaL_checkinteger(L, 2), (GLint)luaL_checkinteger(L, 3)); return 0;}
static int l_gl_tex_genfv(lua_State* L) {
    GLenum coord = (GLenum)luaL_checkinteger(L, 1);
    GLenum pname = (GLenum)luaL_checkinteger(L, 2);
    GLfloat params[4] = {
        (GLfloat)luaL_checknumber(L, 3),
        (GLfloat)luaL_checknumber(L, 4),
        (GLfloat)luaL_checknumber(L, 5),
        (GLfloat)luaL_checknumber(L, 6)
    };
    glTexGenfv(coord, pname, params);
    return 0;
}
static int l_glu_build2d_mipmaps(lua_State* L) {
    GLenum target = (GLenum)luaL_checkinteger(L, 1);
    GLint components = (GLint)luaL_checkinteger(L, 2);
    GLsizei width = (GLsizei)luaL_checkinteger(L, 3);
    GLsizei height = (GLsizei)luaL_checkinteger(L, 4);
    GLenum format = (GLenum)luaL_checkinteger(L, 5);
    GLenum type = (GLenum)luaL_checkinteger(L, 6);
    size_t len;
    const char* data = luaL_checklstring(L, 7, &len);
    int res = gluBuild2DMipmaps(target, components, width, height, format, type, data);
    lua_pushinteger(L, res);
    return 1;
}
static int l_gl_load_texture_mipmapped(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    wchar_t wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
    Gdiplus::Bitmap bmp(wpath);
    if (bmp.GetLastStatus() != Gdiplus::Ok) { lua_pushnil(L); return 1; }
    int w = bmp.GetWidth();
    int h = bmp.GetHeight();
    LumeVector<unsigned char> pixels(w * h * 4);
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmpData);
    const unsigned char* src = (const unsigned char*)bmpData.Scan0;
    int stride = bmpData.Stride;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * stride + x * 4;
            int p = (y * w + x) * 4;
            pixels[p + 0] = src[i + 2];
            pixels[p + 1] = src[i + 1];
            pixels[p + 2] = src[i + 0];
            pixels[p + 3] = src[i + 3];
        }
    }
    bmp.UnlockBits(&bmpData);
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D, 4, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    lua_pushinteger(L, texID);
    return 1;
}
static int l_gl_print(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    float z = (float)luaL_optnumber(L, 4, 0.0f);
    int sz = (int)luaL_optinteger(L, 5, 16);
    const char* fontName = luaL_optstring(L, 6, "Segoe UI");
    HDC hdc = wglGetCurrentDC();
    HGLRC ctx = wglGetCurrentContext();
    if (!hdc || !ctx) return 0;
    uint64_t hash = 2166136261u;
    for (const char* p = fontName; *p; ++p) { hash ^= (uint8_t)*p; hash *= 16777619u; }
    hash ^= (uint64_t)sz;
    HFONT font = FontCache::get(sz, false, false, fontName);
    GLuint base = GLFont::get(hdc, ctx, font, hash);
    thread_local wchar_t wbuf[4096];
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf, 4096);
    if (wlen > 0) wlen--;
    glRasterPos3f(x, y, z);
    glPushAttrib(GL_LIST_BIT);
    glListBase(base);
    glCallLists(wlen, GL_UNSIGNED_SHORT, wbuf);
    glPopAttrib();
    return 0;
}
static int l_download_async(lua_State* L) {
    LumeString url = luaL_checkstring(L, 1);
    LumeString fname = luaL_checkstring(L, 2);
    AsyncNet::startDownload(url, fname);
    return 0;
}
static int l_download_status(lua_State* L) {
    LumeString url = luaL_checkstring(L, 1);
    LumeLockGuard<LumeMutex> lock(AsyncNet::g_dlMutex);
    auto it = AsyncNet::g_downloads.find(url);
    if (it != AsyncNet::g_downloads.end()) {
        lua_pushinteger(L, it->second.status);
        lua_pushstring(L, it->second.localPath.c_str());
        lua_pushinteger(L, (lua_Integer)it->second.downloadedBytes);
        lua_pushinteger(L, (lua_Integer)it->second.totalBytes);
        return 4;
    }
    lua_pushinteger(L, -404);
    return 1;
}
static int l_plugin_from_net(lua_State* L) {
    LumeString url = luaL_checkstring(L, 1);
    bool ok = Plugins::loadFromNet(url, g_curUrl, L);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}
static int l_load_lua(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    LumeString fullUrl = resolveUrl(url, g_curUrl);
    if (fullUrl.find("http://") != 0 && fullUrl.find("https://") != 0 && !isLocalAccessAllowed()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "SECURITY_ERROR");
        return 2;
    }
    LumeString luaCode = fetch_resource(fullUrl);
    if (luaCode.empty()) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "File not found or network error");
        return 2;
    }
    if (luaL_dostring(L, luaCode.c_str()) != 0) {
        const char* errMsg = lua_tostring(L, -1);
        lua_pushboolean(L, 0);
        lua_pushstring(L, errMsg);
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}
static int l_wasm_load(lua_State* L) {
    const char* url = luaL_checkstring(L, 1);
    LumeString fullUrl = resolveUrl(url, g_curUrl);
    if (fullUrl.find("http://") != 0 && fullUrl.find("https://") != 0 && !isLocalAccessAllowed()) {
        lua_pushnil(L);
        lua_pushstring(L, "SECURITY_ERROR");
        return 2;
    }
    LumeString wasmBytes = fetch_resource(fullUrl);
    if (wasmBytes.empty()) {
        lua_pushnil(L);
        lua_pushstring(L, "File not found");
        return 2;
    }

    int id = WasmEngine::load(wasmBytes);
    if (id < 0) { lua_pushnil(L); lua_pushstring(L, "Invalid WASM format"); return 2; }
    lua_pushinteger(L, id); return 1;
}
static int l_wasm_load_raw(lua_State* L) {
    size_t len;
    const char* data = luaL_checklstring(L, 1, &len);
    LumeString bytes(data, len);
    int id = WasmEngine::load(bytes);
    if (id < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to parse RAW WASM module");
        return 2;
    }
    lua_pushinteger(L, id); return 1;
}
static int l_wasm_call(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    const char* funcName = luaL_checkstring(L, 2);
    auto it = WasmEngine::instances.find(id);
    if (it == WasmEngine::instances.end()) return 0;
    IM3Function func;
    M3Result res = m3_FindFunction(&func, it->second.runtime, funcName);
    if (res) {
        lua_pushstring(L, res);
        lua_error(L);
        return 0;
    }
    int numArgs = m3_GetArgCount(func);
    LumeVector<uint64_t> argValues(numArgs, 0);
    LumeVector<const void*> argPtrs(numArgs, nullptr);
    for (int i = 0; i < numArgs; ++i) {
        int luaArgIdx = 3 + i;
        switch (m3_GetArgType(func, i)) {
        case c_m3Type_i32: { int32_t v = (int32_t)luaL_optinteger(L, luaArgIdx, 0); CopyMemory(&argValues[i], &v, 4); break; }
        case c_m3Type_i64: { int64_t v = (int64_t)luaL_optinteger(L, luaArgIdx, 0); CopyMemory(&argValues[i], &v, 8); break; }
        case c_m3Type_f32: { float v = (float)luaL_optnumber(L, luaArgIdx, 0.0);    CopyMemory(&argValues[i], &v, 4); break; }
        case c_m3Type_f64: { double v = (double)luaL_optnumber(L, luaArgIdx, 0.0);  CopyMemory(&argValues[i], &v, 8); break; }
        default: break;
        }
        argPtrs[i] = &argValues[i];
    }
    res = m3_Call(func, numArgs, argPtrs.data());
    if (res) {
        lua_pushstring(L, res);
        lua_error(L);
        return 0;
    }
    int numRets = m3_GetRetCount(func);
    if (numRets == 0) return 0;
    LumeVector<uint64_t> retValues(numRets, 0);
    LumeVector<const void*> retPtrs(numRets, nullptr);
    for (int i = 0; i < numRets; ++i) retPtrs[i] = &retValues[i];
    res = m3_GetResults(func, numRets, retPtrs.data());
    if (res) return 0;
    int pushed = 0;
    for (int i = 0; i < numRets; ++i) {
        switch (m3_GetRetType(func, i)) {
        case c_m3Type_i32: lua_pushinteger(L, *(int32_t*)&retValues[i]); pushed++; break;
        case c_m3Type_i64: lua_pushinteger(L, *(int64_t*)&retValues[i]); pushed++; break;
        case c_m3Type_f32: lua_pushnumber(L, *(float*)&retValues[i]); pushed++; break;
        case c_m3Type_f64: lua_pushnumber(L, *(double*)&retValues[i]); pushed++; break;
        default: break;
        }
    }
    return pushed;
}
void registerGLConstants(lua_State* L) {
    struct { const char* n; int v; } cs[] = {
        {"GL_POINTS",GL_POINTS},{"GL_LINES",GL_LINES},{"GL_LINE_STRIP",GL_LINE_STRIP},{"GL_LINE_LOOP",GL_LINE_LOOP},
        {"GL_TRIANGLES",GL_TRIANGLES},{"GL_TRIANGLE_STRIP",GL_TRIANGLE_STRIP},{"GL_TRIANGLE_FAN",GL_TRIANGLE_FAN},
        {"GL_QUADS",GL_QUADS},{"GL_POLYGON",GL_POLYGON},{"GL_MODELVIEW",GL_MODELVIEW},{"GL_PROJECTION",GL_PROJECTION},
        {"GL_DEPTH_TEST",GL_DEPTH_TEST},{"GL_BLEND",GL_BLEND},{"GL_LINE_SMOOTH",GL_LINE_SMOOTH},
        {"GL_LIGHTING",0x0B50},{"GL_LIGHT0",0x4000},{"GL_COLOR_MATERIAL",0x0B57},{"GL_CULL_FACE",0x0B44},
        {"VK_LEFT",VK_LEFT},{"VK_RIGHT",VK_RIGHT},{"VK_UP",VK_UP},{"VK_DOWN",VK_DOWN},
        {"VK_W",0x57},{"VK_A",0x41},{"VK_S",0x53},{"VK_D",0x44},
        {"VK_SPACE",VK_SPACE},{"VK_SHIFT",VK_SHIFT},{"VK_CONTROL",VK_CONTROL},
        {"VK_ESCAPE",VK_ESCAPE},{"VK_RETURN",VK_RETURN},
        {"VK_Q",0x51},{"VK_E",0x45},{"VK_R",0x52},{"VK_F",0x46},
        {"VK_LBUTTON",VK_LBUTTON},{"VK_RBUTTON",VK_RBUTTON},{"VK_F11",VK_F11},{"VK_P",0x50},
        {"GL_SRC_ALPHA", 0x0302},
        {"GL_ONE_MINUS_SRC_ALPHA", 0x0303},
        {"GL_ONE", 1},
        {"GL_ZERO", 0},
        {"GL_TEXTURE_2D", 0x0DE1},
        {"GL_TEXTURE_MIN_FILTER", 0x2801},
        {"GL_TEXTURE_MAG_FILTER", 0x2800},
        {"GL_NEAREST", 0x2600},
        {"GL_LINEAR", 0x2601},
        {"GL_POSITION", 0x1203},
        {"GL_AMBIENT", 0x1200},
        {"GL_DIFFUSE", 0x1201},
        {"GL_SPECULAR", 0x1202},
        {"GL_SHININESS", 0x1601},
        {"GL_FRONT", 0x0404},
        {"GL_BACK", 0x0405},
        {"GL_FRONT_AND_BACK", 0x0408},
        {"GL_LIGHT1", 0x4001},
        {"GL_LIGHT2", 0x4002},
        {"GL_FOG", 0x0B60},
        {"GL_FOG_COLOR", 0x0B66},
        {"GL_FOG_DENSITY", 0x0B62},
        {"GL_FOG_START", 0x0B63},
        {"GL_FOG_END", 0x0B64},
        {"GL_FOG_MODE", 0x0B65},
        {"GL_EXP", 0x0800},
        {"GL_EXP2", 0x0801},
        {"GL_S", 0x2000},
        {"GL_T", 0x2001},
        {"GL_R", 0x2002},
        {"GL_Q", 0x2003},
        {"GL_TEXTURE_GEN_MODE", 0x2500},
        {"GL_OBJECT_LINEAR", 0x2401},
        {"GL_SPHERE_MAP", 0x2402},
        {"GL_RENDER", 0x1C00},
        {"GL_SELECT", 0x1C01},
        {"GL_FEEDBACK", 0x1C02},
        {nullptr,0}
    };
    for (int i = 0; cs[i].n; i++) {
        lua_pushinteger(L, cs[i].v);
        lua_setglobal(L, cs[i].n);
    }
}
void init() {
    if (g_L) lua_close(g_L);
    g_L = luaL_newstate();
    luaL_requiref(g_L, "_G", luaopen_base, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "table", luaopen_table, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "string", luaopen_string, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "math", luaopen_math, 1); lua_pop(g_L, 1);
    luaL_requiref(g_L, "os", luaopen_os, 1);
    lua_pushnil(g_L); lua_setfield(g_L, -2, "execute");
    lua_pushnil(g_L); lua_setfield(g_L, -2, "remove");
    lua_pushnil(g_L); lua_setfield(g_L, -2, "rename");
    lua_pushnil(g_L); lua_setfield(g_L, -2, "exit");
    lua_pushnil(g_L); lua_setfield(g_L, -2, "getenv");
    lua_register(g_L, "set_prop", l_set_prop);
    lua_register(g_L, "get_node_prop", l_get_node_prop);
    lua_register(g_L, "set_inner_htp", l_set_inner_htp);
    lua_register(g_L, "set_text", l_set_text);
    lua_register(g_L, "get_text", l_get_text);
    lua_register(g_L, "set_offset_y", l_set_offset_y);
    lua_register(g_L, "get_offset_y", l_get_offset_y);
    lua_register(g_L, "set_shimmer", l_set_shimmer);
    lua_register(g_L, "get_input", l_get_input);
    lua_register(g_L, "set_input", l_set_input);
    lua_register(g_L, "on_click", l_on_click);
    lua_register(g_L, "on_right_click", l_on_right_click);
    lua_register(g_L, "on_key_down", l_on_key_down);
    lua_register(g_L, "on_canvas_click", l_on_canvas_click);
    lua_register(g_L, "is_key_pressed", l_is_key_pressed);
    lua_register(g_L, "is_key_released", l_is_key_released);
    lua_register(g_L, "key_down", l_key_down);
    lua_register(g_L, "get_mouse_pos", l_get_mouse);
    lua_register(g_L, "get_canvas_mouse", l_get_canvas_mouse);
    lua_register(g_L, "get_mouse_delta", l_get_mouse_delta);
    lua_register(g_L, "get_mouse_wheel", l_get_mouse_wheel);
    lua_register(g_L, "capture_mouse", l_capture_mouse);
    lua_register(g_L, "release_mouse", l_release_mouse);
    lua_register(g_L, "is_mouse_captured", l_is_mouse_captured);
    lua_register(g_L, "fullscreen_canvas", l_fullscreen_canvas);
    lua_register(g_L, "is_fullscreen", l_is_fullscreen);
    lua_register(g_L, "window_active", l_window_active);
    lua_register(g_L, "navigate", l_navigate);
    lua_register(g_L, "alert", l_alert);
    lua_register(g_L, "refresh", l_refresh);
    lua_register(g_L, "set_timer", l_set_timer);
    lua_register(g_L, "kill_timer", l_kill_timer);
    lua_register(g_L, "get_time", l_get_time);
    lua_register(g_L, "get_window_size", l_get_window_size);
    lua_register(g_L, "http_request", l_http);
    lua_register(g_L, "download_async", l_download_async);
    lua_register(g_L, "download_status", l_download_status);
    lua_register(g_L, "json_parse", l_json_parse);
    lua_register(g_L, "plugin_from_net", l_plugin_from_net);
    lua_register(g_L, "load_lua", l_load_lua);
    lua_register(g_L, "wasm_load", l_wasm_load);
    lua_register(g_L, "wasm_load_raw", l_wasm_load_raw);
    lua_register(g_L, "wasm_call", l_wasm_call);
    lua_register(g_L, "canvas_clear", l_cv_clear);
    lua_register(g_L, "canvas_rect", l_cv_rect);
    lua_register(g_L, "canvas_circle", l_cv_circle);
    lua_register(g_L, "canvas_line", l_cv_line);
    lua_register(g_L, "canvas_text", l_cv_text);
    lua_register(g_L, "gl_available", l_gl_available);
    lua_register(g_L, "gl_begin_render", l_gl_begin);
    lua_register(g_L, "gl_end_render", l_gl_end);
    lua_register(g_L, "gl_refresh", l_gl_refresh);
    lua_register(g_L, "gl_clear", l_gl_clear);
    lua_register(g_L, "gl_viewport", l_gl_viewport);
    lua_register(g_L, "gl_color", l_gl_color);
    lua_register(g_L, "gl_begin", l_gl_begin_prim);
    lua_register(g_L, "gl_end", l_gl_end_prim);
    lua_register(g_L, "gl_vertex2f", l_gl_vertex2f);
    lua_register(g_L, "gl_vertex3f", l_gl_vertex3f);
    lua_register(g_L, "gl_ortho", l_gl_ortho);
    lua_register(g_L, "gl_frustum", l_gl_frustum);
    lua_register(g_L, "gl_load_identity", l_gl_load_identity);
    lua_register(g_L, "gl_matrix_mode", l_gl_matrix_mode);
    lua_register(g_L, "gl_push_matrix", l_gl_push_matrix);
    lua_register(g_L, "gl_pop_matrix", l_gl_pop_matrix);
    lua_register(g_L, "gl_translatef", l_gl_translatef);
    lua_register(g_L, "gl_rotatef", l_gl_rotatef);
    lua_register(g_L, "gl_scalef", l_gl_scalef);
    lua_register(g_L, "gl_enable", l_gl_enable);
    lua_register(g_L, "gl_disable", l_gl_disable);
    lua_register(g_L, "gl_line_width", l_gl_line_width);
    lua_register(g_L, "gl_point_size", l_gl_point_size);
    lua_register(g_L, "gl_normal3f", l_gl_normal3f);
    lua_register(g_L, "glu_perspective", l_glu_perspective);
    lua_register(g_L, "glu_look_at", l_glu_look_at);
    lua_register(g_L, "gl_gen_list", l_gl_gen_list);
    lua_register(g_L, "gl_new_list", l_gl_new_list);
    lua_register(g_L, "gl_end_list", l_gl_end_list);
    lua_register(g_L, "gl_call_list", l_gl_call_list);
    lua_register(g_L, "gl_delete_list", l_gl_delete_list);
    lua_register(g_L, "gl_blend_func", l_gl_blend_func);
    lua_register(g_L, "glu_sphere", l_glu_sphere);
    lua_register(g_L, "gl_bind_texture", l_gl_bind_texture);
    lua_register(g_L, "gl_text_coord2f", l_gl_text_coord2f);
    lua_register(g_L, "gl_load_texture", l_gl_load_texture);
    lua_register(g_L, "gl_load_bbmodel", l_gl_load_bbmodel);
    lua_register(g_L, "gl_load_obj", l_gl_load_obj);
    lua_register(g_L, "load_ttf", l_load_ttf);
    lua_register(g_L, "gl_delete_texture", l_gl_delete_texture);
    lua_register(g_L, "gl_tex_parameteri", l_gl_tex_parameteri);
    lua_register(g_L, "gl_lightfv", l_gl_lightfv);
    lua_register(g_L, "gl_materialfv", l_gl_materialfv);
    lua_register(g_L, "gl_create_buffer", l_gl_create_buffer);
    lua_register(g_L, "gl_draw_buffer", l_gl_draw_buffer);
    lua_register(g_L, "gl_fogf", l_gl_fogf);
    lua_register(g_L, "gl_fogi", l_gl_fogi);
    lua_register(g_L, "gl_fogfv", l_gl_fogfv);
    lua_register(g_L, "gl_render_mode", l_gl_render_mode);
    lua_register(g_L, "gl_tex_genf", l_gl_tex_genf);
    lua_register(g_L, "gl_tex_geni", l_gl_tex_geni);
    lua_register(g_L, "gl_tex_genfv", l_gl_tex_genfv);
    lua_register(g_L, "glu_build2d_mipmaps", l_glu_build2d_mipmaps);
    lua_register(g_L, "gl_load_texture_mipmapped", l_gl_load_texture_mipmapped);
    lua_register(g_L, "gl_print", l_gl_print);
    registerGLConstants(g_L);
    Plugins::initAllPlugins(g_L);
}
void exec(const LumeString& code) {
    if (!g_L) init();
    if (luaL_dostring(g_L, code.c_str()) != 0) {
        const char* e = lua_tostring(g_L, -1);
        OutputDebugStringA(e ? e : "lua err\n");
        MessageBoxU(g_mainWnd, e ? e : "Unknown error", "Lua Error", MB_OK | MB_ICONERROR);
        lua_pop(g_L, 1);
    }
}
void reset() {
    for (auto cb : Plugins::g_onResetCallbacks) {
        cb();
    }
    Plugins::g_onResetCallbacks.clear();
    releaseMouse();
    if (g_fullscreenCanvas) exitFullscreenCanvas();
    for (auto& kv : g_timerRefs) {
        KillTimer(g_mainWnd, kv.first);
        if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, kv.second);
    }
    for (auto& kv : g_canvasClickRefs) {
        if (g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, kv.second);
    }
    g_canvasClickRefs.clear();
    g_timerRefs.clear();
    g_timerN = 9000;
    g_mouseWheelDelta = 0;
    g_mouseDeltaX = 0;
    if (g_keyDownRef != LUA_NOREF && g_L) luaL_unref(g_L, LUA_REGISTRYINDEX, g_keyDownRef);
    g_keyDownRef = LUA_NOREF;
    Bindings::reset();
    Canvas::g_bufs.clear();
    GLCanvas::destroyAll();
    ImageCache::clear();
    WasmEngine::clear();
    GLBuffers::clear();
    GLFont::clear();
    GLModelCache::clear();
    GradientText::cleanupBuffers();
    if (g_L) {
        lua_close(g_L);
        g_L = nullptr;
    }
}
}
struct PendingPage {
    LumeString body;
    LumeString url;
    bool error = false;
    int navId = 0;
    bool isHistoryNav = false;
};
LumeVector<PendingPage> g_pendingPages;
LumeMutex g_pageMutex;
int g_currentNavId = 0;
static LumeString g_curUrl;
LumeString resolveUrl(const LumeString& url, const LumeString& baseUrl) {
    LumeString target = url;
    if (target.find("file://") == 0) {
        LumeString pathPart = target.substr(7);
        if (pathPart.find(':') == LumeString::npos && pathPart.find('/') != 0 && pathPart.find('\\') != 0) {
            target = pathPart;
        }
        else {
            return target;
        }
    }
    else if (target.find("://") != LumeString::npos) {
        return target;
    }
    if (target.length() >= 2 && target[1] == ':') return "file://" + target;
    if (target.find("about:") == 0) return target;
    LumeString base = baseUrl;
    bool hasFilePrefix = false;
    if (base.find("file://") == 0) {
        hasFilePrefix = true;
        base = base.substr(7);
    }
    if (hasFilePrefix || base.find('\\') != LumeString::npos || (base.length() >= 2 && base[1] == ':')) {
        char baseBuf[MAX_PATH];
        lstrcpynA(baseBuf, base.c_str(), MAX_PATH);
        for (int i = 0; baseBuf[i]; i++) if (baseBuf[i] == '/') baseBuf[i] = '\\';
        PathRemoveFileSpecA(baseBuf);
        char targetBuf[MAX_PATH];
        lstrcpynA(targetBuf, target.c_str(), MAX_PATH);
        for (int i = 0; targetBuf[i]; i++) if (targetBuf[i] == '/') targetBuf[i] = '\\';
        char out[MAX_PATH];
        PathCombineA(out, baseBuf, targetBuf);
        LumeString result = out;
        if (hasFilePrefix) return "file://" + result;
        return result;
    }
    wchar_t outW[2048];
    DWORD lenW = 2048;
    LumeWString wBase = utf8_to_wstring(base);
    LumeWString wUrl = utf8_to_wstring(target);
    if (UrlCombineW(wBase.c_str(), wUrl.c_str(), outW, &lenW, 0) == S_OK) {
        return wstring_to_utf8(outW);
    }
    return "http://" + target;
}
namespace Render {
    struct Hit {
        RECT r;
        LumeString url, action, elemId;
        bool isInput = false, isBtn = false, isGLCanvas = false;
        LumeString canvasId;
        int canvasW = 0, canvasH = 0;
        int zIndex = 0;
        bool autoCapture = true;
        void* customData = nullptr;
    };
    class Engine {
    private:
        struct RenderCmd {
            int zIndex;
            LumeAction drawCall;
            RenderCmd() : zIndex(0) {}
            template<typename F>
            RenderCmd(int z, F f) : zIndex(z), drawCall(static_cast<F&&>(f)) {}
        };
        LumeVector<RenderCmd> renderQueue;
        LumeVector<int> blockHeights;
    private:
        struct TH_Entry {
            uint64_t k;
            int v;
            bool occ;
        };
        TH_Entry* th_tab = nullptr; size_t th_cap = 0, th_sz = 0;
        struct TS_Entry {
            uint64_t k;
            SIZE v;
            bool occ;
        };
        TS_Entry* ts_tab = nullptr; size_t ts_cap = 0, ts_sz = 0;
        struct Est_Entry {
            const HTP::Elem* k;
            int v;
            bool occ;
        };
        Est_Entry* est_tab = nullptr; size_t est_cap = 0, est_sz = 0;
        void clear_th() {
            if (th_tab) HeapFree(GetProcessHeap(), 0, th_tab);
            th_tab = nullptr;
            th_cap = th_sz = 0;
        }
        void clear_ts() {
            if (ts_tab) HeapFree(GetProcessHeap(), 0, ts_tab);
            ts_tab = nullptr;
            ts_cap = ts_sz = 0;
        }
        void clear_est() {
            if (est_tab) HeapFree(GetProcessHeap(), 0, est_tab);
            est_tab = nullptr;
            est_cap = est_sz = 0;
        }
        int* get_th(uint64_t k) {
            if (!th_cap) return nullptr;
            size_t idx = (k ^ (k >> 32)) & (th_cap - 1);
            while (th_tab[idx].occ) {
                if (th_tab[idx].k == k) return &th_tab[idx].v;
                idx = (idx + 1) & (th_cap - 1);
            }
            return nullptr;
        }
        void set_th(uint64_t k, int v) {
            if (th_sz >= th_cap / 2) {
                size_t oCap = th_cap;
                TH_Entry* oTab = th_tab;
                th_cap = oCap == 0 ? 16 : oCap * 2;
                th_tab = (TH_Entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, th_cap * sizeof(TH_Entry));
                th_sz = 0;
                if (oTab) {
                    for (size_t j = 0; j < oCap; ++j) if (oTab[j].occ) set_th(oTab[j].k, oTab[j].v);
                    HeapFree(GetProcessHeap(), 0, oTab);
                }
            }
            size_t idx = (k ^ (k >> 32)) & (th_cap - 1);
            while (th_tab[idx].occ) {
                if (th_tab[idx].k == k) {
                    th_tab[idx].v = v;
                    return;
                } idx = (idx + 1) & (th_cap - 1);
            }
            th_tab[idx] = {k, v, true}; th_sz++;
        }
        SIZE* get_ts(uint64_t k) {
            if (!ts_cap) return nullptr;
            size_t idx = (k ^ (k >> 32)) & (ts_cap - 1);
            while (ts_tab[idx].occ) {
                if (ts_tab[idx].k == k) return &ts_tab[idx].v;
                idx = (idx + 1) & (ts_cap - 1);
            }
            return nullptr;
        }
        void set_ts(uint64_t k, SIZE v) {
            if (ts_sz >= ts_cap / 2) {
                size_t oCap = ts_cap; TS_Entry* oTab = ts_tab;
                ts_cap = oCap == 0 ? 16 : oCap * 2;
                ts_tab = (TS_Entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ts_cap * sizeof(TS_Entry));
                ts_sz = 0;
                if (oTab) {
                    for (size_t j = 0; j < oCap; ++j) if (oTab[j].occ) set_ts(oTab[j].k, oTab[j].v);
                    HeapFree(GetProcessHeap(), 0, oTab);
                }
            }
            size_t idx = (k ^ (k >> 32)) & (ts_cap - 1);
            while (ts_tab[idx].occ) {if (ts_tab[idx].k == k) {ts_tab[idx].v = v; return;} idx = (idx + 1) & (ts_cap - 1);}
            ts_tab[idx] = {k, v, true}; ts_sz++;
        }

        int* get_est(const HTP::Elem* k) {
            if (!est_cap) return nullptr;
            uint64_t ptr = (uint64_t)k; size_t idx = (ptr ^ (ptr >> 32)) & (est_cap - 1);
            while (est_tab[idx].occ) {if (est_tab[idx].k == k) return &est_tab[idx].v; idx = (idx + 1) & (est_cap - 1);}
            return nullptr;
        }
        void set_est(const HTP::Elem* k, int v) {
            if (est_sz >= est_cap / 2) {
                size_t oCap = est_cap;
                Est_Entry* oTab = est_tab;
                est_cap = oCap == 0 ? 16 : oCap * 2;
                est_tab = (Est_Entry*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, est_cap * sizeof(Est_Entry));
                est_sz = 0;
                if (oTab) {
                    for (size_t j = 0; j < oCap; ++j) if (oTab[j].occ) set_est(oTab[j].k, oTab[j].v);
                    HeapFree(GetProcessHeap(), 0, oTab);
                }
            }
            uint64_t ptr = (uint64_t)k; size_t idx = (ptr ^ (ptr >> 32)) & (est_cap - 1);
            while (est_tab[idx].occ) {
                if (est_tab[idx].k == k) {
                    est_tab[idx].v = v;
                    return;
                } idx = (idx + 1) & (est_cap - 1);
            }
            est_tab[idx] = {k, v, true};
            est_sz++;
        }
        int scrollY = 0, contentH = 0, curY = 0;
        int viewportH = 0;
        struct GLCache { LumeString id; int x, y, w, h; };
        LumeVector<GLCache> glCanvases;
        LumeVector<Hit> docHits;
        LumeVector<Hit>* pH = nullptr;
        inline const char* getRefPtr(const HTP::Props& p, const char* k) const { return p.get(k, ""); }
        uint64_t hashText(const char* str, int w, HFONT f) {
            uint64_t h = 2166136261u;
            while (*str) { h ^= (uint8_t)*str++; h *= 16777619u; }
            h ^= (uint64_t)w; h *= 16777619u;
            h ^= (uint64_t)(uintptr_t)f;
            return h;
        }
        void fillRR(HDC dc, int x, int y, int w, int h, HTP::Color c, int rad = 5) {
            HBRUSH b = CreateSolidBrush(c.cr());
            HPEN p = CreatePen(PS_SOLID, 1, c.cr());
            auto ob = SelectObject(dc, b);
            auto op = SelectObject(dc, p);
            RoundRect(dc, x, y, x + w, y + h, rad, rad);
            SelectObject(dc, ob); SelectObject(dc, op);
            DeleteObject(b); DeleteObject(p);
        }
        int estH(const LumeSharedPtr<HTP::Elem>& e) {
            if (!e) return 0;
            int* cached = get_est(e.get());
            if (cached) return *cached;
            int res = 20;
            switch (e->type) {
            case HTP::EType::TEXT: case HTP::EType::ITEM: res = e->props.getInt("size", 16) + 8; break;
            case HTP::EType::LINK: res = e->props.getInt("size", 16) + 8; break;
            case HTP::EType::BUTTON: res = e->props.getInt("height", 35) + 8; break;
            case HTP::EType::INPUT_FIELD: res = e->props.getInt("height", 28) + 8; break;
            case HTP::EType::DIVIDER: res = e->props.getInt("margin", 10) * 2 + e->props.getInt("thickness", 1); break;
            case HTP::EType::IMAGE: res = e->props.getInt("height", 100) + 8; break;
            case HTP::EType::BR: res = e->props.getInt("size", 16); break;
            case HTP::EType::CANVAS: case HTP::EType::GL_CANVAS: res = e->props.getInt("height", 200) + 8; break;
            case HTP::EType::SCRIPT: res = 0; break;
            case HTP::EType::BLOCK: case HTP::EType::COLUMN: {
                int h = e->props.getInt("padding", 10) * 2 + e->props.getInt("margin", 5) * 2;
                for (auto& c : e->children) h += estH(c);
                res = h; break;
            }
            case HTP::EType::ROW: {
                int m = 0; for (auto& c : e->children) m = lume_max(m, estH(c));
                res = m + e->props.getInt("margin", 5) * 2; break;
            }
            case HTP::EType::LIST: {
                int h = 0; for (auto& c : e->children) h += estH(c);
                res = h; break;
            }
            case HTP::EType::UNKNOWN: {
                auto it = Plugins::g_customTags.find(e->tag);
                if (it != Plugins::g_customTags.end() && it->second.estimate_height) {
                    res = it->second.estimate_height((HTP_NodeHandle)e.get());
                }
                break;
            }
            default: break;
            }
            set_est(e.get(), res);
            return res;
        }
        int estW(const LumeSharedPtr<HTP::Elem>& e, int mw) {
            if (!e) return mw;
            int w = e->props.getInt("width", 0);
            if (w > 0) return w;
            switch (e->type) {
            case HTP::EType::BUTTON: return 120;
            case HTP::EType::INPUT_FIELD: return 250;
            case HTP::EType::IMAGE: return 100;
            case HTP::EType::CANVAS: case HTP::EType::GL_CANVAS: return 400;
            case HTP::EType::ROW: {
                int rowW = 0, gap = e->props.getInt("gap", 10), nc = 0;
                for (auto& c : e->children) { rowW += estW(c, mw); nc++; }
                if (nc > 1) rowW += gap * (nc - 1);
                return rowW + e->props.getInt("margin", 5) * 2 + e->props.getInt("padding", 0) * 2;
            }
            case HTP::EType::COLUMN: {
                int maxW = 0;
                for (auto& c : e->children) { int cw = estW(c, mw); if (cw > maxW) maxW = cw; }
                return maxW + e->props.getInt("padding", 5) * 2;
            }
            case HTP::EType::BLOCK: {
                int maxW = 0;
                for (auto& c : e->children) { int cw = estW(c, mw); if (cw > maxW) maxW = cw; }
                return maxW + e->props.getInt("padding", 10) * 2 + e->props.getInt("margin", 5) * 2;
            }
            case HTP::EType::UNKNOWN: {
                auto it = Plugins::g_customTags.find(e->tag);
                if (it != Plugins::g_customTags.end() && it->second.estimate_width) {
                    return it->second.estimate_width((HTP_NodeHandle)e.get(), mw);
                }
                return mw;
            }
            default: return mw;
            }
        }
        void draw(HDC refDC, const LumeSharedPtr<HTP::Elem>& e, int x, int y, int mw, const LumeString& parentAlign = "") {
            if (!e) return;
            const char* pAlign = getRefPtr(e->props, "align");
            LumeString currentAlign = (pAlign[0] == '\0') ? parentAlign : pAlign;
            switch (e->type) {
            case HTP::EType::PAGE: {
                int cy = y;
                for (auto& c : e->children) {
                    draw(refDC, c, x, cy, mw);
                    cy = curY;
                }
                break;
            }
            case HTP::EType::UNKNOWN: {
                auto it = Plugins::g_customTags.find(e->tag);
                if (it != Plugins::g_customTags.end()) {
                    int z = e->props.getInt("z-index", 0);
                    int eh = estH(e);
                    int ew = estW(e, mw);
                    int drawY = y;
                    if (it->second.render) {
                        renderQueue.push_back({ z, [this, e, x, drawY, ew, eh, it]() {
                            it->second.render(this->docDC, (HTP_NodeHandle)e.get(), x, drawY, ew, eh, this->getScroll());
                        }});
                    }
                    if (pH && it->second.on_click) {
                        Hit h; h.r = { x, drawY, x + ew, drawY + eh };
                        h.elemId = e->tag;
                        h.customData = e.get();
                        h.canvasId = (const char*)e.get();
                        h.zIndex = z;
                        pH->push_back(h);
                    }
                    curY = y + eh;
                }
                else {
                    int cy = y;
                    for (auto& c : e->children) {
                        draw(refDC, c, x, cy, mw, currentAlign);
                        cy = curY;
                    }
                }
                break;
            }
            case HTP::EType::SCRIPT:
                curY = y;
                break;
            case HTP::EType::ROW: {
                int mar = e->props.getInt("margin", 5), pad = e->props.getInt("padding", 0), gap = e->props.getInt("gap", 10);
                int nc = 0, fw = 0, fc = 0, totalContentWidth = 0;
                for (auto& c : e->children) {
                    nc++; int w = c->props.getInt("width", 0);
                    if (w > 0) { fw += w; totalContentWidth += w; }
                    else { fc++; int est = estW(c, 0); if (est > 0) { fw += est; totalContentWidth += est; } }
                }
                if (nc > 1) totalContentWidth += gap * (nc - 1);
                int avail = mw - mar * 2 - pad * 2, flexW = 0;
                if (fc > 0 && avail > fw) flexW = (avail - fw) / fc;
                if (flexW < 50 && fc > 0) flexW = 50;
                int cx = x + mar + pad;
                if (currentAlign == "center" && totalContentWidth < avail) cx = x + mar + pad + (avail - totalContentWidth) / 2;
                else if (currentAlign == "right" && totalContentWidth < avail) cx = x + mar + pad + (avail - totalContentWidth);
                int ry = y + mar, maxH = 0;
                for (auto& c : e->children) {
                    int cw = c->props.getInt("width", 0);
                    if (cw == 0) cw = estW(c, flexW > 0 ? flexW : 100);
                    int sv = curY; curY = ry;
                    draw(refDC, c, cx, ry, cw, "left");
                    int ch = curY - ry; if (ch > maxH) maxH = ch;
                    cx += cw + gap; curY = sv;
                }
                curY = ry + maxH + mar; break;
            }
            case HTP::EType::COLUMN: {
                int pad = e->props.getInt("padding", 5), rad = e->props.getInt("border-radius", 4), z = e->props.getInt("z-index", 0);
                const char* pBg = getRefPtr(e->props, "background");
                int heightIdx = blockHeights.size(); blockHeights.push_back(0);
                int startY = y;
                if (pBg[0] != '\0') {
                    HTP::Color bgColor = HTP::Color::fromHex(pBg);
                    renderQueue.push_back({ z, [this, x, startY, mw, bgColor, rad, heightIdx]() {
                        fillRR(this->docDC, x, startY, mw, blockHeights[heightIdx], bgColor, rad);
                    } });
                }
                curY = y + pad;
                for (auto& c : e->children) {
                    int cmw = mw - pad * 2, cx = x + pad, cwToPass = cmw;
                    if (currentAlign == "center") { int ew = estW(c, cmw); if (ew < cmw) { cx += (cmw - ew) / 2; cwToPass = ew; } }
                    else if (currentAlign == "right") { int ew = estW(c, cmw); if (ew < cmw) { cx += (cmw - ew); cwToPass = ew; } }
                    draw(refDC, c, cx, curY, cwToPass, currentAlign);
                }
                blockHeights[heightIdx] = (curY - y) + pad; curY += pad; break;
            }
            case HTP::EType::BLOCK: {
                int pad = e->props.getInt("padding", 10), mar = e->props.getInt("margin", 5), rad = e->props.getInt("border-radius", 6), z = e->props.getInt("z-index", 0);
                const char* pBg = getRefPtr(e->props, "background");
                int heightIdx = blockHeights.size(); blockHeights.push_back(0);
                int startY = y + mar;
                if (pBg[0] != '\0') {
                    HTP::Color bgColor = HTP::Color::fromHex(pBg);
                    renderQueue.push_back({ z, [this, x, mar, startY, mw, bgColor, rad, heightIdx]() {
                        fillRR(this->docDC, x + mar, startY, mw - mar * 2, blockHeights[heightIdx], bgColor, rad);
                    } });
                }
                curY = startY + pad;
                for (auto& c : e->children) {
                    int cmw = mw - (mar + pad) * 2, cx = x + mar + pad, cwToPass = cmw;
                    if (currentAlign == "center") { int ew = estW(c, cmw); if (ew < cmw) { cx += (cmw - ew) / 2; cwToPass = ew; } }
                    else if (currentAlign == "right") { int ew = estW(c, cmw); if (ew < cmw) { cx += (cmw - ew); cwToPass = ew; } }
                    draw(refDC, c, cx, curY, cwToPass, currentAlign);
                }
                blockHeights[heightIdx] = (curY - startY) + pad; curY += pad + mar; break;
            }
            case HTP::EType::TEXT: {
                const char* pId = getRefPtr(e->props, "id");
                const char* pCt = getRefPtr(e->props, "content");
                if (pId[0] != '\0') {
                    auto i = Bindings::g_texts.find(pId);
                    if (i != Bindings::g_texts.end()) pCt = i->second.c_str();
                    else {
                        Bindings::g_texts[pId] = pCt;
                        pCt = Bindings::g_texts[pId].c_str();
                    }
                }
                int sz = e->props.getInt("size", 16);
                auto col = e->props.getColor("color", { 255,255,255 });
                const char* pGrad = getRefPtr(e->props, "gradient");
                int offset_y = 0; float shimmer = 0.0f;
                if (pId[0] != '\0') {
                    auto oi = Bindings::g_offsets_y.find(pId); if (oi != Bindings::g_offsets_y.end()) offset_y = oi->second;
                    auto si = Bindings::g_shimmer_offsets.find(pId); if (si != Bindings::g_shimmer_offsets.end()) shimmer = si->second;
                }
                int z = e->props.getInt("z-index", 0);
                const char* pFontName = getRefPtr(e->props, "font");
                HFONT f = FontCache::get(sz, e->props.getBool("bold"), e->props.getBool("italic"), (pFontName[0] == '\0') ? "Segoe UI" : pFontName);
                int drawY = y + offset_y;
                int th = 0;
                UINT dtFormat = DT_WORDBREAK;
                if (currentAlign == "center") dtFormat |= DT_CENTER;
                else if (currentAlign == "right") dtFormat |= DT_RIGHT;
                uint64_t mKey = hashText(pCt, mw, f);
                int* tIt = get_th(mKey);
                if (tIt) {th = *tIt;}
                else {
                    auto of = SelectObject(refDC, f);
                    RECT rcCalc = {x, 0, x + mw, 10000};
                    DrawTextU(refDC, pCt, -1, &rcCalc, dtFormat | DT_CALCRECT);
                    th = rcCalc.bottom - rcCalc.top;
                    SelectObject(refDC, of);
                    set_th(mKey, th);
                }
                HTP::Color gradColor; if (pGrad[0] != '\0') gradColor = HTP::Color::fromHex(pGrad);
                LumeString textStr = pCt;
                renderQueue.push_back({ z, [this, x, drawY, mw, th, f, textStr, dtFormat, col, pGrad, gradColor, shimmer, currentAlign]() {
                    HDC dc = this->docDC;
                    if (pGrad[0] != '\0') {
                        int drawX = x;
                        if (currentAlign == "center" || currentAlign == "right") {
                            SIZE ts; auto of = SelectObject(dc, f);
                            GetTextExtentPoint32U(dc, textStr.c_str(), lstrlenA(textStr.c_str()), &ts);
                            SelectObject(dc, of);
                            if (currentAlign == "center") drawX = x + (mw - ts.cx) / 2;
                            else if (currentAlign == "right") drawX = x + (mw - ts.cx);
                        }
                        GradientText::draw(dc, textStr.c_str(), f, drawX, drawY, col, gradColor, shimmer);
                    }
                    else {
                        auto of = SelectObject(dc, f);
                        SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
                        RECT rcDraw = { x, drawY, x + mw, drawY + th };
                        DrawTextU(dc, textStr.c_str(), -1, &rcDraw, dtFormat);
                        SelectObject(dc, of);
                    }
                } });
                curY = y + ((pGrad[0] == '\0') ? th : sz) + 4;
                break;
            }
            case HTP::EType::LINK: {
                const char* pCt = getRefPtr(e->props, "content");
                if (pCt[0] == '\0') pCt = "[link]";
                int sz = e->props.getInt("size", 16);
                auto col = e->props.getColor("color", { 10,189,227 });
                int z = e->props.getInt("z-index", 0);
                HFONT f = FontCache::get(sz);
                SIZE ts = {0, 0};
                uint64_t mKey = hashText(pCt, 0, f);
                SIZE* tIt = get_ts(mKey);
                if (tIt) ts = *tIt;
                else {
                    auto of = SelectObject(refDC, f);
                    GetTextExtentPoint32U(refDC, pCt, lstrlenA(pCt), &ts);
                    SelectObject(refDC, of);
                    set_ts(mKey, ts);
                }
                int drawY = y;
                LumeString url = resolveUrl(getRefPtr(e->props, "url"), g_curUrl);
                LumeString textStr = pCt;
                renderQueue.push_back({ z, [this, x, drawY, ts, f, col, textStr]() {
                    HDC dc = this->docDC;
                    auto of2 = SelectObject(dc, f);
                    SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
                    RECT rc = { x, drawY, x + ts.cx, drawY + ts.cy };
                    DrawTextU(dc, textStr.c_str(), -1, &rc, 0);
                    HPEN p = CreatePen(PS_SOLID, 1, col.cr()); auto op = SelectObject(dc, p);
                    MoveToEx(dc, x, drawY + ts.cy - 1, 0); LineTo(dc, x + ts.cx, drawY + ts.cy - 1);
                    SelectObject(dc, op); DeleteObject(p); SelectObject(dc, of2);
                } });
                if (pH) { Hit h; h.r = { x, drawY, x + ts.cx, drawY + ts.cy }; h.url = url; h.zIndex = z; pH->push_back(h); }
                curY = y + ts.cy + 4; break;
            }
            case HTP::EType::BUTTON: {
                int bw = e->props.getInt("width", 120), bh = e->props.getInt("height", 35), by = y;
                const char* pId = getRefPtr(e->props, "id"), * pCt = getRefPtr(e->props, "content");
                if (pCt[0] == '\0') pCt = "Button";
                const char* pAct = getRefPtr(e->props, "action"), * pUrl = getRefPtr(e->props, "url");
                LumeString url = (pUrl[0] == '\0') ? pAct : pUrl;
                if (!url.empty()) url = resolveUrl(url, g_curUrl);
                int sz = e->props.getInt("size", 14), rad = e->props.getInt("border-radius", 6), z = e->props.getInt("z-index", 0);
                auto bg = e->props.getColor("background", { 233,69,96 }), col = e->props.getColor("color", { 255,255,255 });
                LumeString textStr = pCt;
                renderQueue.push_back({ z, [this, x, by, bw, bh, bg, rad, f = FontCache::get(sz, true), col, textStr]() {
                    HDC dc = this->docDC;
                    fillRR(dc, x, by, bw, bh, bg, rad);
                    auto of = SelectObject(dc, f);
                    SetTextColor(dc, col.cr()); SetBkMode(dc, TRANSPARENT);
                    RECT rc = { x, by, x + bw, by + bh };
                    DrawTextU(dc, textStr.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    SelectObject(dc, of);
                } });
                if (pH) { Hit h; h.r = { x, by, x + bw, by + bh }; h.url = url; h.action = pAct; h.elemId = pId; h.isBtn = true; h.zIndex = z; pH->push_back(h); }
                curY = y + bh + 8; break;
            }
            case HTP::EType::INPUT_FIELD: {
                int ih = e->props.getInt("height", 28), iy = y, iw = e->props.getInt("width", 250), z = e->props.getInt("z-index", 0);
                const char* pId = getRefPtr(e->props, "id"), * pPh = getRefPtr(e->props, "placeholder");
                LumeString id = (pId[0] == '\0') ? ("inp_" + lume_to_string(y)) : pId;
                auto& inp = Bindings::g_inputs[id];
                if (inp.placeholder.empty() && pPh[0] != '\0') inp.placeholder = pPh;
                inp.x = x; inp.y = iy; inp.w = iw; inp.h = ih;
                bool foc = (Bindings::g_focusId == id);
                int cursorToDraw = inp.cursor; LumeString textStr = inp.text, phStr = inp.placeholder;
                renderQueue.push_back({ z, [this, x, iy, iw, ih, foc, textStr, phStr, cursorToDraw]() {
                    HDC dc = this->docDC;
                    HBRUSH br = CreateSolidBrush(foc ? RGB(50, 50, 80) : RGB(40, 40, 60));
                    HPEN pn = CreatePen(PS_SOLID, foc ? 2 : 1, foc ? RGB(10, 189, 227) : RGB(100, 100, 140));
                    auto ob = SelectObject(dc, br);
                    auto op = SelectObject(dc, pn);
                    RoundRect(dc, x, iy, x + iw, iy + ih, 4, 4);
                    SelectObject(dc, ob);
                    SelectObject(dc, op);
                    DeleteObject(br);
                    DeleteObject(pn);
                    auto of = SelectObject(dc, FontCache::get(14)); SetBkMode(dc, TRANSPARENT);
                    if (textStr[0] != '\0') {
                        SetTextColor(dc, RGB(230, 230, 240));
                        RECT rc = {x + 6, iy + 2, x + iw - 4, iy + ih - 2};
                        DrawTextU(dc, textStr.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE);
                        if (foc) {
                            int cp = lume_min(cursorToDraw, (int)textStr.length());
                            SIZE ts;
                            GetTextExtentPoint32U(dc, textStr.c_str(), cp, &ts);
                            HPEN cpen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                            auto ocp = SelectObject(dc, cpen);
                            MoveToEx(dc, x + 6 + ts.cx, iy + 4, 0);
                            LineTo(dc, x + 6 + ts.cx, iy + ih - 4);
                            SelectObject(dc, ocp);
                            DeleteObject(cpen);
                        }
                    }
                    else if (phStr[0] != '\0') {
                        SetTextColor(dc, RGB(120, 120, 140));
                        RECT rc = {x + 6, iy + 2, x + iw - 4, iy + ih - 2};
                        DrawTextU(dc, phStr.c_str(), -1, &rc, DT_VCENTER | DT_SINGLELINE);
                    }
                    SelectObject(dc, of);
                } });
                if (pH) {
                    Hit h; h.r = {x, iy, x + iw, iy + ih};
                    h.elemId = id;
                    h.isInput = true;
                    h.zIndex = z;
                    pH->push_back(h);
                }
                curY = y + ih + 8; break;
            }
            case HTP::EType::CANVAS: {
                int ch = e->props.getInt("height", 200), cy = y, cw = e->props.getInt("width", 400), z = e->props.getInt("z-index", 0);
                const char* pId = getRefPtr(e->props, "id");
                LumeString id = (pId[0] == '\0') ? ("cv_" + lume_to_string(y)) : pId;
                auto bdr = e->props.getColor("border-color", {80,80,100});
                auto cb = Canvas::get(id, refDC, cw, ch);
                renderQueue.push_back({z, [this, x, cy, cw, ch, bdr, cb]() {
                    HDC dc = this->docDC;
                    HPEN p = CreatePen(PS_SOLID, 1, bdr.cr()); auto op = SelectObject(dc, p);
                    auto ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    Rectangle(dc, x - 1, cy - 1, x + cw + 1, cy + ch + 1);
                    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(p);
                    if (cb->dc) BitBlt(dc, x, cy, cw, ch, cb->dc, 0, 0, SRCCOPY);
                }});
                curY = y + ch + 8; break;
            }
            case HTP::EType::GL_CANVAS: {
                int ch = e->props.getInt("height", 200), cw = e->props.getInt("width", 400), cy = y, z = e->props.getInt("z-index", 0);
                const char* pId = getRefPtr(e->props, "id");
                LumeString id = (pId[0] == '\0') ? ("glcv_" + lume_to_string(y)) : pId;
                auto bdr = e->props.getColor("border-color", {80,80,100});
                bool autoCapture = e->props.getBool("capture", true);
                renderQueue.push_back({z, [this, x, cy, cw, ch, bdr]() {
                    HDC dc = this->docDC;
                    HPEN p = CreatePen(PS_SOLID, 1, bdr.cr()); auto op = SelectObject(dc, p);
                    auto ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                    Rectangle(dc, x - 1, cy - 1, x + cw + 1, cy + ch + 1);
                    SelectObject(dc, ob);
                    SelectObject(dc, op);
                    DeleteObject(p);
                    if (!g_opt_gpu || !GLLoader::available()) {
                        HBRUSH fb = CreateSolidBrush(RGB(40, 40, 60));
                        RECT fr = {x, cy, x + cw, cy + ch};
                        FillRect(dc, &fr, fb);
                        DeleteObject(fb);
                        auto of = SelectObject(dc, FontCache::get(14));
                        SetTextColor(dc, RGB(150, 150, 170));
                        SetBkMode(dc, TRANSPARENT);
                        RECT rc = {x, cy, x + cw, cy + ch};
                        DrawTextU(dc, "GPU Disabled", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        SelectObject(dc, of);
                    }
                }});
                glCanvases.push_back({id, x, cy, cw, ch});
                if (pH) {
                    Hit h; h.r = {x, cy, x + cw, cy + ch};
                    h.isGLCanvas = true; h.canvasId = id;
                    h.canvasW = cw;
                    h.canvasH = ch;
                    h.zIndex = z;
                    h.autoCapture = autoCapture;
                    pH->push_back(h);
                }
                GLCanvas::ensure(id, cw, ch);
                curY = y + ch + 8;
                break;
            }
            case HTP::EType::DIVIDER: {
                int t = e->props.getInt("thickness", 1), m = e->props.getInt("margin", 10), drawY = y + m, z = e->props.getInt("z-index", 0);
                auto col = e->props.getColor("color", { 60,60,80 });
                renderQueue.push_back({ z, [this, x, mw, drawY, t, col]() {
                    HDC dc = this->docDC;
                    HPEN p = CreatePen(PS_SOLID, t, col.cr()); auto op = SelectObject(dc, p);
                    MoveToEx(dc, x, drawY, 0); LineTo(dc, x + mw, drawY);
                    SelectObject(dc, op); DeleteObject(p);
                } });
                curY = y + m * 2 + t; break;
            }
            case HTP::EType::ITEM: {
                const char* pCt = getRefPtr(e->props, "content");
                int sz = e->props.getInt("size", 14), by = y + sz / 2 - 2, z = e->props.getInt("z-index", 0), drawY = y;
                HFONT f = FontCache::get(sz); int th = 0;
                uint64_t mKey = hashText(pCt, mw, f);
                int* tIt = get_th(mKey);
                if (tIt) th = *tIt;
                else {
                    auto of = SelectObject(refDC, f);
                    RECT rcCalc = {x, 0, x + mw, 200};
                    DrawTextU(refDC, pCt, -1, &rcCalc, DT_WORDBREAK | DT_CALCRECT);
                    th = rcCalc.bottom - rcCalc.top;
                    SelectObject(refDC, of);
                    set_th(mKey, th);
                }
                auto col = e->props.getColor("color", { 200,200,200 });
                LumeString textStr = pCt;
                renderQueue.push_back({ z, [this, x, by, drawY, mw, th, col, f, textStr]() {
                    HDC dc = this->docDC;
                    HBRUSH b = CreateSolidBrush(col.cr());
                    HPEN pn = CreatePen(PS_SOLID, 1, col.cr());
                    auto ob = SelectObject(dc, b);
                    auto op = SelectObject(dc, pn);
                    Ellipse(dc, x - 12, by, x - 4, by + 6);
                    SelectObject(dc, ob);
                    SelectObject(dc, op);
                    DeleteObject(b);
                    DeleteObject(pn);
                    auto of2 = SelectObject(dc, f);
                    SetTextColor(dc, col.cr());
                    SetBkMode(dc, TRANSPARENT);
                    RECT rcDraw = {x, drawY, x + mw, drawY + th};
                    DrawTextU(dc, textStr.c_str(), -1, &rcDraw, DT_WORDBREAK);
                    SelectObject(dc, of2);
                }});
                curY = y + th + 4; break;
            }
            case HTP::EType::LIST: {
                int cy = y;
                for (auto& c : e->children) {
                    draw(refDC, c, x + 20, cy, mw - 20);
                    cy = curY;
                }
                break;
            }
            case HTP::EType::BR:
                curY = y + e->props.getInt("size", 16);
                break;
            case HTP::EType::IMAGE: {
                int ih = e->props.getInt("height", 100);
                int iw = e->props.getInt("width", 100);
                int z = e->props.getInt("z-index", 0);
                int drawY = y;
                const char* pSrc = getRefPtr(e->props, "src");
                const char* pAlt = getRefPtr(e->props, "alt");
                if (pAlt[0] == '\0') pAlt = "[image]";
                auto bdr = e->props.getColor("border-color", {80,80,100});
                auto imgSp = (pSrc[0] != '\0') ? ImageCache::get(resolveUrl(pSrc, g_curUrl)) : nullptr;
                LumeString altStr = pAlt;
                renderQueue.push_back({ z, [this, x, drawY, iw, ih, bdr, imgSp, altStr]() {
                    HDC dc = this->docDC;
                    if (imgSp && imgSp.get()) {
                        Gdiplus::Graphics g(dc);
                        g.DrawImage(imgSp.get(), x, drawY, iw, ih);
                        HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
                        auto op = SelectObject(dc, p);
                        auto ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
                        Rectangle(dc, x, drawY, x + iw, drawY + ih);
                        SelectObject(dc, ob);
                        SelectObject(dc, op);
                        DeleteObject(p);
                    }
                    else {
                        HBRUSH b = CreateSolidBrush(RGB(50, 50, 70));
                        HPEN p = CreatePen(PS_SOLID, 1, bdr.cr());
                        auto ob = SelectObject(dc, b);
                        auto op = SelectObject(dc, p);
                        Rectangle(dc, x, drawY, x + iw, drawY + ih);
                        SelectObject(dc, ob);
                        SelectObject(dc, op);
                        DeleteObject(b);
                        DeleteObject(p);
                        auto of = SelectObject(dc, FontCache::get(12));
                        SetTextColor(dc, RGB(150, 150, 170));
                        SetBkMode(dc, TRANSPARENT);
                        RECT rc = {x + 4, drawY + 4, x + iw - 4, drawY + ih - 4};
                        DrawTextU(dc, altStr.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                        SelectObject(dc, of);
                    }
                }});
                curY = y + ih + 8;
                break;
            }
            default: break;
            }
            if (curY > contentH) contentH = curY;
        }
    public:
        int lastW = 0;
        HDC docDC = nullptr;
        LumeVector<Hit> hits;
        ~Engine() {
            clear_th();
            clear_ts();
            clear_est();
        }
        int totalH() const {return contentH;}
        void setScroll(int y) {scrollY = lume_max(0, y);}
        int getScroll() const {return scrollY;}
        void updateLayout(HDC tdc, int w, HTP::Doc& doc) {
            if (w != lastW) {
                clear_th();
                clear_ts();
                lastW = w;
            }
            clear_est();
            contentH = 0; curY = 10;
            docHits.clear(); glCanvases.clear();
            pH = &docHits;
            renderQueue.clear(); blockHeights.clear();
            draw(tdc, doc.root, 10, 10, w - 20, "left");
            LumeStableSort(renderQueue.begin(), renderQueue.end(), [](const RenderCmd& a, const RenderCmd& b) {
                return a.zIndex < b.zIndex;
                });
            LumeStableSort(docHits.begin(), docHits.end(), [](const Hit& a, const Hit& b) {
                return a.zIndex > b.zIndex;
                });
        }
        void drawToScreen(HDC targetDC, int w, int h) {
            HBRUSH bgBrush = CreateSolidBrush(::g_doc.bg.cr());
            RECT r = { 0, 0, w, h };
            FillRect(targetDC, &r, bgBrush);
            DeleteObject(bgBrush);
            POINT originalOrg;
            SetViewportOrgEx(targetDC, 0, 0, &originalOrg);
            this->docDC = targetDC;
            for (auto& cmd : renderQueue) {
                int currentScroll = scrollY;
                if (cmd.zIndex != 0) currentScroll = scrollY - (scrollY * cmd.zIndex / 10);
                SetViewportOrgEx(targetDC, 0, -currentScroll, NULL);
                cmd.drawCall();
            }
            this->docDC = nullptr;
            SetViewportOrgEx(targetDC, originalOrg.x, originalOrg.y, NULL);
            hits = docHits;
            for (auto& hit : hits) {
                int currentScroll = scrollY;
                if (hit.zIndex != 0) currentScroll = scrollY - (scrollY * hit.zIndex / 10);
                hit.r.top -= currentScroll;
                hit.r.bottom -= currentScroll;
            }
            for (auto& gl : glCanvases) {
                GLCanvas::place(gl.id, gl.x, gl.y - scrollY + TOOLBAR_H, gl.w, gl.h, scrollY, TOOLBAR_H);
            }
        }
    };
}
LumeString escapeHtp(const LumeString& s) {
    LumeString res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c != '\r') res += c;
    }
    return res;
}
namespace Pages {
    LumeString readF(const LumeString& p) {return fastReadFile(p);}
    LumeString home() {
        auto c = readF("home.htp");
        if (!c.empty()) return c;
        return R"htp(
@page {title:"Lume - Start"; background:"#09090b";}
@block {align:"center"; padding:50; background:"#09090b"; margin:0;
  @br{size:10;}
  @text {id:"logo"; content:"LUME"; size:86; color:"#ff0000"; gradient:"#0000ff"; bold:"true"; align:"center";}
  @br {size:5;}
  @text {content:"The Custom Lightweight Engine"; size:18; color:"#71717a"; italic:"true"; align:"center";}
  @br {size:40;}
  @row {align:"center"; gap:10;
     @input {id:"url_box"; placeholder:"Type address (e.g. about:info or file://...)"; width:420; height:40;}
     @button {id:"btn_go"; content:"Navigate"; width:110; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; size:15;}
  }
  @br{size:8;}
  @text {id:"search_err"; content:""; size:14; color:"#ef4444"; align:"center";}
}
@block { align:"center"; padding:30; margin:10; background:"#09090b";
    @row {align:"center"; gap:20;
        @column {background:"#18181b"; padding:25; border-radius:10; width:280;
            @text {content:"3D Graphics"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"Hardware accelerated OpenGL rendering built directly into the UI layer."; size:15; color:"#a1a1aa";}
            @br {size:20;}
            @button {content:"Launch Demo"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:gldemo";}
            @br {size:30;}
        }
        @column {background:"#18181b"; padding:25; border-radius:10; width:280;
            @text {content:"Native Plugins"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15; }
            @text {content:"Extend functionality using the dynamic C++ DLL system. Build anything."; size:15; color:"#a1a1aa";}
            @br {size:20;}
            @button {content:"View Plugins"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:plugindemo";}
            @br {size:30;}
        }
    }
    @br {size:20;}
    @row {align:"center"; gap:20;
        @column {background:"#18181b"; padding:25; border-radius:10; width:280;
            @text {content:"About"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"Learn about the Lua 5.4 integration, HTP tags, and custom networking."; size:15; color:"#a1a1aa";}
            @br {size:20;}
            @button {content:"Read More"; width:140; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6; url:"about:info";}
            @br {size:30;}
        }
        @column {background:"#18181b"; padding:25; border-radius:10; width:280;
            @text {content:"WebAssembly"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"Run C/Rust/Zig binaries directly inside your pages at native speeds."; size:15; color:"#a1a1aa";}
            @br {size:20;}
            @button { content:"WASM Demo"; width:140; height:35; background:"#3b82f6"; color:"#ffffff"; border-radius:6; url:"about:wasm";}
            @br {size:30;}
        }
    }
}
@script {
    local function hsv_to_hex(h, s, v)
        local c = v * s
        local x = c * (1 - math.abs((h / 60) % 2 - 1))
        local m = v - c
        local r, g, b = 0, 0, 0
        if h < 60 then r, g, b = c, x, 0
        elseif h < 120 then r, g, b = x, c, 0
        elseif h < 180 then r, g, b = 0, c, x
        elseif h < 240 then r, g, b = 0, x, c
        elseif h < 300 then r, g, b = x, 0, c
        else r, g, b = c, 0, x end
        r = math.floor((r + m) * 255)
        g = math.floor((g + m) * 255)
        b = math.floor((b + m) * 255)
        return string.format("#%02X%02X%02X", r, g, b)
    end
    local hue = 0
    local spread = 150
    set_timer(16, function()
        hue = hue + 2
        if hue >= 360 then hue = hue - 360 end
        local hex1 = hsv_to_hex(hue % 360, 1.0, 1.0)
        local hex2 = hsv_to_hex((hue + spread) % 360, 1.0, 1.0)
        set_prop('logo', 'color', hex1)
        set_prop('logo', 'gradient', hex2)
    end)
    local function do_search()
        local query = get_input('url_box')
        if query and query ~= "" then
            set_text('search_err', '')
            navigate(query)
        else
            set_text('search_err', 'Please enter a valid URL')
        end
    end
    on_click('btn_go', do_search)
    on_key_down(function(vk)
        if vk == 13 then do_search() end
    end)
}
)htp";
    }
    LumeString about() {
        auto c = readF("about_browser.htp");
        if (!c.empty()) return c;
        return R"htp(
@page {title:"Lume - About"; background:"#09090b";}
@block {align:"center"; padding:40; background:"#09090b"; margin:0;
  @text {content:"About Lume"; size:46; color:"#fafafa"; bold:"true"; align:"center";}
  @br {size:5;}
  @text {content:"The architecture behind the custom engine."; size:18; color:"#a1a1aa"; italic:"true"; align:"center";}
}
@block {align:"center"; padding:20; margin:0; background:"#09090b";
    @row {align:"center"; gap:20;
        @column {background:"#18181b"; padding:25; border-radius:10; width:300;
            @text {content:"[*] Custom Rendering"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"No CEF or Chromium overhead. Everything is drawn natively using WinAPI, GDI+, and custom layout algorithms."; size:15; color:"#a1a1aa";}
        }
        @column {background:"#18181b"; padding:25; border-radius:10; width:300;
            @text {content:"[+] Lua 5.4 Powered"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15; }
            @text {content:"Fast and lightweight scripting. Direct bindings to DOM elements, inputs, and canvases without JS bloat."; size:15; color:"#a1a1aa";}
        }
    }
    @br {size:20;}
    @row {align:"center"; gap:20;
        @column {background:"#18181b"; padding:25; border-radius:10; width:300;
            @text {content:"[>] OpenGL Integration"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"Hardware-accelerated canvases embedded directly into the UI flow with full mouse capture support."; size:15; color:"#a1a1aa";}
        }
        @column {background:"#18181b"; padding:25; border-radius:10; width:300;
            @text {content:"[~] Native Plugins"; size:20; color:"#fafafa"; bold:"true";}
            @divider {color:"#27272a"; thickness:1; margin:15;}
            @text {content:"Extend the engine dynamically using C++ DLLs. Add new network protocols, physics, or OS integrations."; size:15; color:"#a1a1aa";}
        }
    }
    @br {size:40;}
    @button {content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15;}
}
)htp";
    }
    LumeString glDemo() {
        return R"htp(
@page {title:"Lume - 3D Engine"; background:"#09090b";}
@block {align:"center"; padding:30; background:"#09090b"; margin:0;
  @text {content:"Hardware Acceleration"; size:36; color:"#fafafa"; bold:"true"; align:"center";}
  @br {size:5;}
  @text {content:"Click canvas to capture mouse | ESC to release | F11 for Fullscreen"; size:15; color:"#71717a"; align:"center";}
}
@block {align:"center"; padding:0; margin:10; background:"#09090b";
  @column {align:"center"; background:"#18181b"; padding:25; border-radius:12; width:650;
    @glcanvas {id:"gl1"; width:600; height:400; border-color:"#27272a";}
    @br {size:20;}
    @row {align:"center"; gap:20;
      @button {id:"btn_spin"; content:"Toggle Animation"; width:160; height:35; background:"#3f3f46"; color:"#ffffff"; border-radius:6;}
      @text {id:"gl_status"; content:"Status: Animated"; size:15; color:"#10b981";}
    }
  }
  @br {size:30;}
  @button {content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15;}
}
@script {
  local animating = true 
  local tid = nil
  local function rgl()
    if not gl_available() then return end
    if not gl_begin_render('gl1', 600, 400) then return end
    local time = get_time()
    if not animating then time = 0 end
    local curW, curH = 600, 400
    if is_fullscreen() then
        curW, curH = get_window_size()
    end
    if curH == 0 then curH = 1 end
    gl_viewport(0, 0, curW, curH)
    gl_matrix_mode(GL_PROJECTION)
    gl_load_identity()
    glu_perspective(50, curW / curH, 0.1, 100)
    gl_matrix_mode(GL_MODELVIEW)
    gl_load_identity()
    glu_look_at(0, 2.5, 7, 0, 0, 0, 0, 1, 0)
    gl_clear(0.04, 0.04, 0.06, 1)
    gl_enable(GL_DEPTH_TEST)
    gl_enable(GL_BLEND)
    gl_blend_func(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    gl_push_matrix()
    local grid_offset = (time * 3.0) % 1.0
    gl_translatef(0, -1.5, grid_offset)
    gl_line_width(2.0)
    gl_begin(GL_LINES)
    for i = -15, 15 do
        gl_color(0.9, 0.2, 0.6, 0.4)
        gl_vertex3f(i, 0, -15)
        gl_vertex3f(i, 0, 15)
        local alpha = 1.0 - (math.abs(i) / 15.0)
        if alpha < 0 then alpha = 0 end
        gl_color(0.1, 0.8, 0.9, alpha * 0.6)
        gl_vertex3f(-15, 0, i)
        gl_vertex3f(15, 0, i)
    end
    gl_end()
    gl_pop_matrix()
    gl_push_matrix()
    local float_y = math.sin(time * 2.5) * 0.4
    gl_translatef(0, float_y + 0.5, 0)
    gl_rotatef(time * 40, 0, 1, 0)
    gl_rotatef(time * 15, 1, 0, 0)
    gl_begin(GL_TRIANGLES)
    gl_color(0.9, 0.2, 0.6, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(-1,0,1) gl_vertex3f(1,0,1)
    gl_color(0.7, 0.1, 0.8, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(1,0,1) gl_vertex3f(1,0,-1)
    gl_color(0.5, 0.0, 0.9, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(1,0,-1) gl_vertex3f(-1,0,-1)
    gl_color(0.8, 0.0, 0.5, 0.6) gl_vertex3f(0,1.5,0) gl_vertex3f(-1,0,-1) gl_vertex3f(-1,0,1)
    gl_color(0.1, 0.8, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(1,0,1) gl_vertex3f(-1,0,1)
    gl_color(0.1, 0.6, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(1,0,-1) gl_vertex3f(1,0,1)
    gl_color(0.1, 0.4, 0.9, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(-1,0,-1) gl_vertex3f(1,0,-1)
    gl_color(0.1, 0.5, 0.7, 0.6) gl_vertex3f(0,-1.5,0) gl_vertex3f(-1,0,1) gl_vertex3f(-1,0,-1)
    gl_end()
    gl_disable(GL_DEPTH_TEST)
    gl_line_width(2.0)
    gl_color(1, 1, 1, 0.9)
    gl_begin(GL_LINES)
    local pts = {{0,1.5,0}, {0,-1.5,0}, {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1}}
    local edges = {{1,3},{1,4},{1,5},{1,6}, {2,3},{2,4},{2,5},{2,6}, {3,4},{4,6},{6,5},{5,3}}
    for i=1, #edges do
        gl_vertex3f(pts[edges[i][1]][1], pts[edges[i][1]][2], pts[edges[i][1]][3])
        gl_vertex3f(pts[edges[i][2]][1], pts[edges[i][2]][2], pts[edges[i][2]][3])
    end
    gl_end()
    gl_pop_matrix()
    gl_end_render('gl1') 
    gl_refresh('gl1')
  end
  rgl()
  tid = set_timer(16, rgl)
  on_click('btn_spin', function()
    animating = not animating
    if animating then 
      set_text('gl_status', 'Status: Animated')
      if not tid then tid = set_timer(16, rgl) end
    else 
      set_text('gl_status', 'Status: Paused') 
      if tid then kill_timer(tid) tid = nil end 
      rgl() 
    end
  end)
  on_key_down(function(vk)
    if vk == VK_F11 then
        fullscreen_canvas('gl1', not is_fullscreen())
    end
  end)
}
)htp";
    }
    LumeString pluginDemo() {
        return R"htp(
@page {title:"Lume - Plugins"; background:"#09090b";}
@block {align:"center"; padding:30; background:"#09090b"; margin:0;
  @text {content:"Native DLL Integrations"; size:36; color:"#fafafa"; bold:"true"; align:"center";}
  @br {size:5;}
  @text {content:"Dynamically loaded C++ extensions with direct memory and OS API access."; size:15; color:"#71717a"; align:"center";}
}
@block {align:"center"; padding:0; margin:10; background:"#09090b";
  @row {align:"center"; gap:20;
    @column {background:"#18181b"; padding:25; border-radius:12; width:320;
      @text {content:"1. Cloud Plugin Loader"; size:20; color:"#3b82f6"; bold:"true";}
      @divider {color:"#27272a"; thickness:1; margin:15;}
      @text {content:"Click below to download and inject the official 'SystemMon' plugin directly into RAM without restarting the engine."; size:14; color:"#a1a1aa";}
      @br {size:20;}
      @button {id:"btn_load_plg"; content:"Inject SystemMon"; width:220; height:35; background:"#2563eb"; color:"#ffffff"; border-radius:6;}
      @br {size:10;}
      @text {id:"plg_status"; content:"Status: Not Loaded"; size:13; color:"#f59e0b";}
    }
    @column {background:"#18181b"; padding:25; border-radius:12; width:320;
      @text {content:"2. Hardware Monitor"; size:20; color:"#10b981"; bold:"true";}
      @divider {color:"#27272a"; thickness:1; margin:15;}
      @row {align:"left"; gap:10;
        @text {content:"CPU Usage:"; size:14; color:"#a1a1aa"; width:90;}
        @text {id:"txt_cpu"; content:"0%"; size:15; color:"#fafafa"; bold:"true";}
      }
      @br {size:5;}
      @row {align:"left"; gap:10;
        @text {content:"RAM Free:"; size:14; color:"#a1a1aa"; width:90;}
        @text {id:"txt_ram"; content:"0 MB"; size:15; color:"#fafafa"; bold:"true";}
      }
      @br {size:15;}
      @glcanvas {id:"gl_graph"; width:270; height:80; border-color:"#27272a";}
    }

  }
  @br {size:30;}
  @button {content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15;}
}
@script {
  local history = {}
  for i=1, 50 do history[i] = 0 end
  local function draw_graph()
    if not gl_available() then return end
    if not gl_begin_render('gl_graph', 270, 80) then return end
    gl_viewport(0, 0, 270, 80)
    gl_matrix_mode(GL_PROJECTION)
    gl_load_identity()
    gl_ortho(0, 50, 0, 100, -1, 1)
    gl_clear(0.06, 0.06, 0.08, 1)
    gl_color(0.2, 0.2, 0.25, 1)
    gl_begin(GL_LINES)
    for i=0, 100, 25 do
       gl_vertex2f(0, i) gl_vertex2f(50, i)
    end
    gl_end()
    gl_color(0.1, 0.8, 0.5, 1)
    gl_line_width(2.0)
    gl_begin(GL_LINE_STRIP)
    for i=1, 50 do
       gl_vertex2f(i, history[i])
    end
    gl_end()
    gl_end_render('gl_graph') 
  end
  local timer_id = nil
  on_click('btn_load_plg', function()
    local ok = plugin_from_net("https://raw.githubusercontent.com/Lume-corp/LumeSources/refs/heads/main/plugins/aaOfficial/V0.6.1/sysmon.dll")
    if ok then
      set_text('plg_status', 'Status: Loaded & Active!')
      set_prop('plg_status', 'color', '#10b981')
      if sysmon_get_cpu then
         if not timer_id then
             timer_id = set_timer(200, function()
                 local cpu = sysmon_get_cpu()
                 local ram = sysmon_get_ram()
                 set_text('txt_cpu', math.floor(cpu) .. "%")
                 set_text('txt_ram', math.floor(ram) .. " MB")
                 for i=1, 49 do history[i] = history[i+1] end
                 history[50] = cpu
                 draw_graph()
             end)
         end
      else
         set_text('plg_status', 'Status: Plugin loaded, but API missing.')
         set_prop('plg_status', 'color', '#ef4444')
      end
    else
      set_text('plg_status', 'Status: Injection Failed')
      set_prop('plg_status', 'color', '#ef4444')
    end
  end)
  draw_graph()
}
)htp";
    }
    LumeString wasmDemo() {
        return R"htp(
@page {title:"Lume - WebAssembly"; background:"#09090b";}
@block {align:"center"; padding:40; background:"#09090b"; margin:0;
  @text {content:"WebAssembly Engine"; size:36; color:"#fafafa"; bold:"true"; align:"center";}
  @br {size:5;}
  @text {content:"Native C/Rust performance directly in your HTP pages."; size:15; color:"#71717a"; align:"center";}
}
@block {align:"center"; padding:30; margin:10; background:"#18181b"; border-radius:12;
  @text {content:"Calculate via WASM Module"; size:20; color:"#10b981"; bold:"true"; align:"center";}
  @divider {color:"#27272a"; thickness:1; margin:15; }
  
  /* ЗДЕСЬ ИЗМЕНЕНИЯ: добавил width и align для текста внутри row */
  @row {align:"center"; gap:15;
    @input {id:"num_a"; placeholder:"Number A"; width:100; height:35;}
    @text {content:"+"; size:24; color:"#fafafa"; width:30; align:"center";}
    @input {id:"num_b"; placeholder:"Number B"; width:100; height:35;}
    @button {id:"btn_calc"; content:"="; width:50; height:35; background:"#3b82f6"; color:"#ffffff"; border-radius:6;}
    @text {id:"result"; content:"?"; size:24; color:"#f59e0b"; bold:"true"; width:120; align:"center";}
  }

  @br {size:30;}
  @button {content:"Back to Home"; width:180; height:40; background:"#27272a"; color:"#fafafa"; border-radius:6; url:"about:home"; size:15;}
}
@script {
  local wasm_bytes = "\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x07\x01\x60\x02\x7f\x7f\x01\x7f\x03\x02\x01\x00\x07\x07\x01\x03\x61\x64\x64\x00\x00\x0a\x09\x01\x07\x00\x20\x00\x20\x01\x6a\x0b"
  local mod_id = wasm_load_raw(wasm_bytes)
  on_click('btn_calc', function()
    if not mod_id then
      set_text('result', "ERR")
      return
    end
    local val_a = tonumber(get_input('num_a')) or 0
    local val_b = tonumber(get_input('num_b')) or 0
    local res = wasm_call(mod_id, "add", val_a, val_b)
    set_text('result', tostring(res))
  end)
}
)htp";
    }
    LumeString error(const LumeString& e, const LumeString& u) {
        return "@page{title:\"Error\";background:#1a0000;}\n"
            "@block{align:center;padding:40;margin:30;background:#2a1010;border-radius:10;\n"
            "  @text{content:\"Error\";size:32;color:#ff4444;bold:true; align:\"center\";}\n"
            "  @br{size:15;}\n"
            "  @text{content:\"" + escapeHtp(e) + "\";size:16;color:#ff8888; align:\"center\";}\n"
            "  @text{content:\"" + escapeHtp(u) + "\";size:14;color:#aa6666; align:\"center\";}\n"
            "  @br{size:20;}\n"
            "  @button{content:\"Back to Home\";url:\"about:home\";background:\"#ff4444\";color:\"#ffffff\";size:16;width:150;height:40;border-radius:6;}\n}\n";
    }
}
HTP::Doc g_doc;
Render::Engine g_ren;
LumeVector<LumeString> g_hist;
int g_histPos = -1;
LumeString buildTextHtp(const LumeString& rawText) {
    LumeString htp = "@page {title:\"Text Document\"; background:\"#1e1e1e\";}\n";
    htp += "@column {padding:10; background:\"#1e1e1e\";}\n";
    size_t pos = 0;
    while (pos < rawText.length()) {
        size_t next = rawText.find('\n', pos);
        LumeString line = (next == LumeString::npos) ? rawText.substr(pos) : rawText.substr(pos, next - pos);
        pos = (next == LumeString::npos) ? rawText.length() : next + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        LumeString escaped;
        for (char c : line) {
            if (c == '"') escaped += "\\\"";
            else if (c == '\\') escaped += "\\\\";
            else if (c == '{') escaped += "\\{";
            else if (c == '}') escaped += "\\}";
            else escaped += c;
        }
        if (escaped.empty()) {
            htp += "  @br { size:14; }\n";
        }
        else {
            htp += "  @text { content:\"" + escaped + "\"; size:14; color:\"#cccccc\"; font:\"Consolas\"; }\n";
        }
    }
    return htp;
}
LumeString parseMarkdown(const LumeString& mdText) {
    LumeString htp = "@page {title:\"Markdown Document\"; background:\"#09090b\";}\n";
    htp += "@column {padding:20; background:\"#09090b\"; align:\"left\";}\n";
    bool inCodeBlock = false;
    bool inDetails = false;
    bool inList = false;
    auto formatInline = [](LumeString text) {
        bool bold = false, italic = false;
        if (text.find("**") != LumeString::npos && text.rfind("**") != text.find("**")) {
            bold = true;
            size_t pos;
            while ((pos = text.find("**")) != LumeString::npos) text.erase(pos, 2);
        }
        else if (text.find("*") != LumeString::npos && text.rfind("*") != text.find("*")) {
            italic = true;
            size_t pos;
            while ((pos = text.find("*")) != LumeString::npos) text.erase(pos, 1);
        }
        LumeString props;
        if (bold) props += "bold:\"true\"; ";
        if (italic) props += "italic:\"true\"; ";
        return LumePair<LumeString, LumeString>(text, props);
        };
    auto closeList = [&]() {
        if (inList) { htp += "  }\n"; inList = false; }
        };
    size_t pos = 0;
    while (pos < mdText.length()) {
        size_t next = mdText.find('\n', pos);
        LumeString line = (next == LumeString::npos) ? mdText.substr(pos) : mdText.substr(pos, next - pos);
        pos = (next == LumeString::npos) ? mdText.length() : next + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.empty()) {
            closeList();
            if (!inCodeBlock) htp += "  @br {size:10;}\n";
            continue;
        }
        if (line.find("```") == 0) {
            closeList();
            inCodeBlock = !inCodeBlock;
            if (inCodeBlock) htp += "  @block {background:\"#1e1e1e\"; padding:10; border-radius:6; margin:5; align:\"left\";\n";
            else htp += "  }\n";
            continue;
        }
        if (inCodeBlock) {
            htp += "    @text {content:\"" + escapeHtp(line) + "\"; size:14; color:\"#cccccc\"; font:\"Consolas\";}\n";
            continue;
        }
        if (line.find("<details>") != LumeString::npos) {
            closeList();
            inDetails = true;
            htp += "  @block {background:\"#18181b\"; padding:15; border-radius:8; margin:5; align:\"left\";\n";
            continue;
        }
        if (line.find("</details>") != LumeString::npos) {
            inDetails = false;
            htp += "  }\n";
            continue;
        }
        if (inDetails && line.find("<summary>") != LumeString::npos) {
            size_t s1 = line.find("<summary>") + 9;
            size_t s2 = line.find("</summary>");
            LumeString summary = (s2 != LumeString::npos) ? line.substr(s1, s2 - s1) : line.substr(s1);
            htp += "    @text {content:\"► " + escapeHtp(summary) + "\"; size:16; color:\"#10b981\"; bold:\"true\";}\n";
            htp += "    @divider {color:\"#27272a\"; thickness:1; margin:5;}\n";
            continue;
        }
        int hLevel = 0;
        while (hLevel < line.length() && line[hLevel] == '#') hLevel++;
        if (hLevel > 0 && hLevel <= 6 && line[hLevel] == ' ') {
            closeList();
            LumeString text = line.substr(hLevel + 1);
            int size = 32 - (hLevel * 3);
            htp += "  @text {content:\"" + escapeHtp(text) + "\"; size:" + lume_to_string(size) + "; color:\"#ffffff\"; bold:\"true\";}\n";
            if (hLevel <= 2) htp += "  @divider {color:\"#3f3f46\"; thickness:1; margin:8;}\n";
            continue;
        }
        if (line.find("* ") == 0 || line.find("- ") == 0) {
            if (!inList) {
                htp += "  @list {\n";
                inList = true;
            }
            auto fmt = formatInline(line.substr(2));
            htp += "    @item {content:\"" + escapeHtp(fmt.first) + "\"; size:15; color:\"#d4d4d8\"; " + fmt.second + "}\n";
            continue;
        }
        closeList();
        auto fmt = formatInline(line);
        htp += "  @text {content:\"" + escapeHtp(fmt.first) + "\"; size:15; color:\"#a1a1aa\"; " + fmt.second + "}\n";
    }
    closeList();
    htp += "}\n";
    return htp;
}
void loadContent(const LumeString& content, const LumeString& url, bool isInternalHtp = false) {
    LumeString finalContent = content;
    LumeString displayUrl = url;
    if (Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext) {
        Plugins::g_activeProtocol->free_page(Plugins::g_activeCustomPageContext);
        Plugins::g_activeProtocol = nullptr;
        Plugins::g_activeCustomPageContext = nullptr;
    }
    Script::reset();
    Script::init();
    Plugins::initAllPlugins(Script::g_L);
    LumeString scheme = displayUrl.substr(0, displayUrl.find("://"));
    if (Plugins::g_protocols.find(scheme) != Plugins::g_protocols.end() && !content.empty() && content[0] >= '0' && content[0] <= '9') {
        unsigned long long ptrVal = 0;
        for (char c : content) { if (c >= '0' && c <= '9') ptrVal = ptrVal * 10 + (c - '0'); }
        Plugins::g_activeProtocol = &Plugins::g_protocols[scheme];
        Plugins::g_activeCustomPageContext = (void*)ptrVal;
        g_curUrl = displayUrl;
        g_ren.setScroll(0);
        SetWindowTextU(g_mainWnd, (LumeString("Lume [") + scheme + LumeString("] - ") + displayUrl).c_str());
        SetWindowTextU(g_addressBar, url.c_str());
        invalidateDOM();
        return;
    }
    bool forceRaw = false;
    bool forceHtp = false;
    if (displayUrl.length() > 5 && displayUrl.substr(displayUrl.length() - 5) == " !raw") {
        forceRaw = true;
        displayUrl = displayUrl.substr(0, displayUrl.length() - 5);
    }
    else if (displayUrl.length() > 5 && displayUrl.substr(displayUrl.length() - 5) == " !htp") {
        forceHtp = true;
        displayUrl = displayUrl.substr(0, displayUrl.length() - 5);
    }
    LumeString ext;
    auto dot = displayUrl.find_last_of('.');
    if (dot != LumeString::npos) {
        ext = displayUrl.substr(dot);
        CharLowerBuffA(&ext[0], (DWORD)ext.length());
    }
    auto customEngineIt = Plugins::g_customPages.find(ext);
    if (!isInternalHtp && !forceHtp && !forceRaw && customEngineIt != Plugins::g_customPages.end()) {
        Plugins::g_activeCustomEngine = &customEngineIt->second;
        Plugins::g_activeCustomPageContext = Plugins::g_activeCustomEngine->load_page(
            displayUrl.c_str(), content.c_str(), content.length());
        g_curUrl = displayUrl;
        g_ren.setScroll(0);
        SetWindowTextU(g_mainWnd, (LumeString("Lume (Custom Render) - ") + displayUrl).c_str());
        SetWindowTextU(g_addressBar, url.c_str());
        invalidateContent();
        return;
    }
    if (!isInternalHtp) {
        bool isText = false;
        if (forceRaw) {
            isText = true;
        }
        else if (forceHtp) {
            isText = false;
        }
        else {
            LumeString path = displayUrl;
            auto q = path.find('?');
            if (q != LumeString::npos) path = path.substr(0, q);
            auto slash = path.find_last_of('/');
            if (slash != LumeString::npos) path = path.substr(slash + 1);
            if (!path.empty()) {
                auto p_dot = path.find_last_of('.');
                if (p_dot == LumeString::npos) {
                    isText = true;
                }
                else {
                    LumeString p_ext = path.substr(p_dot);
                    CharLowerBuffA(&p_ext[0], (DWORD)p_ext.length());
                    if (p_ext == ".txt" || p_ext == ".json" || p_ext == ".log" || p_ext == ".cpp" || p_ext == ".h") {
                        isText = true;
                    }
                    else if (p_ext == ".md") {
                        finalContent = parseMarkdown(content);
                        isText = false;
                    }
                    else if (p_ext == ".wasm") {
                        finalContent = "@page{title:\"WASM Module\";background:\"#09090b\";}@block{align:\"center\";padding:50;@text{content:\"WebAssembly Binary Module\";size:30;color:\"#10b981\";bold:\"true\";}@br{size:10;}@text{content:\"Size: " + lume_to_string(content.size()) + " bytes\";size:16;color:\"#a1a1aa\";}}";
                        isText = false;
                    }
                }
            }
        }
        if (isText) finalContent = buildTextHtp(content);
    }
    Plugins::checkDynamicUnloads(displayUrl);
    HTP::Parser p;
    g_doc = p.parse(finalContent);
    g_curUrl = displayUrl;
    g_ren.setScroll(0);
    struct Runner {
        void run(LumeSharedPtr<HTP::Elem> e) {
            if (!e) return;
            if (e->type == HTP::EType::SCRIPT && !e->scriptCode.empty()) {
                LumeString lang = e->props.get("lang", "lua");
                if (lang == "lua") {
                    Script::exec(e->scriptCode.c_str());
                }
                else {
                    auto it = Plugins::g_scriptEngines.find(lang);
                    if (it != Plugins::g_scriptEngines.end() && it->second) {
                        it->second(e->scriptCode.c_str());
                    }
                    else {
                        LumeString err = "Lume Engine: Unknown script language '" + lang + "'\n";
                        OutputDebugStringA(err.c_str());
                    }
                }
            }
            for (auto& c : e->children) run(c);
        }
    } runner;
    runner.run(g_doc.root);
    SetWindowTextU(g_mainWnd, (g_doc.title + " - Lume").c_str());
    SetWindowTextU(g_addressBar, url.c_str());
    invalidateContent();
}
void loadFile(const LumeString& fp) {
    LumeString originalUrl = fp;
    LumeString path = fp;
    if (path.length() > 5 && path.substr(path.length() - 5) == " !raw") path = path.substr(0, path.length() - 5);
    else if (path.length() > 5 && path.substr(path.length() - 5) == " !htp") path = path.substr(0, path.length() - 5);
    if (path.length() > 7 && path.substr(0, 7) == "file://") {
        wchar_t outPathW[MAX_PATH];
        DWORD lenW = MAX_PATH;
        LumeWString wpath = utf8_to_wstring(path);
        if (PathCreateFromUrlW(wpath.c_str(), outPathW, &lenW, 0) == S_OK) {
            path = wstring_to_utf8(outPathW);
        }
        else {
            path = path.substr(7);
            if (!path.empty() && path[0] == '/') path = path.substr(1);
        }
    }
    LumeString content = fastReadFile(path);
    if (content.empty()) {
        loadContent(Pages::error("Not found", path), originalUrl, true);
        return;
    }
    loadContent(content, originalUrl);
    setStatus("Loaded: " + path);
}
void navigateTo(const LumeString& url) {
    g_currentNavId++;
    int myNavId = g_currentNavId;
    LumeString rawUrl = url;
    LumeString flags = "";
    if (rawUrl.length() > 5 && rawUrl.substr(rawUrl.length() - 5) == " !raw") {
        flags = " !raw";
        rawUrl = rawUrl.substr(0, rawUrl.length() - 5);
    }
    else if (rawUrl.length() > 5 && rawUrl.substr(rawUrl.length() - 5) == " !htp") {
        flags = " !htp";
        rawUrl = rawUrl.substr(0, rawUrl.length() - 5);
    }
    LumeString u = resolveUrl(rawUrl, g_curUrl);
    LumeString urlWithFlags = u + flags;
    if (g_histPos >= 0 && g_histPos < (int)g_hist.size() - 1)
        g_hist.resize(g_histPos + 1);
    auto nav = [&](const LumeString& c, const LumeString& nu) {
        g_hist.push_back(nu);
        if (g_hist.size() > 50) {
            g_hist.erase(g_hist.begin());
            if (g_histPos > 0) g_histPos--;
        }
        g_histPos = (int)g_hist.size() - 1;
        loadContent(c, nu, true);
        setStatus("Ready");
        };
    if (u == "about:home" || u == "about:blank" || u.empty()) { nav(Pages::home(), "about:home"); return; }
    if (u == "about:info") { nav(Pages::about(), u); return; }
    if (u == "about:gldemo") { nav(Pages::glDemo(), u); return; }
    if (u == "about:plugindemo") { nav(Pages::pluginDemo(), u); return; }
    if (u == "about:wasm") { nav(Pages::wasmDemo(), u); return; }
    if (u.length() > 7 && u.substr(0, 7) == "file://") {
        g_hist.push_back(urlWithFlags);
        g_histPos = (int)g_hist.size() - 1;
        loadFile(urlWithFlags);
        return;
    }
    if (u.find("://") == LumeString::npos) {
        if (u.length() > 4 && u.substr(u.length() - 4) == ".htp") {
            if (GetFileAttributesA(u.c_str()) != INVALID_FILE_ATTRIBUTES) {
                g_hist.push_back("file://" + urlWithFlags);
                g_histPos = (int)g_hist.size() - 1;
                loadFile("file://" + urlWithFlags);
                return;
            }
        }
        u = "http://" + u;
        urlWithFlags = u + flags;
    }
    LumeString scheme = u.substr(0, u.find("://"));
    auto protoIt = Plugins::g_protocols.find(scheme);
    if (protoIt != Plugins::g_protocols.end()) {
        setStatus("Loading via plugin...");
        LumeThread::RunAsync([u, urlWithFlags, myNavId, scheme]() {
            auto handler = Plugins::g_protocols[scheme];
            void* ctx = handler.fetch_and_parse(u.c_str());
            PendingPage pp;
            pp.url = urlWithFlags;
            pp.navId = myNavId;
            if (ctx) {
                char ptrStr[64];
                sprintf_s(ptrStr, sizeof(ptrStr), "%llu", (unsigned long long)ctx);
                pp.body = ptrStr;
            }
            else {
                pp.error = true;
                pp.body = Pages::error("Protocol plugin failed to fetch/parse", u);
            }
            {
                LumeLockGuard<LumeMutex> lock(g_pageMutex);
                g_pendingPages.push_back(pp);
            }
            PostMessage(g_mainWnd, WM_PAGE_LOADED, 0, 0);
            });
        return;
    }
    if ((u.length() > 7 && u.substr(0, 7) == "http://") || (u.length() > 8 && u.substr(0, 8) == "https://")) {
        setStatus("Loading...");
        LumeThread::RunAsync([u, urlWithFlags, myNavId]() {
            auto r = Net::fetch(Net::URL::parse(u), true);
            PendingPage pp;
            pp.url = urlWithFlags;
            pp.navId = myNavId;
            if (r.ok && !r.body.empty()) {
                pp.body = r.body;
            }
            else {
                pp.error = true;
                pp.body = Pages::error(r.error, u);
            }
            {
                LumeLockGuard<LumeMutex> lock(g_pageMutex);
                g_pendingPages.push_back(pp);
            }
            PostMessage(g_mainWnd, WM_PAGE_LOADED, 0, 0);

            });
        return;
    }
    loadContent(Pages::error("Unknown protocol", u), urlWithFlags, true);
}
void histNav(const LumeString& u) {
    LumeString cleanU = u;
    if (cleanU.length() > 5 && cleanU.substr(cleanU.length() - 5) == " !raw") cleanU = cleanU.substr(0, cleanU.length() - 5);
    else if (cleanU.length() > 5 && cleanU.substr(cleanU.length() - 5) == " !htp") cleanU = cleanU.substr(0, cleanU.length() - 5);
    g_currentNavId++;
    int myNavId = g_currentNavId;
    if (cleanU == "about:home") loadContent(Pages::home(), u, true);
    else if (cleanU == "about:info") loadContent(Pages::about(), u, true);
    else if (cleanU == "about:gldemo") loadContent(Pages::glDemo(), u, true);
    else if (cleanU == "about:plugindemo") loadContent(Pages::pluginDemo(), u, true);
    else if (cleanU == "about:wasm") loadContent(Pages::wasmDemo(), u, true);
    else if (cleanU.length() > 7 && cleanU.substr(0, 7) == "file://") loadFile(u);
    else {
        LumeString scheme = cleanU.substr(0, cleanU.find("://"));
        auto protoIt = Plugins::g_protocols.find(scheme);
        if (protoIt != Plugins::g_protocols.end()) {
            setStatus("Loading history via plugin...");
            LumeThread::RunAsync([cleanU, u, myNavId, scheme]() {
                auto handler = Plugins::g_protocols[scheme];
                void* ctx = handler.fetch_and_parse(cleanU.c_str());
                PendingPage pp;
                pp.url = u;
                pp.navId = myNavId;
                pp.isHistoryNav = true;
                if (ctx) {
                    char ptrStr[64];
                    sprintf_s(ptrStr, sizeof(ptrStr), "%llu", (unsigned long long)ctx);
                    pp.body = ptrStr;
                }
                else {
                    pp.error = true;
                    pp.body = Pages::error("Protocol plugin failed to fetch/parse", cleanU);
                }
                {
                    LumeLockGuard<LumeMutex> lock(g_pageMutex);
                    g_pendingPages.push_back(pp);
                }
                PostMessage(g_mainWnd, WM_PAGE_LOADED, 0, 0);
                });
            return;
        }
        setStatus("Loading history...");
        LumeThread([cleanU, u, myNavId]() {
            auto r = Net::fetch(Net::URL::parse(cleanU), true);
            PendingPage pp;
            pp.url = u;
            pp.navId = myNavId;
            pp.isHistoryNav = true;
            if (r.ok) pp.body = r.body;
            else {
                pp.error = true;
                pp.body = Pages::error(r.error, cleanU);
            }
            {
                LumeLockGuard<LumeMutex> lock(g_pageMutex);
                g_pendingPages.push_back(pp);
            }
            PostMessage(g_mainWnd, WM_PAGE_LOADED, 0, 0);
            }).detach();
    }
}
void goBack() { if (g_histPos > 0) { g_histPos--; histNav(g_hist[g_histPos]); } }
void goFwd() { if (g_histPos < (int)g_hist.size() - 1) { g_histPos++; histNav(g_hist[g_histPos]); } }
WNDPROC g_origAddr = 0;
LRESULT CALLBACK AddrProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        wchar_t b[2048] = {};
        GetWindowTextW(h, b, 2048);
        navigateTo(wstring_to_utf8(b));
        return 0;
    }
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    return CallWindowProcA(g_origAddr, h, m, w, l);
}
LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"BUTTON", L"<", WS_CHILD | WS_VISIBLE, 4, 6, 30, 28, hw, (HMENU)ID_BACK, 0, 0);
        CreateWindowW(L"BUTTON", L">", WS_CHILD | WS_VISIBLE, 38, 6, 30, 28, hw, (HMENU)ID_FWD, 0, 0);
        CreateWindowW(L"BUTTON", L"R", WS_CHILD | WS_VISIBLE, 72, 6, 30, 28, hw, (HMENU)ID_REF, 0, 0);
        g_addressBar = CreateWindowW(L"EDIT", L"about:home", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 108, 8, 500, 24, hw, (HMENU)ID_ADDR, 0, 0);
        CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE, 0, 0, 40, 28, hw, (HMENU)ID_GO, 0, 0);
        g_menuBtn = CreateWindowW(L"BUTTON", L"\x2630", WS_CHILD | WS_VISIBLE, 0, 0, 30, 28, hw, (HMENU)ID_MENU_BTN, 0, 0);
        g_statusBar = CreateWindowW(L"STATIC", L"Ready", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 800, STATUS_H, hw, 0, 0, 0);
        HFONT uf = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        SendMessage(g_addressBar, WM_SETFONT, (WPARAM)uf, 1);
        SendMessage(g_statusBar, WM_SETFONT, (WPARAM)uf, 1);
        SendMessage(g_menuBtn, WM_SETFONT, (WPARAM)uf, 1);
        g_origAddr = (WNDPROC)SetWindowLongPtrW(g_addressBar, GWLP_WNDPROC, (LONG_PTR)AddrProc);
        navigateTo("about:home");
        return 0;
    }
    case WM_SIZE: {
        if (g_fullscreenCanvas) {
            GLCanvas::moveToFullscreen(g_fullscreenCanvasId);
            g_contentDirty = true;
            InvalidateRect(hw, nullptr, FALSE);
            return 0;
        }
        int w = LOWORD(lp), h = HIWORD(lp);
        int menuW = 30, goW = 40, pad = 4;
        int menuX = w - pad - menuW;
        int goX = menuX - pad - goW;
        int addrW = goX - pad - 108;
        if (addrW < 100) addrW = 100;
        MoveWindow(g_addressBar, 108, 8, addrW, 24, TRUE);
        HWND gb = GetDlgItem(hw, ID_GO);
        if (gb) MoveWindow(gb, goX, 6, goW, 28, TRUE);
        MoveWindow(g_menuBtn, menuX, 6, menuW, 28, TRUE);
        MoveWindow(g_statusBar, 0, h - STATUS_H, w, STATUS_H, TRUE);
        g_contentDirty = true;
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hw, &ps);
        RECT cr;
        GetClientRect(hw, &cr);
        if (g_fullscreenCanvas) {
            HBRUSH bb = CreateSolidBrush(RGB(0, 0, 0));
            FillRect(hdc, &cr, bb);
            DeleteObject(bb);
        }
        else {
            int w = cr.right;
            int h = cr.bottom - TOOLBAR_H - STATUS_H;
            if (h < 1) h = 1;
            if (ps.rcPaint.top < TOOLBAR_H) {
                HBRUSH tb = CreateSolidBrush(RGB(30, 30, 50));
                RECT tr = { ps.rcPaint.left, 0, ps.rcPaint.right, TOOLBAR_H };
                FillRect(hdc, &tr, tb);
                DeleteObject(tb);
            }
            if (ps.rcPaint.bottom > TOOLBAR_H) {
                ensureBackbuffer(hdc, w, h);
                if (g_contentDirty) {
                    if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
                        HBRUSH bg = CreateSolidBrush(RGB(26, 26, 46));
                        RECT r = { 0, 0, w, h };
                        FillRect(g_backDC, &r, bg);
                        DeleteObject(bg);
                        GLCanvas::beginLayoutPass();
                        if (Plugins::g_activeCustomEngine->render_page) {
                            Plugins::g_activeCustomEngine->render_page(Plugins::g_activeCustomPageContext, g_backDC, w, h, g_ren.getScroll());
                        }
                        GLCanvas::beginLayoutPass();
                    }
                    else if (g_opt_plugins && Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext) {
                        HBRUSH bg = CreateSolidBrush(RGB(20, 20, 30));
                        RECT r = { 0, 0, w, h };
                        FillRect(g_backDC, &r, bg);
                        DeleteObject(bg);
                        if (Plugins::g_activeProtocol->render_page) {
                            Plugins::g_activeProtocol->render_page(Plugins::g_activeCustomPageContext, g_backDC, w, h, g_ren.getScroll());
                        }
                    }
                    else {
                        if (g_domDirty || g_ren.lastW != w) {
                            GLCanvas::beginLayoutPass();
                            g_ren.updateLayout(g_backDC, w, g_doc);
                            GLCanvas::endLayoutPass();
                            g_domDirty = false;
                        }
                        g_ren.drawToScreen(g_backDC, w, h);
                    }
                    g_contentDirty = false;
                }
                int srcTop = lume_max(0, (int)ps.rcPaint.top - TOOLBAR_H);
                int srcBot = lume_min(h, (int)ps.rcPaint.bottom - TOOLBAR_H);
                int srcLeft = lume_max(0, (int)ps.rcPaint.left);
                int srcRight = lume_min(w, (int)ps.rcPaint.right);
                if (srcBot > srcTop && srcRight > srcLeft) {
                    BitBlt(hdc, srcLeft, TOOLBAR_H + srcTop, srcRight - srcLeft, srcBot - srcTop,
                        g_backDC, srcLeft, srcTop, SRCCOPY);
                }
            }
        }
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        if (g_mouseCaptured) {
            if (g_captureMode == 0) {
                if (g_ignoreWarpMouse) {
                    g_ignoreWarpMouse = false;
                    return 0;
                }
                int mx = (short)LOWORD(lp), my = (short)HIWORD(lp);
                int centerX = g_capturedCanvasX + g_capturedCanvasW / 2;
                int centerY = TOOLBAR_H + g_capturedCanvasY + g_capturedCanvasH / 2;
                if (g_fullscreenCanvas) {
                    RECT cr;
                    GetClientRect(hw, &cr);
                    centerX = cr.right / 2;
                    centerY = cr.bottom / 2;
                }
                int dx = mx - centerX;
                int dy = my - centerY;
                if (dx != 0 || dy != 0) {
                    g_mouseDeltaX += dx;
                    g_mouseDeltaY += dy;
                    POINT c = { centerX, centerY };
                    ClientToScreen(hw, &c);
                    g_ignoreWarpMouse = true;
                    SetCursorPos(c.x, c.y);
                }
            }
            return 0;
        }
        if (Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext && Plugins::g_activeCustomEngine->on_mouse_move) {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp) - TOOLBAR_H;
            Plugins::g_activeCustomEngine->on_mouse_move(Plugins::g_activeCustomPageContext, mx, my);
            return 0;
        }
        else if (Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext && Plugins::g_activeProtocol->on_mouse_move) {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp) - TOOLBAR_H;
            Plugins::g_activeProtocol->on_mouse_move(Plugins::g_activeCustomPageContext, mx, my);
            return 0;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        int d = GET_WHEEL_DELTA_WPARAM(wp);
        g_mouseWheelDelta += d;
        if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext && Plugins::g_activeCustomEngine->on_mouse_wheel) {
            Plugins::g_activeCustomEngine->on_mouse_wheel(Plugins::g_activeCustomPageContext, d);
        }
        else if (g_opt_plugins && Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext && Plugins::g_activeProtocol->on_mouse_wheel) {
            Plugins::g_activeProtocol->on_mouse_wheel(Plugins::g_activeCustomPageContext, d);
        }
        if (g_fullscreenCanvas) return 0;
        int s = g_ren.getScroll() - d / 2;
        if (s < 0) s = 0;
        RECT cr;
        GetClientRect(hw, &cr);
        int totalH = g_ren.totalH();
        if (Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext && Plugins::g_activeCustomEngine->get_document_height) {
            totalH = Plugins::g_activeCustomEngine->get_document_height(Plugins::g_activeCustomPageContext);
        }
        else if (Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext && Plugins::g_activeProtocol->get_document_height) {
            totalH = Plugins::g_activeProtocol->get_document_height(Plugins::g_activeCustomPageContext);
        }
        int mx = totalH - (cr.bottom - TOOLBAR_H - STATUS_H);
        if (mx < 0) mx = 0;
        if (s > mx) s = mx;
        if (s != g_ren.getScroll()) {
            g_ren.setScroll(s);
            invalidateContent();
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (g_fullscreenCanvas || g_mouseCaptured) {
            SetFocus(hw);
            return 0;
        }
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp) - TOOLBAR_H;
        if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
            if (Plugins::g_activeCustomEngine->on_mouse_down) {
                Plugins::g_activeCustomEngine->on_mouse_down(Plugins::g_activeCustomPageContext, mx, my, 1);
            }
            SetFocus(hw);
            return 0;
        }
        else if (g_opt_plugins && Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext) {
            if (Plugins::g_activeProtocol->on_mouse_down) {
                Plugins::g_activeProtocol->on_mouse_down(Plugins::g_activeCustomPageContext, mx, my, 1);
            }
            SetFocus(hw);
            return 0;
        }
        Render::Hit clickedHit;
        bool found = false;
        for (const auto& h : g_ren.hits) {
            if (mx >= h.r.left && mx <= h.r.right && my >= h.r.top && my <= h.r.bottom) {
                clickedHit = h;
                found = true;
                break;
            }
        }
        bool ci = false;
        if (found) {
            auto itPlugin = Plugins::g_customTags.find(clickedHit.elemId);
            if (itPlugin != Plugins::g_customTags.end() && itPlugin->second.on_click && clickedHit.customData) {
                int localX = mx - clickedHit.r.left;
                int localY = my - clickedHit.r.top;
                HTP_NodeHandle nodePtr = (HTP_NodeHandle)clickedHit.customData;
                itPlugin->second.on_click(nodePtr, localX, localY, 1);
                return 0;
            }
            if (clickedHit.isGLCanvas) {
                int localX = mx - clickedHit.r.left;
                int localY = my - clickedHit.r.top;
                Script::fireCanvasClick(clickedHit.canvasId, localX, localY, 1);
                if (clickedHit.autoCapture) {
                    auto e = HTP::findById(g_doc.root, clickedHit.canvasId);
                    int mode = e ? e->props.getInt("capture-mode", 0) : 0;
                    captureCanvasMouse(clickedHit.canvasId, mode);
                }
                SetFocus(hw);
                return 0;
            }
            if (clickedHit.isInput) {
                Bindings::g_focusId = clickedHit.elemId;
                Bindings::g_inputs[clickedHit.elemId].focused = true;
                Bindings::g_inputs[clickedHit.elemId].cursor = (int)Bindings::g_inputs[clickedHit.elemId].text.length();
                ci = true;
                invalidateContent();
            }
            else {
                if (clickedHit.isBtn && !clickedHit.elemId.empty()) {
                    Bindings::fire_click(clickedHit.elemId.c_str());
                    invalidateContent();
                    if (!clickedHit.url.empty()) navigateTo(clickedHit.url);
                    return 0;
                }
                if (!clickedHit.url.empty()) {
                    navigateTo(clickedHit.url);
                    return 0;
                }
            }
        }
        if (!ci) {
            Bindings::g_focusId.clear();
            for (auto& kv : Bindings::g_inputs) kv.second.focused = false;
            invalidateContent();
        }
        SetFocus(hw);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        if (g_fullscreenCanvas || g_mouseCaptured) { SetFocus(hw); return 0; }
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp) - TOOLBAR_H;
        Render::Hit clickedHit; bool found = false;
        for (const auto& h : g_ren.hits) {
            if (mx >= h.r.left && mx <= h.r.right && my >= h.r.top && my <= h.r.bottom) {
                clickedHit = h; found = true; break;
            }
        }
        if (found) {
            if (clickedHit.isGLCanvas) {
                Script::fireCanvasClick(clickedHit.canvasId, mx - clickedHit.r.left, my - clickedHit.r.top, 2);
            }
            else if (clickedHit.isBtn && !clickedHit.elemId.empty()) {
                Bindings::fire_right_click(clickedHit.elemId.c_str());
            }
        }
        SetFocus(hw);
        return 0;
    }
    case WM_CHAR: {
        if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
            if (Plugins::g_activeCustomEngine->on_char) {
                Plugins::g_activeCustomEngine->on_char(Plugins::g_activeCustomPageContext, (unsigned int)wp);
            }
            return 0;
        }
        if (Bindings::g_focusId.empty()) break;
        auto it = Bindings::g_inputs.find(Bindings::g_focusId);
        if (it != Bindings::g_inputs.end()) {
            auto& inp = it->second;
            wchar_t wc = (wchar_t)wp;
            if (wc == L'\b') {
                if (inp.cursor > 0 && !inp.text.empty()) {
                    while (inp.cursor > 0) {
                        inp.cursor--;
                        char erased = inp.text[inp.cursor];
                        inp.text.erase(inp.cursor, 1);
                        if ((erased & 0xC0) != 0x80) break;
                    }
                }
            }
            else if (wc == L'\r' || wc == L'\n') {
                inp.focused = false;
                Bindings::g_focusId.clear();
            }
            else if (wc >= 32) {
                LumeWString ws(1, wc);
                LumeString utf8_char = wstring_to_utf8(ws);
                inp.text.insert(inp.cursor, utf8_char);
                inp.cursor += (int)utf8_char.length();
            }
            invalidateDOM();
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        if ((lp & (1 << 30)) == 0 && wp < 256) {
            Bindings::g_keyPressed[wp] = true;
        }
        if (wp == VK_ESCAPE) {
            if (g_mouseCaptured) {
                releaseMouse();
                return 0;
            }
            if (g_fullscreenCanvas) {
                exitFullscreenCanvas();
                return 0;
            }
        }
        if (wp == VK_F11) {
            if (g_fullscreenCanvas) exitFullscreenCanvas();
            else {
                for (auto& h : g_ren.hits) {
                    if (h.isGLCanvas) {
                        enterFullscreenCanvas(h.canvasId);
                        break;
                    }
                }
            }
            return 0;
        }
        if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
            if (Plugins::g_activeCustomEngine->on_key_down) {
                Plugins::g_activeCustomEngine->on_key_down(Plugins::g_activeCustomPageContext, (int)wp);
            }
            if (wp == VK_F5) navigateTo(g_curUrl);
            else if (wp == VK_BACK && GetFocus() != g_addressBar) goBack();
            return 0;
        }
        if (Script::g_keyDownRef != LUA_NOREF && Script::g_L && GetFocus() == hw && Bindings::g_focusId.empty()) {
            lua_rawgeti(Script::g_L, LUA_REGISTRYINDEX, Script::g_keyDownRef);
            lua_pushinteger(Script::g_L, (int)wp);
            if (lua_pcall(Script::g_L, 1, 0, 0) != 0) {
                OutputDebugStringA(lua_tostring(Script::g_L, -1));
                lua_pop(Script::g_L, 1);
            }
        }
        if (!Bindings::g_focusId.empty() && GetFocus() == hw) {
            auto it = Bindings::g_inputs.find(Bindings::g_focusId);
            if (it != Bindings::g_inputs.end()) {
                auto& inp = it->second;
                if (wp == VK_LEFT) {
                    while (inp.cursor > 0) {
                        inp.cursor--;
                        if ((inp.text[inp.cursor] & 0xC0) != 0x80) break;
                    }
                    invalidateDOM();
                    return 0;
                }
                if (wp == VK_RIGHT) {
                    while (inp.cursor < (int)inp.text.length()) {
                        inp.cursor++;
                        if (inp.cursor >= (int)inp.text.length() || (inp.text[inp.cursor] & 0xC0) != 0x80) break;
                    }
                    invalidateDOM();
                    return 0;
                }
                if (wp == VK_HOME) {
                    inp.cursor = 0;
                    invalidateDOM();
                    return 0;
                }
                if (wp == VK_END) {
                    inp.cursor = (int)inp.text.length();
                    invalidateDOM();
                    return 0;
                }
                if (wp == VK_DELETE) {
                    if (inp.cursor < (int)inp.text.length()) {
                        inp.text.erase(inp.cursor, 1);
                        invalidateDOM();
                    }
                    return 0;
                }
                if (wp == 'V' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                    if (OpenClipboard(hw)) {
                        HANDLE h = GetClipboardData(CF_TEXT);
                        if (h) {
                            const char* cl = (const char*)GlobalLock(h);
                            if (cl) {
                                LumeString p = cl;
                                p.remove_char('\r');
                                p.remove_char('\n');
                                inp.text.insert(inp.cursor, p);
                                inp.cursor += (int)p.length();
                                GlobalUnlock(h);
                            }
                        }
                        CloseClipboard();
                    }
                    invalidateDOM();
                    return 0;
                }
            }
        }
        if (wp == VK_F5) navigateTo(g_curUrl);
        else if (wp == VK_BACK && Bindings::g_focusId.empty() && GetFocus() != g_addressBar) goBack();
        return 0;
    }
    case WM_KEYUP: {
        if (wp < 256) Bindings::g_keyReleased[wp] = true;
        return 0;
    }
    case WM_SETCURSOR:
        if (g_mouseCaptured) {
            SetCursor(nullptr);
            return TRUE;
        }
        if (LOWORD(lp) == HTCLIENT) {
            if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
                SetCursor(LoadCursor(0, IDC_ARROW));
                return TRUE;
            }
            if (Plugins::g_activeProtocol && Plugins::g_activeCustomPageContext) {
                return TRUE;
            }
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hw, &pt);
            pt.y -= TOOLBAR_H;
            bool ov = false;
            for (auto& h : g_ren.hits) {
                if (pt.x >= h.r.left && pt.x <= h.r.right && pt.y >= h.r.top && pt.y <= h.r.bottom) {
                    if (!h.url.empty() || h.isBtn || h.isGLCanvas) { ov = true; break; }
                    if (h.isInput) {
                        SetCursor(LoadCursor(0, IDC_IBEAM));
                        return TRUE;
                    }
                }
            }
            SetCursor(LoadCursor(0, ov ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == ID_MENU_BTN) {
            HMENU hMenu = CreatePopupMenu();
            HMENU hHistMenu = CreatePopupMenu();
            int histCount = 0;
            for (int i = (int)g_hist.size() - 1; i >= 0 && histCount < 15; --i) {
                LumeWString wurl = utf8_to_wstring(g_hist[i]);
                AppendMenuW(hHistMenu, MF_STRING, IDM_HIST_START + i, wurl.c_str());
                histCount++;
            }
            if (histCount == 0) AppendMenuW(hHistMenu, MF_STRING | MF_DISABLED, 0, L"No History");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHistMenu, L"History");
            HMENU hLuaMenu = CreatePopupMenu();
            AppendMenuW(hLuaMenu, MF_STRING | (g_opt_lua_http ? MF_CHECKED : MF_UNCHECKED), IDM_LUA_HTTP, L"Allow HTTP Requests (Lua)");
            AppendMenuW(hLuaMenu, MF_STRING | (g_opt_lua_mouse ? MF_CHECKED : MF_UNCHECKED), IDM_LUA_MOUSE, L"Allow Mouse Tracking (Lua)");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hLuaMenu, L"Lua Bindings");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            HMENU hPlugMenu = CreatePopupMenu();
            int pIdx = 0;
            for (auto& p : Plugins::g_plugins) {
                UINT flags = MF_STRING | (p.enabled ? MF_CHECKED : MF_UNCHECKED);
                AppendMenuW(hPlugMenu, flags, IDM_PLUGIN_START + pIdx, utf8_to_wstring(p.name).c_str());
                pIdx++;
            }
            if (!Plugins::g_dynPlugins.empty() && pIdx > 0) AppendMenuW(hPlugMenu, MF_SEPARATOR, 0, NULL);
            for (auto& dp : Plugins::g_dynPlugins) {
                UINT flags = MF_STRING | (dp.enabled ? MF_CHECKED : MF_UNCHECKED);
                LumeString shortName = dp.url.substr(dp.url.find_last_of('/') + 1);
                AppendMenuW(hPlugMenu, flags, IDM_PLUGIN_START + pIdx, utf8_to_wstring("[Net] " + shortName).c_str());
                pIdx++;
            }
            if (pIdx == 0) AppendMenuW(hPlugMenu, MF_STRING | MF_DISABLED, 0, L"No plugins loaded");
            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPlugMenu, L"Plugins");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(hMenu, MF_STRING | (g_opt_gpu ? MF_CHECKED : MF_UNCHECKED), IDM_GPU_TOGGLE, L"Enable GPU (OpenGL)");
            RECT r; GetWindowRect(g_menuBtn, &r);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTALIGN | TPM_TOPALIGN, r.right, r.bottom, 0, hw, NULL);
            DestroyMenu(hMenu);
            if (cmd >= IDM_HIST_START && cmd < IDM_HIST_START + 1000) {
                int idx = cmd - IDM_HIST_START;
                if (idx >= 0 && idx < g_hist.size()) {
                    g_histPos = idx;
                    histNav(g_hist[idx]);
                }
            }
            else if (cmd >= IDM_PLUGIN_START && cmd < IDM_PLUGIN_START + 1000) {
                int pId = cmd - IDM_PLUGIN_START;
                if (pId < Plugins::g_plugins.size()) Plugins::g_plugins[pId].enabled = !Plugins::g_plugins[pId].enabled;
                else Plugins::g_dynPlugins[pId - Plugins::g_plugins.size()].enabled = !Plugins::g_dynPlugins[pId - Plugins::g_plugins.size()].enabled;
                Script::reset();
                Plugins::reboot();
                navigateTo(g_curUrl);
            }
            else if (cmd == IDM_LUA_HTTP) g_opt_lua_http = !g_opt_lua_http;
            else if (cmd == IDM_LUA_MOUSE) g_opt_lua_mouse = !g_opt_lua_mouse;
            else if (cmd == IDM_GPU_TOGGLE) {
                g_opt_gpu = !g_opt_gpu;
                if (g_opt_gpu) GLLoader::load();
                else {
                    GLCanvas::destroyAll();
                    GLLoader::unload();
                }
                invalidateContent();
            }
            return 0;
        }
        if (id == ID_GO) {
            wchar_t b[2048] = {};
            GetWindowTextW(g_addressBar, b, 2048);
            navigateTo(wstring_to_utf8(b));
        }
        else if (id == ID_BACK) goBack();
        else if (id == ID_FWD) goFwd();
        else if (id == ID_REF) navigateTo(g_curUrl);
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp;
        wchar_t wfp[MAX_PATH] = {};
        DragQueryFileW(hd, 0, wfp, MAX_PATH);
        DragFinish(hd);
        loadFile(wstring_to_utf8(wfp));
        return 0;
    }
    case WM_TIMER:
        return 0;
    case WM_DESTROY:
        InterlockedExchange(&g_isShuttingDown, 1);
        MSG tempMsg;
        while (PeekMessage(&tempMsg, hw, WM_NAVIGATE_DEFERRED, WM_NAVIGATE_DEFERRED, PM_REMOVE)) {
            LumeString* str = (LumeString*)tempMsg.wParam;
            if (str) delete str;
        }
        releaseMouse();
        if (g_fullscreenCanvas) exitFullscreenCanvas();
        Script::reset();
        if (g_opt_plugins && Plugins::g_activeCustomEngine && Plugins::g_activeCustomPageContext) {
            Plugins::g_activeCustomEngine->free_page(Plugins::g_activeCustomPageContext);
            Plugins::g_activeCustomEngine = nullptr;
            Plugins::g_activeCustomPageContext = nullptr;
        }
        Plugins::unloadAll();
        cleanupBackbuffer();
        FontCache::clear();
        Gdiplus::GdiplusShutdown(g_gdiplusToken);
        PostQuitMessage(0);
        return 0;
    case WM_UPDATE_CONTENT:
        g_contentDirty = true;
        g_domDirty = true;
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    case WM_PAGE_LOADED: {
        LumeLockGuard<LumeMutex> lock(g_pageMutex);
        for (const auto& pp : g_pendingPages) {
            if (pp.navId != g_currentNavId) continue;
            if (!pp.isHistoryNav) {
                g_hist.push_back(pp.url);
                if (g_hist.size() > 50) { g_hist.erase(g_hist.begin()); if (g_histPos > 0) g_histPos--; }
                g_histPos = (int)g_hist.size() - 1;
            }
            if (pp.error) {
                loadContent(pp.body, pp.url, true);
                setStatus("Err");
            }
            else {
                loadContent(pp.body, pp.url);
                setStatus("OK");
            }
        }
        g_pendingPages.clear();
        return 0;
    }
    case WM_NAVIGATE_DEFERRED: {
        LumeString* url = (LumeString*)wp;
        if (url) {
            navigateTo(*url);
            delete url;
        }
        return 0;
    }
    }
    return DefWindowProcW(hw, msg, wp, lp);
}
int WINAPI WinMain(HINSTANCE hI, HINSTANCE, LPSTR cmd, int show) {
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&ic);
    initHighResTimer();
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    GLLoader::load();
    Plugins::initHostAPI();
    Plugins::discoverPlugins();
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hI;
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszClassName = L"LumeClass";
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    RegisterClassExW(&wc);
    GLCanvas::registerClass(hI);
    g_mainWnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"LumeClass",
        L"Lume",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        0, 0, hI, 0);
    if (!g_mainWnd) return 1;
    ShowWindow(g_mainWnd, show);
    UpdateWindow(g_mainWnd);
    if (cmd && lstrlenA(cmd) > 0) {
        LumeString a = cmd;
        if (!a.empty() && a.front() == '"') a.erase(a.begin());
        if (!a.empty() && a.back() == '"') a.pop_back();
        if (!a.empty()) navigateTo(a);
    }
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Plugins::unloadAll();
    cleanupBackbuffer();
    FontCache::clear();
    GLLoader::unload();
    return (int)msg.wParam;
}
