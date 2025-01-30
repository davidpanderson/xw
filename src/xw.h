#include <cstring>
#include <vector>
#include <unordered_set>
#include <stack>
#include <cstdlib>
#include <ncurses.h>

using namespace std;

#define DEFAULT_WORD_LIST   "../words/words"

#define NO_DUPS     1
    // don't allow duplicate words

#define VERBOSE_INIT            0
    // initialization of grid
#define VERBOSE_STEP_STATE      0
    // show detailed state after each step
#define VERBOSE_NEXT_USABLE     0
    // SLOT::find_next_usable_word()
#define VERBOSE_FILL_NEXT_SLOT  0
    // GRID::fill_next_slot()
#define VERBOSE_FILL_SLOT       0
    // GRID::fill_slot()
#define VERBOSE_BACKTRACK       0
    // GRID::backtrack()

#define CHECK_ASSERTS           0
    // do sanity checks: conditions that should always hold


///////////// WORD LISTS AND PATTERNS //////////////

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
    void read(const char* word_list);
    void read_veto_file(const char* fname);
    void print_counts();
    void shuffle();
};

extern WORDS words;
extern void init_pattern_cache();

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

typedef enum {LETTER_UNKNOWN, LETTER_OK, LETTER_NOT_OK} LETTER_STATUS;

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
    int stack_level;
        // if filled, the level on the filled stack
    int dup_stack_level;
        // if we skipped a compatible word because it was already used,
        // the stack level of the slot that used it.

    int row, col;
    bool is_across;
        // for planar grids

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
    void remove_filled_word();
    int top_affecting_level();
};

struct GRID {
    vector<SLOT*> slots;
    vector<SLOT*> filled_slots;
    int npreset_slots;
        // number of preset slots.
        // these are marked as filled but not pushed on the filled stack

    SLOT* add_slot(SLOT* slot) {
        slots.push_back(slot);
        return slot;
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

    bool fill_next_slot();
    bool backtrack();
    void fill_slot(SLOT*);
    bool find_solutions(bool curses, double period);
    void restart();
    int get_commands();
};
