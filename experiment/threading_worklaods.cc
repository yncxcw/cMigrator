/* A single thread app that runs and sleep */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono> 
#include <math.h>
#include <ratio>
#include <sys/types.h>
#include <unistd.h>
#include <thread>
using namespace std;


void compute_function(double compute){

    //use linear regression to get # of interatations to cosumes the compute time.
    int iterate = (compute - 0.333891175735)/0.00116243322436;
    //!0k x sizeof(double) = 800k data
    std::vector<double> array(1024*100, 40.0);      
    for(int i=0; i<iterate; i++){
        for(int j=0; j<array.size(); j++)
            array[j] = sqrt(array[j])*sqrt(j);
    }    
}


int main(){

cout<<"pid: "<<getpid()<<endl;


double compute_time;
long thread_count, iteration_count;

cout<<"input compute time (s), thread count and iteration count. "<<endl;
cin>>compute_time>>thread_count>>iteration_count;


for(int i=0; i<iteration_count; i++)
{

    std::cout<<"start iteration "<<i<<std::endl;
    vector<std::thread> threads;
    auto begin = std::chrono::high_resolution_clock::now();
    for(int j=0; j<thread_count; j++){
        threads.push_back(std::thread(compute_function, compute_time));    
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
