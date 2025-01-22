#include <cstdio>
#include <stdlib.h>

#include "xw.h"

// fill a black-square grid, described by a 'grid file'
// usage:
// black_square [options] filename
//
// options:
// --mirror
//      2n+1 rows are given; append the 180 deg rotation of the first 2n
// --wrap_row, --wrap_col
//      words wrap around in the x or y directions
// --twist_row, --twist_col
//      if wrap, apply a twist (e.g. Klein bottle)

#define MAX_SIZE 22
    // NYT sunday is 21x21

// grid file format:
//
//  **...........**
//  *.............*
//  ...............
//  table***...*...
// etc.
//
// * represents a black square.
// other cells can have
// - an ASCII lower-case char (can hard-code entries)
// - . or space (blank cell)
//
// By convention there are no unchecked squares,
// so to print a grid you can just print the acrosses.
//
// Lots of sample grids: https://crosswordgrids.com/

bool mirror;
bool wrap[2];   // [wrap columns?, wrap rows?]
bool twist[2];  // whether to twist when wrap
int size[2];    // [#cols, #rows]

// file contents
// first coord is row, 2nd is col
char chars[MAX_SIZE][MAX_SIZE];

// for each cell, the across and down slots if any
SLOT *across_slots[MAX_SIZE][MAX_SIZE];
SLOT *down_slots[MAX_SIZE][MAX_SIZE];

// and the position in that slot
int across_pos[MAX_SIZE][MAX_SIZE];
int down_pos[MAX_SIZE][MAX_SIZE];

vector<SLOT*> across_slots_list;
vector<SLOT*> down_slots_list;

// return coords of previous cell in that row (coord=0) or column (1)
// (taking into account wrapping and/or twisting in that direction)
//
void prev(int (&c)[2], int coord, int (&d)[2]) {
    d[0] = c[0];
    d[1] = c[1];
    int coord2 = 1 - coord;
    d[coord2] -= 1;
    if (d[coord2] < 0) {
        d[coord2] = size[coord2] - 1;
        if (twist[coord]) {
            d[coord] = size[coord] - c[coord] - 1;
        }
    }
}

void next(int (&c)[2], int coord, int (&d)[2]) {
    d[0] = c[0];
    d[1] = c[1];
    int coord2 = 1 - coord;
    d[coord2] += 1;
    if (d[coord2] == size[coord2]) {
        d[coord2] = 0;
        if (twist[coord]) {
            d[coord] = size[coord] - c[coord] - 1;
        }
    }
}

#if 0
int main(int argc, char** argv) {
    int c[2], d[2];
    size[0] = 3;
    size[1] = 5;
    twist[0] = true;
    while(1) {
        scanf("%d %d", &c[0], &c[1]);
        prev(c, 0, d);
        printf("prev 0: %d %d\n", d[0], d[1]);
        next(c, 0, d);
        printf("next 0: %d %d\n", d[0], d[1]);
        prev(c, 1, d);
        printf("prev 1: %d %d\n", d[0], d[1]);
        next(c, 1, d);
        printf("next 1: %d %d\n", d[0], d[1]);

    }
}

void print_grid(GRID &grid, bool) {
}
#endif

bool is_prev_black(int (&c)[2], int coord) {
    int d[2];
    int coord2 = 1 - coord;
    if (c[coord2] == 0) {
        if (wrap[coord]) {
            prev(c, coord, d);
            return chars[d[0]][d[1]] == '*';
        } else {
            return true;
        }
    } else {
        d[0] = c[0];
        d[1] = c[1];
        d[coord2] -= 1;
        return chars[d[0]][d[1]] == '*';
    }
}
bool is_next_black(int (&c)[2], int coord) {
    int d[2];
    int coord2 = 1 - coord;
    if (c[coord2] == size[coord2]) {
        if (wrap[coord]) {
            next(c, coord, d);
            return chars[d[0]][d[1]] == '*';
        } else {
            return true;
        }
    } else {
        d[0] = c[0];
        d[1] = c[1];
        d[coord2] += 1;
        return chars[d[0]][d[1]] == '*';
    }
}

// read file into chars array
//
void read_grid_file(FILE *f) {
    int i, j, k, start, len;
    char buf[256];
    int nrows=0, ncols=0;

    // read file into chars array
    // check for flags
    //
    while (fgets(buf, sizeof(buf), f)) {
        if (!strcmp(buf, "mirror\n")) {
            mirror = true;
            continue;
        } else if (!strcmp(buf, "wrap_row\n")) {
            wrap[0] = true;
            continue;
        } else if (!strcmp(buf, "wrap_col\n")) {
            wrap[1] = true;
            continue;
        } else if (!strcmp(buf, "twist_row\n")) {
            twist[0] = true;
            continue;
        } else if (!strcmp(buf, "twist_col\n")) {
            twist[1] = true;
            continue;
        }
        int nc = strlen(buf)-1;
        if (ncols) {
            if (nc != ncols) {
                fprintf(stderr, "size mismatch\n");
                exit(1);
            }
        } else {
            ncols = nc;
        }
        strncpy(chars[nrows], buf, ncols);
        nrows++;
    }

    if (mirror) {
        for (i=0; i<nrows-1; i++) {
            for (j=0; j<ncols; j++) {
                chars[nrows+i][j] = chars[nrows-i-2][ncols-j-1];
            }
        }
        nrows += (nrows-1);
    }

    size[0] = nrows;
    size[1] = ncols;
}

// scan chars array in both row and col directions, finding and linking slots
//
void find_slots(GRID &grid) {

    // loop over rows; make across slots
    //
    for (int row=0; row<size[0]; row++) {
        int col = 0;
        SLOT *slot = NULL;
        bool wrapped = false;
        while (1) {
            if (chars[row][col] == '*') {
                if (slot) {
                    slot = NULL;
                }
                if (wrapped) break;
            } else {
                // non-black
                if (slot) {
                    across_slots[row][col] = slot;
                    across_pos[row][col] = slot->len;
                    slot->len += 1;
                    int c[2]={row, col};
                    if (is_next_black(c, 0)) {
                        slot = NULL;
                    }
                } else {
                    int c[2]={row, col};
                    if (is_prev_black(c, 0)) {
                        slot = new SLOT(1);
                        slot->row = row;
                        slot->col = col;
                        slot->is_across = true;
                        across_slots[row][col] = slot;
                        across_pos[row][col] = 0;
                        across_slots_list.push_back(slot);
                    }
                }
            }
            if (col == size[1]-1) {
                if (slot && wrap[0]) {
                    int c[2]={row, col}, d[2];
                    next(c, 0, d);
                    row = d[0];
                    col = d[1];
                    wrapped = true;
                } else {
                    break;
                }
            } else {
                col++;
            }
        }
    }

    // make down slots
    //
    for (int col=0; col<size[1]; col++) {
        int row = 0;
        SLOT *slot = NULL;
        bool wrapped = false;
        while (1) {
            if (chars[row][col] == '*') {
                if (slot) {
                    slot = NULL;
                }
                if (wrapped) break;
            } else {
                // non-black
                if (slot) {
                    down_slots[row][col] = slot;
                    down_pos[row][col] = slot->len;
                    slot->len += 1;
                    int c[2]={row, col};
                    if (is_next_black(c, 1)) {
                        slot = NULL;
                    }
                } else {
                    int c[2]={row, col};
                    if (is_prev_black(c, 1)) {
                        slot = new SLOT(1);
                        slot->row = row;
                        slot->col = col;
                        slot->is_across = false;
                        down_slots[row][col] = slot;
                        down_pos[row][col] = 0;
                        down_slots_list.push_back(slot);
                    }
                }
            }
            if (row == size[0]-1) {
                if (slot && wrap[1]) {
                    int c[2]={row, col}, d[2];
                    next(c, 1, d);
                    row = d[0];
                    col = d[1];
                    wrapped = true;
                } else {
                    break;
                }
            } else {
                row++;
            }
        }
    }

    // link slots and add preset chars
    //
    for (int i=0; i<size[0]; i++) {
        for (int j=0; j<size[1]; j++) {
            char c = chars[i][j];
            if (c == '*') {
                continue;
            }
            SLOT *aslot = across_slots[i][j];
            SLOT *dslot = down_slots[i][j];
            if (!aslot || !dslot) {
                fprintf(stderr, "unchecked cell at %d %d\n", i, j);
                exit(1);
            }
            int apos = across_pos[i][j];
            int dpos = down_pos[i][j];
            if (c == '.') {
                aslot->add_link(apos, dslot, dpos);
                dslot->add_link(dpos, aslot, apos);
            } else {
                aslot->preset_char(apos, c);
                dslot->preset_char(dpos, c);
            }
        }
    }

    // add slots to grid
    //
    for (SLOT *slot: across_slots_list) {
        grid.add_slot(slot);
    }
    for (SLOT *slot: down_slots_list) {
        grid.add_slot(slot);
    }
}

void print_grid(GRID &grid, bool curses) {
    char chars[MAX_SIZE][MAX_SIZE*2];
    int i, j;
    for (i=0; i<size[0]; i++) {
        for (j=0; j<size[1]; j++) {
            SLOT *slot = across_slots[i][j];
            if (slot) {
                int pos = across_pos[i][j];
                if (slot->filled) {
                    chars[i][j*2] = slot->current_word[pos];
                } else {
                    chars[i][j*2] = slot->filled_pattern[pos];
                }
            } else {
                chars[i][j*2] = '*';
            }
            chars[i][j*2+1] = ' ';
        }
        chars[i][size[1]*2] = 0;
    }
    if (curses) {
        for (i=0; i<size[0]; i++) {
            move(i, 0);
            printw("%s", &(chars[i][0]));
        }
        refresh();
    } else {
        for (i=0; i<size[0]; i++) {
            printf("%s\n", &(chars[i][0]));
        }
    }
}

void make_grid(const char* fname, GRID &grid) {
    if (!fname) fname = "bs_11_1";
    FILE *f = fopen(fname, "r");
    if (!f) {
        printf("no grid file %s\n", fname);
        exit(1);
    }
    read_grid_file(f);
    find_slots(grid);
}
