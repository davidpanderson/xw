# Compare the performance of algorithm variants;
# currently: none, backjump, and prune.
#
# A 'task' is the combination of a grid and a word list.
# We measure performance over lots of tasks;
# an algorithm might work well for one, but badly for others.
#
# In addition, performance depends on the random number seed
# used to shuffle word lists.
# So for a given (variant, task) pair we do runs with
# NSEEDS different seeds (currently 10).
#
# Performance is measured in terms of "steps":
# the installation of a word in a slot.
#
# MAXTIME is the max CPU time for a run.
# If no solution is found by then, the run fails.

# input files:
# bs_grids.txt: a list of black-square grid filenames
# bar_grids.txt: a list of bar grid filenames
# word_lists.txt: a list of word list filenames

MAXTIME = 180
NSEEDS = 10

variants = {
    'default': '',
    'prune': '--prune',
    'backjump': '--backjump'
}
bs_grids = []
bar_grids = []
word_lists = []

def do_task(var, gtype, gfile, wfile):
    cmd = f'{gtype} {var} --grid_file {gfile} --word_list {wfile} --max_time 180'
    print(cmd)
    return [0]

def do_grid(var, gtype, gridfile):
    results = []
    for wlist in word_lists:
        results.extend(do_task(var, gtype, gridfile, wlist))
    return results

def do_grid_type(var, gtype):
    results = []
    if gtype == 'blacksquare':
        for grid in bs_grids:
            results.extend(do_grid(var, gtype, grid))
    elif gtype == 'bar':
        for grid in bar_grids:
            results.extend(do_grid(var, gtype, grid))
    return results

# return a list (one per task) of result records
# (nsuccess, nfail, median_nsteps)
#
def do_variant(var):
    results = do_grid_type(var, 'blacksquare')
    results.extend(do_grid_type(var, 'bar'))
    return results

def main():
    with open('bs_grids.txt') as f:
        for line in f:
            bs_grids.append(line.strip())
    with open('bar_grids.txt') as f:
        for line in f:
            bar_grids.append(line.strip())
    with open('word_lists.txt') as f:
        for line in f:
            word_lists.append(line.strip())

    var_results = {}
    for var in variants:
        results = do_variant(var)
        print(var, results)
        var_results[var] = results


main()
