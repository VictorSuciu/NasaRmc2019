/**
 * controller_launcher.cpp
 * 
 * This is the primary node for the control package, it registers the controller
 * instance with the controller manager and handles calling updates on the
 * hardware.
 */
#include <ros/ros.h>
#include <urdf/model.h>
#include <sstream>
#include <controller_manager/controller_manager.h>
#include "controller.h"

// Whether we're running on hardware or using fake values
const bool use_fake_values = true;

int main(int argc, char **argv)
{
    // "controller_launcher is the name of this node at run-time"
    ros::init(argc, argv, "controller_launcher");
    ros::NodeHandle n;

    // Start a spinner with one thread, we don't need any more than that
    // This is actually required (vs calling ros::spinOnce() in the loop)
    // by ControllerManager for some reason, as I found in testing.
    ros::AsyncSpinner spinner(1);
    spinner.start();

    // If we're faking the inputs, we need to know the model constraints on
    // the arm: load them here.
    // If not, just use zeroes, the limits don't matter.
    double lower_limits[tfr_control::Controller::kControllerCount] = {}, upper_limits[tfr_control::Controller::kControllerCount] = {};

    if (use_fake_values) {
        // Get the model description 
        std::string desc;
        n.param<std::string>("robot_description", desc, "");
    
        if (desc.length() == 0) {
            ROS_ERROR("robot_description is empty and controller_launcher is using fake values, quitting.");
            return -1;
        }

        urdf::Model model;
        if (!model.initString(desc)) {
            ROS_ERROR("Couldn't load robot_description, quitting.");
            return -1;
        }

        ROS_INFO("Model loaded successfully, loading joint limits.");
        lower_limits[tfr_control::Actuator::kBin] = model.getJoint("bin_joint")->limits->lower;
        upper_limits[tfr_control::Actuator::kBin] = model.getJoint("bin_joint")->limits->upper;
        lower_limits[tfr_control::Actuator::kLowerArm] = model.getJoint("lower_arm_joint")->limits->lower;
        upper_limits[tfr_control::Actuator::kLowerArm] = model.getJoint("lower_arm_joint")->limits->upper;
        lower_limits[tfr_control::Actuator::kUpperArm] = model.getJoint("upper_arm_joint")->limits->lower;
        upper_limits[tfr_control::Actuator::kUpperArm] = model.getJoint("upper_arm_joint")->limits->upper;
        lower_limits[tfr_control::Actuator::kScoop] = model.getJoint("scoop_joint")->limits->lower;
        upper_limits[tfr_control::Actuator::kScoop] = model.getJoint("scoop_joint")->limits->upper;
    }

    // The controller that we'll be registering with the controller_manager
    tfr_control::Controller controller(use_fake_values, lower_limits, upper_limits);

    // Register our controller with the controller_manager
    controller_manager::ControllerManager cm(&controller);

    // Store the time of the last update to feed to ControllerManager
    ros::Time then = ros::Time::now();

    while (ros::ok())
    {
        const ros::Time now = ros::Time::now();

        // Read to and write from hardware based on values from the
        // controller_manager
        controller.read();
        cm.update(now, now - then);
        controller.write();

        // Update the times so that we can keep an accurate measurement between
        // update cycles
        then = now;
    }
    return 0;
}