/*
 * The teleop action server is  in charge of processing user commands quickly 
 * and performing smooth remote operation for our operations team. All commands
 * for teleoperation will be processed by this server, except for emergency 
 * stop, which is handled by the control system directly for fast response time.
 * 
 * The commands it supports:
 * - None
 * - Move
 *   - Forward
 *   - Backward
 *   - Turn left
 *   - Turn right
 * - Dig
 *   - Executes digging for some duration calculated by the `get_digging_time` 
 *     service, must support preemption.
 * - Dump
 *   - Raising the dumping bin, currently does not support preemption.
 * - Reset Motor state
 *   - Resets the arm, motor and control system to a safe position, ready to 
 *     recieve commands.
 *
 * PRECONDITION
 * The clock service must be up and started, anything else is undefined
 * behavior.
 * 
 * PARAMETERS 
 * -~linear_velocity: the max linear velocity. (double, default: 0.25)
 * -~angular_velocity: the max angular velocity. (double, default: 0.1)
 * - ~rate: the rate in hz to check to preemption during long running calls, (double, default: 10)
 */ 
#include <ros/ros.h>
#include <ros/console.h>
#include <tfr_utilities/teleop_code.h>
#include <tfr_utilities/control_code.h>
#include <tfr_msgs/TeleopAction.h>
#include <tfr_msgs/DiggingAction.h>
#include <tfr_msgs/EmptySrv.h>
#include <tfr_msgs/CodeSrv.h>
#include <tfr_msgs/DurationSrv.h>
#include <geometry_msgs/Twist.h>
#include <std_msgs/Float64.h>
#include <actionlib/server/simple_action_server.h>
#include <actionlib/client/simple_action_client.h>


class TeleopExecutive
{
    public:

        struct DriveVelocity
        {
            private:
                double linear;
                double angular;
            public:
                DriveVelocity(double lin, double ang):
                    linear{lin}, angular{ang}{}
                double getLinear(){return linear;}
                double getAngular(){return angular;}
        };

        TeleopExecutive(ros::NodeHandle &n , DriveVelocity &drive, double f) :
            server{n, "teleop_action_server",
                boost::bind(&TeleopExecutive::processCommand, this, _1),
                false},
            drivebase_publisher{n.advertise<geometry_msgs::Twist>("cmd_vel", 5)},
            bin_publisher{n.advertise<std_msgs::Float64>("/bin_position_controller/command", 5)},
            digging_client{n, "dig"},
            drive_stats{drive},
            frequency{f}
        {
            digging_client.waitForServer();
            server.start();
            ROS_INFO("Teleop Action Server: Online %f", ros::Time::now().toSec());
        }
        ~TeleopExecutive() = default;
        TeleopExecutive(const TeleopExecutive&) = delete;
        TeleopExecutive& operator=(const TeleopExecutive&) = delete;
        TeleopExecutive(TeleopExecutive&&) = delete;
        TeleopExecutive& operator=(TeleopExecutive&&) = delete;

    private:

        /*
         *The main callback for processing user commands
         * 
         * ACTION SPECIFICATION
         * The logic of the processing commands is:
         * 1. unpack the message
         * 2. if dumping dump
         * 2. else if digging process asynchronously while checking for preemption
         * 2. else if reset reset
         * 3. send the most relevant driving message (forward, backward, left, right, or stop)
         * 3. exit
         * 
         * This approach avoids additional threading beyond the action server, and should meet our response requirements.
         * 
         * ACTION MESSAGE
         * - Goal: uint8 command
         * - Feedback: none
         * - Result: none
         * */
        void processCommand(const tfr_msgs::TeleopGoalConstPtr& goal)
        {
            geometry_msgs::Twist move_cmd{};
            auto code = static_cast<tfr_utilities::TeleopCode>(goal->code);
            switch(code)
            {
                case (tfr_utilities::TeleopCode::STOP_DRIVEBASE):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, STOP_DRIVEBASE");
                        //all zeros by default
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::STOP_TURNTABLE):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, STOP_TURNTABLE");
                        //all zeros by default
                        //TODO integrate manual turntable control
                        break;
                    }

                case (tfr_utilities::TeleopCode::FORWARD):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, FORWARD");
                        move_cmd.linear.x = drive_stats.getLinear();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::BACKWARD):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, BACKWARD");
                        move_cmd.linear.x = -drive_stats.getLinear();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::LEFT):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, LEFT");
                        move_cmd.angular.z = drive_stats.getAngular();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::RIGHT):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, RIGHT");
                        move_cmd.angular.z = -drive_stats.getAngular();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::CLOCKWISE):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, CLOCKWISE");
                        move_cmd.angular.z = drive_stats.getAngular();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::COUNTERCLOCKWISE):
                    {
                        ROS_INFO("Teleop Action Server: Command Recieved, COUNTERCLOCKWISE");
                        move_cmd.angular.z = -drive_stats.getAngular();
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                case (tfr_utilities::TeleopCode::DIG):
                    {
                        ROS_INFO("Teleop Action Server: commencing digging");
                        tfr_msgs::DiggingGoal goal{};
                        ROS_INFO("Teleop Action Server: retrieving digging time");
                        tfr_msgs::DurationSrv digging_time;
                        ros::service::call("digging_time", digging_time);
                        ROS_INFO("Teleop Action Server: digging time retreived %f",
                                digging_time.response.duration.toSec());
                        goal.diggingTime = digging_time.response.duration;
                        digging_client.sendGoal(goal);

                        //handle preemption
                        while (!digging_client.getState().isDone())
                        {
                            if (server.isPreemptRequested() || ! ros::ok())
                            {
                                digging_client.cancelAllGoals();
                                server.setPreempted();
                                ROS_INFO("Teleop Action Server: digging preempted");
                                return;
                            }
                            frequency.sleep();
                        }
                        ROS_INFO("Teleop Action Server: digging finished");
                        break;
                    }

                case (tfr_utilities::TeleopCode::DUMP):
                    {
                        drivebase_publisher.publish(move_cmd);
                        ROS_INFO("Teleop Action Server: Command Recieved, DUMP");
                        //all zeros by default
                        std_msgs::Float64 bin_cmd;
                        bin_cmd.data = tfr_utilities::JointAngles::BIN_MAX;
                        tfr_msgs::CodeSrv query;
                        while (!server.isPreemptRequested() && ros::ok())
                        {
                            ros::service::call("bin_state", query);
                            if (static_cast<tfr_utilities::BinCode>(query.response.code) == tfr_utilities::BinCode::RAISED)
                                break;
                            bin_publisher.publish(bin_cmd);
                            frequency.sleep();
                        }
                        if (server.isPreemptRequested())
                        {
                            ROS_INFO("Teleop Action Server: DUMP preempted");
                            server.setPreempted();
                            return;
                        }
                        ROS_INFO("Teleop Action Server: DUMP finished");
                        break;
                    }

                case (tfr_utilities::TeleopCode::RESET_DUMPING):
                    {
                        drivebase_publisher.publish(move_cmd);
                        ROS_INFO("Teleop Action Server: Command Recieved, RESET_DUMPING");
                        //all zeros by default
                        std_msgs::Float64 bin_cmd;
                        bin_cmd.data = tfr_utilities::JointAngles::BIN_MIN;
                        tfr_msgs::CodeSrv query;
                        while (!server.isPreemptRequested() && ros::ok())
                        {
                            ros::service::call("bin_state", query);
                            if (static_cast<tfr_utilities::BinCode>(query.response.code) == tfr_utilities::BinCode::LOWERED)
                                break;
                            bin_publisher.publish(bin_cmd);
                            frequency.sleep();
                        }
                        if (server.isPreemptRequested())
                        {
                            ROS_INFO("Teleop Action Server: DUMPING_RESET preempted");
                            server.setPreempted();
                            return;
                        }
                        ROS_INFO("Teleop Action Server: DUMPING_RESET finished");
                        break;
                    }

                case (tfr_utilities::TeleopCode::RESET_STARTING):
                    {
                        // TODO integrate  
                        ROS_INFO("Teleop Action Server: Command Recieved, RESET_STARTING");
                        //all zeros by default
                        drivebase_publisher.publish(move_cmd);
                        break;
                    }

                default:
                    {
                        ROS_WARN("Teleop Action Server: UNRECOGNIZED COMMAND");
                        tfr_msgs::TeleopResult result{};
                        server.setAborted(result);
                        return;
                    }
            }

            tfr_msgs::TeleopResult result{};
            server.setSucceeded(result);
        }

        actionlib::SimpleActionServer<tfr_msgs::TeleopAction> server;
        actionlib::SimpleActionClient<tfr_msgs::DiggingAction> digging_client;
        ros::Publisher drivebase_publisher;
        ros::Publisher bin_publisher;
        DriveVelocity &drive_stats;
        //how often to check for preemption
        ros::Duration frequency;
};


int main(int argc, char** argv)
{
    ros::init(argc, argv, "teleop_action_server");
    ros::NodeHandle n{};
    double linear_velocity, angular_velocity, rate;
    ros::param::param<double>("~linear_velocity", linear_velocity, 0.25);
    ros::param::param<double>("~angular_velocity", angular_velocity, 0.1);
    ros::param::param<double>("~rate", rate, 10.0);
    TeleopExecutive::DriveVelocity velocities{linear_velocity, angular_velocity};
    TeleopExecutive teleop{n, velocities, 1.0/rate};
    ros::spin();
    return 0;
}
