/**
 * 
 *  Developer:  Iason Batzianoulis
 *  email:      iasonbatz@gmail.com
 *  
 *  
*/

#include <iostream>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <Eigen/Eigen>
#include <Eigen/Geometry>

#include "ros/ros.h"

#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Quaternion.h"
#include "geometry_msgs/Twist.h"
#include "sensor_msgs/JointState.h"
#include "visualization_msgs/Marker.h"
#include <geometry_msgs/PoseStamped.h>

#include "Utils.h"
#include <robot_arm_motion/obstacle_msg.h>

bool _firstRealPoseReceived = false;

Eigen::Vector3f _eePosition; // target position (for the end-effector of the robot)
Eigen::Vector4f _eeOrientation; // target orientation (for the end-effector of the robot)
Eigen::MatrixXf _eeRotMat; // target's rotation matrix (according to the frame on the base of the robot)

Eigen::Vector3f tool_translation;
Eigen::Matrix4f tool_transformation_mat;

int parse_obs_params(std::string position_param,
    std::string orientation_param,
    std::string alpha_param,
    std::string power_param,
    std::vector<visualization_msgs::Marker>* obs_markers,
    robot_arm_motion::obstacle_msg* obst_msg)
{

    // load ros-parameters related to the obstacles

    // obstacles position
    XmlRpc::XmlRpcValue obstacles_pos_param;
    ros::param::get("/target_obstacles_sim/" + position_param, obstacles_pos_param);
    ROS_ASSERT(obstacles_pos_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // obstacles orientation
    XmlRpc::XmlRpcValue obstacles_orient_param;
    ros::param::get("/target_obstacles_sim/" + orientation_param, obstacles_orient_param);
    ROS_ASSERT(obstacles_orient_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // obstacles alpha parameters
    XmlRpc::XmlRpcValue obstacles_alpha_param;
    ros::param::get("/target_obstacles_sim/" + alpha_param, obstacles_alpha_param);
    ROS_ASSERT(obstacles_alpha_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // obstacles power parameters
    XmlRpc::XmlRpcValue obstacles_power_param;
    ros::param::get("/target_obstacles_sim/" + power_param, obstacles_power_param);
    ROS_ASSERT(obstacles_power_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // check that all the parameters have the same length (as the number of obstacles)
    size_t nb_obstacles = obstacles_pos_param.size();
    if (obstacles_orient_param.size() != nb_obstacles) {
        std::cout << "[obstacle-target simulation] The number of obstacles in position parameter (rows) does not match the number of rows of the orientation parameters" << std::endl;
        return -1;
    }

    if (obstacles_alpha_param.size() != nb_obstacles) {
        std::cout << "[obstacle-target simulation] The number of obstacles in position parameter (rows) does not match the number of rows of the alpha parameters" << std::endl;
        return -2;
    }

    if (obstacles_power_param.size() != nb_obstacles) {
        std::cout << "[obstacle-target simulation] The number of obstacles in position parameter (rows) does not match the number of rows of the power parameters" << std::endl;
        return -3;
    }

    // setting the size of the obsacles in the obstacle message
    obst_msg->obstacles.resize(nb_obstacles);

    // parsing the parameters to the corresponding messages for each obstacle
    for (int32_t i = 0; i < nb_obstacles; ++i) {
        visualization_msgs::Marker obs_marker;
        obs_marker.header.frame_id = "/world";
        obs_marker.header.stamp = ros::Time();
        obs_marker.ns = "obstacle_"; // setting the name-space
        obs_marker.id = i; // setting the id of the marker
        obs_marker.type = visualization_msgs::Marker::SPHERE; // setting the shape of the marker
        obs_marker.action = visualization_msgs::Marker::ADD; // add this marker to the existing ones

        // parsing the position
        if (obstacles_pos_param[i].size() != 3) {
            std::cout << "[obstacle-target simulation] The position coordinates for the " << i << " obstacle are not equal to 3" << std::endl;
            return -1 - i * 10;
        }
        // to the marker message
        obs_marker.pose.position.x = static_cast<double>(obstacles_pos_param[i][0]);
        obs_marker.pose.position.y = static_cast<double>(obstacles_pos_param[i][1]);
        obs_marker.pose.position.z = static_cast<double>(obstacles_pos_param[i][2]);

        // to the obstacle message
        obst_msg->obstacles[i].pose.position.x = obs_marker.pose.position.x;
        obst_msg->obstacles[i].pose.position.y = obs_marker.pose.position.y;
        obst_msg->obstacles[i].pose.position.z = obs_marker.pose.position.z;

        // parsing the orientation
        if (obstacles_orient_param[i].size() != 4) {
            std::cout << "[obstacle-target simulation] The orientation coordinates for the " << i << " obstacle are not equal to 4" << std::endl;
            return -2 - i * 10;
        }
        //  to the marker message
        obs_marker.pose.orientation.x = static_cast<double>(obstacles_orient_param[i][1]);
        obs_marker.pose.orientation.y = static_cast<double>(obstacles_orient_param[i][2]);
        obs_marker.pose.orientation.z = static_cast<double>(obstacles_orient_param[i][3]);
        obs_marker.pose.orientation.w = static_cast<double>(obstacles_orient_param[i][0]);

        // to the obstacle message
        obst_msg->obstacles[i].pose.orientation.w = obs_marker.pose.orientation.w;
        obst_msg->obstacles[i].pose.orientation.x = obs_marker.pose.orientation.x;
        obst_msg->obstacles[i].pose.orientation.y = obs_marker.pose.orientation.y;
        obst_msg->obstacles[i].pose.orientation.z = obs_marker.pose.orientation.z;

        // parsing the alpha parameters
        if (obstacles_alpha_param[i].size() != 3) {
            std::cout << "[obstacle-target simulation] The alpha parameters for the " << i << " obstacle are not equal to 3" << std::endl;
            return -3 - i * 10;
        }
        // to the marker message
        obs_marker.scale.x = static_cast<double>(obstacles_alpha_param[i][0]);
        obs_marker.scale.y = static_cast<double>(obstacles_alpha_param[i][1]);
        obs_marker.scale.z = static_cast<double>(obstacles_alpha_param[i][2]);

        // to the obstacle message
        obst_msg->obstacles[i].alpha[0] = obs_marker.scale.x;
        obst_msg->obstacles[i].alpha[1] = obs_marker.scale.y;
        obst_msg->obstacles[i].alpha[2] = obs_marker.scale.z;

        // setting the color (rbg) and the opacity (a) of the obstacle
        obs_marker.color.a = 0.7;
        obs_marker.color.r = 0.0;
        obs_marker.color.g = 1.0;
        obs_marker.color.b = 0.0;
        obs_marker.lifetime = ros::Duration();

        obs_markers->push_back(obs_marker);

        // parsing the alpha parameters
        if (obstacles_power_param[i].size() != 3) {
            std::cout << "[obstacle-target simulation] The power parameters for the " << i << " obstacle are not equal to 3" << std::endl;
            return -4 - i * 10;
        }
        // to the obstacle message
        obst_msg->obstacles[i].power_terms[0] = static_cast<double>(obstacles_power_param[i][0]);
        obst_msg->obstacles[i].power_terms[1] = static_cast<double>(obstacles_power_param[i][1]);
        obst_msg->obstacles[i].power_terms[2] = static_cast<double>(obstacles_power_param[i][2]);
    }

    return 0;
}

int parse_target_params(std::string position_param,
    std::string orientation_param,
    // visualization_msgs::Marker* tgt_marker,
    // geometry_msgs::Pose* tgt_msg,
    std::vector<visualization_msgs::Marker>* tgt_markers,
    std::vector<geometry_msgs::Pose>* tgt_msgs)
{
    // load ros-parameters related to the target

    // target position
    XmlRpc::XmlRpcValue target_pos_param;
    ros::param::get("/target_obstacles_sim/" + position_param, target_pos_param);
    ROS_ASSERT(target_pos_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // if (target_pos_param[0].getType() == XmlRpc::XmlRpcValue::TypeArray) {
    //     std::cout << "[obstacle-target simulation] Multiple targets are not supported. The target position and orientation should contain only one row." << std::endl;
    //     return -1;
    // }

    // target orientation
    XmlRpc::XmlRpcValue target_orient_param;
    ros::param::get("/target_obstacles_sim/" + orientation_param, target_orient_param);
    ROS_ASSERT(target_orient_param.getType() == XmlRpc::XmlRpcValue::TypeArray);

    // check that all the parameters have the same length (as the number of tergets)
    size_t nb_targets = target_pos_param.size();
    if (target_orient_param.size() != nb_targets) {
        std::cout << "[obstacle-target simulation] The number of tergets in position parameter (rows) does not match the number of rows of the orientation parameters" << std::endl;
        return -1;
    }

    for (size_t i = 0; i < nb_targets; ++i) {
        visualization_msgs::Marker tgt_marker;
        geometry_msgs::Pose tgt_msg;

        // set the target's visualization parameters
        tgt_marker.header.frame_id = "/world";
        tgt_marker.header.stamp = ros::Time();
        tgt_marker.ns = "target_marker_";
        tgt_marker.id = i;
        tgt_marker.type = visualization_msgs::Marker::CYLINDER;
        tgt_marker.action = visualization_msgs::Marker::ADD;

        // parsing the position
        if (target_pos_param[i].size() != 3) {
            std::cout << "[obstacle-target simulation] The position coordinates of the target " << i << "are not equal to 3" << std::endl;
            return -1 - i * 10;
        }

        // to the message of the target
        tgt_msg.position.x = static_cast<double>(target_pos_param[i][0]);
        tgt_msg.position.y = static_cast<double>(target_pos_param[i][1]);
        tgt_msg.position.z = static_cast<double>(target_pos_param[i][2]);

        // to the message of the marker
        tgt_marker.pose.position.x = tgt_msg.position.x;
        tgt_marker.pose.position.y = tgt_msg.position.y;
        tgt_marker.pose.position.z = tgt_msg.position.z;

        // parsing the orientation
        if (target_orient_param[i].size() != 4) {
            std::cout << "[obstacle-target simulation] The orientation coordinates of the target " << i << " are not equal to 4" << std::endl;
            return -2 - i * 10;
        }

        // to the message of the target
        tgt_msg.orientation.w = static_cast<double>(target_orient_param[i][0]);
        tgt_msg.orientation.x = static_cast<double>(target_orient_param[i][1]);
        tgt_msg.orientation.y = static_cast<double>(target_orient_param[i][2]);
        tgt_msg.orientation.z = static_cast<double>(target_orient_param[i][3]);

        tgt_marker.pose.orientation.x = tgt_msg.orientation.x;
        tgt_marker.pose.orientation.y = tgt_msg.orientation.y;
        tgt_marker.pose.orientation.z = tgt_msg.orientation.z;
        tgt_marker.pose.orientation.w = tgt_msg.orientation.w;

        // setting the scale, opacity and color of the marker of the target
        tgt_marker.scale.x = 0.07;
        tgt_marker.scale.y = 0.07;
        tgt_marker.scale.z = 0.01;
        tgt_marker.color.a = 0.9;
        tgt_marker.color.r = 0.0;
        tgt_marker.color.g = 0.4;
        tgt_marker.color.b = 0.9;
        tgt_marker.lifetime = ros::Duration();

        // append the new marker at the end of the vector of target markers
        tgt_markers->push_back(tgt_marker);

        // append the new target's geometry_msgs::Pose message at the end of the vector of target messages
        tgt_msgs->push_back(tgt_msg);
    }

    return 0;
}

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

int main(int argc, char** argv)
{
    // initialize ros node
    ros::init(argc, argv, "fake_target_publisher");

    // define ros node-handler
    ros::NodeHandle n;

    // subscribers for listening the pose (position-orientation) for the end-effector of the robot
    ros::Subscriber robotSub = n.subscribe("/lwr/ee_pose", 1, robotListener);

    // define ros-publisher for the target
    ros::Publisher _pubTarget = n.advertise<geometry_msgs::Pose>("/target", 1);

    // define ros-publisher for the obstacles
    ros::Publisher _pubOstacles = n.advertise<robot_arm_motion::obstacle_msg>("/obstacles", 1);

    ros::Publisher vis_pub = n.advertise<visualization_msgs::Marker>("/obstacles_visualization", 10);

    // define ros-message for the obstacles
    robot_arm_motion::obstacle_msg obst_msg;

    // define vector of marker messages for the visualization of the obstacles
    std::vector<visualization_msgs::Marker> obs_marker_msgs;

    // define the message for the target
    std::vector<geometry_msgs::Pose> target_msg;

    // define marker message for visualizing the target
    std::vector<visualization_msgs::Marker> target_marker;

    // fill the messeges with the parameters of the obstacles
    if (parse_obs_params("ram_obstacles_position", "ram_obstacles_orientation", "ram_obstacles_alpha", "ram_obstacles_power_params", &obs_marker_msgs, &obst_msg) < 0) {
        std::cout << "[obstacle-target simulation] Correct the obstacles parameters in yaml file and launch again the node" << std::endl;
        return -1;
    }

    // fill the messeges with the parameters of the target
    if (parse_target_params("ram_target_position", "ram_target_orientation", &target_marker, &target_msg) < 0) {
        std::cout << "[obstacle-target simulation] Correct the obstacles parameters in yaml file and launch again the node" << std::endl;
        return -2;
    }

    // set the tool translation from the robot ee to the tip of the tool (or center point the two fingers of the gripper)
    tool_translation << 0.0f, 0.0f, 0.16f;

    // set the transformation matrix from the robot ee to the tip of the tool (or center point the two fingers of the gripper)
    tool_transformation_mat = Eigen::Matrix4f::Identity(4, 4);
    tool_transformation_mat.col(3).head(3) = tool_translation;

    // set initial target choice
    int currentTarget = 0;
    // set distance threshold from target
    float target_dis_threshold = 0.09;

    Eigen::Vector3f currentTarget_pos(3);
    currentTarget_pos << target_msg[currentTarget].position.x, target_msg[currentTarget].position.y, target_msg[currentTarget].position.z;

    ros::Rate loop_rate(100);

    while (ros::ok()) {

        // publish the messages

        _pubTarget.publish(target_msg[currentTarget]);

        obst_msg.header.stamp = ros::Time::now();

        _pubOstacles.publish(obst_msg);

        for (size_t i = 0; i < obs_marker_msgs.size(); i++) {
            obs_marker_msgs[i].header.stamp = ros::Time::now();
            vis_pub.publish(obs_marker_msgs[i]);
        }

        for (size_t i = 0; i < target_marker.size(); i++) {
            target_marker[i].header.stamp = ros::Time::now();
            vis_pub.publish(target_marker[i]);
        }

        if (_firstRealPoseReceived) {
            // std::cout << "distance: " << (currentTarget_pos - _eePosition).norm() << std::endl;
            if ((currentTarget_pos - _eePosition).norm() <= target_dis_threshold) {
                std::cout << "[obstacle-target simulation] Switching target from " << currentTarget << " to ";
                currentTarget++;
                if (currentTarget >= target_msg.size()) {
                    currentTarget = 0;
                }
                currentTarget_pos << target_msg[currentTarget].position.x, target_msg[currentTarget].position.y, target_msg[currentTarget].position.z;
                std::cout << currentTarget + 1 << std::endl;
            }
        }
        ros::spinOnce();
        loop_rate.sleep();
        // ++count;
    }

    return 0;
}