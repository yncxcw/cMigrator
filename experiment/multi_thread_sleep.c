/* multi-thread app that runs and sleeps */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono> 
#include <math.h>
#include <ratio>
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
using namespace std;


// benchmark configuration.
int   threads_count= 2;     // threads count
bool  rt_scheduler = true;  // use rt scheduler
int   pin_cpu      = 0;     // the first cpu to pin the thread
long  total_quota  = 1000;  // total quota in each iterate
// initialize loads
std::vector<std::pair<unsigned long, double>> 
    loads = {
        std::pair<unsigned long, double>(36000, 0.95),
        std::pair<unsigned long, double>(60, 0.95)
    };



//Each pair is the execution time (second) for this period with laod (load > 0 && load < 1)
void run_and_sleep(int index){

if(rt_scheduler){
    struct sched_param param;
    param.sched_priority = 99;
    if (sched_setscheduler(0, SCHED_FIFO, & param) != 0) {
        std::cout<<"sched_setscheduler error";
        exit(EXIT_FAILURE);  
    }
}


if(loads.size() == 0){

    return;
}

vector<double> array(1024*100, 40);

for(int i=0; i<loads.size(); i++){

    std::cout<<"threads "<<index<<" is entering "<<"stage "<<i<<std::endl;
    //execution time (milli second) for this run
    double load_exe_time = 0;
    double comp_time     = 0;
    //compute time for each run
    int comp_quota    = total_quota * loads[i].second;
    //sleep time for each run
    int sleep_quota   = total_quota * (1 - loads[i].second);
    while(true){

        if(load_exe_time / 1000 > loads[i].first){
            break;
        }

        if(comp_time > comp_quota && sleep_quota > 0){
            //sleep for sleep time
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_quota));
            comp_time = 0;
            load_exe_time += sleep_quota;
        }else{

            auto begin = std::chrono::high_resolution_clock::now();

            //CPU intensieve computation
            for(int j=0; j < array.size(); j++)
                array[j] = sqrt(array[j])*sqrt(j) + 1;

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
            comp_time += duration.count() * 1.0 / 1000;
            load_exe_time += duration.count() * 1.0 / 1000;
        }

    }

}


}

int main(){

vector<std::thread> threads;

std::cout<<"start "<<std::endl;
int numCPU = sysconf(_SC_NPROCESSORS_ONLN);

for(int i=0; i < threads_count; i++){
    threads.push_back(std::thread(run_and_sleep, i));
    if(pin_cpu >= 0){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET((i+pin_cpu)%numCPU, &cpuset);
        int rc = pthread_setaffinity_np(threads[i].native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cout << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }

    }
}

for(int i=0; i < threads_count; i++){
    threads[i].join();
}

std::cout<<"completes "<<std::endl;

return 0;
}
