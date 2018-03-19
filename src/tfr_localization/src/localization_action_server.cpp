/*
 * The action server in charge of localizing the robot.
 *
 * Takes in the empty action request, and provides no feedback.
 * Turns until it sees the aruco markers, exits succesfully once it does.
 *
 * Needs access to the image wrapper topic wrapper to fetch images, 
 * name is specified as a parameter.
 *
 * parameters:
 *  - ~turn_speed: how fast to turn [rad/s] (double, default: 0.0)
 *  - ~turn_duration: how long to turn [s] (double, default: 0.0)
 * */

#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>
#include <actionlib/server/simple_action_server.h>
#include <tfr_msgs/ArucoAction.h>
#include <tfr_msgs/EmptyAction.h>
#include <tfr_msgs/WrappedImage.h>
#include <tfr_msgs/LocalizePoint.h>
#include <tfr_utilities/tf_manipulator.h>

class Localizer
{
    public:
        Localizer(ros::NodeHandle &n) : 
            aruco{n, "aruco_action_server"},
            server{n, "localize", boost::bind(&Localizer::localize, this, _1) ,false}

        {
            ROS_INFO("Localization Action Server: Connecting Aruco");
            aruco.waitForServer();
            ROS_INFO("Localization Action Server: Connected Aruco");

            ROS_INFO("Localization Action Server: Connecting Image Client");
            image_client = n.serviceClient<tfr_msgs::WrappedImage>("/on_demand/rear_cam/image_raw");
            tfr_msgs::WrappedImage request{};
            while(!image_client.call(request));
            ROS_INFO("Localization Action Server: Connected Image Client");
            ROS_INFO("Localization Action Server: Starting");
            server.start();
            ROS_INFO("Localization Action Server: Started");
        };
        ~Localizer() = default;
        Localizer(const Localizer&) = delete;
        Localizer& operator=(const Localizer&) = delete;
        Localizer(Localizer&&) = delete;
        Localizer& operator=(Localizer&&) = delete;
    private:
        actionlib::SimpleActionServer<tfr_msgs::EmptyAction> server;
        actionlib::SimpleActionClient<tfr_msgs::ArucoAction> aruco;
        ros::ServiceClient image_client;
        TfManipulator tf_manipulator;

        void localize( const tfr_msgs::EmptyGoalConstPtr &goal)
        {
            ROS_INFO("Localization Action Server: Localize Starting");
            //setup

            //loop
            while (true)
            {
                if (server.isPreemptRequested() || !ros::ok())
                {
                    ROS_INFO("Localization Action Server: preempt requested");
                    server.setPreempted();
                    break;
                }
                tfr_msgs::WrappedImage request{};
                //grab an image
                if(!image_client.call(request))
                {
                    ROS_WARN("Localization Action Server: Could not reach image client");
                    continue;
                }

                tfr_msgs::ArucoGoal goal;
                goal.image = request.response.image;
                goal.camera_info = request.response.camera_info;
                //send it to the server
                aruco.sendGoal(goal);
                if (aruco.waitForResult())
                {
                    //make sure we can see
                    auto result = aruco.getResult();
                    if (result->number_found ==0)
                    {
                        ROS_INFO("Localization Action Server: No markers detected");
                        continue;
                    }
                    //We found something, transform relative to the base
                    geometry_msgs::PoseStamped bin_pose{};
                    if (!tf_manipulator.transform_pose(
                                aruco.getResult()->relative_pose, 
                                bin_pose, 
                                "base_footprint"))
                    {
                        ROS_WARN("Localization Action Server: Transformation failed");
                        continue;
                    }
                    bin_pose.pose.position.y *=-1;
                    bin_pose.pose.position.z *=-1;
                    bin_pose.header.frame_id = "odom";
                    bin_pose.header.stamp = ros::Time::now();

                    //send the message
                    tfr_msgs::LocalizePoint::Request request;
                    request.pose = bin_pose;
                    tfr_msgs::LocalizePoint::Response response;
                    if(ros::service::call("localize_bin", request, response))
                    {
                        ROS_INFO("Localization Action Server: Success");
                        server.setSucceeded();
                        break;
                    }
                    else
                        ROS_INFO("Localization Action Server: retrying to localize movable point");
                }
                else
                    ROS_WARN("Localization Action Server: Could not reach aruco");
            }
            //teardown
            ROS_INFO("Localization Action Server: Localize Finished");
        }
};

int main(int argc, char** argv)
{
    ros::init(argc, argv, "localization_action_server");
    ros::NodeHandle n{};
    double turn_speed, turn_duration;
    ros::param::param<double>("~turn_speed", turn_speed, 0.0);
    ros::param::param<double>("~turn_duration", turn_duration, 0.0);
    if (turn_speed == 0.0 || turn_duration == 0.0)
        ROS_WARN("Localization Action Server: Uninitialized Parameters");
    Localizer localizer(n);
    ros::spin();
    return 0;

}
