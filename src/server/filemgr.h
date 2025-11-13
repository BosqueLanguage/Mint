#pragma once

#include "common.h"
#include "alloc.h"

//TODO: don't want malloc so later we should do a custom implementation of this
#include <map>

#define SMALL_CACHE_PATH 32

template<size_t MAX>
class FileCacheSmallKey
{
private:
    size_t m_len;
    char m_path[MAX];
public:
    FileCacheSmallKey() : m_len(0), m_path{} { ; }
    FileCacheSmallKey(const char* path, size_t len) : m_len(len), m_path{} { strncpy(this->m_path, path, MAX); }

    FileCacheSmallKey(const FileCacheSmallKey& other) : m_len(other.m_len), m_path{} 
    { 
        strncpy(this->m_path, other.m_path, MAX);
        this->m_len = other.m_len;
    }

    FileCacheSmallKey& operator=(const FileCacheSmallKey& other)
    {
        if(this != &other) {
            this->m_len = other.m_len;
            strncpy(this->m_path, other.m_path, MAX);
        }

        return *this;
    }

    bool operator==(const FileCacheSmallKey& other) const
    {
        if(this->m_len != other.m_len) {
            return false;
        }

        return strncmp(this->m_path, other.m_path, MAX) == 0;
    }

    bool operator!=(const FileCacheSmallKey& other) const
    {
        if(this->m_len != other.m_len) {
            return true;
        }

        return strncmp(this->m_path, other.m_path, MAX) != 0;
    }

    bool operator<(const FileCacheSmallKey& other) const
    {
        if(this->m_len != other.m_len) {
            return this->m_len < other.m_len;
        }
        else {
            return strncmp(this->m_path, other.m_path, MAX) < 0;
        }
    }
};

class FileCachePermanentEntry
{
public:
    const char* m_data;
    size_t m_size;

    FileCachePermanentEntry(const char* data, size_t size) : m_data(data), m_size(size) { ; }
    ~FileCachePermanentEntry() { ; }

    FileCachePermanentEntry(const FileCachePermanentEntry& other) = default;
    FileCachePermanentEntry& operator=(const FileCachePermanentEntry& other) = default;
};

//TODO: we don't ever evict right now so no need for more complex logic but later keep a last accessed tick for eviction

class FileCacheManager
{
private:
    std::map<FileCacheSmallKey<SMALL_CACHE_PATH>, FileCachePermanentEntry> memoizedsmall;

    //TODO: later do a memoized general and then LRU flavors

public:
    FileCacheManager() { ; }
    ~FileCacheManager() { ; }

    void clear(ServerAllocator& allocator)
    {
        for(auto& pair : this->memoizedsmall) {
            const FileCachePermanentEntry& entry = pair.second;
            allocator.freebytesp2((uint8_t*)entry.m_data, entry.m_size);
        }
        this->memoizedsmall.clear();
    }

    std::pair<const char*, size_t> tryGet(const char* path)
    {
        size_t len = strlen(path);
        if(len <= SMALL_CACHE_PATH) {
            FileCacheSmallKey<SMALL_CACHE_PATH> key(path, len);

            auto it = this->memoizedsmall.find(key);
            if(it != this->memoizedsmall.end()) {
                return std::make_pair(it->second.m_data, it->second.m_size);
            }
            else {
                return std::make_pair(nullptr, 0);
            }
        }
        else {
            assert(false); //TODO: later implement larger key caching
        }
    }

    const char* put(const char* path, size_t pathsize, const char* data, size_t datasize)
    {
        if(pathsize <= SMALL_CACHE_PATH) {
            auto it = this->memoizedsmall.emplace(FileCacheSmallKey<SMALL_CACHE_PATH>{path, pathsize}, FileCachePermanentEntry{data, datasize});

            return it.first->second.m_data;
        }
        else {
            assert(false); //TODO: later implement larger key caching
        }
    }
};
