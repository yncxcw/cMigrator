#include <iostream>
#include <ifstream>
#include <string>
#include <cerrno>

#include "BPF.h"

#define CHECK(condition, msg)     \
({                                \
  if (condition) {                \
    std::cerr << msg <<std::endl  \ 
    return -1;                    \
  }                               \
})                                \



class BPFMonitor

    public:

        //interpret info in user space
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

        BPFMonitor(){std::vector<pid_t> pids}: pids(pids){
            std::ifstream fin("rqlat.c");
            if (fin){
                bpf_context = 
                    std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }else{
                throw(errno); 
            }

        }
        
        ~BPFMonitor(){}

        //probe time_ms, return # of rq latency violation 
        //return the lc_process id needs to migrate
        int probe(long time_ms, vector<pid_t> migrate_lc_process){
            ebpf::BPF* bpf = new ebpf::BPF();
            auto res = bpf.init(bpf_context);
            CHECK(res.code(), res.msg());

            res = bpf.attac_kprobe("ttwu_do_wakeup", "trace_wakeup");
            CHECK(res.code(), res.msg());

            res = bpf.attac_kprobe("finish_task_switch", "trace_run");
            CHECK(res.code(), res.msg());

            //sleep for probing
            std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
           
            res = bpf.detach_kprobe("ttwu_do_wakeup");            
            CHECK(res.code(), res.msg());

            auto detach_res2 = bpf.detach_kprobe("finish_task_switch"); 
            CHECK(res.code(), res.msg()); 

            auto table=
                bpf.get_hash_table<uint32_t, struct prorqlat>("lat").get_table_offline();
            for(auto it: table.get_table_offline()){
                //TODO current for test purpose, need to refine this algorithms
                if(it.second._u100 > 0){
                    migrate_lc_process.push_back(it.first);
                }     
            }

            delete bpf;
            return 0; 
        }

        
    private:
        //process to monitor
        std::vector<pid_t> pids;

        std::srting bpf_context;
}; 
