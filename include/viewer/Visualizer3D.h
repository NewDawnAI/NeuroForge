#pragma once
#include <vector>

namespace NeuroForge { namespace Viewer {

class Visualizer3D {
public:
    Visualizer3D();
    ~Visualizer3D();

    bool initialize();

    // Set 3D line segments: xyzxyz per segment (positions only)
    void setLines(const std::vector<float>& xyzxyz);

    // New: set 3D line segments with per-vertex weights (two weights per segment)
    void setLinesWithWeights(const std::vector<float>& xyzxyz, const std::vector<float>& weights);

    // New: weight visualization control
    void setWeightMaxAbs(float maxAbs);

    // 2D view transform for screen space offset and scale
    void setView(float scale, float offset_x, float offset_y);

    // New: orbit camera controls
    void setOrbitAngles(float yaw, float pitch);
    void addOrbitDelta(float dYaw, float dPitch);
    void setCameraDistance(float dist);
    void addCameraDistance(float dDist);

    // Spike overlay API: render additive glowing points at neuron positions
    void setSpikePoints(const std::vector<float>& xyz, const std::vector<float>& intensities, float point_size);
    void clearSpikePoints();
    void setSpikePointSize(float s);

    // Highlight overlay API: render additive blended highlight along edges
    void setHighlightLines(const std::vector<float>& xyzxyz, const std::vector<float>& intensities);
    void clearHighlightLines();

    void render(int framebuffer_width, int framebuffer_height);

private:
    unsigned int vao_ = 0;
    unsigned int vbo_pos_ = 0;
    unsigned int vbo_w_ = 0; // weights
    unsigned int shader_ = 0;

    // Spike points GPU objects
    unsigned int vao_pts_ = 0;
    unsigned int vbo_pts_pos_ = 0;
    unsigned int vbo_pts_i_ = 0; // intensities
    unsigned int shader_pts_ = 0;

    // Highlight lines GPU objects
    unsigned int vao_hl_ = 0;
    unsigned int vbo_hl_pos_ = 0;
    unsigned int vbo_hl_i_ = 0; // per-vertex intensity
    unsigned int shader_hl_ = 0;

    int u_mvp_loc_ = -1;
    int u_weightMax_loc_ = -1;

    int u_mvp_pts_loc_ = -1;
    int u_pointSize_loc_ = -1;

    int u_mvp_hl_loc_ = -1;

    int vertex_count_ = 0;
    int point_count_ = 0;
    int hl_vertex_count_ = 0;

    // view transform (screen-space like pan/scale applied in model matrix)
    float scale_ = 1.0f;
    float offset_x_ = 0.0f;
    float offset_y_ = 0.0f;

    // orbit camera
    float cam_yaw_ = 0.0f;
    float cam_pitch_ = 0.0f;
    float cam_dist_ = 600.0f;

    // weight normalization
    float weight_max_abs_ = 1.0f;

    // spike points size
    float point_size_ = 6.0f;
};

}} // namespace NeuroForge::Viewer