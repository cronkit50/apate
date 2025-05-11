#ifndef LRUCACHE_HPP
#define LRUCACHE_HPP

#include <list>
#include <unordered_map>
#include <utility>


template <typename KEY_T, typename OBJ_T>
class lruCache{
public:
    lruCache(const size_t cacheSize = 100) : m_cacheSize(cacheSize) {
    }

    lruCache(lruCache&) = delete;
    lruCache(lruCache&&) = delete;
    lruCache& operator=(lruCache&) = delete;
    lruCache& operator=(lruCache&&) = delete;

    ~lruCache() = default;

    bool Get(const KEY_T&key, OBJ_T &obj){
        bool found = false;

        auto it = m_cacheIndex.find(key);
        if (it != m_cacheIndex.end()){
            // cache hit, move to the beginning
            m_cacheQueue.splice(m_cacheQueue.begin(), m_cacheQueue, it->second);

            obj = (it->second)->second;
        }
        else{
            // cache miss
        }

        return found;

    }

    void Set(const KEY_T& key, const OBJ_T& obj){
        auto it = m_cacheIndex.find(key);

        if (it != m_cacheIndex.end()){
            // already cached, just update it
            (*m_cacheIndex[key]).second = obj;

            // move to beginning
            m_cacheQueue.splice(m_cacheQueue.begin(), m_cacheQueue, it->second);
        }
        else{
            m_cacheQueue.push_front(cachePair{key,
                                               obj});

            m_cacheIndex[key] = m_cacheQueue.begin();

            if(m_cacheQueue.size() > m_cacheSize){
                // full, so evict the last (least recently used) object
                m_cacheIndex.erase(m_cacheQueue.back().first);
                m_cacheQueue.pop_back();
            }
        }
    }

    void Clear(){
        m_cacheQueue.clear();
        m_cacheIndex.clear();
    }



private:
    typedef std::pair<KEY_T, OBJ_T> cachePair;

    std::list<cachePair> m_cacheQueue;
    std::unordered_map<KEY_T, typename decltype(m_cacheQueue)::iterator> m_cacheIndex;


    size_t m_cacheSize;
};

#endif