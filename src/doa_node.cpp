/**
 * 
 *  Developers:  Iason Batzianoulis
 *               Carolina Coreia
 *  email:       iasonbatz@gmail.com
 *               carolina.correia@epfl.ch
 *  
 *  
*/

#include "ros/ros.h"
#include "std_msgs/Int32.h"
#include "std_msgs/Int8.h"
#include "std_msgs/String.h"
#include "std_msgs/Int32MultiArray.h"

#include "sensor_msgs/JointState.h"
#include <geometry_msgs/PoseStamped.h>

#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Quaternion.h"
#include "geometry_msgs/Twist.h"

#include "nav_msgs/Path.h"
#include <visualization_msgs/Marker.h>

#include <robot_arm_motion/obstacle_msg.h>

#include "DSObstacleAvoidance.h"

#include <sstream>
#include "Utils.h"

#include <Eigen/Dense>
#include <Eigen/Eigen>
#include <Eigen/Geometry>

#include <robotiq_2f_gripper_control/Robotiq2FGripper_robot_input.h>

#include <algorithm>
#include <numeric>
#include <fstream>
#include <ros/package.h>

bool _firstRealPoseReceived = false;
bool _firstObstacleReceived = false;
bool _firstTargetPoseReceived = false;
bool _firstAudioLabels = false;

Eigen::VectorXf _eePosition(3); // robot's end-effector position
Eigen::Matrix3f _eeRotMat; // robot's end-effector rotation matrix (according to the frame on the base of the robot)
Eigen::Vector4f _eeOrientation; // robot's end-effector orientation

Eigen::VectorXf _targetPosition(3); // target position (for the end-effector of the robot)
Eigen::VectorXf _targetOrientation(4); // target orientation (for the end-effector of the robot)
Eigen::MatrixXf _targetRotMatrix; // target's rotation matrix (according to the frame on the base of the robot)
Eigen::Vector3f _v; // generated end-effector velocity (from the DS) [m/s] (3x1)

std::vector<Obstacle> obstacles;

Eigen::Vector3f tool_translation;
Eigen::Matrix4f tool_transformation_mat;

std_msgs::Int32 predicted_label;
std_msgs::Int32 gripper_position;
std_msgs::Int32 gripper_status;

bool _startRobotMotion = false;
bool _pauseRobotMotion = false;
bool _activateGripper = false;
int GripperState = 0; // 0 is opened, 1 is closed. starts open

std::ofstream _outputFile; 
std::vector<int> _audio_labels(3);

// bool isGripperOpen = false;
// bool isGripperClosed = false;
// bool gripperStoppedClosing = false;
// bool gripperStoppedOpening = false;


void robotListener(const geometry_msgs::Pose::ConstPtr& msg)
{
    Eigen::Vector4f tmp_ee;
    Eigen::Matrix4f ee_tranformation;
    ee_tranformation = Eigen::Matrix4f::Identity(4, 4);
    tmp_ee << msg->position.x, msg->position.y, msg->position.z, 1.0f;

    _eeOrientation << msg->orientation.w, msg->orientation.x, msg->orientation.y, msg->orientation.z;
    _eeRotMat = Utils<float>::quaternionToRotationMatrix(_eeOrientation);

    ee_tranformation.col(3) = tmp_ee;
    ee_tranformation.block(0, 0, 3, 3) = _eeRotMat;

    Eigen::Matrix4f tt = ee_tranformation * tool_transformation_mat;
    _eePosition = tt.col(3).head(3);

    if (!_firstRealPoseReceived) {
        _firstRealPoseReceived = true;
    }
}

void updateRealTwist(const geometry_msgs::Twist::ConstPtr& msg)
{
    _v << msg->linear.x, msg->linear.y, msg->linear.z;
}

void targetListener(const geometry_msgs::Pose::ConstPtr& msg)
{

    _targetPosition << msg->position.x, msg->position.y, msg->position.z;
    _targetOrientation << msg->orientation.w, msg->orientation.x, msg->orientation.y, msg->orientation.z;
    _targetRotMatrix = Utils<float>::quaternionToRotationMatrix(_targetOrientation);
    if (!_firstTargetPoseReceived) {
        _firstTargetPoseReceived = true;
        ROS_INFO("[robot_arm_motion:doa] Target Pose received\n");
    }
}

void obstacleListener(const robot_arm_motion::obstacle_msg& msg)
{

    obstacles = std::vector<Obstacle>(msg.obstacles.size());

    for (int i = 0; i < msg.obstacles.size(); i++) {
        // set the obstacle parameters
        obstacles[i]._x0 << msg.obstacles[i].pose.position.x, msg.obstacles[i].pose.position.y, msg.obstacles[i].pose.position.z; // obstacle position
        obstacles[i]._a << msg.obstacles[i].alpha[0], msg.obstacles[i].alpha[1], msg.obstacles[i].alpha[2]; // obstacle axis lengths
        obstacles[i]._p << msg.obstacles[i].power_terms[0], msg.obstacles[i].power_terms[1], msg.obstacles[i].power_terms[2]; // obstacle power terms
        obstacles[i]._safetyFactor = 1.1; // safety factor
        obstacles[i]._rho = 1.1; // reactivity
        obstacles[i]._tailEffect = false;
        obstacles[i]._bContour = true;
    }

    if (!_firstObstacleReceived) {
        _firstObstacleReceived = true;
        ROS_INFO("[robot_arm_motion:doa] Obstacles' poses received\n");
    }
}

void emgLabelListener(const std_msgs::Int32::ConstPtr& msg)
{
    predicted_label.data = msg->data;

    if (predicted_label.data == 1) {
        _startRobotMotion = true;
        _pauseRobotMotion = false;
    }

    if (predicted_label.data == 2) {
        _startRobotMotion = false;
        _pauseRobotMotion = true;
    }

    // if (predicted_label.data == 5) {
    //     _activateGripper = true;
    // }

    ROS_INFO("Classification output received: [%i]", msg->data);
}

void gripperListener(const robotiq_2f_gripper_control::Robotiq2FGripper_robot_input& msg)
{
    gripper_position.data = msg.gPO;
    gripper_status.data = msg.gOBJ;

    if (gripper_position.data <= 20) {
        GripperState = 0; // open
    }

    if (gripper_position.data >= 200) {
        GripperState = 1; // closed
    }

    // if (gripper_status == 2) {
    //     gripperStoppedClosing = true; //due to contact with object
    // }

    // if (gripper_status == 1) {
    //     gripperStoppedOpening = true; //due to contact with object
    // }
    ROS_INFO("Gripper_position received: [%i] \n", gripper_position.data);
}

void audiocuesListener(const std_msgs::Int32MultiArray::ConstPtr& msg){

    if(!_firstAudioLabels){
        _firstAudioLabels = true;
        ROS_INFO("[robot_arm_motion:doa] Audio labels received\n");
    }
    
    _audio_labels[0]= msg->data[0]; 
    _audio_labels[1]= msg->data[1];
    _audio_labels[2]= msg->data[2];

    std::cout << "y_start: " << _audio_labels[0] << "\n";
    std::cout << "y_pause: " << _audio_labels[1] << "\n";
    std::cout << "y_grasp: " << _audio_labels[2] << "\n";

}


void logData(std::vector<double> data)
{

    for(int k = 0; k < data.size(); k++)
    {
        _outputFile << data[k] << " ";
    }
    _outputFile << std::endl;
}


int main(int argc, char** argv)
{

    // initialize ros-node
    ros::init(argc, argv, "obstacle_avoidance");

    // ros node-handler
    ros::NodeHandle n;

    // subscribers for listening the pose (position-orientation) for the end-effector of the robot
    // ros::Subscriber robotSub=n.subscribe("IIWA/Real_E_Pos", 10, robotListener);
    ros::Subscriber robotSub = n.subscribe("/lwr/ee_pose", 10, robotListener);

    // subscriber for listening the velocity of the end-effector (ee) of the robot
    ros::Subscriber _subRealTwist = n.subscribe("/lwr/ee_vel", 1, updateRealTwist);

    // subscriber for receiving the target
    ros::Subscriber _targetSub = n.subscribe("/target", 1, targetListener);

    // subscriber for receiving the obstacles
    ros::Subscriber _obstaclesSub = n.subscribe("/obstacles", 1, obstacleListener);

    // subscribe to true or fake labels of emg classification
    // ros::Subscriber _emgclassifSub = n.subscribe("/predictedLabel", 1, emgLabelListener);
    ros::Subscriber _emgclassifSub = n.subscribe("/fakeLabels", 1, emgLabelListener);

    // subscriber for receiving gripper
    ros::Subscriber _gripperSub = n.subscribe("/gripper/Robotiq2FGripperRobotInput", 1, gripperListener);

    // subscribe to audio cues (ground truth)
    ros::Subscriber _audiocuesSub = n.subscribe("/audiocues", 1, audiocuesListener);

    // define a publisher for visualizing the trajectory of the robot ee
    ros::Publisher marker_pub = n.advertise<visualization_msgs::Marker>("ee_trajectory", 10);

    ros::Publisher _pubDesiredPose = n.advertise<geometry_msgs::Twist>("/lwr/joint_controllers/passive_ds_command_vel", 10);
    ros::Publisher _pubDesiredOrientation = n.advertise<geometry_msgs::Quaternion>("/lwr/joint_controllers/passive_ds_command_orient", 1);
    ros::Publisher _pubGripperCommand = n.advertise<std_msgs::Int8>("/gripper/command", 10);

    // define a message for the desired velocity
    geometry_msgs::Twist _msgDesiredPose;

    // define a message for the desired orientation
    geometry_msgs::Quaternion _msgDesiredOrientation;

    // define a marker for visualizing the trajectory of the robot ee
    visualization_msgs::Marker traj_line;

    std_msgs::Int8 _desGripperCommand;

    traj_line.header.frame_id = "/world";
    traj_line.pose.orientation.w = 1.0;
    traj_line.id = 0;
    traj_line.type = visualization_msgs::Marker::POINTS;
    traj_line.scale.x = 0.005;
    traj_line.scale.y = 0.005;

    // set the color of the trajectory line
    traj_line.color.a = 1.0f;
    traj_line.color.r = 1.0f;
    traj_line.color.b = 1.0f;
    traj_line.color.g = 0.0f;
    traj_line.lifetime = ros::Duration();

    // set the tool translation from the robot ee to the tip of the tool (or center point the two fingers of the gripper)
    tool_translation << 0.0f, 0.0f, 0.16f;

    // set the transformation matrix from the robot ee to the tip of the tool (or center point the two fingers of the gripper)
    tool_transformation_mat = Eigen::Matrix4f::Identity(4, 4);
    tool_transformation_mat.col(3).head(3) = tool_translation;

    // velocity generated by the original linear dynamical system (DS)
    Eigen::VectorXf xD(3);

    // velocity generated by the modulated dynamical system
    Eigen::Vector3f _vd;
    _vd.setConstant(0.0f);

    // velocity orientation
    Eigen::Vector3f velUnitVector;

    // velocity orientation matrix
    Eigen::Matrix3f vel_orient_mat = Eigen::Matrix3f::Zero(3, 3);

    // velocity orientation in quaternions
    Eigen::Vector4f vel_orient_q;

    // define the weight-matrix of the original linear dynamical system
    Eigen::MatrixXf W_M(3, 3);
    W_M << -1.0, 0.0, 0.0,
        0.0, -1.0, 0.0,
        0.0, 0.0, -1.0;

    // define ds-modulator for modulating the dynamical system according to the position and size of the obstacles
    DSObstacleAvoidance obsModulator;

    // emg part
    int count = 0;
    int vec_size = 0;
    int nb_pred = 5;
    int last_label = 7;

    // name of file to save
    std::string filename = "test_audio";
    _outputFile.open(ros::package::getPath(std::string("robot_arm_motion"))+"/data/" + filename + ".txt");

    double t_start = ros::Time::now().toSec();
    ros::Rate loop_rate(100);

    while (ros::ok()) {

        std::vector<double> data_vec(19);
        data_vec[0] = ros::Time::now().toSec() - t_start;  // time


        if (_firstRealPoseReceived && _firstTargetPoseReceived && _firstAudioLabels) {

            // compute the velocity from the original DS
            xD = W_M * (_eePosition - _targetPosition);

            // translation from target
            Eigen::Vector3f tr_vec = _targetPosition - _eePosition;

            // if there are obstacles, modify the velocity according to the modulated DS
            if (_firstObstacleReceived && tr_vec.norm() > 0.1) {
                // add the obstacles to the DS-modulator

                obsModulator.addObstacles(obstacles);

                // compute the velocity from the modulated DS
                _vd = obsModulator.obsModulationEllipsoid(_eePosition, xD, false);

                // clear the obstacles in the modulator, so that the new obstacles will not be appended to the old-ones
                obsModulator.clearObstacles();
            }
            else {
                _vd = xD;
            }


            // Bound desired velocity
            if (_vd.norm() > 0.3f) {
                _vd = _vd * 0.2f / xD.norm();
            }

            // std::cout << "[robot_arm_motion:doa] Speed: " << _vd.norm() << "\n";
            std::cout << "[robot_arm_motion:doa] Distance from target: " << (_eePosition - _targetPosition).norm() << "\n";

            // Start motion if label = 1, pause if label = 2
            if (_startRobotMotion) {
                // std::cout << "Starting robot motion "
                //           << "\n";

                // define desired vel
                _msgDesiredPose.linear.x = _vd[0];
                _msgDesiredPose.linear.y = _vd[1];
                _msgDesiredPose.linear.z = _vd[2];
                _msgDesiredPose.angular.x = 0.0;
                _msgDesiredPose.angular.y = 0.0;
                _msgDesiredPose.angular.z = 0.0;

                // Define desired orientation
                _msgDesiredOrientation.w = _targetOrientation(0);
                _msgDesiredOrientation.x = _targetOrientation(1);
                _msgDesiredOrientation.y = _targetOrientation(2);
                _msgDesiredOrientation.z = _targetOrientation(3);

                _pubDesiredPose.publish(_msgDesiredPose);
                _pubDesiredOrientation.publish(_msgDesiredOrientation);
            }

            if (_pauseRobotMotion) {
                // std::cout << "Orientation of ee [0]: " << _eeOrientation[0] << "\n";
                // std::cout << "Orientation of ee [1]: " << _eeOrientation[1] << "\n";
                // std::cout << "Pausing robot motion "
                //           << "\n";

                // define desired position
                _msgDesiredPose.linear.x = 0.0;
                _msgDesiredPose.linear.y = 0.0;
                _msgDesiredPose.linear.z = 0.0;
                _msgDesiredPose.angular.x = 0.0;
                _msgDesiredPose.angular.y = 0.0;
                _msgDesiredPose.angular.z = 0.0;

                // Define desired orientation
                _msgDesiredOrientation.w = _targetOrientation(0);
                _msgDesiredOrientation.x = _targetOrientation(1);
                _msgDesiredOrientation.y = _targetOrientation(2);
                _msgDesiredOrientation.z = _targetOrientation(3);

                _pubDesiredPose.publish(_msgDesiredPose);
                _pubDesiredOrientation.publish(_msgDesiredOrientation);
            }

            // compute desired velocity orientation
            velUnitVector = tr_vec / tr_vec.norm();

            // compute the velocity orientation in quaternions

            vel_orient_mat = Eigen::AngleAxisf(std::atan(velUnitVector(1) / velUnitVector(0)), Eigen::Vector3f::UnitZ());

            Eigen::Matrix3f vel_rot_mat_x = Eigen::Matrix3f::Zero(3, 3);

            // vel_rot_mat_x = Eigen::AngleAxisf(std::atan2(velUnitVector(2), velUnitVector(0)),Eigen::Vector3f::UnitY());

            vel_rot_mat_x = Eigen::AngleAxisf(M_PI, Eigen::Vector3f::UnitY());

            vel_orient_mat = vel_orient_mat * vel_rot_mat_x;

            vel_orient_q = Utils<float>::rotationMatrixToQuaternion(vel_orient_mat);

            Eigen::Vector4f desOrient = Utils<float>::slerpQuaternion(vel_orient_q, _eeOrientation, 0.8);

            // if (tr_vec.norm() < 0.05){
            //     desOrient = Utils<float>::slerpQuaternion(_targetOrientation, _eeOrientation, 0.8);
            // }

            // update the message with the desired orientation
            _msgDesiredOrientation.w = desOrient(0);
            _msgDesiredOrientation.x = desOrient(1);
            _msgDesiredOrientation.y = desOrient(2);
            _msgDesiredOrientation.z = desOrient(3);

            // Publish desired orientation
            _pubDesiredOrientation.publish(_msgDesiredOrientation);

            // update and publish trajectory-visualization points
            geometry_msgs::Point p;
            p.x = _eePosition(0);
            p.y = _eePosition(1);
            p.z = _eePosition(2);

            traj_line.header.stamp = ros::Time::now();
            traj_line.points.push_back(p);
            marker_pub.publish(traj_line);
        
            // Gripper control:
            // take last 5 predicted labels and check if muscle was activated
            if (last_label == 7 && predicted_label.data == 5) {
                _activateGripper = true;
                std::cout << "Gripper activated"
                        << "\n";
            }
            else {
                _activateGripper = false;
            }

            last_label = predicted_label.data;

            if (_activateGripper && GripperState == 0) {
                _desGripperCommand.data = 1;
                _pubGripperCommand.publish(_desGripperCommand);
                std::cout << "Closing gripper"
                        << "\n";
            }
            if (_activateGripper && GripperState == 1) {
                _desGripperCommand.data = 2;
                _pubGripperCommand.publish(_desGripperCommand);
                std::cout << "Opening gripper"
                        << "\n";
            }

            // saving all data in vector
            data_vec[1] = _eePosition(0);     
            data_vec[2] = _eePosition(1);      
            data_vec[3] = _eePosition(2);      
            data_vec[4] = _targetPosition(0);      
            data_vec[5] = _targetPosition(1);     
            data_vec[6] = _targetPosition(2);      
            data_vec[7] = _vd.norm();                // end effector velocity (norm)
            data_vec[8] = predicted_label.data;      // final emg label
            data_vec[9] = _startRobotMotion;         // if received command to start motion
            data_vec[10] = _pauseRobotMotion;
            data_vec[11] = gripper_position.data;
            data_vec[12] = gripper_status.data;
            data_vec[13] = GripperState;
            data_vec[14] = _activateGripper;
            data_vec[15] = _desGripperCommand.data;
            data_vec[16] = _audio_labels[0]; //y_start;
            data_vec[17] = _audio_labels[1]; //y_pause;
            data_vec[18] = _audio_labels[2]; //y_grasp;

            logData(data_vec);
            data_vec.clear();
        }

        ros::spinOnce();
        loop_rate.sleep();
        ++count;
    }


    return 0;
}