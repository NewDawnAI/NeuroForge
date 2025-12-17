#include "viewer/Visualizer3D.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <cmath>

using namespace NeuroForge::Viewer;

struct Args {
    std::string snapshot_file;
    std::string spikes_file;
    int refresh_ms = 500;
    float weight_threshold = 0.0f;
    std::string layout = "shells"; // or "layers"
};

static void usage(){
    std::cout << "Usage: neuroforge_viewer --snapshot-file <csv> [--spikes-file <csv>] [--refresh-ms <int>] [--weight-threshold <float>] [--layout shells|layers]\n";
}

static bool parse_args(int argc, char** argv, Args& out){
    for(int i=1;i<argc;i++){
        if (!std::strcmp(argv[i], "--snapshot-file") && i+1 < argc){ out.snapshot_file = argv[++i]; }
        else if (!std::strcmp(argv[i], "--spikes-file") && i+1 < argc){ out.spikes_file = argv[++i]; }
        else if (!std::strcmp(argv[i], "--refresh-ms") && i+1 < argc){ out.refresh_ms = std::atoi(argv[++i]); }
        else if (!std::strcmp(argv[i], "--weight-threshold") && i+1 < argc){ out.weight_threshold = std::strtof(argv[++i], nullptr); }
        else if (!std::strcmp(argv[i], "--layout") && i+1 < argc){ out.layout = argv[++i]; }
        else { std::cerr << "Unknown or incomplete argument: " << argv[i] << "\n"; usage(); return false; }
    }
    return !out.snapshot_file.empty();
}

struct Connection { int src, dst; float w; };
struct Spike { int neuron; float intensity; };

static bool load_snapshot_csv(const std::string& path, int& neuron_count, std::vector<Connection>& conns, float& maxAbsW){
    std::ifstream f(path);
    if (!f.is_open()) { std::cerr << "Failed to open snapshot: " << path << "\n"; return false; }
    conns.clear(); maxAbsW = 0.0f; neuron_count = 0;
    std::string line; bool first = true; int maxIndex = -1;
    while (std::getline(f, line)){
        if (line.empty()) continue;
        if (!line.empty() && line[0] == '#') continue;
        if (line.rfind("neuron_count=", 0) == 0){
            size_t eq = line.find('=');
            if (eq != std::string::npos){
                try { neuron_count = std::stoi(line.substr(eq+1)); } catch(...) {}
            }
            continue;
        }
        if (first){
            first = false;
            if (line.find("pre") != std::string::npos && line.find("post") != std::string::npos){
                // header line; skip and continue
                continue;
            }
        }
        std::istringstream ss(line);
        std::string s_src, s_dst, s_w;
        if (!std::getline(ss, s_src, ',')) continue;
        if (!std::getline(ss, s_dst, ',')) continue;
        if (!std::getline(ss, s_w, ',')) continue;
        int src=0, dst=0; float w=0.0f;
        try {
            src = std::stoi(s_src);
            dst = std::stoi(s_dst);
            w = std::stof(s_w);
        } catch (...) {
            continue;
        }
        conns.push_back({src,dst,w});
        maxAbsW = std::max(maxAbsW, std::fabs(w));
        maxIndex = std::max(maxIndex, std::max(src,dst));
    }
    if (neuron_count <= 0 && maxIndex >= 0) neuron_count = maxIndex + 1;
    return !conns.empty() && neuron_count > 0;
}

static bool load_spikes_csv(const std::string& path, std::vector<Spike>& spikes){
    spikes.clear();
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)){
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        std::istringstream ss(line);
        int n; float i; char comma;
        if (ss >> n >> comma >> i){ spikes.push_back({n,i}); }
    }
    return !spikes.empty();
}

static void build_geometry_shells(int neuron_count, const std::vector<Connection>& conns,
                                  std::vector<float>& line_xyz, std::vector<float>& line_w,
                                  std::vector<float>& neuron_pos){
    // Place neurons on spherical shells
    const int shell_count = 3;
    const float radius_base = 60.0f;
    neuron_pos.resize(neuron_count*3);
    for(int i=0;i<neuron_count;i++){
        int shell = i % shell_count;
        float r = radius_base * (1 + shell);
        float ang = (i / (float)neuron_count) * 6.2831853f;
        neuron_pos[i*3+0] = r * std::cos(ang);
        neuron_pos[i*3+1] = r * std::sin(ang);
        neuron_pos[i*3+2] = r * std::sin(ang*0.5f);
    }

    line_xyz.clear(); line_w.clear();
    for(const auto& c : conns){
        if (std::fabs(c.w) < 1e-6f) continue; // skip near-zero weights
        if (c.src<0 || c.src>=neuron_count || c.dst<0 || c.dst>=neuron_count) continue;
        line_xyz.push_back(neuron_pos[c.src*3+0]);
        line_xyz.push_back(neuron_pos[c.src*3+1]);
        line_xyz.push_back(neuron_pos[c.src*3+2]);
        line_xyz.push_back(neuron_pos[c.dst*3+0]);
        line_xyz.push_back(neuron_pos[c.dst*3+1]);
        line_xyz.push_back(neuron_pos[c.dst*3+2]);
        line_w.push_back(c.w);
        line_w.push_back(c.w);
    }
}

static void build_geometry_layers(int neuron_count, const std::vector<Connection>& conns,
                                  std::vector<float>& line_xyz, std::vector<float>& line_w,
                                  std::vector<float>& neuron_pos){
    const int layers = 4;
    const float pitch = 40.0f;
    const float span = layers * pitch;
    neuron_pos.resize(neuron_count*3);
    for(int i=0;i<neuron_count;i++){
        int layer = i % layers;
        neuron_pos[i*3+0] = (i % 20) * 10.0f - 100.0f;
        neuron_pos[i*3+1] = layer * pitch - span*0.5f;
        neuron_pos[i*3+2] = ((i/20)%20) * 10.0f - 100.0f;
    }

    line_xyz.clear(); line_w.clear();
    for(const auto& c : conns){
        if (std::fabs(c.w) < 1e-6f) continue;
        if (c.src<0 || c.src>=neuron_count || c.dst<0 || c.dst>=neuron_count) continue;
        line_xyz.push_back(neuron_pos[c.src*3+0]);
        line_xyz.push_back(neuron_pos[c.src*3+1]);
        line_xyz.push_back(neuron_pos[c.src*3+2]);
        line_xyz.push_back(neuron_pos[c.dst*3+0]);
        line_xyz.push_back(neuron_pos[c.dst*3+1]);
        line_xyz.push_back(neuron_pos[c.dst*3+2]);
        line_w.push_back(c.w);
        line_w.push_back(c.w);
    }
}

static void fit_camera_to_bounds(const std::vector<float>& neuron_pos, Visualizer3D& vis){
    if (neuron_pos.empty()) return;
    float minx=1e9f,miny=1e9f,minz=1e9f, maxx=-1e9f,maxy=-1e9f,maxz=-1e9f;
    for(size_t i=0;i<neuron_pos.size();i+=3){
        minx = std::min(minx, neuron_pos[i+0]);
        miny = std::min(miny, neuron_pos[i+1]);
        minz = std::min(minz, neuron_pos[i+2]);
        maxx = std::max(maxx, neuron_pos[i+0]);
        maxy = std::max(maxy, neuron_pos[i+1]);
        maxz = std::max(maxz, neuron_pos[i+2]);
    }
    float cx = 0.5f*(minx+maxx);
    float cy = 0.5f*(miny+maxy);
    float cz = 0.5f*(minz+maxz);
    float dx = (maxx-minx);
    float dy = (maxy-miny);
    float dz = (maxz-minz);
    float radius = std::sqrt(dx*dx + dy*dy + dz*dz) * 0.5f;
    float dist = std::max(60.0f, radius * 2.2f);
    vis.setCameraDistance(dist);
    // keep looking at origin; if needed we could offset model by -center
}

int main(int argc, char** argv){
    Args args;
    if (!parse_args(argc, argv, args)) return 1;

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(1280, 800, "NeuroForge Viewer", nullptr, nullptr);
    if (!win){ std::cerr << "Window create failed\n"; glfwTerminate(); return 1; }
    glfwMakeContextCurrent(win);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cerr << "Failed to init GLAD\n"; return 1;
    }

    Visualizer3D vis;
    if (!vis.initialize()){ std::cerr << "Visualizer init failed\n"; return 1; }

    // Load initial data
    int neuron_count = 0; std::vector<Connection> conns; float maxAbsW = 1.0f;
    if (!load_snapshot_csv(args.snapshot_file, neuron_count, conns, maxAbsW)){
        std::cerr << "Failed to load snapshot csv\n"; return 1;
    }

    std::vector<float> line_xyz, line_w, neuron_pos;
    if (args.layout == "layers"){
        build_geometry_layers(neuron_count, conns, line_xyz, line_w, neuron_pos);
    } else {
        build_geometry_shells(neuron_count, conns, line_xyz, line_w, neuron_pos);
    }

    vis.setLinesWithWeights(line_xyz, line_w);
    vis.setWeightMaxAbs(maxAbsW);
    std::cout << "Viewer init: neurons=" << neuron_count << " connections=" << (line_w.size()/2) << " vertices=" << (line_xyz.size()/3) << " max|w|=" << maxAbsW << std::endl;

    // Fit camera distance to bounds so geometry is visible on first frame
    fit_camera_to_bounds(neuron_pos, vis);

    // Spike points state
    std::vector<Spike> spikes; std::vector<float> spike_xyz; std::vector<float> spike_i;
    auto reload_spikes = [&](){
        if (args.spikes_file.empty()) { vis.clearSpikePoints(); return; }
        if (!load_spikes_csv(args.spikes_file, spikes)) { vis.clearSpikePoints(); return; }
        spike_xyz.clear(); spike_i.clear();
        spike_xyz.reserve(spikes.size()*3);
        spike_i.reserve(spikes.size());
        for(const auto& s : spikes){
            if (s.neuron<0 || s.neuron>=neuron_count) continue;
            float x = neuron_pos[s.neuron*3+0];
            float y = neuron_pos[s.neuron*3+1];
            float z = neuron_pos[s.neuron*3+2];
            spike_xyz.push_back(x); spike_xyz.push_back(y); spike_xyz.push_back(z);
            spike_i.push_back(std::clamp(s.intensity, 0.0f, 1.0f));
        }
        vis.setSpikePoints(spike_xyz, spike_i, /*point_size*/6.0f);
        std::cout << "Viewer spikes: count=" << spike_i.size() << std::endl;
    };

    reload_spikes();

    auto last_reload = std::chrono::steady_clock::now();

    double last_x=0.0, last_y=0.0; bool rotating=false, panning=false;
    glfwSetInputMode(win, GLFW_STICKY_KEYS, GLFW_TRUE);

    while(!glfwWindowShouldClose(win)){
        int fbw=0, fbh=0; glfwGetFramebufferSize(win, &fbw, &fbh);
        glViewport(0,0,fbw,fbh);
        glClearColor(0.05f,0.06f,0.08f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // input
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS){
            double x,y; glfwGetCursorPos(win, &x, &y);
            if (!rotating){ rotating=true; last_x=x; last_y=y; }
            double dx = x-last_x, dy=y-last_y; last_x=x; last_y=y;
            vis.addOrbitDelta((float)dx*0.01f, (float)-dy*0.01f);
        } else { rotating=false; }
        if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS){
            double x,y; glfwGetCursorPos(win, &x, &y);
            if (!panning){ panning=true; last_x=x; last_y=y; }
            double dx = x-last_x, dy=y-last_y; last_x=x; last_y=y;
            // Optional: implement panning by offsetting model
        } else { panning=false; }
        // zoom
        if (glfwGetKey(win, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_ADD) == GLFW_PRESS){ vis.addCameraDistance(-3.0f); }
        if (glfwGetKey(win, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(win, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS){ vis.addCameraDistance(3.0f); }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_reload).count() >= args.refresh_ms){
            last_reload = now;
            // Reload snapshot
            std::vector<Connection> conns2; float maxAbsW2 = 1.0f; int neuron_count2 = 0;
            if (load_snapshot_csv(args.snapshot_file, neuron_count2, conns2, maxAbsW2) && neuron_count2 == neuron_count){
                // rebuild lines only if counts match
                std::vector<float> line_xyz2, line_w2;
                if (args.layout == "layers"){
                    build_geometry_layers(neuron_count, conns2, line_xyz2, line_w2, neuron_pos); // neuron_pos kept for spikes
                } else {
                    build_geometry_shells(neuron_count, conns2, line_xyz2, line_w2, neuron_pos);
                }
                vis.setLinesWithWeights(line_xyz2, line_w2);
                vis.setWeightMaxAbs(maxAbsW2);
                std::cout << "Viewer refresh: connections=" << (line_w2.size()/2) << " vertices=" << (line_xyz2.size()/3) << " max|w|=" << maxAbsW2 << std::endl;
            }
            // Reload spikes
            reload_spikes();
        }

        vis.render(fbw, fbh);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}