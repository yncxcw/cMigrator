#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <sys/types.h>

#include <vector>
#include <set>
#include <string>
#include <mutex>
#include <thread>
#include <sched.h>
#include <unistd.h>
#include <filesystem>


//third party
#include <zmq.hpp>
namespace migrator{


    class Migrator{

        ~Migrator(){

        }

        enum State {R, S, D, T, N};
        
        //shared global vairable
        const unsigned int CPUS_NUM = sysconf(_SC_NPROCESSORS_ONLN);
        const unsigned int CPU_SLEEP_TIME = 50;
        const unsigned int MAX_STATE_ARRAY = 50;

        bool FINISH = false;

        //Core data structure declare 
        class Process{
   
            public:
                Process(){};

                process(pid_t pid, pid_t tid):pid(pid), tid(tid){};

                ~Process(){
                    //release set
                    std::set<unsigned int>().swap(allowed_cpus);
                    //release vector
                    std::set<State>().swap(states);
                };    

            //pid of this process
            pid_t pid;
            //pid of thie thread
            pid_t tid;
            //process type, is latency critical;
            bool is_latency;
            //current cpu;
            unsigned int cpu_id;
            //allowed cpus;
            std::set<unsigned int> allowed_cpus;
            //current state;
            State state;
            //state in last MAX_STATE_ARRAY ms
            std::vector<State> states();
            
             
   
            //is the main process of the application
            bool is_main(){
                return pid == tid;
            }
        };
        
        
        struct Application{
            // application name
            std::string app_name;
            // pid of the main thread of the application
            pid_t pid;
            // processes(threads) belong to this applications
            std::vector<Process* > processes;
        };
        
        
        struct Applications{
            //mutex to protec the shared applications
            std::mutex apps_lock;
            // vector of applications
            std::vector<Application> apps;
        };
   
 
        bool file_exist(std::string& path){
        
            std::ifstream file(path);
            if(!file.good())
                return false;
            else
                return true;
        }
        
        std::string read_line(std::string path){
            std::string line;
            std::ifstream file(path);
            std::getline(file, line); 
            return line;
        }
        
        std::string get_item_proc(Process* process, int item_idx){
        
            std::string path = "/proc/"+ std::to_string(process->pid)+
                                  + "/"+ std::to_string(process->tid)+"/stat";
            std::ifstream proc_file(path);
            std::string line, item;
            if(proc_file.good()){
                std::getline(proc_file, line);
            }else{
                std::cout<<"open proc file for pid "<<process->pid<<" fails"<<std::endl;
            }
            proc_file.close();
            if (line.length() > 0){
                //split the line by " "
                std::string delimiter = " ";
                size_t pos = 0;
                int count = 0;
                std::string token;
                while ((pos = line.find(delimiter)) != std::string::npos) {
                    token = line.substr(0, pos);
                    //the third ele in this line.
                    if (count == item_idx){
                        item = token;
                    }
                    line.erase(0, pos + delimiter.length());
                    ++count;
                }
            }
            return item;
        }
        
        
        unsigned int get_process_cpu(Process* process){ 
            //The 38th item in /proc/pid/stat
            unsigned int cpu = std::stoi(get_item_proc(process, 38));
            process->cpu_id = cpu;
        
            return cpu;
        }
        
        State get_process_state(Process* process){
        
            char _state = get_item_proc(process, 2)[0];
            State state;
            switch(_state){
                case 'R':
                    state = R;
                    break;
                case 'S':
                    state = S;
                    break;
                case 'D':
                    state = D;
                    break;
                case 'T':
                    state = T;
                    break;
                default:
                    state = N; 
            }
        
            process->state = state;
            return state;
        }
        
        
        void set_process_schedaffnity(Process* process, std::vector<int> cpus){
            process->allowed_cpus.clear();
            cpu_set_t cpu_set;
            CPU_ZERO(&cpu_set);
            for(auto& cpu: cpus){
                CPU_SET(cpu, &cpu_set);
                process->allowed_cpus.insert(cpu);
            }
        
            int ret=sched_setaffinity(process->pid, sizeof(cpu_set_t), &cpu_set);
            if(ret == -1){
                std::cout<<"Error when setting process "<<process->pid<<" affinity";
            }
            
            return;
        }
        
        
        
        
        int create_application(pid_t pid){
        
            std::string path = "/proc/"+std::to_string(pid);
            //Test /proc/pid
            if(file_exist(path))
            {
                std::cout<<"pid "<<pid<<" does not exist "<<std::endl;
                return -1;
            }
        
            Application application;
            application.pid = pid;
            application.app_name = read_line(std::string(path+"/comm")); 
        
            path += "/task";
            for (const auto & entry: std::filesystem::directory_iterator(path)){
                std::string thread_path = entry.path();
                std::size_t found = thread_path.find_last_of("/\\");
                pid_t thread_pid = std::stoi(thread_path.substr(found+1));
                Process* process = new Process(pid, thread_pid);
        
                if(sched_getscheduler(thread_pid) == SCHED_FIFO || 
                   sched_getscheduler(thread_pid) == SCHED_RR){
                    unsigned int cpu = get_process_cpu(process);
                    process->is_latency = true;
                    process->allowed_cpus.insert(cpu);
                    //cpu to LC process must be one to one mapping
                    assert(cpu_to_process.find(cpu) == cpu_to_process.end());
                    cpu_to_process[cpu] = process;
                }else{ 
                    process->is_latency = false;
                    for(auto cpu: cpus)
                        process->allowed_cpus.insert(cpu);

                    set_process_schedaffnity(process, process->allowed_cpus);
                }
                application.processes.push_back(process); 
            }
        
            {
                std::lock_guard<std::mutex> lock(applications.apps_lock);
                applications.apps.push_back(application);
        
            }
            return 0; 
        }
        
        
        //false if the process finished
        bool update_process_profile(Process* process){
            std::string path = "/proc/"+ std::to_string(process.pid)+
                                  + "/"+ std::to_string(process.tid)+"/stat";
            if(!file_exist(path)){
                return false;
            }
            
            process->cpu_id = get_process_cpu(process);
            process->state = get_process_state(process);
            
            process->states.push_back(process->state);
            if(process->states.size() > MAX_STATE_ARRAY){
                process->states.erase(process->state.begin());
            }   
            return true;
        }
        
        
        //false if the appliaction finished 
        bool update_appliation_profile(Application& application){
        
            std::string path = "/proc/"+std::to_string(application.pid);
            //application completes
            if(!file_exist(path)){
                return false;
            }
        
        
            for(auto iter = application.processes.begin();
                     iter!= application.processes.end();){
        
                if(update_process_profile(*iter)){
        
                    iter++;
                //process of this application finishes
                }else{
                    if((*iter)->is_latency){
                        unsigned int cpu = std::vector((*iter)->allowed_cpus.begin(),
                                                       (*iter)->allowed_cpus.end())[0];
                        cpu_to_process.erase(cpu); 
                    } 
                    delete *iter;
                    iter = application.processes.erase(iter);
                }
            } 
            return true;
        }
        
        
        //thread to inspect process cpu stats
        void thread_cpu_update(){
        
            std::cout<<"Start CPU update thread."<<std::endl;
        
            while(!FINISH){
                 
                //update application profile
                {
                    std::lock_guard<std::mutex> lock(applications.apps_lock); 
                    for(auto iter =applications.apps.begin(); iter!=applications.apps.end();){
                        if(update_appliation_profile(*iter, cpu_to_process)){
                            iter++;
                        }else{
                            for(auto process: iter->processes){
                                if(process->is_latency){
                                    unsigned int cpu = std::vector(process->allowed_cpus.begin(),
                                                                   process->allowed_cpus.end())[0];
                                    cpu_to_process.erase(cpu);
                                }
                                delete process;
                            }  
                            iter = applications.apps.erase(iter);
                        }
                    }
                }

                //RT Load balancing migration (migrate if LC process hold a core for too long)
                {

                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(CPU_SLEEP_TIME)); 
            }
            
            std::cout<<"Finish CPU update thread."<<std::endl;
        
        }

        void thread_user_cmd(){
            
            zmq::context_t context(1);
            zmq::socket_t socket(context, ZMQ_REP);
            socket.bind("tcp://*.1125");
            std::cout<<"Start user cmd thread."<<std::endl;

            while(!FINISH){
                zqm::message_t command;
                zqm::message_t reply(3);
                //wait for the next command
                socket.recv(&command);
                std::string command_str = std::string(
                    static_cast<char*>(command.data()), command.size());

                // first 4 chars are command types
                // now supports QUIT, LAUN pid
                std::string command_type = command_str.substr(0, 4);
                switch(cmmand_type){
                    case "QUIT":
                        FINISH = true;
                        break;
                    case "LAUN":
                        pid_t app_id = std::stoi(command_str.substr(5));
                        create_application(app_id);
                        break;
                    default:
                        FINISh = true;
                        std::cout<<"Unexpected command, quitting."<<std::endl;
                        break;
                } 
                memcopy((void *) reply.data(), "ACK", 3);
                socket.send(reply);
            } 

            std::cout<<"Finish cmd thread."<<std::endl;
        }
  
         
        //Core data structure
        Applications applications;

        //mapping from cpu to LC process, one cpu core at most hold one LC process.
        //for fast checking cpu status
        std::map<unsigned int, Process* > cpu_to_process;

        //keep all available CPU cores for BS processes
        std::vector<unsigned int> cpus(CPUS_NUM); 

    }

    void launch_service(){
        Migrator migrator;
        //launch the cpu update thread
        std::thread cpu_thread = std::thread(migrator.thread_cpu_update);
        //launch the user command thread
        std::thread cmd_thread = std::thread(migrator.thread_user_cmd); 
        //wait to completes
        cpu_thread.join();
        cmd_thread.join();
    }


}
int main(){
 
    migrator::launch_service();
    return 0;
}

