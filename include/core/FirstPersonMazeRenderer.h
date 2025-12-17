#pragma once

#include <vector>
#include <memory>
#include <cmath>

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

namespace NeuroForge {
namespace Core {

/**
 * @brief First-person perspective maze renderer for visual navigation benchmarks
 * 
 * This class renders a maze from the agent's first-person perspective, providing
 * realistic visual input for neural substrate processing. The renderer creates
 * a 3D-like view of the maze walls, floor, and goal, simulating what an agent
 * would see when navigating through the environment.
 */
class FirstPersonMazeRenderer {
public:
    struct RenderConfig {
        int width = 320;           // Render width in pixels
        int height = 240;          // Render height in pixels
        float fov = 60.0f;         // Field of view in degrees
        float view_distance = 5.0f; // Maximum view distance
        int wall_height = 100;     // Wall height in pixels
        bool enable_textures = true; // Enable wall textures
        bool enable_shadows = false; // Enable simple shadows
    };

    /**
     * @brief Agent state in the maze
     */
    struct AgentState {
        float x, y;           // Position in maze coordinates
        float angle;          // Facing direction in radians (0 = right, π/2 = up)
        int maze_x, maze_y;   // Discrete maze cell position
    };

    /**
     * @brief Construct renderer with configuration
     */
    explicit FirstPersonMazeRenderer(const RenderConfig& config);

    /**
     * @brief Set the maze layout
     * @param walls 2D array of wall positions (true = wall, false = open)
     * @param size Maze size (N x N)
     * @param goal_x Goal X coordinate
     * @param goal_y Goal Y coordinate
     */
    void setMaze(const std::vector<bool>& walls, int size, int goal_x, int goal_y);

    /**
     * @brief Render the maze from agent's perspective
     * @param agent Current agent state
     * @return Rendered image as grayscale values [0,1]
     */
    std::vector<float> render(const AgentState& agent);

#ifdef NF_HAVE_OPENCV
    /**
     * @brief Render maze to OpenCV Mat for visualization
     * @param agent Current agent state
     * @return OpenCV Mat (CV_8UC3) for display
     */
    cv::Mat renderToMat(const AgentState& agent);

    /**
     * @brief Capture screen region for visual input processing
     * @param x Screen X coordinate
     * @param y Screen Y coordinate
     * @param width Capture width
     * @param height Capture height
     * @return Captured screen region as grayscale values [0,1]
     */
    std::vector<float> captureScreen(int x, int y, int width, int height);
#endif

    /**
     * @brief Update agent position based on action
     * @param agent Current agent state (modified in place)
     * @param action Action to take (0=forward, 1=backward, 2=turn_left, 3=turn_right)
     * @param maze_walls Wall layout for collision detection
     * @param maze_size Maze size
     * @return True if movement was successful, false if blocked
     */
    bool updateAgentPosition(AgentState& agent, int action, const std::vector<bool>& maze_walls, int maze_size);

    /**
     * @brief Check if agent can see the goal
     * @param agent Current agent state
     * @return Distance to goal if visible, -1 if not visible
     */
    float getGoalVisibility(const AgentState& agent) const;

    /**
     * @brief Get configuration
     */
    const RenderConfig& getConfig() const { return config_; }

private:
    RenderConfig config_;
    std::vector<bool> maze_walls_;
    int maze_size_;
    int goal_x_, goal_y_;

    /**
     * @brief Cast a ray to find wall intersection
     * @param start_x Ray start X
     * @param start_y Ray start Y
     * @param angle Ray direction
     * @return Distance to wall intersection
     */
    float castRay(float start_x, float start_y, float angle) const;

    /**
     * @brief Check if a position is a wall
     * @param x Maze X coordinate
     * @param y Maze Y coordinate
     * @return True if wall, false if open
     */
    bool isWall(int x, int y) const;

    /**
     * @brief Get wall texture value based on position and distance
     * @param wall_x Wall X coordinate
     * @param wall_y Wall Y coordinate
     * @param distance Distance to wall
     * @param hit_side Which side of wall was hit (0-3)
     * @return Texture intensity [0,1]
     */
    float getWallTexture(int wall_x, int wall_y, float distance, int hit_side) const;

    /**
     * @brief Apply distance-based shading
     * @param base_color Base color intensity
     * @param distance Distance to surface
     * @return Shaded color intensity
     */
    float applyDistanceShading(float base_color, float distance) const;
};

} // namespace Core
} // namespace NeuroForge