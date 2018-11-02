#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Fri Nov  2 13:49:50 2018

@author: admin
"""

import matplotlib.pyplot as plt
import numpy as np

def perf_graph():
    R = [0.057, 0.058, 2.079, 4.118, 64.502, 558.757]
    U = [0.015, 0.020, 0.663, 1.403, 23.168, 207.170]
    S = [0.045, 0.050, 1.429, 2.729, 41.393, 302.152]
    N = [10, 100, 1000, 10000, 100000, 1000000]
    
    logN = np.log(N)
    logR = np.log(R)
    logU = np.log(U)
    logS = np.log(S)
    
    fitR = np.polyfit(logN, logR, 1)
    fitU = np.polyfit(logN, logU, 1)
    fitS = np.polyfit(logN, logS, 1)
    
    fit_fnR = np.poly1d(fitR)
    fit_fnU = np.poly1d(fitU)
    fit_fnS = np.poly1d(fitS)
    
    plot1, plot2, plot3, plot4, plot5, plot6 = plt.plot(logN, logR, 'ro', logN, logU, 'bo', logN, logS, 'go', logN, fit_fnR(logN), '--r', logN, fit_fnU(logN), '--b', logN, fit_fnS(logN), '--g')
    plt.xlabel(r"Taille du fichier [MB] en logarithmique")
    plt.ylabel(r"Temps logarithmique [s]")
    plt.title(r"Temps d'exécution en fonction de la taille du fichier d'entrée")
    plt.legend((plot1, plot2, plot3), ('Real time', 'User time', 'System time'))
    plt.show()
    

if __name__ == "__main__":
    perf_graph()