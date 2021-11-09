//
// Copyright [2020] Nobleo Technology"  [legal/copyright]
//
#pragma once

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <list>
#include <memory>
#include <string>
#include <vector>

// #include <pluginlib/class_list_macros.h>
#include "full_coverage_path_planner/full_coverage_path_planner.hpp"
#include "nav2_core/global_planner.hpp"
#include "nav2_costmap_2d/costmap_2d_ros.hpp"
#include "nav2_util/lifecycle_node.hpp"
#include "nav2_util/node_utils.hpp"
#include "nav2_util/robot_utils.hpp"
#include "rclcpp/rclcpp.hpp"
#include <angles/angles.h>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <nav_msgs/msg/path.hpp>
#include <nav_msgs/srv/get_map.hpp>
#include "visualization_msgs/msg/marker.hpp"

using namespace std::chrono_literals;
using std::string;

namespace full_coverage_path_planner
{
  class SpiralSTC : public nav2_core::GlobalPlanner , private full_coverage_path_planner::FullCoveragePathPlanner
  {
  public:

    int division_factor = 3; // Grid size is chosen as tool width divided by  this factor (preferably an uneven number)
    int max_overlap;
    int max_overlap_forward = 0; // Maximum allowable overlapping grids between a forward menoeuvre and already visited grids
    int max_overlap_turn = 6; // Maximum allowable overlapping grids between a turning menoeuvre and already visited grids
    int N_footprints = 20; // Orientation steps in between footprint 1 and footprint 2 to check for a manoeuvre

    int spiral_counter = 0; // Temporary for debugging purposes

    // Relative manoeuvre footprints (in robot frame)
    std::vector<nav2_costmap_2d::MapLocation> left_turn;
    std::vector<nav2_costmap_2d::MapLocation> forward;
    std::vector<nav2_costmap_2d::MapLocation> right_turn;

    /**
     * @brief constructor
     */
    SpiralSTC();

    /**
     * @brief destructor
     */
    ~SpiralSTC();

    /**
     * @brief Configuring plugin
     * @param parent Lifecycle node pointer
     * @param name Name of plugin map
     * @param tf Shared ptr of TF2 buffer
     * @param costmap_ros Costmap2DROS object
     */
    void configure(
        const rclcpp_lifecycle::LifecycleNode::WeakPtr &parent,
        std::string name, std::shared_ptr<tf2_ros::Buffer> tf,
        std::shared_ptr<nav2_costmap_2d::Costmap2DROS> costmap_ros) override;

    /**
     * @brief Cleanup lifecycle node
     */
    void cleanup() override;

    /**
     * @brief Activate lifecycle node
     */
    void activate() override;

    /**
     * @brief Deactivate lifecycle node
     */
    void deactivate() override;

    /**
     * @brief Creating a plan from start and goal poses
     * @param start Start pose
     * @param goal Goal pose
     * @return nav_msgs::Path of the generated path
     */
    nav_msgs::msg::Path createPlan(
        const geometry_msgs::msg::PoseStamped &start,
        const geometry_msgs::msg::PoseStamped &goal) override;

    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<visualization_msgs::msg::Marker>> vis_pub_grid_;
    std::shared_ptr<rclcpp_lifecycle::LifecyclePublisher<visualization_msgs::msg::Marker>> vis_pub_spirals_;

    visualization_msgs::msg::Marker cubeMarker(std::string frame_id, std::string name_space, int id, float size, float a, float r, float g, float b);
    visualization_msgs::msg::Marker lineStrip(std::string frame_id, std::string name_space, int id, float size, float a, float r, float g, float b);

    void visualizeGrid(std::vector<std::vector<bool>> const &grid, std::string name_space, float a, float r, float g, float b);
    void visualizeGridlines();
    void visualizeSpirals(std::list<gridNode_t> &spiralNodes, std::string name_space, float w, float a, float r, float g, float b);

    nav2_costmap_2d::Costmap2D coarse_grid_;
    nav2_costmap_2d::Costmap2DROS *coarse_grid_ros_;

  protected:
    /**
     * @brief Given a goal pose in the world, compute a plan
     * @param start The start pose
     * @param goal The goal pose
     * @param plan The plan... filled by the planner
     * @return True if a valid plan was found, false otherwise
     */
    bool makePlan(const geometry_msgs::msg::PoseStamped &start, const geometry_msgs::msg::PoseStamped &goal,
                  std::vector<geometry_msgs::msg::PoseStamped> &plan);

    /**
     * @brief  Initialization function for the FullCoveragePathPlanner object
     * @param  name The name of this planner
     * @param  costmap A pointer to the ROS wrapper of the costmap to use for planning
     */
    void initialize(std::string name, nav2_costmap_2d::Costmap2DROS *costmap_ros);

    /**
     * Find a path that spirals inwards from init until an obstacle is seen in the grid
     * @param grid 2D grid of bools. true == occupied/blocked/obstacle
     * @param init start position
     * @param yawStart start orientation
     * @param visited all the nodes visited by the spiral
     * @return list of nodes that form the spiral
     */
    std::list<gridNode_t> spiral(std::vector<std::vector<bool>> const &grid, std::list<gridNode_t> &init,
                                 double &yaw_start, std::vector<std::vector<bool>> &visited);

    /**
     * Perform Spiral-STC (Spanning Tree Coverage) coverage path planning.
     * In essence, the robot moves forward until an obstacle or visited node is met, then turns right (making a spiral)
     * When stuck in the middle of the spiral, use A* to get out again and start a new spiral, until a* can't find a path to uncovered cells
     * @param grid
     * @param init
     * @return
     */
    std::list<Point_t> spiral_stc(std::vector<std::vector<bool>> const &grid, Point_t &init, double &yaw_start,
                                  int &multiple_pass_counter, int &visited_counter);

    /**
     * Compute the cells in a grid that lie below a convex footprint
     * @param x_m the x coordinate of the map location around which the footprint is defined
     * @param y_m the y coordinate of the map location around which the footprint is defined
     * @param yaw the orientation of footprint
     * @param footprint_cells the output vector of map locations what are covered by the footprint
     * @return boolean that indicates if the entire footprint lies inside the map boundaries
     */
    bool computeFootprintCells(int &x_m, int &y_m, double &yaw, std::string part, std::vector<nav2_costmap_2d::MapLocation> &footprint_cells);

    /**
     * Compute the cells in a grid make up the swept path of a manoeuvre build from footprints
     * @param x1 the x coordinate of the map location around which the initial footprint is defined
     * @param y1 the y coordinate of the map location around which the initial footprint is defined
     * @param x1 the x coordinate of the map location around which the final footprint is defined
     * @param y1 the y coordinate of the map location around which the final footprint is defined
     * @param yaw1 the starting orientation of the manoeuvre
     * @param man_grids the output vector of map locations that are covered by the manoeuvre
     * @return boolean that indicates if the entire manoeuvre lies inside the map boundaries
     */
    bool computeManoeuvreFootprint(int &x1, int &y1, int &x2, int &y2, double &yaw1, std::vector<nav2_costmap_2d::MapLocation> &man_grids);
  };
}  // namespace full_coverage_path_planner
