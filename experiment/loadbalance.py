#!/usr/bin/python
#
# Trace the load_balance() it prints out the parameters to this function once called. 
#
# USAGE: loadbalance.py
#

from __future__ import print_function
from bcc import BPF
from bcc.utils import ArgString, printb
import argparse
from time import strftime
import ctypes as ct

# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/pid_namespace.h>
//#include <linux/sched/topology.h>


struct rq; // forward declaration

struct val_t {
   int this_cpu;
   //these are from sched_domain
   unsigned int busy_idx;
   unsigned int idle_idx;
   unsigned int newidle_idx;
   unsigned int wake_idx;
};

BPF_PERF_OUTPUT(events);

int kprobe_load_balance(struct pt_regs *ctx, int this_cpu, struct rq *rq, struct sched_domain *sd)
{

    struct val_t data = {};
    data.this_cpu = this_cpu;
    data.busy_idx = sd->busy_idx;
    data.idle_idx = sd->idle_idx;
    data.newidle_idx = sd->newidle_idx;
    data.wake_idx = sd->wake_idx;
    events.perf_submit(ctx, &data, sizeof(data));    
    
    return 0;
};

"""


# initialize BPF
b = BPF(text=bpf_text)
b.attach_kprobe(event="load_balance", fn_name="kprobe_load_balance")

class Data(ct.Structure):
    _fields_ = [
        ("this_cpu", ct.c_int),
        ("busy_idx", ct.c_uint),
        ("idle_idx", ct.c_uint),
        ("newidle_idx", ct.c_uint),
        ("wake_idx", ct.c_uint),
    ]


# process event
def print_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    printb(b"%-9s  %d  %d  %d  %d  %d" % (strftime("%H:%M:%S").encode('ascii'), 
        event.this_cpu, event.busy_idx, event.idle_idx, event.newidle_idx, event.wake_idx))

# loop with callback to print_event
b["events"].open_perf_buffer(print_event)
while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
