#!/usr/bin/python
# @lint-avoid-python-3-compatibility-imports
#
# runqlat   Run queue (scheduler) latency as a histogram.
#           For Linux, uses BCC, eBPF.
#
# USAGE: runqlat [-h] [-T] [-m] [-P] [-L] [-p PID] [interval] [count]
#
# This measures the time a task spends waiting on a run queue for a turn
# on-CPU, and shows this time as a histogram. This time should be small, but a
# task may need to wait its turn due to CPU load.
#
# This measures two types of run queue latency:
# 1. The time from a task being enqueued on a run queue to its context switch
#    and execution. This traces enqueue_task_*() -> finish_task_switch(),
#    and instruments the run queue latency after a voluntary context switch.
# 2. The time from when a task was involuntary context switched and still
#    in the runnable state, to when it next executed. This is instrumented
#    from finish_task_switch() alone.
#
# Copyright 2016 Netflix, Inc.
# Licensed under the Apache License, Version 2.0 (the "License")
#
# 07-Feb-2016   Brendan Gregg   Created this.

from __future__ import print_function
from bcc import BPF
from time import sleep, strftime
import argparse

def stack_id_err(stack_id):
    # -EFAULT in get_stackid normally means the stack-trace is not availible,
    # Such as getting kernel stack trace in userspace code
    return (stack_id < 0) and (stack_id != -errno.EFAULT)

# arguments
examples = """examples:
    ./runqlat            # summarize run queue latency as a histogram
    ./runqlat 1 10       # print 1 second summaries, 10 times
    ./runqlat -mT 1      # 1s summaries, milliseconds, and timestamps
    ./runqlat -P         # show each PID separately
    ./runqlat -p 185     # trace PID 185 only
"""
parser = argparse.ArgumentParser(
    description="Summarize run queue (scheduler) latency as a histogram",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-T", "--timestamp", action="store_true",
    help="include timestamp on output")
parser.add_argument("-m", "--milliseconds", action="store_true",
    help="millisecond histogram")
parser.add_argument("-P", "--pids", action="store_true",
    help="print a histogram per process ID")
parser.add_argument("-S", "--throttle", default=0, 
    help="Throttling for printing stack trace")
# PID options are --pid and --pids, so namespaces should be --pidns (not done
# yet) and --pidnss:
parser.add_argument("--pidnss", action="store_true",
    help="print a histogram per PID namespace")
parser.add_argument("-L", "--tids", action="store_true",
    help="print a histogram per thread ID")
parser.add_argument("-p", "--pid",
    help="trace this PID only")
parser.add_argument("interval", nargs="?", default=99999999,
    help="output interval, in seconds")
parser.add_argument("count", nargs="?", default=99999999,
    help="number of outputs")
args = parser.parse_args()
countdown = int(args.count)
debug = 0

# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>

typedef struct pid_key {
    u64 id;    // work around
    u64 slot;
} pid_key_t;

typedef struct pidns_key {
    u64 id;    // work around
    u64 slot;
} pidns_key_t;


BPF_STACK_TRACE(stack_trace, 1024);
BPF_HASH(trace_hash, u64, int);

BPF_HASH(start, u32);
STORAGE

struct rq;

// record wakeup timestamp
int trace_wakeup(struct pt_regs *ctx, struct rq *rq, struct task_struct *p, int wake_flags)
{
    u32 tgid = p->tgid;
    u32 pid = p->pid;
    if (FILTER)
        return 0;
    u64 ts = bpf_ktime_get_ns();
    start.update(&pid, &ts);

    int kstack_id = stack_trace.get_stackid(ctx, BPF_F_USER_STACK);      
    trace_hash.update(&ts, &kstack_id);
    
    return 0;
}

// calculate latency
int trace_run(struct pt_regs *ctx, struct task_struct *prev)
{
    u32 pid, tgid;

    // ivcsw: treat like an enqueue event and store timestamp
    if (prev->state == TASK_RUNNING) {
        tgid = prev->tgid;
        pid = prev->pid;
        if (!(FILTER)) {
            u64 ts = bpf_ktime_get_ns();
            start.update(&pid, &ts);
        }
    }

    tgid = bpf_get_current_pid_tgid() >> 32;
    pid = bpf_get_current_pid_tgid();
    if (FILTER)
        return 0;
    u64 *tsp, delta;

    // fetch timestamp and calculate delta
    tsp = start.lookup(&pid);
    if (tsp == 0) {
        return 0;   // missed enqueue
    }
    delta = bpf_ktime_get_ns() - *tsp;
    FACTOR

    // store as histogram
    STORE

    
    
    start.delete(&pid);
    return 0;
}
"""

# code substitutions
if args.pid:
    # pid from userspace point of view is thread group from kernel pov
    bpf_text = bpf_text.replace('FILTER', 'pid != %s' % args.pid)
else:
    bpf_text = bpf_text.replace('FILTER', '0')
if args.milliseconds:
    bpf_text = bpf_text.replace('FACTOR', 'delta /= 1000000;')
    label = "msecs"
else:
    bpf_text = bpf_text.replace('FACTOR', 'delta /= 1000;')
    label = "usecs"
if args.pids or args.tids:
    section = "pid"
    pid = "tgid"
    if args.tids:
        pid = "pid"
        section = "tid"
    bpf_text = bpf_text.replace('STORAGE',
        'BPF_HISTOGRAM(dist, pid_key_t);')
    bpf_text = bpf_text.replace('STORE',
        'pid_key_t key = {.id = ' + pid + ', .slot = bpf_log2l(delta)}; ' +
        'dist.increment(key);')
elif args.pidnss:
    section = "pidns"
    bpf_text = bpf_text.replace('STORAGE',
        'BPF_HISTOGRAM(dist, pidns_key_t);')
    bpf_text = bpf_text.replace('STORE', 'pidns_key_t key = ' +
        '{.id = prev->nsproxy->pid_ns_for_children->ns.inum, ' +
        '.slot = bpf_log2l(delta)}; dist.increment(key);')
else:
    section = ""
    bpf_text = bpf_text.replace('STORAGE', 'BPF_HISTOGRAM(dist);')
    bpf_text = bpf_text.replace('STORE',
        'dist.increment(bpf_log2l(delta));')

if args.throttle > 10:
    bpf_text = bpf_text.replace('THROTTLE',
        'delta >> 10 > ' + args.throttle)
else:
    bpf_text = bpf_text.replace('THROTTLE',
        '0')

if debug:
    print(bpf_text)

# load BPF program
b = BPF(text=bpf_text)
b.attach_kprobe(event="ttwu_do_wakeup",  fn_name="trace_wakeup")
b.attach_kprobe(event="finish_task_switch", fn_name="trace_run")

print("Tracing run queue latency... Hit Ctrl-C to end.")

# output
exiting = 0 if args.interval else 1
dist = b.get_table("dist")
stack = b.get_table("stack_trace")
trace = b.get_table("trace_hash")
while (1):
    try:
        sleep(int(args.interval))
    except KeyboardInterrupt:
        exiting = 1
        break

print()
if args.timestamp:
    print("%-8s\n" % strftime("%H:%M:%S"), end="")

dist.print_log2_hist(label, section, section_print_fn=int)
dist.clear()

print(len(stack))
print(len(trace))
if args.throttle > 10:
    # k for runqueue latency
    # v for stack trace id
    for k, v in sorted(trace.items(), key=lambda item: item[0], reverse=True):
        kernel_stack = stack.walk(v.value)
        print("start")
        if stack_id_err(v.value):
            print("    [Missed Kernel Stack]")
        else:
            for addr in kernel_stack:
                print("addr {}".format(addr))
                print("    %s" % b.ksym(addr))

