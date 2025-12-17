#include "core/FirstPersonMazeRenderer.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cassert>
#include <cstring>
#include <memory>
#include <functional>
#include <numeric>
#include <set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper functions to replace std::clamp which may not be available
template<typename T>
T clamp_value(const T& value, const T& min_val, const T& max_val) {
    return (value < min_val) ? min_val : (value > max_val) ? max_val : value;
}

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#endif

namespace NeuroForge {
namespace Core {

FirstPersonMazeRenderer::FirstPersonMazeRenderer(const RenderConfig& config)
    : config_(config), maze_size_(0), goal_x_(0), goal_y_(0) {
}

void FirstPersonMazeRenderer::setMaze(const std::vector<bool>& walls, int size, int goal_x, int goal_y) {
    maze_walls_ = walls;
    maze_size_ = size;
    goal_x_ = goal_x;
    goal_y_ = goal_y;
}

std::vector<float> FirstPersonMazeRenderer::render(const AgentState& agent) {
    std::vector<float> pixels(static_cast<size_t>(config_.width * config_.height), 0.0f);
    
    if (maze_size_ == 0) {
        return pixels; // No maze set
    }

    const float fov_rad = config_.fov * static_cast<float>(M_PI) / 180.0f;
    const float half_fov = fov_rad / 2.0f;
    const int half_height = config_.height / 2;

    // Render each column of pixels
    for (int x = 0; x < config_.width; ++x) {
        // Calculate ray angle for this column
        float ray_angle = agent.angle + (static_cast<float>(x) / config_.width - 0.5f) * fov_rad;
        
        // Cast ray to find wall distance
        float distance = castRay(agent.x, agent.y, ray_angle);
        
        // Calculate wall height on screen based on distance
        int wall_screen_height = static_cast<int>(config_.wall_height / (distance > 0.1f ? distance : 0.1f));
        wall_screen_height = (wall_screen_height < config_.height) ? wall_screen_height : config_.height;
        
        // Calculate wall top and bottom positions
        int wall_top = half_height - wall_screen_height / 2;
        int wall_bottom = half_height + wall_screen_height / 2;
        
        // Determine wall texture and shading
        float wall_intensity = 0.8f;
        if (config_.enable_textures) {
            // Simple texture based on distance and position
            int wall_x = static_cast<int>(agent.x + distance * std::cos(ray_angle));
            int wall_y = static_cast<int>(agent.y + distance * std::sin(ray_angle));
            wall_intensity = getWallTexture(wall_x, wall_y, distance, 0);
        }
        
        if (config_.enable_shadows) {
            wall_intensity = applyDistanceShading(wall_intensity, distance);
        }
        
        // Check if goal is visible in this ray direction
        float goal_distance = getGoalVisibility(agent);
        bool goal_visible = (goal_distance > 0 && goal_distance < distance);
        
        // Fill column pixels
        for (int y = 0; y < config_.height; ++y) {
            float pixel_value = 0.0f; // Default: black (sky/void)
            
            if (y >= wall_top && y <= wall_bottom) {
                // Wall pixel
                pixel_value = wall_intensity;
                
                // Goal highlighting if visible
                if (goal_visible) {
                    float goal_x_screen = agent.x + goal_distance * std::cos(ray_angle);
                    float goal_y_screen = agent.y + goal_distance * std::sin(ray_angle);
                    if (std::abs(goal_x_screen - goal_x_) < 0.5f && std::abs(goal_y_screen - goal_y_) < 0.5f) {
                        pixel_value = (pixel_value + 0.3f < 1.0f) ? pixel_value + 0.3f : 1.0f; // Brighten goal area
                    }
                }
            } else if (y > wall_bottom) {
                // Floor pixel - simple gradient
                float floor_distance = static_cast<float>(half_height) / ((y - half_height) > 1.0f ? static_cast<float>(y - half_height) : 1.0f);
                pixel_value = 0.2f * applyDistanceShading(1.0f, floor_distance);
            }
            
            pixels[static_cast<size_t>(y * config_.width + x)] = clamp_value(pixel_value, 0.0f, 1.0f);
        }
    }
    
    return pixels;
}

#ifdef NF_HAVE_OPENCV
cv::Mat FirstPersonMazeRenderer::renderToMat(const AgentState& agent) {
    auto pixels = render(agent);
    cv::Mat img(config_.height, config_.width, CV_8UC3);
    
    for (int y = 0; y < config_.height; ++y) {
        for (int x = 0; x < config_.width; ++x) {
            float intensity = pixels[static_cast<size_t>(y * config_.width + x)];
            uint8_t gray_val = static_cast<uint8_t>(intensity * 255.0f);
            
            // Create colored visualization
            cv::Vec3b color;
            if (intensity > 0.7f) {
                // Bright areas (walls) - light gray
                color = cv::Vec3b(gray_val, gray_val, gray_val);
            } else if (intensity > 0.3f) {
                // Medium areas (textured walls) - brownish
                color = cv::Vec3b(static_cast<uint8_t>(gray_val * 0.6f), 
                                static_cast<uint8_t>(gray_val * 0.8f), gray_val);
            } else if (intensity > 0.1f) {
                // Floor areas - darker brown
                color = cv::Vec3b(static_cast<uint8_t>(gray_val * 0.4f), 
                                static_cast<uint8_t>(gray_val * 0.6f), 
                                static_cast<uint8_t>(gray_val * 0.3f));
            } else {
                // Sky/void - dark blue
                color = cv::Vec3b(20, 10, 5);
            }
            
            img.at<cv::Vec3b>(y, x) = color;
        }
    }
    
    // Add crosshair for aiming
    int center_x = config_.width / 2;
    int center_y = config_.height / 2;
    cv::line(img, cv::Point(center_x - 10, center_y), cv::Point(center_x + 10, center_y), cv::Scalar(0, 255, 0), 2);
    cv::line(img, cv::Point(center_x, center_y - 10), cv::Point(center_x, center_y + 10), cv::Scalar(0, 255, 0), 2);
    
    return img;
}

std::vector<float> FirstPersonMazeRenderer::captureScreen(int x, int y, int width, int height) {
    std::vector<float> pixels(static_cast<size_t>(width * height), 0.0f);
    
#ifdef _WIN32
    // Windows screen capture implementation
    HDC hdc = GetDC(NULL);
    HDC memdc = CreateCompatibleDC(hdc);
    HBITMAP hbitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldbitmap = (HBITMAP)SelectObject(memdc, hbitmap);
    
    BitBlt(memdc, 0, 0, width, height, hdc, x, y, SRCCOPY);
    
    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    
    std::vector<uint8_t> buffer(static_cast<size_t>(width * height * 3));
    GetDIBits(hdc, hbitmap, 0, height, buffer.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    // Convert BGR to grayscale
    for (int i = 0; i < width * height; ++i) {
        uint8_t b = buffer[static_cast<size_t>(i * 3)];
        uint8_t g = buffer[static_cast<size_t>(i * 3 + 1)];
        uint8_t r = buffer[static_cast<size_t>(i * 3 + 2)];
        float gray = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
        pixels[static_cast<size_t>(i)] = gray;
    }
    
    SelectObject(memdc, oldbitmap);
    DeleteObject(hbitmap);
    DeleteDC(memdc);
    ReleaseDC(NULL, hdc);
#endif
    
    return pixels;
}
#endif

bool FirstPersonMazeRenderer::updateAgentPosition(AgentState& agent, int action, 
                                                const std::vector<bool>& maze_walls, int maze_size) {
    const float move_speed = 0.3f;  // Increased movement speed for more visible movement
    const float turn_speed = static_cast<float>(M_PI) / 4.0f; // 45 degrees for faster turning
    
    AgentState new_agent = agent;
    
    switch (action) {
        case 0: // Forward
            new_agent.x += move_speed * std::cos(agent.angle);
            new_agent.y += move_speed * std::sin(agent.angle);
            break;
        case 1: // Backward
            new_agent.x -= move_speed * std::cos(agent.angle);
            new_agent.y -= move_speed * std::sin(agent.angle);
            break;
        case 2: // Turn left
            new_agent.angle += turn_speed;
            if (new_agent.angle > 2 * static_cast<float>(M_PI)) new_agent.angle -= 2 * static_cast<float>(M_PI);
            break;
        case 3: // Turn right
            new_agent.angle -= turn_speed;
            if (new_agent.angle < 0) new_agent.angle += 2 * static_cast<float>(M_PI);
            break;
        default:
            return false;
    }
    
    // Update discrete maze position
    new_agent.maze_x = static_cast<int>(new_agent.x);
    new_agent.maze_y = static_cast<int>(new_agent.y);
    
    // Check bounds and collision for movement actions
    if (action <= 1) { // Movement actions
        if (new_agent.maze_x < 0 || new_agent.maze_x >= maze_size ||
            new_agent.maze_y < 0 || new_agent.maze_y >= maze_size) {
            return false; // Out of bounds
        }
        
        if (isWall(new_agent.maze_x, new_agent.maze_y)) {
            return false; // Wall collision
        }
    }
    
    agent = new_agent;
    return true;
}

float FirstPersonMazeRenderer::getGoalVisibility(const AgentState& agent) const {
    if (maze_size_ == 0) return -1.0f;
    
    // Calculate direction to goal
    float dx = goal_x_ + 0.5f - agent.x;
    float dy = goal_y_ + 0.5f - agent.y;
    float goal_distance = std::sqrt(dx * dx + dy * dy);
    
    if (goal_distance > config_.view_distance) {
        return -1.0f; // Too far
    }
    
    float goal_angle = std::atan2(dy, dx);
    float angle_diff = std::abs(goal_angle - agent.angle);
    if (angle_diff > static_cast<float>(M_PI)) angle_diff = 2 * static_cast<float>(M_PI) - angle_diff;
    
    const float fov_rad = config_.fov * static_cast<float>(M_PI) / 180.0f;
    if (angle_diff > fov_rad / 2.0f) {
        return -1.0f; // Outside field of view
    }
    
    // Check if path to goal is clear
    float wall_distance = castRay(agent.x, agent.y, goal_angle);
    if (wall_distance < goal_distance) {
        return -1.0f; // Blocked by wall
    }
    
    return goal_distance;
}

float FirstPersonMazeRenderer::castRay(float start_x, float start_y, float angle) const {
    const float step_size = 0.02f;
    const float max_distance = config_.view_distance;
    
    float x = start_x;
    float y = start_y;
    float dx = std::cos(angle) * step_size;
    float dy = std::sin(angle) * step_size;
    
    for (float distance = 0; distance < max_distance; distance += step_size) {
        x += dx;
        y += dy;
        
        int maze_x = static_cast<int>(x);
        int maze_y = static_cast<int>(y);
        
        if (isWall(maze_x, maze_y)) {
            return distance;
        }
    }
    
    return max_distance;
}

bool FirstPersonMazeRenderer::isWall(int x, int y) const {
    if (x < 0 || x >= maze_size_ || y < 0 || y >= maze_size_) {
        return true; // Boundary walls
    }
    
    return maze_walls_[static_cast<size_t>(y * maze_size_ + x)];
}

float FirstPersonMazeRenderer::getWallTexture(int wall_x, int wall_y, float distance, int hit_side) const {
    // Simple procedural texture based on position
    float base_intensity = 0.7f;
    
    // Add some variation based on wall position
    int pattern = (wall_x + wall_y) % 4;
    switch (pattern) {
        case 0: base_intensity = 0.8f; break;
        case 1: base_intensity = 0.7f; break;
        case 2: base_intensity = 0.75f; break;
        case 3: base_intensity = 0.65f; break;
    }
    
    // Add subtle brick-like pattern
    if ((wall_x % 2 == 0) != (wall_y % 2 == 0)) {
        base_intensity *= 0.9f;
    }
    
    return base_intensity;
}

float FirstPersonMazeRenderer::applyDistanceShading(float base_color, float distance) const {
    // Simple distance-based fog/shading
    float fog_factor = 1.0f - (distance / config_.view_distance);
    fog_factor = clamp_value(fog_factor, 0.2f, 1.0f);
    return base_color * fog_factor;
}

} // namespace Core
} // namespace NeuroForge