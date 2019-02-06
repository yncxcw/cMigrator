/* A single thread app that runs and sleep */

#include <iostream>
#include <vector>
#include <thread>
#include <chrono> 
#include <math.h>
#include <ratio>
#include <sys/types.h>
#include <unistd.h>
using namespace std;


int main(){

cout<<"pid: "<<getpid()<<endl;

vector<double> array(1024*10, 40);

long compute, sleep;
double accumulate=0;

cout<<"input compute time and sleep time in millisecond. "<<endl;
cin>>compute>>sleep;

//A infinite loop
while(true){

    if(accumulate > compute && sleep > 0){
        //sleep for sleep time
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep));
        accumulate = 0;
    }else{

        auto begin = std::chrono::high_resolution_clock::now();

        //CPU intensieve computation
        for(int i=0; i < array.size(); i++)
            array[i] = sqrt(array[i])*sqrt(i) + 1;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
        accumulate += duration.count() * 1.0 /1000;
    }

}

return 0;

}
