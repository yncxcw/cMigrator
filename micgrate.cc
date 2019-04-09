#include <iostream>
#include <stdlib.h>
#include <vector>
#include <set>
#include <string>
#include <sched.h>
#include <filesystem>

enum State {R, S, D, T, N}

const unsigned int CPUS = sysconf(_SC_NPROCESSORS_ONLN);


struct process{

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

    //is the main process of the application
    bool is_main(){
        return pid == tid;
    }
}


struct Application{
    // application name
    std::string app_name;
    // pid of the main thread of the application
    pid_t pid;
    // processes(threads) belong to this applications
    std::vector<Process> processes;
}


bool file_exist(std::string path){

    std::ifstream file(path);
    if(!file.good())
        return false;
    else
        return true;
}

std::string read_line(std::string path){
    std::string line;
    std::ifsream file(path);
    std::getline(path, line); 
    return line;
}

int create_process(pid_t pid, std::vector<Application>& applications){

    std::string path = "/proc/"+std::to_string(pid);
    //Test /proc/pid
    if(file_exist(path))
    {
        std::cout<<"pid "<<pid<<" does not exist "<<std::endl;
        return -1;
    }

    Application application;
    application.pid = pid;
    application.app_name = read_line(path+"/comm"); 

    path += "/task";
    for (const auto & entry : fs::directory_iterator(path)){
        std::string thread_path = entry.path();
        std::size_t found = str.find_last_of("/\\");
        pid_t thread_pid = std::stoi(str.substr(found+1));
        Process process;
        process.pid = pid;
        process.tid = thread_pid;

        if(sched_getscheduler(thread_pid) == SCHED_FIFO || 
           sched_getscheduler(thread_pid) == SCHED_RT){
            process.is_latency = true;
            process.allowed_cpus.insert(
                get_process_cpu(process));
        }else{ 
            process.is_latency = false;
            for(int idx = 0; idx < CPUS; idx++)
                process.allowed_cpus.insert(idx);
        }
        application.processes.push_back(process); 
    }

    applications.push_back(application);
    return 0; 
}

//thread to inspect process cpu stats
void thread_cpu_update(std::vector<Application>& applications){

    std::cout<<"Start CPU update thread."<<std::endl;

    while(true){

         
        for(auto iter =applications.begin(); iter!=applications.end();){
            if(update_appliation_profile(*iter)){
                iter++;
            }else{
                iter = appliactions.erase(iter);
            }
        }

    
    }
}

//false if the appliaction finished 
bool update_appliation_profile(Application& application){

    std::string path = "/proc/"+std::to_string(pid);
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

            iter = application.processes.erase(iter);
        }
    } 
    return true;
}


//false if the process finished
bool update_process_profile(Process& process){
    std::string path = "/proc/"+ std::to_string(process.pid)+
                          + "/"+ std::to_string(process.tid)+"/stat";
    if(!file_exist(path)){
        return false;
    }
    
    process.cpu_id = get_process_cpu(process);
    process.state = get_process_state(process); 
    return true;
}

std::string get_item_proc(Process& process, int item_idx){

    std::string path = "/proc/"+ std::to_string(process.pid)+
                          + "/"+ std::to_string(process.tid)+"/stat";
    std::ifstream proc_file(path);
    std::string line, item;
    if(proc_file.good()){
        std::getline(proc_file, line);
    }else{
        std::cout<<"open proc file for pid "<<process.pid<<" fails"<<std::endl;
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


unsigned int get_process_cpu(Process& process){

//The 38th item in /proc/pid/stat
unsigned int cpu = std::stoi(get_item_proc(process, 38);
process.cpu_id = cpu;

return cpu;

}

State get_process_state(Process& process){

char S = get_item_proc(process, 2)[0];
State state;
switch(S){
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

process.state = state;
return state;
}


void set_process_schedaffnity(Process& process, vector<int> cpus){
    process.cpus.clear();
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for(auto& cpu: cpus){
        CPU_SET(cpu, &cpu_set);
        process.cpus.insert(cpu);
    }

    int ret=sched_setaffinity(process.pid, sizeof(cpu_set_t), &my_set);
    if(ret == -1){
        std::cout<<"Error when setting process "<<process.pid<<" affinity";
    }
    
    return;
}



int main(){

    return 0;
}

