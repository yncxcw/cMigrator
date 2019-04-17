#!/usr/bin/python
#
# Trace the load_balance_trace() it prints out the parameters to this function once called. 
#
# USAGE: loadbalance_trace.py
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


struct val_t {
   int src_cpu;
   int des_cpu;
   int tasks;
};

BPF_PERF_OUTPUT(events);

int kprobe_trace_load_balance(struct pt_regs *ctx, int src_cpu, int des_cpu, int ld_moved)
{

    struct val_t data = {};
    data.src_cpu = src_cpu;
    data.des_cpu = des_cpu;
    data.tasks = ld_moved;
    events.perf_submit(ctx, &data, sizeof(data));    
    
    return 0;
};

"""


# initialize BPF
b = BPF(text=bpf_text)
b.attach_kprobe(event="trace_load_balance", fn_name="kprobe_trace_load_balance")

class Data(ct.Structure):
    _fields_ = [
        ("src_cpu",  ct.c_int),
        ("des_cpu",  ct.c_int),
        ("tasks",    ct.c_int),
    ]


# process event
def print_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    printb(b"%-9s  %d  %d  %d %d" % (strftime("%H:%M:%S").encode('ascii'), 
           cpu, event.src_cpu, event.des_cpu, event.tasks))

# loop with callback to print_event
b["events"].open_perf_buffer(print_event)
while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
