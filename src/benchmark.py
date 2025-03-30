# Compare the performance of algorithm variants;
# currently: none, backjump, and prune.
#
# A 'task' is the combination of a grid and a word list.
# We measure performance of a variant over lots of tasks;
# it might work well for one, but badly for others.
#
# In addition, performance depends on the random number seed
# used to shuffle word lists.
# So for a given (variant, task) pair we do runs with
# NSEEDS different seeds (currently 10).
#
# Performance is measured in terms of "steps":
# the installation of a word in a slot.
#
# MAXSTEPS is the max # of steps for a run.
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

import subprocess, json, statistics

# for a given task and variant,
# run the task NSEEDS times w/ different seeds.
# return a 'perf summary' array with
# - grid type and file
# - number of runs that succeeded
# - of the runs that succeeded, median # of steps
# - of the runs that succeeded, median CPU time
#
def do_task(var, gtype, gfile, wfile):
    cmd = [
        gtype,
        '--grid_file', gfile,
        '--word_list', wfile,
        '--max_time', '180',
        '--perf',
        '--shuffle'
    ]
    if variants[var] != '':
        cmd.append(variants[var])
    print('cmd: ', cmd)
    nsteps = []
    cpu_time = []
    nsuccess = 0
    for i in range(NSEEDS):
        out = subprocess.check_output(cmd).decode()
        print('out: ', out)
        out = json.loads(out)
        if out['success']:
            nsteps.append(out['nsteps'])
            cpu_time.append(out['cpu_time'])
    ret = {}
    ret['grid_type'] = gtype
    ret['grid_file'] = gfile
    ret['nsuccess'] = nsuccess
    if nsuccess:
        ret['med_nsteps'] = statistics.median(nsteps)
        ret['med_cpu_time'] = statistics.median(cpu_time)
    return ret

# for a given variant and grid, loop over word lists
# and return a list of the resulting perf summaries
#
def do_grid(var, gtype, gridfile):
    results = []
    for wlist in word_lists:
        results.extend(do_task(var, gtype, gridfile, wlist))
    return results

# for a given grid type, loop over grid files,
# and return a list of the resulting perf summaries
#
def do_grid_type(var, gtype):
    results = []
    if gtype == 'blacksquare':
        for grid in bs_grids:
            results.extend(do_grid(var, gtype, grid))
    elif gtype == 'bar':
        for grid in bar_grids:
            results.extend(do_grid(var, gtype, grid))
    return results

# for a given variant, loop over all grids
# and return a list of the resulting perf summaries
#
def do_variant(var):
    results = do_grid_type(var, 'black_square')
    results.extend(do_grid_type(var, 'bar'))
    return results

# print for each variant, show:
# - the fraction of tasks for which it succeeds
# - of the tasks for which all variants succeeded,
#   the fraction for which this variant had the smallest median nsteps
#
def print_summary(results):
    pass

# for each task, show the results for each variant
#
def print_details(results):
    pass

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

    print("Summary\n")
    for var in variants:
        print_summary(var_results[var])
    print("Details\n")
    for var in variants:
        print_details(var_results[var])

main()
