# Neon Highway (Headline Technique: Real-Time HDR Bloom)

**Course:** Computer Graphics Fundamentals (Academic Year 2026–2027)  
**Institution:** Astana IT University  
**Instructor:** Almas Zhanabilov  
**Group:** SE-2416  
**Team Members:**
* **Ayauzhan Baurzhanova:** Renderer, Phong Lighting, Procedural 2D Textures, Multi-Pass HDR FBOs & Gaussian Blur Bloom Shaders
* **Zhanassyl Sherkenov:** Core OpenGL/Camera Initialization, Hierarchical Car Matrix Stack, Procedural Spawning, AABB Collisions & 2D Orthographic UI/HUD

---

## 1. Project Overview
**Neon Highway** is a real-time 3D endless-runner game built from scratch in **OpenGL 3.3 Core Profile** (C++17). The player steers a hierarchically modeled cyberpunk vehicle down a scrolling synthwave highway, dodging hazard barriers and collecting glowing neon batteries (+50 bonus points).

### Key Technical Features
1. **Multi-Pass HDR Bloom Pipeline (`scene.frag`, `blur.vert`, `blur.frag`):**
   * **Pass 1 (MRT Extraction):** Renders the 3D scene into a 16-bit floating-point Framebuffer Object (`GL_RGBA16F`) with two color attachments (`GL_COLOR_ATTACHMENT0` for the base HDR scene, and `GL_COLOR_ATTACHMENT1` for pixels whose relative luminance $Y = 0.2126R + 0.7152G + 0.0722B > 1.0$).
   * **Pass 2 (Separable Gaussian Blur):** Uses two ping-pong FBOs to execute 10 alternating horizontal and vertical 9-tap Gaussian blur passes on the extracted bright pixels.
   * **Pass 3 (HDR Additive Blending & Tone Mapping):** Combines the blurred glow texture with the base HDR image (toggleable via the **`B`** key) and applies exponential exposure tone mapping and gamma correction ($\gamma = 2.2$).
2. **Hierarchical 3D Vehicle Modeling (`renderHierarchicalCar`):**
   * Uses `std::stack<glm::mat4>` (`push` / `pop`) with a parent Chassis root node (which translates and tilts during steering) and attached child nodes: Cockpit Canopy, Twin Glowing Tail Lights, and 4 independently spinning Wheels.
3. **Per-Fragment Phong Lighting & Movable 3D Light (`scene.vert`, `scene.frag`):**
   * Full Ambient, Diffuse, and Specular Phong shading with distance attenuation. The 3D light source automatically glides along the highway or can be moved manually along X, Y, and Z axes using the keyboard.
4. **Procedural Textures & Entity Spawning:**
   * Generates a 256×256 synthwave grid road texture, hazard-striped obstacle texture, and a 1024×8 2D bitmap font atlas procedurally in C++ memory—eliminating missing external asset file errors.
5. **2D Orthographic HUD Overlay (`ui.vert`, `ui.frag`):**
   * Renders on-screen text displaying Team Names, Group (`SE-2416`), Instructor (`Almas Zhanabilov`), Project Title, live Score, High Score, FPS, Camera Mode, and Bloom State (`ON`/`OFF`).

---

## 2. System & Build Requirements
* **OS:** Windows 10 or Windows 11 (64-bit)
* **Compiler / IDE:** Microsoft Visual Studio 2022 (with *Desktop development with C++* workload, MSVC v143, x64 target)
* **OpenGL Version:** OpenGL 3.3 Core Profile
* **Dependencies (Managed automatically via Visual Studio NuGet):**
  * `glfw` (Window creation, OpenGL context, keyboard input)
  * `nupengl.core` (GLEW for modern OpenGL 3.3+ extension loading)
  * `glm` (OpenGL Mathematics for vectors, matrices, `lookAt`, `perspective`, `ortho`)
  * `opengl32.lib` (Windows system OpenGL library)

---

## 3. How to Build and Run (Visual Studio on Windows 10/11)

### Option A: Opening the Existing Visual Studio Solution
1. Extract the project archive and open `NeonHighway.sln` in **Visual Studio 2022**.
2. In the top toolbar, set the build configuration to **`Debug`** (or **`Release`**) and the platform to **`x64`**.
3. If NuGet packages are not yet restored, right-click theSolution in Solution Explorer and click **Restore NuGet Packages**.
4. Ensure all 6 shader files (`scene.vert`, `scene.frag`, `blur.vert`, `blur.frag`, `ui.vert`, `ui.frag`) are located in the same directory as `main.cpp` (and the working directory).
5. Press **`F5`** (or click **Local Windows Debugger**) to build and run.

### Option B: Clean Build From Scratch
1. Open Visual Studio -> **Create a new project** -> **Empty Project (C++, Windows, Console)** -> Name it `NeonHighway`.
2. Set top toolbar dropdowns to **`Debug`** and **`x64`**.
3. Right-click `NeonHighway` in Solution Explorer -> **Manage NuGet Packages...** -> **Browse** tab -> Install:
   * `glfw`
   * `nupengl.core`
   * `glm`
4. Right-click `NeonHighway` -> **Properties** -> **Linker** -> **Input** -> **Additional Dependencies** -> append `;opengl32.lib` -> Click **Apply** and **OK**.
5. Add `main.cpp` to **Source Files** and place `scene.vert`, `scene.frag`, `blur.vert`, `blur.frag`, `ui.vert`, and `ui.frag` in the project directory alongside `main.cpp`.
6. Press **`F5`** to compile and run.

---

## 4. Controls & Keyboard Shortcuts

| Key | Action / Description |
| :--- | :--- |
| **`SPACE`** | Start Game (from Start Screen) or Restart Game (from Game Over Screen) |
| **`A` / `D`** | Steer Player Car Left / Right across the 3 highway lanes |
| **`B`** | **Toggle Headline Technique (Bloom ON / OFF)** *(Mandatory Defense Toggle)* |
| **`P`** | **Pause / Resume** all game & animation updates |
| **`R`** | **Reset Game** immediately (clears obstacles and resets score to 0) |
| **`C`** | **Toggle Camera Mode:** Game Follow Camera vs. 6-DOF Free-Fly Camera |
| **`W` / `S` / `A` / `D` / `Q` / `E`** | Fly Camera Forward / Backward / Left / Right / Down / Up *(when in Free-Fly Camera mode)* |
| **`Arrow Keys` (`Left`/`Right`/`Up`/`Down`)** | Manually move the 3D Light Source along the X and Z axes |
| **`U` / `O`** | Manually move the 3D Light Source Up / Down along the Y axis |
| **`L`** | Toggle Automatic Streetlamp Light Motion ON / Manual |
| **`H` or `F1`** | Toggle the on-screen 2D Help / Controls Popup Overlay & print controls to console |
| **`ESC`** | Exit the application |
