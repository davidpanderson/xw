# Compare the performance of algorithm variants;
# none, backjump, and prune.
#
# A 'task' is the combination of
# - a variant
# - a grid (type and file)
# - a word list
#
# We measure the performance of a variant over multiple tasks;
# it might work well for one, but badly for others.
#
# Performance depends on the random number seed
# used to shuffle word lists.
# So for a given task pair we do runs with
# NSEEDS different seeds (currently 10).
#
# Performance is measured in terms of "steps":
# the installation of a word in a slot.
#
# MAX_TIME_PER_CELL is the max # of CPU seconds per cell for a run.
# If no solution is found by then, the run fails.

# input files:
# bs_grids.txt: a list of black-square grid filenames
# bar_grids.txt: a list of bar grid filenames
# word_lists.txt: a list of word list filenames

MAX_TIME_PER_CELL = 1.
NSEEDS = 10

variant_names = [
    'default',
    'prune',
    'backjump'
]
variant_args = [
    '',
    '--prune',
    '--backjump'
]
nvariants = 3

bs_grids = []
bar_grids = []
word_lists = []

import subprocess, json, statistics

class TASK_RESULT:
    def __init__(self, var, gtype, gfile, wfile):
        self.var = var
        self.gtype = gtype
        self.gfile = gfile
        self.wfile = wfile
        self.nsuccess = 0
        self.median_nsteps = 0
        self.median_cpu_time = 0

    def __str__(self):
        return 'grid type: %s\ngrid file: %s\nword file: %s\nsuccess: %d\nmedian_nsteps: %f\nmedian_cpu_time: %f\n'%(
            self.gtype, self.gfile, self.wfile,
            self.nsuccess, self.median_nsteps, self.median_cpu_time
        )

# for a given task and variant,
# run the task NSEEDS times w/ different seeds.
# Compute the stats, and return a TASK_RESULT
#
def do_task(var: int, gtype: str, gfile: str, wfile: str) -> TASK_RESULT:
    cmd = [
        gtype,
        '--grid_file', gfile,
        '--word_list', wfile,
        '--max_time', '180',
        '--perf',
        '--shuffle'
    ]
    if variant_args[var] != '':
        cmd.append(variant_args[var])
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
            nsuccess += 1
    ret = TASK_RESULT(var, gtype, gfile, wfile)
    ret.nsuccess = nsuccess
    if nsuccess:
        ret.median_nsteps = statistics.median(nsteps)
        ret.median_cpu_time = statistics.median(cpu_time)
    return ret

# for a given variant and grid, loop over word lists
# and return a list of the results
#
def do_grid(var: int, gtype: str, gridfile: str) -> list[TASK_RESULT]:
    results = []
    for wlist in word_lists:
        results.append(do_task(var, gtype, gridfile, wlist))
    return results

# for a given grid type, loop over grid files,
# and return a list of the resulting perf summaries
#
def do_grid_type(var: int, gtype: str) -> list[TASK_RESULT]:
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
def do_variant(var: int) -> list[TASK_RESULT]:
    results = do_grid_type(var, 'black_square')
    results.extend(do_grid_type(var, 'bar'))
    return results

# show a comparison of the variants
# 'var_results' is a list of the list of TASK_RESULTS for the 3 variants
# These lists are in the same order.
# For each task, compute
# - the 'success rank' of each variant
#   (1 if it had the most or was tied, etc.)
# - the 'median nsteps' rank of each variant
#   (similar; should this just be a tie-breaker for success rank?)
#
# and for each variant print
# - the sum of its success ranks
# - the sum of its nsteps rank
#
def print_comparison(var_results: list[list[TASK_RESULT]]):
    ntasks = len(var_results[0])
    success_sum = [0]*nvariants
    nsteps_sum = [0]*nvariants
    success_val = [0]*nvariants
    nsteps_val = [0]*nvariants
    for i in range(ntasks):
        for j in range(nvariants):
            success_val[j] = var_results[j][i].nsuccess
            nsteps_val[j] = var_results[j][i].median_nsteps
        success_ranks = rank(success_val)
        nsteps_ranks = rank(nsteps_val)
        for j in range(nvariants):
            success_sum[j] += success_ranks[j]
            nsteps_sum[j] += nsteps_ranks[j]
    for j in range(nvariants):
        print('%s: success rank %d, nsteps rank %d'%(
            variant_names[j], success_sum[j], nsteps_sum[j]
        ))

# given a list of numbers, return a list of their ranks
# (lowest to highest)
def rank(vals: list[float]):
    sv = vals.copy()
    sv.sort(reverse=True)
    print(sv)
    out = []
    for v in vals:
        out.append(sv.index(v))
    return out

# show the # TASK_RESULTS for the given variant.
#
def print_var_details(results: list[TASK_RESULT]):
    for res in results:
        print(res)

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
    for var in range(nvariants):
        results = do_variant(var)
        print(var, results)
        var_results[var] = results

    print("Details\n")
    for var in range(nvariants):
        print('Variant: ', variant_names[var])
        print_var_details(var_results[var])

    print("Comparison\n")
    print_comparison(var_results)

main()
