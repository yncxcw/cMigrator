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
  int exe_time;
  std::cin>>process>>exe_time;  
  std::vector<std::string> states;  
  std::vector<unsigned int> run_time;
  std::vector<unsigned int> idl_time;

  int con_idl=0;
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
				
	    if(con_run == 0 && con_idl > 0){
		idl_time.push_back(con_idl);
	    	con_idl = 0;
            }
	    con_run++;
        }else{
	    if(con_idl == 0 && con_run > 0){
	        run_time.push_back(con_run);
	        con_run = 0;	
	    }

            con_idl++;
        }


        states.push_back(state);
        
    }else{
        states.push_back("N");

    }

    usleep(1000);

    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);
    if (duration.count() > exe_time)
        break;
  }  


  //analyze the tracing results
  std::ofstream outfile0("state.txt");
  for(auto& v: states){ 
    outfile0 << v << std::endl;
  }
  outfile0.close();

  std::ofstream outfile1("run.txt");
  for(auto& v: run_time){ 
    outfile1 << v << std::endl;
  }
  outfile1.close();

  std::ofstream outfile2("ide.txt");
  for(auto& v: idl_time){ 
    outfile2 << v << std::endl;
  }
  outfile2.close();

  return 0;    
}
