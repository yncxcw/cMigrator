# This tool monitors the load and queue length of each runq. Needs the kernel monification.
from time import sleep
import numpy as np
import sys

cpu_allowed = range(20) #[0, 1, 2, 3] 


def save_np_array(path, data):
    np.save(path, data)

def load_np_array(path):
    return np.load(path+".npy")

if __name__=="__main__":


    if len(sys.argv) > 1 and sys.argv[1] == "v":
        import matplotlib.pyplot as plt

        
        load_data = np.array([load_np_array("mapl")[cpu_index] for cpu_index in cpu_allowed])
        rque_data = np.array([load_np_array("mapr")[cpu_index] for cpu_index in cpu_allowed])
        capa_data = np.array([load_np_array("mapc")[cpu_index] for cpu_index in cpu_allowed])
        
        print(rque_data.shape)
        print(load_data.shape)
        
        fig = plt.figure()
        max_load_data =  np.amax(load_data)
        median_load_data = np.median(load_data)
        load_data = np.where(load_data < 0.9 * max_load_data, load_data, 0.9 * max_load_data) 


        max_capa_data = np.amax(capa_data)

        ax1 = fig.add_subplot(121)
        im1 = ax1.imshow(capa_data, cmap=plt.cm.hot_r, aspect='auto')
        plt.colorbar(im1) 
        # ax1.plot(range(len(rque_data[0])), rque_data[0])
        # ax1.plot(range(len(rque_data[1])), rque_data[1])

        max_rque_data = np.amax(rque_data)
        median_rque_data = np.median(rque_data)
       
        ax2 = fig.add_subplot(122)
        im2 = ax2.imshow(rque_data, cmap=plt.cm.hot_r, aspect='auto')
        plt.colorbar(im2)

        plt.show()
         

    else:
    
        # cpu to load map
        load_map = {}
        # cpu to rqueue length map
        rque_map = {}
        # cpu capacity
        capa_map = {}
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
                    capa = int(items[-3]);
                    if cpu not in load_map:
                        load_map[cpu] = []
                    if cpu not in rque_map:
                        rque_map[cpu] = []
                    if cpu not in capa_map:
                        capa_map[cpu] = []
                    load_map[cpu].append(load)
                    rque_map[cpu].append(rque)
                    capa_map[cpu].append(capa)
                fread.close()
                # monitor per 10 millisecnd
                sleep(0.01)
            except KeyboardInterrupt:
                print("inspection is over.")
                break;

        cpu_count = len(load_map)
        load_data = np.array([load_map[cpu] for cpu in ["cpu"+str(i) for i in range(cpu_count)]])
        rque_data = np.array([rque_map[cpu] for cpu in ["cpu"+str(i) for i in range(cpu_count)]])
        capa_data = np.array([capa_map[cpu] for cpu in ["cpu"+str(i) for i in range(cpu_count)]])

        save_np_array("mapl", load_data)
        save_np_array("mapr", rque_data)
        save_np_array("mapc", capa_data)

               
