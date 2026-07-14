// ds/trie.h
// -----------------------------------------------------------------------
// Custom TRIE (prefix tree) for location autocomplete.
//
// Insert every location name (lowercased); suggest(prefix) walks down
// the prefix in O(L) then DFS-collects up to `limit` completions.
// Children are a fixed 128-slot array indexed by ASCII code — O(1) per
// character, and DFS in index order returns results in stable
// (roughly alphabetical) order for free.
//
// insert O(L) | suggest O(L + results) | space O(total characters)
// -----------------------------------------------------------------------
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace lifeline {

class Trie {
public:
    Trie() = default;
    ~Trie() { destroy(root_); }
    Trie(const Trie&) = delete;             // owns raw pointers: no copies
    Trie& operator=(const Trie&) = delete;

    void insert(const std::string& name, int id) {
        TNode* cur = &rootNode_;
        for (unsigned char c : lower(name)) {
            if (c >= 128) continue;          // ASCII names only
            if (!cur->child[c]) cur->child[c] = new TNode();
            cur = cur->child[c];
        }
        cur->id = id;                        // terminal: stores the node id
        ++count_;
    }

    // Ids of every inserted name starting with `prefix` (max `limit`).
    std::vector<int> suggest(const std::string& prefix, int limit = 8) const {
        std::vector<int> out;
        const TNode* cur = &rootNode_;
        for (unsigned char c : lower(prefix)) {
            if (c >= 128 || !cur->child[c]) return out;   // no such prefix
            cur = cur->child[c];
        }
        collect(cur, out, limit);
        return out;
    }

    size_t size() const { return count_; }

private:
    struct TNode {
        int    id = -1;                      // -1 = not a terminal
        TNode* child[128] = {nullptr};
    };

    TNode  rootNode_;
    TNode* root_ = &rootNode_;               // for destroy()
    size_t count_ = 0;

    static std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return s;
    }

    static void collect(const TNode* n, std::vector<int>& out, int limit) {
        if (static_cast<int>(out.size()) >= limit) return;
        if (n->id != -1) out.push_back(n->id);
        for (int c = 0; c < 128; ++c)
            if (n->child[c]) collect(n->child[c], out, limit);
    }

    static void destroy(TNode* n) {
        for (int c = 0; c < 128; ++c)
            if (n->child[c]) { destroy(n->child[c]); delete n->child[c]; }
    }
};

}  // namespace lifeline
