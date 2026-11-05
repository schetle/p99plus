#pragma once
// Fixed-size map for a handful of long-lived keys (e.g. engine objects). No allocation.

template <class K, class V, int N>
class SmallMap {
public:
    // The entry for `key`, or a fresh default-constructed one (evicting slot 0 when full).
    V& Claim(const K& key)
    {
        Entry* free = nullptr;
        for (Entry& e : entries_) {
            if (e.used && e.key == key) return e.value;
            if (!e.used && !free) free = &e;
        }
        if (!free) free = &entries_[0];
        *free = Entry{};
        free->used = true;
        free->key = key;
        return free->value;
    }

private:
    struct Entry { K key{}; V value{}; bool used = false; };
    Entry entries_[N];
};
