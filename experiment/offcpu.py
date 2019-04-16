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
#    and execution. This traces ttwu_do_wakeup(), wake_up_new_task() ->
#    finish_task_switch() with either raw tracepoints (if supported) or kprobes
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
import ctypes as ct
# arguments
examples = """examples:
    ./offcpu --cpu 0     # trace context switching events on cpu 0
"""
parser = argparse.ArgumentParser(
    description="Summarize run queue (scheduler) latency as a histogram",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("--cpu", type=int, default=0)

args = parser.parse_args()



# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>

typedef struct trace_key {
    u32 pid;
    u64 time;
    u8 type; // 0: wake upte; 1: scheduled on; 2: scheduled off
} trace_key;

BPF_PERF_OUTPUT(events);

void trace_wake_up_new_task(struct pt_regs *ctx, struct task_struct *p)
{
    struct trace_key key= {};
    key.pid  = p->pid;
    key.time = bpf_ktime_get_ns();
    key.type = 0;
    events.perf_submit(ctx, &key, sizeof(key));
 
}

void trace_ttwu_do_wakeup(struct pt_regs *ctx, struct rq *rq, struct task_struct *p,
    int wake_flags)
{
    struct trace_key key = {};
    key.pid  = p->pid;
    key.time = bpf_ktime_get_ns();
    key.type = 0;
    events.perf_submit(ctx, &key, sizeof(key));
}


int trace_run(struct pt_regs *ctx, struct task_struct *prev)
{
    u32 pid, tgid;

    u64 current_time = bpf_ktime_get_ns();

    struct trace_key key1 = {};
    key1.pid  = prev->pid;
    key1.time = current_time;
    key1.type = 2;
    events.perf_submit(ctx, &key1, sizeof(key1));

    struct trace_key key2 = {};
    key2.pid  = bpf_get_current_pid_tgid();
    key2.time = current_time;
    key2.type = 1;
    events.perf_submit(ctx, &key2, sizeof(key2));
    return 0;
}
"""



# load BPF program
b = BPF(text=bpf_text)
b.attach_kprobe(event="ttwu_do_wakeup", fn_name="trace_ttwu_do_wakeup")
b.attach_kprobe(event="wake_up_new_task", fn_name="trace_wake_up_new_task")
b.attach_kprobe(event="finish_task_switch", fn_name="trace_run")

print("Tracing run queue latency... Hit Ctrl-C to end.")


class Event(ct.Structure):
    _fields_ = [
        ("pid",  ct.c_uint32),
        ("time", ct.c_uint64),
        ("type", ct.c_uint8),
    ]

PyEvents = []

def print_event(cpu, data ,size):
    event = ct.cast(data, ct.POINTER(Event)).contents
    
    if cpu != args.cpu:
        return
    pid   = event.pid
    time  = event.time
    etype = event.type
    PyEvents.append((pid, time, etype))

    
b["events"].open_perf_buffer(print_event)
while (1):
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        print(len(PyEvents))
        PyEvents.sort(key=lambda tup: tup[1])
        for event in PyEvents:
            print(event[0]," ",event[1]," ",event[2])
        exit()
        
    
