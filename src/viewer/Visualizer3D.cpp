#include "viewer/Visualizer3D.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

namespace NeuroForge { namespace Viewer {

namespace {

static const char* kVS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aW;

uniform mat4 uMVP;
uniform float uWeightMax;

out float vW;

void main(){
    vW = (uWeightMax > 0.0) ? clamp(aW / uWeightMax, -1.0, 1.0) : 0.0;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* kFS = R"GLSL(
#version 330 core
in float vW;
out vec4 FragColor;

void main(){
    // Map vW in [-1,1] to color: negative = blue, positive = red, near 0 = white
    float t = clamp(vW, -1.0, 1.0);
    vec3 color;
    if (t >= 0.0) {
        // white -> red
        color = mix(vec3(1.0,1.0,1.0), vec3(1.0,0.15,0.15), t);
    } else {
        // white -> blue
        color = mix(vec3(1.0,1.0,1.0), vec3(0.15,0.35,1.0), -t);
    }
    FragColor = vec4(color, 1.0);
}
)GLSL";

// Spike points shaders (additive blended, radial falloff)
static const char* kVS_pts = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aI; // intensity in [0,1]

uniform mat4 uMVP;
uniform float uPointSize;

out float vI;

void main(){
    vI = clamp(aI, 0.0, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = uPointSize;
}
)GLSL";

static const char* kFS_pts = R"GLSL(
#version 330 core
in float vI;
out vec4 FragColor;

void main(){
    // radial falloff for a soft glow
    vec2 uv = gl_PointCoord * 2.0 - 1.0; // [-1,1]
    float r2 = dot(uv, uv);
    float mask = smoothstep(1.0, 0.2, r2); // inside circle
    float glow = exp(-3.0 * r2);
    float a = clamp(vI * glow, 0.0, 1.0) * mask;
    vec3 col = mix(vec3(1.0, 0.85, 0.2), vec3(1.0, 0.2, 0.2), vI);
    FragColor = vec4(col * a, a);
}
)GLSL";

// Highlight lines shaders (additive blended)
static const char* kVS_hl = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in float aI; // per-vertex intensity [0,1]

uniform mat4 uMVP;

out float vI;

void main(){
    vI = clamp(aI, 0.0, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* kFS_hl = R"GLSL(
#version 330 core
in float vI;
out vec4 FragColor;

void main(){
    // bright yellowish glow scaled by intensity
    vec3 col = mix(vec3(0.9, 0.9, 0.9), vec3(1.0, 0.95, 0.3), vI);
    float a = vI;
    FragColor = vec4(col * a, a);
}
)GLSL";

struct Mat4 { float m[16]; };

static Mat4 mat4_identity(){ Mat4 r{}; std::memset(r.m,0,sizeof(r.m)); r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.0f; return r; }
static Mat4 mat4_mul(const Mat4& a, const Mat4& b){ Mat4 r{}; for(int c=0;c<4;++c){ for(int row=0; row<4; ++row){ r.m[c*4 + row] = 0.0f; for(int k=0; k<4; ++k){ r.m[c*4 + row] += a.m[k*4 + row] * b.m[c*4 + k]; } } } return r; }
static Mat4 mat4_translate(float x,float y,float z){ Mat4 r = mat4_identity(); r.m[12]=x; r.m[13]=y; r.m[14]=z; return r; }
static Mat4 mat4_scale(float sx,float sy,float sz){ Mat4 r = mat4_identity(); r.m[0]=sx; r.m[5]=sy; r.m[10]=sz; return r; }
static Mat4 mat4_perspective(float fovyRad, float aspect, float znear, float zfar){ float f=1.0f/std::tan(fovyRad*0.5f); Mat4 r{}; std::memset(r.m,0,sizeof(r.m)); r.m[0]=f/aspect; r.m[5]=f; r.m[10]=(zfar+znear)/(znear-zfar); r.m[11]=-1.0f; r.m[14]=(2.0f*zfar*znear)/(znear-zfar); return r; }
static Mat4 mat4_lookat(const float eye[3], const float center[3], const float up_in[3]){
    float f[3] = { center[0]-eye[0], center[1]-eye[1], center[2]-eye[2] };
    float fl = std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]); if (fl<1e-6f) fl=1e-6f; f[0]/=fl; f[1]/=fl; f[2]/=fl;
    float up[3] = { up_in[0], up_in[1], up_in[2] };
    float s[3] = { f[1]*up[2]-f[2]*up[1], f[2]*up[0]-f[0]*up[2], f[0]*up[1]-f[1]*up[0] };
    float sl = std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]); if (sl<1e-6f) sl=1e-6f; s[0]/=sl; s[1]/=sl; s[2]/=sl;
    float u[3] = { s[1]*f[2]-s[2]*f[1], s[2]*f[0]-s[0]*f[2], s[0]*f[1]-s[1]*f[0] };

    Mat4 r = mat4_identity();
    r.m[0]=s[0]; r.m[4]=s[1]; r.m[8]=s[2];
    r.m[1]=u[0]; r.m[5]=u[1]; r.m[9]=u[2];
    r.m[2]=-f[0]; r.m[6]=-f[1]; r.m[10]=-f[2];
    r.m[12]=-(s[0]*eye[0]+s[1]*eye[1]+s[2]*eye[2]);
    r.m[13]=-(u[0]*eye[0]+u[1]*eye[1]+u[2]*eye[2]);
    r.m[14]=(f[0]*eye[0]+f[1]*eye[1]+f[2]*eye[2]);
    return r;
}

static unsigned int compileShader(unsigned int type, const char* src){
    unsigned int s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok){ char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log); std::cerr << "Shader compile error: " << log << std::endl; }
    return s;
}

static unsigned int linkProgram(unsigned int vs, unsigned int fs){
    unsigned int p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    int ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok){ char log[1024]; glGetProgramInfoLog(p, sizeof(log), nullptr, log); std::cerr << "Program link error: " << log << std::endl; }
    glDetachShader(p, vs); glDetachShader(p, fs);
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}

} // namespace

Visualizer3D::Visualizer3D() {}
Visualizer3D::~Visualizer3D(){
    if (vbo_w_) glDeleteBuffers(1, &vbo_w_);
    if (vbo_pos_) glDeleteBuffers(1, &vbo_pos_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (shader_) glDeleteProgram(shader_);
    if (vbo_pts_i_) glDeleteBuffers(1, &vbo_pts_i_);
    if (vbo_pts_pos_) glDeleteBuffers(1, &vbo_pts_pos_);
    if (vao_pts_) glDeleteVertexArrays(1, &vao_pts_);
    if (shader_pts_) glDeleteProgram(shader_pts_);
    if (vbo_hl_i_) glDeleteBuffers(1, &vbo_hl_i_);
    if (vbo_hl_pos_) glDeleteBuffers(1, &vbo_hl_pos_);
    if (vao_hl_) glDeleteVertexArrays(1, &vao_hl_);
    if (shader_hl_) glDeleteProgram(shader_hl_);
}

bool Visualizer3D::initialize(){
    unsigned int vs = compileShader(GL_VERTEX_SHADER, kVS);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, kFS);
    shader_ = linkProgram(vs, fs);
    u_mvp_loc_ = glGetUniformLocation(shader_, "uMVP");
    u_weightMax_loc_ = glGetUniformLocation(shader_, "uWeightMax");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_pos_);
    glGenBuffers(1, &vbo_w_);

    glBindVertexArray(vao_);
    // position buffer at location 0
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    // weight buffer at location 1
    glBindBuffer(GL_ARRAY_BUFFER, vbo_w_);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    // Spike points program and buffers
    unsigned int vs_pts = compileShader(GL_VERTEX_SHADER, kVS_pts);
    unsigned int fs_pts = compileShader(GL_FRAGMENT_SHADER, kFS_pts);
    shader_pts_ = linkProgram(vs_pts, fs_pts);
    u_mvp_pts_loc_ = glGetUniformLocation(shader_pts_, "uMVP");
    u_pointSize_loc_ = glGetUniformLocation(shader_pts_, "uPointSize");

    glGenVertexArrays(1, &vao_pts_);
    glGenBuffers(1, &vbo_pts_pos_);
    glGenBuffers(1, &vbo_pts_i_);

    glBindVertexArray(vao_pts_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pts_pos_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_pts_i_);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    // Highlight lines program and buffers
    unsigned int vs_hl = compileShader(GL_VERTEX_SHADER, kVS_hl);
    unsigned int fs_hl = compileShader(GL_FRAGMENT_SHADER, kFS_hl);
    shader_hl_ = linkProgram(vs_hl, fs_hl);
    u_mvp_hl_loc_ = glGetUniformLocation(shader_hl_, "uMVP");

    glGenVertexArrays(1, &vao_hl_);
    glGenBuffers(1, &vbo_hl_pos_);
    glGenBuffers(1, &vbo_hl_i_);

    glBindVertexArray(vao_hl_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_hl_pos_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_hl_i_);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glLineWidth(1.5f);
    return true;
}

void Visualizer3D::setLines(const std::vector<float>& xyzxyz){
    vertex_count_ = static_cast<int>(xyzxyz.size()/3);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
    glBufferData(GL_ARRAY_BUFFER, xyzxyz.size()*sizeof(float), xyzxyz.data(), GL_STATIC_DRAW);

    // If no weights are set, fill zeros (white)
    std::vector<float> zeros(vertex_count_, 0.0f);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_w_);
    glBufferData(GL_ARRAY_BUFFER, zeros.size()*sizeof(float), zeros.data(), GL_STATIC_DRAW);
}

void Visualizer3D::setLinesWithWeights(const std::vector<float>& xyzxyz, const std::vector<float>& weights){
    vertex_count_ = static_cast<int>(xyzxyz.size()/3);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
    glBufferData(GL_ARRAY_BUFFER, xyzxyz.size()*sizeof(float), xyzxyz.data(), GL_STATIC_DRAW);

    // Ensure weights length matches vertex count; if not, pad or truncate
    std::vector<float> wbuf(vertex_count_, 0.0f);
    const size_t ncopy = std::min<size_t>(wbuf.size(), weights.size());
    if (ncopy > 0) std::memcpy(wbuf.data(), weights.data(), ncopy*sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, vbo_w_);
    glBufferData(GL_ARRAY_BUFFER, wbuf.size()*sizeof(float), wbuf.data(), GL_STATIC_DRAW);
}

void Visualizer3D::setWeightMaxAbs(float maxAbs){
    weight_max_abs_ = std::max(1e-6f, std::fabs(maxAbs));
}

void Visualizer3D::setView(float scale, float /*offset_x*/, float /*offset_y*/){
    // keep for API compatibility; scale can modulate model matrix if needed
    (void)scale;
}

void Visualizer3D::setOrbitAngles(float yaw, float pitch){
    cam_yaw_ = yaw; cam_pitch_ = pitch;
}
void Visualizer3D::addOrbitDelta(float dYaw, float dPitch){
    cam_yaw_ += dYaw; cam_pitch_ += dPitch;
    const float lim = 1.55f; // ~89 deg
    if (cam_pitch_ > lim) cam_pitch_ = lim;
    if (cam_pitch_ < -lim) cam_pitch_ = -lim;
}
void Visualizer3D::setCameraDistance(float dist){ cam_dist_ = std::max(10.0f, dist); }
void Visualizer3D::addCameraDistance(float dDist){ setCameraDistance(cam_dist_ + dDist); }

void Visualizer3D::setSpikePoints(const std::vector<float>& xyz, const std::vector<float>& intensities, float point_size){
    point_count_ = static_cast<int>(xyz.size()/3);
    if (point_count_ <= 0) { clearSpikePoints(); return; }
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pts_pos_);
    glBufferData(GL_ARRAY_BUFFER, xyz.size()*sizeof(float), xyz.data(), GL_DYNAMIC_DRAW);

    std::vector<float> ibuf(point_count_, 0.0f);
    const size_t ncopy = std::min<size_t>(ibuf.size(), intensities.size());
    if (ncopy > 0) std::memcpy(ibuf.data(), intensities.data(), ncopy*sizeof(float));
    glBindBuffer(GL_ARRAY_BUFFER, vbo_pts_i_);
    glBufferData(GL_ARRAY_BUFFER, ibuf.size()*sizeof(float), ibuf.data(), GL_DYNAMIC_DRAW);

    point_size_ = point_size;
}

void Visualizer3D::clearSpikePoints(){
    point_count_ = 0;
}

void Visualizer3D::setSpikePointSize(float s){ point_size_ = s; }

// New: highlight lines API implementations
void Visualizer3D::setHighlightLines(const std::vector<float>& xyzxyz, const std::vector<float>& intensities){
    hl_vertex_count_ = static_cast<int>(xyzxyz.size() / 3);
    if (hl_vertex_count_ <= 0) { clearHighlightLines(); return; }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_hl_pos_);
    glBufferData(GL_ARRAY_BUFFER, xyzxyz.size() * sizeof(float), xyzxyz.data(), GL_DYNAMIC_DRAW);

    std::vector<float> ibuf(hl_vertex_count_, 0.0f);
    const size_t ncopy = std::min<size_t>(ibuf.size(), intensities.size());
    if (ncopy > 0) std::memcpy(ibuf.data(), intensities.data(), ncopy * sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, vbo_hl_i_);
    glBufferData(GL_ARRAY_BUFFER, ibuf.size() * sizeof(float), ibuf.data(), GL_DYNAMIC_DRAW);
}

void Visualizer3D::clearHighlightLines(){
    hl_vertex_count_ = 0;
}

void Visualizer3D::render(int framebuffer_width, int framebuffer_height){
    if (!shader_ || vertex_count_ <= 0) {
        // We still may render points even if no lines, so do not early return entirely
    }

    const float aspect = (framebuffer_height>0) ? (static_cast<float>(framebuffer_width)/static_cast<float>(framebuffer_height)) : 1.0f;
    Mat4 proj = mat4_perspective(60.0f * 3.14159265f/180.0f, aspect, 0.1f, 10000.0f);

    // Orbit camera around origin
    float cx = cam_dist_ * std::cos(cam_pitch_) * std::cos(cam_yaw_);
    float cy = cam_dist_ * std::sin(cam_pitch_);
    float cz = cam_dist_ * std::cos(cam_pitch_) * std::sin(cam_yaw_);
    float eye[3] = { cx, cy, cz };
    float center[3] = { 0.0f, 0.0f, 0.0f };
    float up[3] = { 0.0f, 1.0f, 0.0f };
    Mat4 view = mat4_lookat(eye, center, up);

    Mat4 model = mat4_identity();
    Mat4 vp = mat4_mul(proj, view);
    Mat4 mvp = mat4_mul(vp, model);

    if (shader_ && vertex_count_ > 0) {
        glUseProgram(shader_);
        glUniformMatrix4fv(u_mvp_loc_, 1, GL_FALSE, mvp.m);
        glUniform1f(u_weightMax_loc_, weight_max_abs_);

        glBindVertexArray(vao_);
        glDrawArrays(GL_LINES, 0, vertex_count_);
        glBindVertexArray(0);
    }

    // Draw highlight lines on top with additive blending
    if (shader_hl_ && hl_vertex_count_ > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(shader_hl_);
        glUniformMatrix4fv(u_mvp_hl_loc_, 1, GL_FALSE, mvp.m);

        glBindVertexArray(vao_hl_);
        glDrawArrays(GL_LINES, 0, hl_vertex_count_);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }

    // Draw spike points on top with additive blending
    if (shader_pts_ && point_count_ > 0) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_DEPTH_TEST); // draw glow without depth test to avoid clipping

        glUseProgram(shader_pts_);
        glUniformMatrix4fv(u_mvp_pts_loc_, 1, GL_FALSE, mvp.m);
        glUniform1f(u_pointSize_loc_, point_size_);

        glBindVertexArray(vao_pts_);
        glDrawArrays(GL_POINTS, 0, point_count_);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
}

}} // namespace NeuroForge::Viewer