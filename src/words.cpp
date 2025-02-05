#include <cstdio>
#include <cstring>
#include <algorithm>

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
        exit(1);
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
    int cur_ind, string &prune_signature, char* prune_pattern, int &new_pos
) {
    string sig = prune_signature + prune_pattern;
    auto it = map.find(sig);
    if (it != map.end()) {
        return it->second;
    }
    it = map.find(prune_signature);
    if (it == map.end()) {
        fprintf(stderr, "signature %s not found\n", prune_signature.c_str());
        exit(1);
    }
    ILIST* ilist = it->second;
    ILIST *ilist2 = new ILIST;
    bool found = false;
    for (int i: *ilist) {
        if (!match(len, prune_pattern, (*wlist)[i])) {
            if (found) {
                new_pos = ilist2->size();
                found = false;
            }
            ilist2->push_back(i);
            if (i == cur_ind) {
                found = true;
            }
        }
    }
    map[sig] = ilist2;
    return ilist2;
}

PATTERN_CACHE pattern_cache[MAX_LEN];

void init_pattern_cache() {
    for (int i=1; i<=MAX_LEN; i++) {
        pattern_cache[i].init(i, &(words.words[i]));
    }
}

