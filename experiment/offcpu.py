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
from threading import Timer
# arguments
examples = """examples:
    ./offcpu --cpu 0     # trace context switching events on cpu 0
"""
parser = argparse.ArgumentParser(
    description="Summarize run queue (scheduler) latency as a histogram",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("--cpu",  type=int, default=0)
parser.add_argument("--time", type=int, default=5)
args = parser.parse_args()



# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>

#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/interrupt.h>


typedef struct trace_key {
    u32 pid;
    u64 time;
    u8  type; // 0: wake upte; 1: scheduled on; 2: scheduled off
              // 3: hardirq enter; 4: hardirq completes
              // 5: softirq enter; 6: softirq completes
    u8  irqtype;
} trace_key;

BPF_PERF_OUTPUT(events);

void trace_wake_up_new_task(struct pt_regs *ctx, struct task_struct *p)
{
    struct trace_key key= {};
    key.pid  = p->pid;
    key.time = bpf_ktime_get_ns();
    key.type = 0;
    key.irqtype = 0xff;
    events.perf_submit(ctx, &key, sizeof(key));
 
}

void trace_ttwu_do_wakeup(struct pt_regs *ctx, struct rq *rq, struct task_struct *p,
    int wake_flags)
{
    struct trace_key key = {};
    key.pid  = p->pid;
    key.time = bpf_ktime_get_ns();
    key.type = 0;
    key.irqtype = 0xff;
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
    key1.irqtype = 0xff;
    events.perf_submit(ctx, &key1, sizeof(key1));

    struct trace_key key2 = {};
    key2.pid  = bpf_get_current_pid_tgid();
    key2.time = current_time;
    key2.type = 1;
    key2.irqtype = 0xff;
    events.perf_submit(ctx, &key2, sizeof(key2));
    return 0;
}


int trace_hardirq_start(struct pt_regs *ctx, struct irq_desc *desc)
{
    u32 pid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();

    struct trace_key key = {};
    key.pid  = pid;
    key.time = ts;
    key.type = 3;
    key.irqtype = desc->action->irq; 
    events.perf_submit(ctx, &key, sizeof(key));
    return 0;
}


int trace_hardirq_complete(struct pt_regs *ctx)
{
    u32 pid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();

    struct trace_key key = {};
    key.pid  = pid;
    key.time = ts;
    key.type = 4;
    key.irqtype = 0xff;
    events.perf_submit(ctx, &key, sizeof(key));
    return 0;
}



TRACEPOINT_PROBE(irq, softirq_entry)
{
    
    u32 pid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();

    struct trace_key key = {};
    key.pid  = pid;
    key.time = ts;
    key.type = 5;
    key.irqtype = args->vec;
    events.perf_submit((struct pt_regs *)args, &key, sizeof(key));
    
    return 0;
}

TRACEPOINT_PROBE(irq, softirq_exit)
{
    u32 pid = bpf_get_current_pid_tgid();
    u64 ts = bpf_ktime_get_ns();

    struct trace_key key = {};
    key.pid  = pid;
    key.time = ts;
    key.type = 6;
    key.irqtype = 0xff;
    events.perf_submit((struct pt_regs *)args, &key, sizeof(key));
    return 0;
}

"""



# load BPF program
b = BPF(text=bpf_text)
b.attach_kprobe(event="ttwu_do_wakeup", fn_name="trace_ttwu_do_wakeup")
b.attach_kprobe(event="wake_up_new_task", fn_name="trace_wake_up_new_task")
b.attach_kprobe(event="finish_task_switch", fn_name="trace_run")
b.attach_kprobe(event="handle_irq_event_percpu", fn_name="trace_hardirq_start")
b.attach_kretprobe(event="handle_irq_event_percpu", fn_name="trace_hardirq_complete")

print("Tracing run queue latency... Hit Ctrl-C to end.")


class Event(ct.Structure):
    _fields_ = [
        ("pid",  ct.c_uint32),
        ("time", ct.c_uint64),
        ("type", ct.c_uint8),
        ("irqtype", ct.c_uint8),
    ]

PyEvents = []
RUN = True

def print_event(cpu, data ,size):
    try:
        if cpu != args.cpu:
            return
        event = ct.cast(data, ct.POINTER(Event)).contents
    
        pid   = event.pid
        time  = event.time/1000
        etype = event.type
        itype = event.irqtype
        PyEvents.append((pid, time, etype, itype))
    except:
        print("exception")
        raise Exception("Exception when printing")
        

# Schedule a timer to stop main process
def timer_stop():
    global RUN
    RUN = False

def store_events():
    log = open("log.txt", "w")
    for event in PyEvents:
        log.write(str(event[0]) + " " + str(event[1]) + " "+ 
                  str(event[2]) + " " + str(event[3]) + "\n")
    log.close()
    

Timer(args.time, timer_stop, ()).start()

b["events"].open_perf_buffer(print_event)
while RUN:
    try:
        b.perf_buffer_poll()
    except:
        break

# Print out or save into file
PyEvents.sort(key=lambda tup: tup[1])
stats = [0] * 7 
for index in range(len(PyEvents)):
    if PyEvents[index][2] == 0 and PyEvents[index+1][2] == 2 \
        and PyEvents[index + 1][1] - PyEvents[index][1] > 100: 
            print(index)

store_events()
