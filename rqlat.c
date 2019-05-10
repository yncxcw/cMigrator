#include <uapi/linux/ptrace.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>

struct prorqlat{
    u32 pid;
    u32 _u5;    //# of rqlat < 5  micro second
    u32 _u10;   //# of rqlat > 5  && rqlat < 10
    u32 _u20;   //# of rqlat > 10 && rqlat < 20
    u32 _u50;   //# of rqlat >20 && rqlat < 50
    u32 _u100;  //# of rqlat >50 && rqlat < 100
    u32 _u1000; //# of rqlat >100 && rqlat < 1000
    u32 _uu1000;//# of rqlat > 1000
};

BPF_HASH(start, u32);
BPF_HASH(lat, u64, struct prorqlat);

struct rq;

// record wakeup timestamp
int trace_wakeup(struct pt_regs *ctx, struct rq *rq, struct task_struct *p, int wake_flags)
{
    u32 tgid = p->tgid;
    u32 pid = p->pid;
    u64 ts = bpf_ktime_get_ns();
    start.update(&pid, &ts);
    
    return 0;
}

void init_prorqlat(prorqlat* _prorqlat, u32 pid){
    prorqlat->pid    = pid;
    prorqlat->_u5i   = 0; 
    prorqlat->_u10   = 0;
    prorqlat->_u20   = 0;
    prorqlat->_u50   = 0;
    prorqlat->_u100  = 0;
    prorqlat->_u1000 = 0;
    prorqlat->_uu1000= 0;
}

void update_prorqlat(prorqlat* _prorqlat, u64 lat){
    if(lat < 5){
        _prorqlat->_u5++;
    }else if(lat >= 5 && lat < 10){
        _prorqlat->_u10++;
    }else if(lat >= 10 && lat < 20){
        _prorqlat->_u20++;
    }else if(lat >= 20 && lat < 50){
        _prorqlat->_u50++;
    }else if(lat >= 50 && lat < 100){
        _prorqlat->_u100++;
    }else if(lat >= 100 && lat < 1000){
        _prorqlat->_u1000++;
    }else if(lat >= 1000){
        _prorqlat->_uu1000++;
    }
}

// calculate latency
int trace_run(struct pt_regs *ctx, struct task_struct *prev)
{
    u32 pid, tgid;

    // ivcsw: treat like an enqueue event and store timestamp
    if (prev->state == TASK_RUNNING) {
        tgid = prev->tgid;
        pid = prev->pid;
        u64 ts = bpf_ktime_get_ns();
        start.update(&pid, &ts);
        }
    }

    tgid = bpf_get_current_pid_tgid() >> 32;
    pid = bpf_get_current_pid_tgid();

    u64 *tsp, delta;

    // fetch timestamp and calculate delta
    tsp = start.lookup(&pid);
    if (tsp == 0) {
        return 0;   // missed enqueue
    }
    
    //to micro second
    delta = (bpf_ktime_get_ns() - *tsp) / 1000;
   
    prorqlat* _prorqlat;
    _prorqlat = lat.lookup(&pid);
    if(_prorqlat == 0){
        prorqlat t_prorqlat;
        init_prorqlat(&t_prorqlat, pid);
        update_prorqlat(&t_prorqlat, delta);     
        lat.update(&pid, &t_prorqlat);
    }else{
        update_prorqlat(_prorqlat, delta);
    }
    start.delete(&pid);
    return 0;
}
