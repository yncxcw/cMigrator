#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <cerrno>
#include <sys/types.h>
#include <thread>
#include <chrono>
#include <bcc/BPF.h>

#define CHECK(condition, msg)     \
({                                \
  if (condition) {                \
    std::cerr << msg <<std::endl; \
    return -1;                    \
  }                               \
})                                \

class BPFMonitor{

    public:
        //interpret info in user space
        struct prorqlat{
            int pid;
            int _u5;    //# of rqlat < 5  micro second
            int _u10;   //# of rqlat > 5  && rqlat < 10
            int _u20;   //# of rqlat > 10 && rqlat < 20
            int _u50;   //# of rqlat >20 && rqlat < 50
            int _u100;  //# of rqlat >50 && rqlat < 100
            int _u1000; //# of rqlat >100 && rqlat < 1000
            int _uu1000;//# of rqlat > 1000
        };

        BPFMonitor() = delete;
        BPFMonitor(std::set<pid_t> pids): pids(pids){
            std::ifstream fin("rqlat.c");
            if (fin){
                bpf_context = 
                    std::string((std::istreambuf_iterator<char>(fin)), std::istreambuf_iterator<char>());
            }else{
                throw(errno); 
            }

        }
        
        ~BPFMonitor(){}

        //probe time_ms, return # of rq latency violation 
        //return the lc_process id needs to migrate
        int probe(long time_ms, std::vector<pid_t>& migrate_lc_process){
            ebpf::BPF* bpf = new ebpf::BPF();
            auto res = bpf->init(bpf_context);
            CHECK(res.code(), res.msg());

            res = bpf->attach_kprobe("ttwu_do_wakeup", "trace_wakeup");
            CHECK(res.code(), res.msg());

            res = bpf->attach_kprobe("finish_task_switch", "trace_run");
            CHECK(res.code(), res.msg());

            //sleep for probing
            std::this_thread::sleep_for(std::chrono::milliseconds(time_ms));
           
            res = bpf->detach_kprobe("ttwu_do_wakeup");            
            CHECK(res.code(), res.msg());

            auto detach_res2 = bpf->detach_kprobe("finish_task_switch"); 
            CHECK(res.code(), res.msg()); 

            auto table=
                bpf->get_hash_table<uint32_t, struct prorqlat>("lat"); //get_table_offline();
            for(auto it: table.get_table_offline()){
                //TODO current for test purpose, need to refine this algorithms
                if(pids.find(it.first) != pids.end() && it.second._u100 > 5){
                    migrate_lc_process.push_back(static_cast<pid_t>(it.first));
                }     
            }

            delete bpf;
            return 0; 
        }
        
    private:
        //process to monitor
        std::set<pid_t> pids;

        std::string bpf_context;
}; 
