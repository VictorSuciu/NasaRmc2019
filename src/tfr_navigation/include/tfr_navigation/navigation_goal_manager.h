#ifndef NAVIGATION_GOAL_MANAGER_H
#define NAVIGATION_GOAL_MANAGER_H
#include <ros/console.h>
#include <ros/ros.h>
#include <geometry_msgs/Pose.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <tfr_msgs/NavigationAction.h>
#include <tfr_utilities/location_codes.h>
#include <cstdint>
#include <cmath>
class NavigationGoalManager
{
    public:
        /* 
         * Immutable struct of geometry constraints for the goal selection
         * algorithm
         * */
        struct GeometryConstraints 
        {
            public:
                GeometryConstraints(double d, double f) :
                    safe_mining_distance{d}, finish_line{f}{};

                double get_safe_mining_distance() const
                {
                    return safe_mining_distance;
                }

                double get_finish_line() const
                {
                    return finish_line;
                }
            private:
                //The distance to travel from the bin
                double safe_mining_distance;
                //the distance from the bin the finish line is
                double finish_line;
        };


        NavigationGoalManager(const std::string &ref_frame, const GeometryConstraints &constraints);
        ~NavigationGoalManager() = default;
        NavigationGoalManager(const NavigationGoalManager&) = delete;
        NavigationGoalManager& operator=(const NavigationGoalManager&) = delete;
        NavigationGoalManager(NavigationGoalManager&&) = delete;
        NavigationGoalManager& operator=(NavigationGoalManager&&) = delete;

        void initialize_goal( move_base_msgs::MoveBaseGoal& nav_goal, 
                const tfr_utilities::LocationCode& goal);

    private:

        //the constraints to the problem
        const GeometryConstraints &constraints;

        const std::string &reference_frame;
};
#endif