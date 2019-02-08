#!/usr/bin/python
#
# Trace the pick_next_fair() in cfs scheduler on a specific CPU to understand 
# the process scheduling. 
#           For Linux, uses BCC, eBPF. Embedded C.
#
# USAGE: picknextfair.py [-c cpu]
#

from __future__ import print_function
from bcc import BPF
from bcc.utils import ArgString, printb
import argparse
from time import strftime
import ctypes as ct

# arguments
examples = """examples:
    ./picknextfair -c 0           # trace the previous task and next task chosen by cfs scheduler.
"""
parser = argparse.ArgumentParser(
    description="Trace tasks scheduled by cfs scheduler",
    formatter_class=argparse.RawDescriptionHelpFormatter,
    epilog=examples)
parser.add_argument("-c", "--cpu",
    help="the cpu to trace")
args = parser.parse_args()
debug = 0

# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>


struct rq; // forward declaration
struct rq_flags; //forward declaration

struct val_t {
   pid_t pid;
   u64 vruntime;
   int type;       // Note, 0 for previous task, 1 for next task.
};

BPF_PERF_OUTPUT(events);

int kprobe__pick_next_fair(struct pt_regs *ctx, struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
    if(FILTER1)
        return 0;

    struct val_t data = {};
    data.pid = prev->pid;
    data.vruntime = prev->se.vruntime;
    data.type = 0;
    events.perf_submit(ctx, &data, sizeof(data));    
    
    return 0;
};
/*
int kproberet__pick_next_fair(struct pt_regs *ctx)
{
    struct task_struct* next = (struct task_struct*)PT_REGS_RC(ctx);

    int pid; 
    u64 vruntime;
    bpf_probe_read(&pid, sizeof(pid_t), &next->pid);
    bpf_probe_read(&vruntime, sizeof(u64), &next->se.vruntime);
    if(FILTER2)
        return 0;

    struct val_t data = {};
    data.pid = pid;
    data.vruntime = vruntime;
    data.type = 1;
    events.perf_submit(ctx, &data, sizeof(data));

    return 0;
}
*/
"""
if args.cpu:
    #bpf_text = bpf_text.replace('FILTER1',
    #    'rqq->cpu != %s' % args.cpu)
    
    bpf_text = bpf_text.replace('FILTER1', 
        'task_cpu(prev) != %s' % args.cpu)


    bpf_text = bpf_text.replace('FILTER2', 
        'task_cpu(next) != %s' % args.cpu)

else:
    print("cpu must be set")

# initialize BPF
b = BPF(text=bpf_text)
kill_fnname = b.get_syscall_fnname("kill")
b.attach_kprobe(event="pick_next_task_fair", fn_name="kprobe__pick_next_fair")
b.attach_kretprobe(event="pick_next_task_fair", fn_name="kproberet__pick_next_fair")


TASK_COMM_LEN = 16    # linux/sched.h

class Data(ct.Structure):
    _fields_ = [
        ("pid", ct.c_int),
        ("vruntime", ct.c_ulonglong),
        ("type", ct.c_int)
    ]

# header
print("%-9s %-6s %-6s %-4s" % (
    "TIME", "PID", "vruntime", "type"))

# process event
def print_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents

    printb(b"%-9s %-6d %-6d %d" % (strftime("%H:%M:%S").encode('ascii'),
        event.pid, event.vruntime, event.type))

# loop with callback to print_event
b["events"].open_perf_buffer(print_event)
while 1:
    try:
        b.perf_buffer_poll()
    except KeyboardInterrupt:
        exit()
