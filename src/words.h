#ifndef WORDS_H
#define WORDS_H

#include <vector>
#include <map>
#include <unordered_set>
#include <unordered_map>

using namespace std;

// word lists and patterns

// a 'pattern' has '_' for wildcard.

#define NULL_PATTERN (char*)"____________________________"

#define MAX_LEN 29
    // longest word plus 1 for NULL

typedef vector<char*> WLIST;
    // a list of words
typedef unordered_set<string> WSET;
    // a set of (vetoed) words; constant-time lookup
typedef vector<int> ILIST;
    // a list of indices into a WLIST (i.e. a subset of the words)

struct WORDS {
    WLIST words[MAX_LEN+1];
    WSET vetoed_words[MAX_LEN+1];
    bool have_vetoed_words[MAX_LEN+1];
    int nwords[MAX_LEN+1];
    int max_len;
    void read(const char* fname, bool reverse_words);
    void read_veto_file(const char* fname);
    void print_counts();
    void shuffle();
};

// for a list of words of given len,
// cache a mapping of pattern -> word index list
//
struct PATTERN_CACHE {
    int len;
    WLIST *wlist;
    unordered_map<string, ILIST*> map;

    void init(int _len, WLIST *_wlist) {
        len = _len;
        wlist = _wlist;
        map.clear();
    }
    ILIST* get_matches(char* pattern);
    ILIST* get_matches_prune(
        ILIST* ilist, int& next_index,
        string &prune_signature, char* prune_pattern
    );
};
extern PATTERN_CACHE pattern_cache[MAX_LEN];

// does word match pattern?
//
inline bool match(int len, char *pattern, char* word) {
    for (int i=0; i<len; i++) {
        if (pattern[i]!='_' && pattern[i]!=word[i]) return false;
    }
    return true;
}

extern WORDS words;
extern void init_pattern_cache();

#endif
