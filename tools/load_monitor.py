# This tool monitors the load and queue length of each runq. Needs the kernel monification.
from time import sleep
import numpy as np
import matplotlib.pyplot as plt

if __name__=="__main__":

    # cpu to load map
    load_map = {}
    # cpu to rqueue length map
    rque_map = {}
    print("inspection starts.")
    while True:
        try:
            fread = open("/proc/schedstat","r")
            for line in fread.readlines():
                if "cpu" not in line:
                    continue
                items = line.split(" ")
                cpu = items[0];
                rque = int(items[-1]);
                load = int(items[-2]);
                if cpu not in load_map:
                    load_map[cpu] = []
                if cpu not in rque_map:
                    rque_map[cpu] = []
                load_map[cpu].append(load)
                rque_map[cpu].append(rque)
            fread.close()
            # monitor per 10 millisecnd
            sleep(0.01)
        except KeyboardInterrupt:
            print("inspection is over.")
            break;


    cpu_allowed = [] # ["cpu0", "cpu1", "cpu2", "cpu3", "cpu4"] 
    if len(cpu_allowed) == 0:
        cpu_allowed = load_map.keys()

    fig = plt.figure()
    load_data = np.array([load_map[cpu] for cpu in cpu_allowed])
    max_load_data =  np.amax(load_data)
    median_load_data = np.median(load_data)
    load_data = np.where(load_data < 0.9 * max_load_data, load_data, 0.9 * max_load_data) 

    ax1 = fig.add_subplot(121)
    im1 = ax1.imshow(load_data, cmap=plt.cm.hot_r, aspect='auto')
    plt.colorbar(im1) 
    
    rque_data = np.array([rque_map[cpu] for cpu in cpu_allowed])
    max_rque_data = np.amax(rque_data)
    median_rque_data = np.median(rque_data)
    
    print(max_load_data, max_rque_data)
    print(median_load_data, median_rque_data)
    ax2 = fig.add_subplot(122)
    im2 = ax2.imshow(rque_data, cmap=plt.cm.hot_r, aspect='auto')
    plt.colorbar(im2) 
    plt.show()
                
