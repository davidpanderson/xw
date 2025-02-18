#include <cstdio>
#include <cstring>
#include <algorithm>

#include "xw.h"
#include "words.h"

WORDS words;

inline void reverse_str(char *s) {
    int length = strlen(s);
    int i, j;
    char c;
    for (i=0, j=length-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

///////////////// WORDS

// read words from file into per-length vectors
//
void WORDS::read(const char* fname, bool reverse_words) {
    FILE* f = fopen(fname, "r");
    if (!f) {
        printf("no word list %s\n", fname);
        exit(1);
    }
    char buf[256];
    while (fgets(buf, 256, f)) {
        int len = strlen(buf)-1;
        if (len >= MAX_LEN) continue;
        buf[len] = 0;
        if (have_vetoed_words[len]) {
            if (vetoed_words[len].find(buf) != vetoed_words[len].end()) {
                continue;
            }
        }
        nwords[len]++;
        words[len].push_back(strdup(buf));
        if (reverse_words) {
            reverse_str(buf);
            words[len].push_back(strdup(buf));
        }
    }
    fclose(f);
}

void WORDS::read_veto_file(const char* fname) {
    FILE* f = fopen(fname, "r");
    if (!f) {
        printf("no veto file %s\n", fname);
        return;
    }
    char buf[256];
    while (fgets(buf, 256, f)) {
        int len = strlen(buf)-1;
        if (len >= MAX_LEN) continue;
        buf[len] = 0;
        vetoed_words[len].insert(buf);
        have_vetoed_words[len] = true;
    }
    fclose(f);
}

void WORDS::shuffle() {
    for (int i=1; i<=MAX_LEN; i++) {
        if (words[i].empty()) continue;
        random_shuffle(words[i].begin(), words[i].end());
    }
}

void WORDS::print_counts() {
    printf("%d\n", max_len);
    for (int i=1; i<=MAX_LEN; i++) {
        printf("%d: %d\n", i, nwords[i]);
    }
}

void show_matches(int len, WLIST &wlist, ILIST &ilist) {
    for (int i: ilist) {
        printf("%s\n", wlist[i]);
    }
}

// PATTERN_CACHE
// (not actually a cache since we never flush anything)
// maps patterns to the list of words matching that pattern.

// 'pattern': a word in which some or all positions are undetermined
// (represented by _)

// Get list of words matching pattern.
// If not in cache, compute and store in cache
//
ILIST* PATTERN_CACHE::get_matches(char* pattern) {
    auto it = map.find(pattern);
    if (it != map.end()) {
        return it->second;
    }
    ILIST *ilist = new ILIST;
    //get_matches(len, pattern, *wlist, *ilist);
    for (unsigned int i=0; i<wlist->size(); i++) {
        if (match(len, pattern, (*wlist)[i])) {
            ilist->push_back(i);
        }
    }
    map[pattern] = ilist;
    return ilist;
}

// From the list corresponding to prune_signature,
// remove words that match prune_pattern.
// Return the resulting list, and memoize the result
//
ILIST* PATTERN_CACHE::get_matches_prune(
    ILIST *ilist, int& next_index,
    string &prune_signature, char* prune_pattern
) {
    int cur_index = next_index-1;
#if VERBOSE_PRUNE
    printf("get_matches_prune():\n"
        "   next_index %d\n"
        "   prune_signature: %s\n"
        "   prune_pattern: %s\n",
        cur_index, prune_signature.c_str(), prune_pattern
    );
#endif

    string sig = prune_signature + prune_pattern;
    auto it = map.find(sig);
    if (it != map.end()) {
        return it->second;
    }
    ILIST *ilist2 = new ILIST;
    bool found = false;

    // make list of words that don't match the prune pattern.
    // cur_index will always match.
    // new_index is index of 1st word after cur
    //
    for (int j=0; j<ilist->size(); j++) {
        if (j == cur_index) {
            next_index = ilist2->size();
            continue;
        }
        int i = (*ilist)[j];
        if (match(len, prune_pattern, (*wlist)[i])) {
#if VERBOSE_PRUNE
            printf("   pruned %s\n", (*wlist)[i]);
#endif
            found = true;
        } else {
            ilist2->push_back(i);
        }
    }
    if (!found) {
#if VERBOSE_PRUNE
        printf("prune: no matching words found\n");
#endif
        delete ilist2;
        return ilist;
    }
    prune_signature += prune_pattern;
    map[sig] = ilist2;
#if VERBOSE_PRUNE
    printf("   pruned from %d to %d words, index old %d new %d\n",
        (int)ilist->size(), (int)ilist2->size(), cur_index, next_index
    );
#endif
    return ilist2;
}

PATTERN_CACHE pattern_cache[MAX_LEN];

void init_pattern_cache() {
    for (int i=1; i<=MAX_LEN; i++) {
        pattern_cache[i].init(i, &(words.words[i]));
    }
}
