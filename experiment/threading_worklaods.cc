/* A multi threads app that computes and sync */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono> 
#include <math.h>
#include <ratio>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
#include <sched.h>
using namespace std;


void compute_function(double compute, int thread_index){

    std::cout<<thread_index<<" thread cpu"<<sched_getcpu()<<std::endl;
    auto begin = std::chrono::high_resolution_clock::now();
    //use linear regression to get # of interatations to cosumes the compute time.
    int iterate = (compute - 0.333891175735)/0.00116243322436;
    //!0k x sizeof(double) = 800k data
    std::vector<double> array(1024*100, 40.0);
    int count = 0;
    for(int i=0; i<iterate; i++){
        for(int j=0; j<array.size(); j++)
            array[j] = sqrt(array[j])*sqrt(j);
	if(i == (count+1) * iterate/10){
	    std::cout<<thread_index<<" thread cpu"<<sched_getcpu()<<std::endl;
	    count++;
	}	
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);
    std::cout<<thread_index<<" thread cpu "<<sched_getcpu()<<" "<<duration.count()*1.0/1000<<" seconds"<<std::endl; 

    
}


int main(){

cout<<"pid: "<<getpid()<<endl;


double compute_time    = 60;
long   thread_count    = 4;
long   iteration_count = 1;


for(int i=0; i<iteration_count; i++)
{

    std::cout<<"start iteration "<<i<<std::endl;
    vector<std::thread> threads;
    auto begin = std::chrono::high_resolution_clock::now();
    for(int j=0; j<thread_count; j++){
        threads.push_back(std::thread(compute_function, compute_time, j));    
    }
    for(int j=0; j<thread_count; j++){
        threads[j].join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin);

    std::cout<<"iteration "<<i<<" takes "<<duration.count()*1.0/1000<<" seconds"<<std::endl; 
}

return 0;

}
