#!/usr/bin/env python

import os
import sys
import pathlib          # to get the current path

import numpy as np  
import copy
import matplotlib.pyplot as plt     # module for plotting
from matplotlib import rcParams
import pylab


# load dataset
# filename = "trial4_02_02_2021.txt"
filename = "trial4_02_02_2021.txt"

data_dir = str(pathlib.Path().absolute()) + "/data/" + filename
dataset = np.loadtxt(data_dir)    #in np array format

# define variables
time = dataset[:,0]
ee_posx = dataset[:,1]
ee_posy = dataset[:,2]
ee_posz = dataset[:,3]
target_posx = dataset[:,4]
target_posy = dataset[:,5]
target_posz = dataset[:,6]
ee_vel_norm = dataset[:,7]
predicted_label = dataset[:,8]
startRobotMotion = dataset[:,9]
pauseRobotMotion = dataset[:,10]
#gripper_pos = dataset[:,11] #between 0(open) and 255(closed)
#gripper_status = dataset[:,12] %if stopped opening/closing due to contact w object
gripper_state = dataset[:,13] #0 open, 1 closed
activategripper = dataset[:,14] #if label 7->5
# desgripper_cmd = dataset[:,15]  #1 to close, 2 to open
#ground truth labels
y_start = dataset[:,16]
y_pause = dataset[:,17]
y_grasp = dataset[:,18]


def eval_performance(des_robotaction, detected_emgtrigger):
    start_cues_idx = np.where(np.diff(des_robotaction) == 1) [0]
    end_cues_idx = np.where(np.diff(des_robotaction) == -1) [0]
    t_cuestart = time[start_cues_idx]
    t_cueend = time[end_cues_idx]

    emgtrigger_idx = np.where(np.diff(detected_emgtrigger) == 1) [0]
    t_emgtrigger = time[emgtrigger_idx]

    nb_failedtriggers = 0
    nb_falsetriggers = 0
    t_usedaudio = []
    t_correct_emg = []

    # get only emg_triggers that correspond to led to desired actions
    for i in range (0, len(t_cueend)):
        
        # for case of first and last cues
        if i == 0:
            a = t_cuestart[i]
            triggers_in_interval = np.where(t_emgtrigger <= a) [0]
            if triggers_in_interval.size > 0:
                nbfalsetriggers_int = triggers_in_interval.size
                nb_failedtriggers = nb_failedtriggers + nbfalsetriggers_int

        if i == len(t_cueend)-1:
            b = t_cuestart[i]
            triggers_in_interval = np.where(t_emgtrigger > b) [0]

            if triggers_in_interval.size == 0:
                nb_failedtriggers = nb_failedtriggers + 1
                
            if triggers_in_interval.size == 1:
                t_usedaudio = np.append(t_usedaudio, t_cueend[i])
                t_correct_emg = np.append(t_correct_emg, t_emgtrigger[triggers_in_interval])
                
            if triggers_in_interval.size > 1:
                nbfalsetriggers_int = len(triggers_in_interval) - 1
                nb_falsetriggers = nb_falsetriggers + nbfalsetriggers_int
                t_correct_emg = np.append(t_correct_emg, t_emgtrigger[triggers_in_interval[0]])
                t_usedaudio = np.append(t_usedaudio, t_cueend[i])
                
        # for in between     
        if 0 <= i < len(t_cueend)-1:              
            a = t_cuestart[i]
            b = t_cuestart[i+1]
        
            triggers_in_interval = np.where(np.logical_and(t_emgtrigger>= a, t_emgtrigger< b)) [0] 

            if triggers_in_interval.size == 1:
                t_usedaudio = np.append(t_usedaudio, t_cueend[i])
                t_correct_emg = np.append(t_correct_emg, t_emgtrigger[triggers_in_interval])

            if triggers_in_interval.size > 1:
                nbfalsetriggers_int = triggers_in_interval.size - 1
                nb_falsetriggers = nb_falsetriggers + nbfalsetriggers_int 
                t_usedaudio = np.append(t_usedaudio, t_cueend[i])
                first_trigger_idx = triggers_in_interval[0]
                t_correct_emg = np.append(t_correct_emg, t_emgtrigger[first_trigger_idx])
            
            if triggers_in_interval.size == 0:
                nb_failedtriggers = nb_failedtriggers + 1

    tdelays_cueemg = t_correct_emg - t_usedaudio
    # tdelays_cueemg = tdelays_cueemg[:,np.newaxis]     
    ave_tdelays_cueemg = np.mean(tdelays_cueemg)
    std_tdelays_cueemg = np.std(tdelays_cueemg, ddof=1)
    CRate = len(t_correct_emg)/ len(t_cuestart) * 100

    # TP = len(t_correct_emg)
    # FP = nb_falsetriggers #false positives, triggers without audio
    # FN = nb_failedtriggers #false negative, should have been positive, tried to trigger but couldn't (i odnt know exact number, as in person may have attempted more!)
    # TPR = TP / (TP + FN) * 100
    # FPR = FP / (FP + all neg?) * 100

    #make it print results with name of action that is evaluating
    print('    Completion Rate: ', np.round(CRate,2))
    print('    Mean time delay: ', np.round(ave_tdelays_cueemg,2))
    print('    Std: ', np.round(std_tdelays_cueemg,2))
    print('    Nb false triggers: ', nb_falsetriggers)
    print('    Nb failed triggers: ', nb_failedtriggers)

    return tdelays_cueemg, CRate, nb_falsetriggers, nb_failedtriggers

print('startRobotMotion: ')
start_tdelay, start_CR, start_falsetrig, start_failedtrig = eval_performance(y_start, startRobotMotion)
print('pauseRobotMotion: ')
pause_tdelay, pause_CR, pause_falsetrig, pause_failedtrig = eval_performance(y_pause, pauseRobotMotion)
print('activategripper: ')
grasp_tdelay, grasp_CR, grasp_falsetrig, grasp_failedtrig = eval_performance(y_grasp, activategripper)


#cannot plot the 3 like that because they have different lengths

# plot results
tdelay_data = [start_tdelay.tolist(), pause_tdelay.tolist(), grasp_tdelay.tolist()]

fig = plt.figure(figsize =(4, 3)) 
ax = fig.add_subplot(111) 

ax.set_xticklabels(['Start\nN='+ str(len(start_tdelay)), 'Pause\nN=' +str(len(pause_tdelay)), 'Grasp\nN=' +str(len(grasp_tdelay))]) 
labelsize = 7
plt.rcParams['xtick.labelsize'] = labelsize
plt.rcParams['ytick.labelsize'] = labelsize

plt.title("Time delay from EMG trigger detection to Robot activation [s]", fontsize = 8)
bp = ax.boxplot(tdelay_data, vert = True, patch_artist = True)

colors = ['lightgreen', 'red','blue'] 
for patch, color in zip(bp['boxes'], colors): 
    patch.set_facecolor(color) 
for median in bp['medians']: 
    median.set(color ='black', linewidth = 1) 

plt.show()



def ccplot(x):
    plt.figure()
    t = np.arange(len(x))
    plt.plot(t,x)
    plt.show()