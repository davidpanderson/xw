#include <cstdio>
#include <ctype.h>
#include <stdlib.h>
#include "xw.h"

#define MAX_SIZE 21
    // NYT sunday is 21x21

// read a file of the following form and make it into a GRID
//
//  -------------------------
//  |. . . .|. . . . . . . .|
//     -   -   -       -
//  |.|.|. . . . . . . . . .|
//         -   -       -
//  |. . . . . .|. . . . . .|
// etc.
//
// | is a vertical bar and - is a horizontal bar
// . represents a cell; can also be letter (preset)
//
//

char file_chars[MAX_SIZE*2+1][MAX_SIZE*2+1];
    // chars from the grid file (with barriers)
int file_nrows=0, file_ncols=0;   // file chars, not grid

char chars[MAX_SIZE][MAX_SIZE];
    // chars without barriers
bool bar_right[MAX_SIZE][MAX_SIZE];
bool bar_left[MAX_SIZE][MAX_SIZE];
bool bar_above[MAX_SIZE][MAX_SIZE];
bool bar_below[MAX_SIZE][MAX_SIZE];

// for each cell, the across and down slots if any
SLOT *across_slots[MAX_SIZE][MAX_SIZE];
SLOT *down_slots[MAX_SIZE][MAX_SIZE];

// and the position in that slot
int across_pos[MAX_SIZE][MAX_SIZE];
int down_pos[MAX_SIZE][MAX_SIZE];

vector<SLOT*> across_slots_list;
vector<SLOT*> down_slots_list;

bool wrap[2];   // [wrap columns?, wrap rows?]
bool twist[2];  // whether to twist when wrap
int size[2];    // [#cols, #rows]

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

// read the file; populate chars and bar_* arrays
//
void read_grid_file(FILE* f, GRID& grid) {
    char buf[256];
    bool mirror = false;

    // read file into file_chars array
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
        if (file_ncols) {
            if (nc != file_ncols) {
                fprintf(stderr, "size mismatch in %s\n", buf);
                exit(1);
            }
        } else {
            file_ncols = nc;
        }
        strncpy(file_chars[file_nrows], buf, file_ncols);
        file_nrows++;
    }

    if (mirror) {
        for (int i=0; i<file_nrows-1; i++) {
            for (int j=0; j<file_ncols; j++) {
                file_chars[file_nrows+i][j] = file_chars[file_nrows-i-2][file_ncols-j-1];
            }
        }
        file_nrows += (file_nrows-1);
    }

    if (file_nrows%2 == 0) {
        fprintf(stderr, "nrows must be odd\n");
        exit(1);
    }
    if (file_ncols%2 == 0) {
        fprintf(stderr, "ncols must be odd\n");
        exit(1);
    }
    size[0] = file_nrows/2;
    size[1] = file_ncols/2;

    for (int i=0; i<size[0]; i++) {
        for (int j=0; j<size[1]; j++) {
            char c = file_chars[i*2+1][j*2+1];
            if (islower(c)) {
                chars[i][j] = c;
            } else {
                chars[i][j] = ' ';
            }
            c = file_chars[i*2+1][j*2+2];
            bar_right[i][j] = (c == '|');
            c = file_chars[i*2+1][j*2];
            bar_left[i][j] = (c == '|');
            c = file_chars[i*2][j*2+1];
            bar_above[i][j] = (c == '-');
            c = file_chars[i*2+2][j*2+1];
            bar_below[i][j] = (c == '-');
        }
    }
}

// chars and bar_* have been populated.
//
void find_slots(GRID &grid) {
    // loop over rows; make across slots
    //
    for (int row=0; row<size[0]; row++) {
        int col = 0;
        SLOT *slot = NULL;
        bool wrapped = false;
        while (1) {
            if (slot && !bar_left[row][col]) {
                // slot continues into this cell
                across_slots[row][col] = slot;
                across_pos[row][col] = slot->len;
                slot->len += 1;
                if (bar_right[row][col]) {
                    slot = NULL;
                }
            } else {
                if (!bar_right[row][col]) {
                    slot = new SLOT(1);
                    slot->row = row;
                    slot->col = col;
                    slot->is_across = true;
                    across_slots[row][col] = slot;
                    across_pos[row][col] = 0;
                    across_slots_list.push_back(slot);
                }
            }
            if (col == size[1]-1) {
                if (slot && wrap[0] && !bar_right[row][col]) {
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
            if (slot && !bar_above[row][col]) {
                down_slots[row][col] = slot;
                down_pos[row][col] = slot->len;
                slot->len += 1;
                int c[2]={row, col};
                if (bar_below[row][col]) {
                    slot = NULL;
                }
            } else {
                if (!bar_below[row][col]) {
                    slot = new SLOT(1);
                    slot->row = row;
                    slot->col = col;
                    slot->is_across = false;
                    down_slots[row][col] = slot;
                    down_pos[row][col] = 0;
                    down_slots_list.push_back(slot);
                }
            }
            if (row == size[0]-1) {
                if (slot && wrap[1] && !bar_below[row][col]) {
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
            SLOT *aslot = across_slots[i][j];
            SLOT *dslot = down_slots[i][j];
            int apos = across_pos[i][j];
            int dpos = down_pos[i][j];
            if (c == ' ') {
                if (aslot && dslot) {
                    aslot->add_link(apos, dslot, dpos);
                    dslot->add_link(dpos, aslot, apos);
                }
            } else {
                if (aslot) aslot->preset_char(apos, c);
                if (dslot) dslot->preset_char(dpos, c);
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

// There can be unchecked squares, so to print the grid it doesn't
// suffice to print just the across entries
//
void print_grid(GRID &grid, bool curses) {
    char c;
    for (int i=0; i<size[0]; i++) {
        for (int j=0; j<size[1]; j++) {
            SLOT *slot = across_slots[i][j];
            int pos = across_pos[i][j];
            if (!slot) {
                slot = down_slots[i][j];
                if (!slot) {
                    printf("no slot at %d %d\n", i, j);
                    exit(1);
                }
                pos = down_pos[i][j];
            }
            if (slot->filled) {
                c = slot->current_word[pos];
            } else {
                c = slot->filled_pattern[pos];
            }
            file_chars[i*2+1][j*2+1] = c;
        }
    }
    if (curses) {
        for (int i=0; i<file_nrows; i++) {
            move(i, 0);
            printw("%s", &(file_chars[i][0]));
        }
    } else {
        for (int i=0; i<file_nrows; i++) {
            printf("%s\n", &(file_chars[i][0]));
        }
    }
}

int make_grid(const char* fname, GRID &grid) {
    if (!fname) {
        fprintf(stderr, "no grid file specified\n");
        exit(1);
    }
    FILE *f = fopen(fname, "r");
    if (!f) {
        fprintf(stderr, "can't open %s\n", fname);
        exit(1);
    }
    read_grid_file(f, grid); 
    find_slots(grid);
}
