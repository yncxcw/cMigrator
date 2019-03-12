from time import sleep
import numpy as np
import sys
import matplotlib.pyplot as plt


if __name__=="__main__":

    X = [0]
    Y = [0]
    fopen = open("result.txt")
    line  = fopen.readline()
    for ch in line:
        if ch is "R":
            Y.append(10)
            X.append(X[-1]+0.5)
        else:
            Y.append(0)
            X.append(X[-1]+0.5)

    print len(X), len(Y)
    plt.plot(X, Y)
    plt.show() 
