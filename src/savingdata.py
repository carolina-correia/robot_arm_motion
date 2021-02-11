#!/usr/bin/env python

import os
# os.environ['FOR_DISABLE_CONSOLE_CTRL_HANDLER'] = '1' #to prevent conflict with ctr-c handling between modules

import sys
import pathlib
import numpy as np
import rospy
from std_msgs.msg import Int32
from std_msgs.msg import Float32MultiArray

def dataListener():



def save_and_process_node():
    
    # initialize node
    rospy.init_node('save_and_process_node', anonymous=True)

    # subscribe to data vec topic from doa
    sub = rospy.Subscriber("/dataTosave", Float32MultiArray, dataListener) 

    
    rate = rospy.Rate(10)
    msg = Int32()

    # import fake labels 
    labels_dir = '/home/carolina/EMGacquire/fakelabels_test.csv'
    labels = np.genfromtxt(labels_dir, delimiter=',')

    i = 0

    # Acquire, process time-windows of emg data and make label predictions 
    while not rospy.is_shutdown():  

        try: 

            if i < len(labels):
                msg.data = int(labels[i]) 
                pub.publish(msg) # publish msg to topic1
                print(msg.data)      
                i += 1

        # if Ctrl+C is pressed interrupt the loop
        except KeyboardInterrupt:
            return

        rate.sleep()


