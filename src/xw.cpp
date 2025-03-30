// xw: fill generalized crossword puzzle grids.
// Enumerates all solutions for a given grid.
// copyright (C) 2025 David P. Anderson

#include <cstdio>
#include <cstring>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>

#include "xw.h"

const char* options = "\
options:\n\
--allow_dups        allow duplicate words\n\
--backjump          backtrack over multiple slots\n\
--curses            show partial solutions with curses\n\
--grid_file f       use the given grid file in ../grids\n\
--help              show options\n\
--max_time x        give up after x CPU seconds\n\
--perf              on 1st solution, print JSON info and exit\n\
--prune             prune compatible word lists\n\
--reverse           allow words to be reversed\n\
--show_grid         show grid details at start\n\
--shuffle           shuffle words with nondeterministic seed\n\
--solution_file f   write solutions to f (default 'solution')\n\
--step_period n     show partial solution and check CPU time every n changes\n\
--verbose           show each slot and word addition\n\
--verbose_slot      show slot selection details\n\
--verbose_word      show word selection details\n\
--verbose_prune     show pruning details\n\
--veto_file f       use given veto file (default 'vetoed_words')\n\
--word_list f       use given word list\n\
";

// This must be linked with two grid-type-specific functions:

extern void make_grid(const char* &filename, GRID&);
    // read the given file and populate the GRID structure.
    // We supply two variants:
    // black-square format (NYT type puzzles)
    // line-grid format (Atlantic cryptic type puzzles)
extern void print_grid(GRID&, bool curses, FILE *f);
    // print the (partially-filled) grid
    // if curses is true, use curses
    // else write to the given FILE*

// files
const char* grid_file = NULL;
const char* veto_fname = "vetoed_words";
const char* solution_fname = "solutions";
const char* word_list = "../words/words";

// algorithm
bool do_prune = false;
bool do_backjump = false;

// debugging output
bool verbose = false;
    // at start, show list of slots (num, across/down, row/col, len)
    // show backtrack
    // show slot push/pop
    // show word install/uninstall
    // print grid after each word install
bool verbose_word = false;
    // show each word tested
    // if no words, show check masks
bool verbose_slot = false;
    // for unfilled slots, show word count and filled_pattern
bool verbose_prune = false;
    // on backtrack, show pruning info
bool curses = false;
int step_period = 10000;
double max_time = 0;
bool perf = false;

// behavior
bool shuffle = false;
bool reverse_words = false;
bool allow_dups = false;

FILE* solution_file;

double get_cpu_time() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (double)ru.ru_utime.tv_sec + ((double)ru.ru_utime.tv_usec) / 1e6
        + (double)ru.ru_stime.tv_sec + ((double)ru.ru_stime.tv_usec) / 1e6;
}

char* date_str() {
    static char buf[256];
    time_t t = time(0);
    strcpy(buf, ctime(&t));
    buf[strlen(buf)-1] = 0;
    return buf;
}

void print_params() {
    printf("date: %s\n", date_str());
    printf("grid file: %s\n", grid_file);
    printf("word list: %s\n", word_list);
    words.print_vetoed_words();
    printf("backjump: %s\n", do_backjump?"yes":"no");
    printf("prune: %s\n", do_prune?"yes":"no");
    printf("reverse: %s\n", reverse_words?"yes":"no");
    printf("allow dups: %s\n", allow_dups?"yes":"no");
}

void print_perf_json(int nsteps, double et) {
    printf("{\n\
        \"success\": 1,\n\
        \"nsteps\": %d,\n\
        \"cpu_time\": %f\n\
}\n",
        nsteps, et
    );
}

void print_fail_json() {
    printf("{\n\
        'success': 0;\n\
}\n"
    );
}

///////////////// SLOT ///////////////////////

// we backtracked to this slot.
// 'ref by higher' positions are marked.
// Prune, from the compatible list, words that match
// the current word in these positions
//
bool SLOT::prune() {
    char prune_pattern[MAX_LEN];
    bool found = false;
    strcpy(prune_pattern, NULL_PATTERN);
    for (int i=0; i<len; i++) {
        if (ref_by_higher[i]) {
            prune_pattern[i] = current_word[i];
            found = true;
        }
    }
    prune_pattern[len] = 0;
    if (verbose_prune) {
        printf("slot %s: prune pattern %s\n", name, prune_pattern);
    }
    if (!found) return false;

    compatible_words = pattern_cache[len].get_matches_prune(
        compatible_words, next_word_index, prune_signature, prune_pattern
    );
    return true;
}

// Initialize a slot.
// get initial list of compatible words.
// If slot is preset, mark as filled
//
void SLOT::prepare_slot() {
    preset_pattern[len] = 0;
    strcpy(filled_pattern, preset_pattern);

    if (strchr(filled_pattern, '_')) {
        compatible_words = pattern_cache[len].get_matches(filled_pattern);
        filled = false;
    } else {
        compatible_words = NULL;
        strcpy(current_word, filled_pattern);
        filled = true;
    }
    sprintf(name, "%c(%d,%d)", is_across?'A':'D', row, col);
}

// debugging
void SLOT::print_usable() {
    printf("usable checked:\n");
    for (int i=0; i<len; i++) {
        for (int j=0; j<26; j++) {
            printf("%d", usable_letter_checked[i][j]);
        }
        printf("\n");
    }
    printf("usable ok:\n");
    for (int i=0; i<len; i++) {
        for (int j=0; j<26; j++) {
            printf("%d", usable_letter_ok[i][j]);
        }
        printf("\n");
    }
}

void SLOT::add_link(int this_pos, SLOT* other_slot, int other_pos) {
    LINK &link = links[this_pos];
    if (link.other_slot) {
        printf("slot %d, pos %d: already linked\n", num, this_pos);
        exit(1);
    }
    link.other_slot = other_slot;
    link.other_pos = other_pos;
}

void SLOT::print_state(bool show_links) {
    printf("slot %d:\n", num);
    printf("   row %d column %d; %s; len %d\n",
        row, col, is_across?"across":"down", len
    );
    if (filled) {
        printf("   filled; word: %s; index %d\n",
            current_word, next_word_index
        );
    } else {
        printf("   unfilled\n");
    }
    printf("   stack pattern: %s\n", filled_pattern);
    if (compatible_words) {
        printf("   %ld compat words\n",
            compatible_words->size()
        );
    } else {
        printf("   compat words is null\n");
    }
    if (show_links) {
        printf("   links\n");
        for (int i=0; i<len; i++) {
            LINK &link = links[i];
            if (link.other_slot) {
                printf("      pos %d -> slot %d pos %d\n",
                    i, link.other_slot->num, link.other_pos
                );
            }
        }
    }
}

////////////////// GRID-FILL ALGORITHMS ////////////////

// Sketch of fill algorithm:
//
// at any point we have a stack of filled slots
// each slot has
//      'filled_pattern' reflecting crossing letters:
//          filled slots: slots lower on the stack
//          unfilled slots: all filled slots
//      compatible_words: list of words compatible with filled_pattern
// filled slots have
//      current word
//      a 'next index' into compatible_words
//
// push_next_slot() picks an unfilled slot S
//      (currently, the one with fewest compatible words).
//      it scans its compatible_words list for an word that's 'usable'
//          (i.e. other unfilled slots would have compatible words)
//      if it finds one it adds that S to the filled stack,
//          sets S.current_word
//          and updates filled_pattern and compatible_words
//          of affected unfilled slots
//      otherwise we backtrack:
//          loop
//              for slot S at top of stack:
//              update filled_patterns of unfilled slots to remove
//                  letters in S.current_word unspecified in S.filled_pattern
//              look for next usable word
//              if none
//                  pop S
//              else
//                  set S.current_word
//                  update filled_patterns of unfilled slots to include new word
//                  return

// Scan compatible words for the given slot, starting from next_word_index.
// If find one that's usable (crossing words still have compat words)
//      copy it to current_word
//      update next_word_index
//      return true
//
// Efficiency trick:
// For each linked position and each possible letter (a-z) either
// - we haven't checked it yet
// - we checked and it's OK (the linked slot had compatible words)
// - we checked and it's not OK
//
// so when scanning words:
//  for each letter
//      if not checked, check it
//      if not OK, skip word
//
bool SLOT::find_next_usable_word(GRID *grid) {
    if (!compatible_words) return false;
    if (next_word_index == 0) {
        clear_usable_letter_checked();
    }
    int n = compatible_words->size();
    if (verbose_word) {
        printf("find_next_usable_word() slot %d: %d of %d\n",
            num, next_word_index, n
        );
        printf("   stack pattern %s\n", filled_pattern);
    }
    while (next_word_index < n) {
        int ind = (*compatible_words)[next_word_index++];
        char* w = words.words[len][ind];
        if (verbose_word) {
            printf("   checking %s\n", w);
        }
        bool usable = true;
        for (int i=0; i<len; i++) {
            if (links[i].empty()) continue;
            if (filled_pattern[i] != '_') continue;
            char c = w[i];
            int nc = c-'a';
            if (!usable_letter_checked[i][nc]) {
                usable_letter_checked[i][nc] = true;
                bool x = letter_compatible(i, c);
                usable_letter_ok[i][nc] = x;
#if CHECK_ASSERTS
            } else {
                bool x = letter_compatible(i, c);
                if (x != usable_letter_ok[i][nc]) {
                    printf("USABLE inconsistent flag i %d char %c x %d mw %s\n", i, c, x, w);
                    exit(1);
                }
#endif
            }
            if (!usable_letter_ok[i][nc]) {
                usable = false;
                break;
            }
        }
        if (!allow_dups) {
            for (SLOT *s2: grid->filled_slots) {
                if (!strcmp(w, s2->current_word)) {
                    usable = false;
                    dup_stack_level = s2->stack_level;
                    break;
                }
            }
        }
        if (usable) {
            if (verbose_word) {
                printf("   %s is usable for slot %d\n", w, num);
                //print_usable();
            }
            strcpy(current_word, w);
            return true;
        }
    }
    if (verbose_word) {
        printf("   no compat words are usable for slot %d\n", num);
        print_usable();
    }
    return false;
}

// see if given letter in given crossed position is compatible with xword
//
bool SLOT::letter_compatible(int pos, char c) {
    LINK &link = links[pos];
    SLOT* slot2 = link.other_slot;
    if (slot2->filled) return true;
    char pattern2[MAX_LEN];
    strcpy(pattern2, slot2->filled_pattern);
    pattern2[link.other_pos] = c;
    return slot2->check_pattern(pattern2);
}

// p differs from current filled pattern by 1 additional letter.
// see if this slot has an compatible word matching this
// (only need to check words compatible with current filled_pattern)
//
bool SLOT::check_pattern(char* p) {
    if (do_prune) {
        for (int i=0; i<len; i++) {
            LINK &link = links[i];
            if (link.empty()) continue;
            SLOT* slot2 = link.other_slot;
            if (!slot2->filled) continue;
            slot2->ref_by_higher[link.other_pos] = true;
        }
    }
    for (int i: *compatible_words) {
        if (match(len, p, words.words[len][i])) {
            return true;
        }
    }
    return false;
}

// Find the unfilled slot with fewest compatible words.
// If any of these words are usable,
// mark slot as filled, push on stack, return true
// Else return false (need to backtrack)
// precondition:
//      there are unfilled slots
//      compat lists of unfilled slots are updated and nonempty
//
bool GRID::push_next_slot() {
    // find unfilled slot with smallest compatible set
    //
    size_t nbest = 9999999;
    SLOT* best=0;
    if (verbose_slot) {
        printf("push_next_slot():\n");
    }
    for (SLOT* slot: slots) {
        if (slot->filled) continue;
        size_t n = slot->compatible_words->size();
        if (verbose_slot) {
            printf("   slot %s, %ld compatible words\n",
                slot->name, n
            );
        }
        if (n < nbest) {
            nbest = n;
            best = slot;
        }
    }
#if CHECK_ASSERTS
    if (!best) {
        printf("no unfilled slot\n");
        exit(1);
    }
#endif

    if (do_prune) {
        // set ref_by_higher in crossed filled slots
        //
        for (int i=0; i<best->len; i++) {
            best->ref_by_higher[i] = false;
            LINK &link = best->links[i];
            if (link.empty()) continue;
            SLOT *slot2 = link.other_slot;
            if (!slot2->filled) continue;
            slot2->ref_by_higher[link.other_pos] = true;
        }
    }

    best->next_word_index = 0;
    if (best->find_next_usable_word(this)) {
        if (verbose_slot) {
            printf("   slot %s has usable words\n", best->name);
        }
        best->filled = true;
#if CHECK_ASSERTS
        if (find(filled_slots.begin(), filled_slots.end(), best) != filled_slots.end()) {
            printf("slot %d is already in filled stack\n", best->num);
            exit(1);
        }
#endif
        // push slot on filled stack
        //
        best->stack_level = filled_slots.size();
        filled_slots.push_back(best);
        best->prune_signature = best->filled_pattern;
        if (verbose) {
            printf("pushing slot %s\n", best->name);
        }

        install_word(best);
        return true;
    } else {
        if (verbose) {
            printf("slot %s has no usable words\n", best->name);
        }
        return false;
    }
}

// we've found a usable word for the given slot.
// for each position where its pattern was _:
// in the linked slot, update the pattern and the compatible_words list.
// If the pattern is full, mark slot as filled and push
//
void GRID::install_word(SLOT* slot) {
    if (verbose) {
        printf("installing %s in slot %s\n", slot->current_word, slot->name);
    }
    nsteps++;
    for (int i=0; i<slot->len; i++) {
        LINK &link = slot->links[i];
        if (link.empty()) continue;
        char c = slot->filled_pattern[i];
        if (c != '_') continue;
        SLOT *slot2 = link.other_slot;
        slot2->filled_pattern[link.other_pos] = slot->current_word[i];
        if (strchr(slot2->filled_pattern, '_')) {
            slot2->compatible_words = pattern_cache[slot2->len].get_matches(
                slot2->filled_pattern
            );
            if (slot2->compatible_words->empty()) {
                printf("empty compat list for slot %d pattern %s\n",
                    slot2->num, slot2->filled_pattern
                );
                exit(1);
            }
        } else {
#if CHECK_ASSERTS
            if (find(filled_slots.begin(), filled_slots.end(), slot2) != filled_slots.end()) {
                printf("slot %d is already in filled stack\n", slot2->num);
                exit(1);
            }
#endif
            // other slot is now filled
            if (verbose) {
                printf("slot %s is now also filled: %s\n",
                    slot2->name, slot2->filled_pattern
                );
            }
            slot2->compatible_words = NULL;
            slot2->filled = true;
            strcpy(slot2->current_word, slot2->filled_pattern);
            slot2->stack_level = filled_slots.size();
            filled_slots.push_back(slot2);
        }
    }
    if (verbose) {
        print_grid(*this, false, stdout);
    }
}

// We just popped this slot S from the stack.
// Find the level of the topmost filled slot that affects it, i.e.
// - had a dup word conflict
// - intersects S
// - intersects an unfilled slot that intersects S
//
// This is used for "backjumping": if we couldn't find a word for this slot,
// we want to backtrack all the way to a slot that will make a difference
//
int SLOT::top_affecting_level() {
    int max_level=-1;
    if (dup_stack_level >= 0) {
        max_level = dup_stack_level;
        if (max_level == stack_level-1) return max_level;
    }
    for (int i=0; i<len; i++) {
        LINK &link = links[i];
        if (link.empty()) continue;
        SLOT* slot2 = link.other_slot;
        if (slot2->filled) {
            if (slot2->stack_level > max_level) {
                max_level = slot2->stack_level;
                if (max_level == stack_level-1) return max_level;
            }
        } else {
            for (int j=0; j<slot2->len; j++) {
                LINK &link2 = slot2->links[j];
                if (link2.empty()) continue;
                SLOT* slot3 = link2.other_slot;
                if (slot3->filled) {
                    if (slot3->stack_level > max_level) {
                        max_level = slot3->stack_level;
                        if (max_level == stack_level-1) return max_level;
                    }
                }
            }
        }
    }
    return max_level;
}

// Remove a filled word.
// Update filled_patterns of unfilled crossing slots
//
void SLOT::uninstall_word() {
    if (verbose) {
        printf("uninstalling %s from slot %s\n", current_word, name);
    }
    for (int i=0; i<len; i++) {
        LINK &link = links[i];
        if (link.empty()) continue;
        SLOT* slot2 = link.other_slot;
        if (slot2->filled) continue;
        slot2->filled_pattern[link.other_pos] = '_';

        // update compatible word lists of crossing slots.
        // push_next_slot() assumes that these are up to date
        //
        slot2->compatible_words = pattern_cache[slot2->len].get_matches(
            slot2->filled_pattern
        );
        if (slot2->compatible_words->empty()) {
            // should never get here
            printf("   empty compat list for slot %d pattern %s\n",
                slot2->num, slot2->filled_pattern
            );
            exit(1);
        }
    }
}

// Remove current word for the slot S on top of the filled stack,
// and update compat word lists for crossing slots
// Look for next usable word for S.
// if find one: add it, update crossing slots, and return true
// else pop S and repeat for next slot down on stack
//
bool GRID::backtrack() {
    while (1) {
        SLOT *slot = filled_slots.back();
        if (verbose) {
            printf("backtracking to slot %d\n", slot->num);
        }

        slot->uninstall_word();
        if (!slot->compatible_words) {
            goto pop;
        }

        if (do_prune) {
            if (!slot->prune()) {
                if (verbose) {
                    printf("popping slot %s because no crossings from higher slots\n",
                        slot->name
                    );
                }
                goto pop;
            }
        }

        if (slot->find_next_usable_word(this)) {
            install_word(slot);
            return true;
        }

        if (verbose) {
            printf("popping slot %s: no more usable words\n", slot->name);
        }
pop:
        filled_slots.pop_back();
        slot->filled = false;
        if (filled_slots.empty()) {
            return false;
        }
        if (do_backjump) {
            int level = slot->top_affecting_level();
            if (verbose) {
                printf("backjumping to level %d\n", level);
            }
            while (filled_slots.size() > level+1) {
                slot = filled_slots.back();
                slot->uninstall_word();
                slot->filled = false;
                filled_slots.pop_back();
                if (verbose) {
                    printf("popping slot %s: backjump\n", slot->name);
                }
            }
        }
    }
}

// returns from get_commands()
#define CONT    1
#define RESTART 2
#define EXIT    3

int GRID::get_commands() {
    int retval = CONT;
    while (1) {
        printf("enter command\n"
            "s: append solution to file (default 'solutions')\n"
            "<CR>: next solution\n"
            "v word: add word to veto list\n> "
            "r: restart with new random word order\n> "
            "q: quit\n> "
        );
        char buf[256];
        fgets(buf, sizeof(buf), stdin);
        int len = strlen(buf);
        if (len == 1) {
            return retval;
        }
        buf[len-1] = 0;
        if (!strcmp(buf, "r")) {
            return RESTART;
        } else if (!strcmp(buf, "q")) {
            return EXIT;
        } else if (!strcmp(buf, "s")) {
            print_grid(*this, false, solution_file);
            fflush(solution_file);
        } else if (strstr(buf, "v ")==buf) {
            FILE *f = fopen(veto_fname, "a");
            fprintf(f, "%s\n", buf+2);
            fclose(f);
            words.read_veto_file(veto_fname);
            words.read(word_list, reverse_words);
            retval = RESTART;
        } else {
            printf("bad command %s\n", buf);
        }
    }
}

bool GRID::find_solutions() {
    double start_cpu_time = get_cpu_time();
    if (verbose) {
        print_state();
    }
    while (1) {
        if (filled_slots.size() + npreset_slots == slots.size()) {
            // we have a solution
            if (curses) {
                clear();
                refresh();
                endwin();
            }
            double now = get_cpu_time();
            double etime = now - start_cpu_time;
            if (perf) {
                print_perf_json(nsteps, now);
                exit(0);
            }
            printf("\nSolution found:\n");
            print_grid(*this, false, stdout);
            printf("CPU time: %f\n", get_cpu_time() - start_cpu_time);
            printf("Steps: %d\n", nsteps);
            if (verbose) {
                exit(0);
            }
            switch (get_commands()) {
            case CONT:
                backtrack();
                break;
            case RESTART:
                restart();
                nsteps = 0;
                start_cpu_time = now;
                break;
            case EXIT:
                exit(0);
            }
            if (curses) {
                initscr();
            }
            continue;
        }
        if (!push_next_slot()) {
            if (!backtrack()) {
                break;
            }
        }
        if (!(nsteps % step_period)) {
            if (max_time) {
                double et = get_cpu_time() - start_cpu_time;
                if (et > max_time) {
                    if (perf) {
                        print_fail_json();
                    } else {
                        printf("max CPU time exceeded\n");
                    }
                    exit(0);
                }
            }
            if (!verbose && !perf) {
                print_grid(*this, curses, stdout);
            }
        }
    }
    printf("no more solutions\n");
}

void GRID::restart() {
    for (SLOT* slot: slots) {
        strcpy(slot->filled_pattern, slot->preset_pattern);
    }
    words.shuffle();
    init_pattern_cache();
    filled_slots.clear();
    prepare_grid();
}

int main(int argc, char** argv) {
    GRID grid;
    bool show_grid = false;
    bool help = false;

    for (int i=1; i<argc; i++) {
        if (!strcmp(argv[i], "--allow_dups")) {
            allow_dups = true;
        } else if (!strcmp(argv[i], "--backjump")) {
            do_backjump = true;
        } else if (!strcmp(argv[i], "--curses")) {
            curses = true;
        } else if (!strcmp(argv[i], "--grid_file")) {
            grid_file = argv[++i];
        } else if (!strcmp(argv[i], "--help")) {
            help = true;
        } else if (!strcmp(argv[i], "--max_time")) {
            max_time = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--perf")) {
            perf = true;
        } else if (!strcmp(argv[i], "--prune")) {
            do_prune = true;
        } else if (!strcmp(argv[i], "--reverse")) {
            reverse_words = true;
        } else if (!strcmp(argv[i], "--show_grid")) {
            show_grid = true;
        } else if (!strcmp(argv[i], "--shuffle")) {
            shuffle = true;
        } else if (!strcmp(argv[i], "--solution_file")) {
            solution_fname = argv[++i];
        } else if (!strcmp(argv[i], "--step_period")) {
            step_period = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--verbose")) {
            verbose = true;
        } else if (!strcmp(argv[i], "--verbose_slot")) {
            verbose_slot = true;
        } else if (!strcmp(argv[i], "--verbose_word")) {
            verbose_word = true;
        } else if (!strcmp(argv[i], "--verbose_prune")) {
            verbose_prune = true;
        } else if (!strcmp(argv[i], "--veto_file")) {
            veto_fname = argv[++i];
        } else if (!strcmp(argv[i], "--word_list")) {
            word_list = argv[++i];
        } else {
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            exit(1);
        }
    }
    if (help) {
        printf("%s", options);
        exit(0);
    }
    solution_file = fopen(solution_fname, "wa");
    words.read_veto_file(veto_fname);
    words.read(word_list, reverse_words);
    if (shuffle) {
        std::srand(time(0)+getpid());
        words.shuffle();
    }
    init_pattern_cache();
    if (grid_file) {
        char buf[256];
        sprintf(buf, "../grids/%s", grid_file);
        grid_file = buf;
    }
    make_grid(grid_file, grid);
    grid.prepare_grid();
    if (show_grid) {
        grid.print_state(true);
        exit(0);
    }
    if (verbose) {
        print_params();
    }
    if (curses) {
        initscr();
    }
    grid.find_solutions();
    if (curses) {
        endwin();
    }
}
