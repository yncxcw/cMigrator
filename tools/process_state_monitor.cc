#include <iostream>
#include <string>
#include <fstream>
#include <chrono> 
#include <vector>
#include <unistd.h>

int main(){

  //process to monitor
  int process;
  //time to inspect in seconds
  int run_time;
  std::cin>>process>>run_time;  
  std::vector<std::string> states;  

  int max_run=0;
  int con_run=0;  
  auto start = std::chrono::high_resolution_clock::now();

  while(1){
    std::string path = "/proc/"+ std::to_string(process)+"/stat";
    std::ifstream proc_file(path);
    std::string line;
    if(proc_file.good()){
        std::getline(proc_file, line);
    }
    proc_file.close();

    if (line.length() > 0){
        //split the line by " "
        std::string state;
        std::string delimiter = " ";
        size_t pos = 0;
        int count = 0;
        std::string token;
        while ((pos = line.find(delimiter)) != std::string::npos) {
            token = line.substr(0, pos);
            //the third ele in this line.
            if (count == 2){
                state = token;
            }
            line.erase(0, pos + delimiter.length());
            ++count;
        }
        if(state == "R"){
            con_run++;
            max_run = con_run > max_run? con_run: max_run;
        }else{
            con_run=0;
        }
        states.push_back(state);
        
    }else{
        states.push_back("N");

    }

    usleep(1000);

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);
    if (duration.count() > run_time)
        break;
  }  

  std::cout<<max_run<<std::endl;
  std::string output;
  std::ofstream outfile("result.txt");
  for(auto state: states){ 
    output += state;
  }
  //write one line
  outfile << output; 
  outfile.close();
  return 0;    
}
