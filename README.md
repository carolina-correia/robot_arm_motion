# Robot-Arm motion

Robot-Arm motion is a package for controlling the rnd effector of a robot arm. Up to this point, it contains 

1) trajectory generation through a dynamical system with objstacle avoidance and
2) trajectory generation through a linear DS, accepting a mocap marker as a target

Lisence: GNU GPL v3

Copyright (c) 2020 Iason Batzianoulis

## Dependencies

- mathlinb library from [here](https://github.com/epfl-lasa/mathlib.git)

- utils library from [here](https://github.com/walidAmanhoud/utils.git)

## Installation

Navigate into the src folder of your ROS workspace and type in the terminal:

```bash
$git clone https://github.com/epfl-lasa/mathlib.git
$git clone https://github.com/walidAmanhoud/utils.git
$git clone https://github.com/yias/robot_arm_motion.git
```

Navigate back to the main folder of the caktin workspace and compile the packages

```bash
$catkin_make
```

## Running the controllers

- For the robot to follow a mocap marker, type:

```bash
$roslaunch robot_arm_motion markerTarget.launch
```

Optional (recommended) arguments:


          markerName:=hand|ball|... the name of the marker to follow as it is broadcasted from the mocap system (default: "hand")
          robotMarker:=robot_right|robot_left ... the name of the marker that corresponds to the robot base as it is broadcasted from the mocap system (default: "robot_right")
          robotName:=lwr|iiwa|franka ... the name of the robot (default:= "lwr")
          serverIP:=128.160.0.1|... the IP of the PC that runs the mocap server (Motive from Optitrack)

- For the robot to move to a target by avoiding obstacles, type:

```bash
$rosrun robot_arm_motion obstacleAvoidance
```

To run a simulation for the obstacle avoidance, run:
```bash
$roslaunch robot_arm_motion kuka_obstacles.launch
```

and from another terminal, run:
```bash
$rosrun robot_arm_motion obstacleAvoidance
```

- For the integration with gaze-tracker and object detection (the package is in the folder gaze of the eurekaRes repo [here](https://github.com/yias/eurekaRes)), run:

```bash
$roslaunch robot_arm_motion gaze_obstacle_avoidance.launch
```

and on another terminal, run:
```bash
$rosrun robot_arm_motion obstacleAvoidance
```
