#ifndef XW_H
#define XW_H

#include <cstring>
#include <vector>
#include <unordered_set>
#include <stack>
#include <cstdlib>
#include <ncurses.h>

#include "words.h"

using namespace std;

extern bool verbose_prune;

#define CHECK_ASSERTS           0
    // do sanity checks: conditions that should always hold

////////////////// SLOTS AND GRIDS ////////////////

struct SLOT;
struct GRID;

// link from a position in a slot to a position in another slot.
//
struct LINK {
    SLOT *other_slot;       // NULL if no link
    int other_pos;
    LINK() {
        other_slot = NULL;
        other_pos = 0;
    }
    inline bool empty() {
        return other_slot == NULL;
    }
};

static int slot_num = 0;

struct SLOT {
    int num;        // number in grid (unique, but otherwise arbitrary)
    int len;
    LINK links[MAX_LEN];    // crossing slots

    bool filled;
        // is this slot filled?
    char preset_pattern[MAX_LEN];
        // preset letters
    char filled_pattern[MAX_LEN];
        // letters from crossing filled slots lower on stack
    ILIST *compatible_words;
        // words compatible with filled pattern
    int next_word_index;
        // if filled, next compatible word to try
    char current_word[MAX_LEN];
        // if filled, current word
    string prune_signature;
    int stack_level;
        // if filled, the level on the filled stack
    int dup_stack_level;
        // if we skipped a compatible word because it was already used,
        // the stack level of the slot that used it.
    bool ref_by_higher[MAX_LEN];
        // if we backtrack to here, was this cell part of
        // any of the higher-level slots that we pushed?

    int row, col;
    bool is_across;
        // for planar grids
    char name[16];
        // e.g. A(2,0)

    // for each position and each letter (a-z)
    // keep track of whether putting the letter in that position
    // was OK (nonzero compatible words in the linked slot).
    // These must be cleared each time we fill this slot.
    //
    bool usable_letter_checked[MAX_LEN][26];
    bool usable_letter_ok[MAX_LEN][26];

    // create SLOT; you may increase len later
    SLOT(int _len=0) {
        len = _len;
        num = slot_num++;
        strcpy(preset_pattern, NULL_PATTERN);
    }
    void add_link(int this_pos, SLOT* other_slot, int other_pos);

    // specify preset cell
    // NOTE: if there's a crossing slot, you must set it there too
    //
    void preset_char(int pos, char c) {
        preset_pattern[pos] = c;
    }

    void prepare_slot();

    inline void clear_usable_letter_checked() {
        memset(usable_letter_checked, 0, sizeof(usable_letter_checked));
    }

    void print_usable();
    void print_state(bool show_links);
    bool find_next_usable_word(GRID*);
    bool letter_compatible(int pos, char c);
    bool check_pattern(char* mp);
    void uninstall_word();
    int top_affecting_level();
    bool prune();
};

struct GRID {
    vector<SLOT*> slots;
    vector<SLOT*> filled_slots;
    int npreset_slots;
        // number of preset slots.
        // these are marked as filled but not pushed on the filled stack
    int nsteps;
        // total number of words installed (for performance testing)

    GRID() {
        nsteps = 0;
    }
    void add_slot(SLOT* slot) {
        slots.push_back(slot);
    }
    void add_link(SLOT *slot1, int pos1, SLOT *slot2, int pos2) {
        char c1 = slot1->filled_pattern[pos1];
        char c2 = slot2->filled_pattern[pos2];
        if (c1 != '_') {
            slot2->filled_pattern[pos2] = c1;
        } else if (c2 != '_') {
            slot1->filled_pattern[pos1] = c2;
        } else {
            slot1->add_link(pos1, slot2, pos2);
            slot2->add_link(pos2, slot1, pos1);
        }
    }
    void print_solution() {
        printf("------ solution --------\n");
        for (SLOT *slot: slots) {
            printf("%d: %s\n", slot->num, slot->current_word);
        }
    }
    void print_state(bool show_links = false) {
        printf("------- grid state ----------\n");
        for (SLOT *slot: filled_slots) {
            slot->print_state(show_links);
        }
        for (SLOT *slot: slots) {
            if (slot->filled) continue;
            slot->print_state(show_links);
        }
        printf("\n------- end ----------\n");
    }

    // call this after adding slots, presets, and links
    //
    void prepare_grid() {
        npreset_slots = 0;
        for (SLOT *s: slots) {
            s->prepare_slot();
                // this sets filled if needed
            if (s->filled) {
                npreset_slots++;
            }
        }
    }

    bool push_next_slot();
    bool backtrack();
    void install_word(SLOT*);
    bool find_solutions();
    void restart();
    int get_commands();
};

#endif
