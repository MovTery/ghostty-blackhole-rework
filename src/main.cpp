 // blackhole standalone — Windows OpenGL host for blackhole.glsl
 // Build: mkdir build && cd build && cmake .. && make
 // Requires: GLFW 3.4 (mingw-w64-ucrt-x86_64-glfw), OpenGL 3.3
 
 #include <cstdio>
 #include <cstdlib>
 #include <cmath>
 #include <ctime>
 #include <string>
 #include <fstream>
#include <sstream>
#include <vector>
#include <regex>
#include <cstring>
#include <windows.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <GL/gl.h>

// =====================================================================
// OpenGL function pointers (loaded via glfwGetProcAddress)
// =====================================================================

#ifndef GL_COMPILE_STATUS
#include <GL/glcorearb.h>
#endif

#define DECL_GL_FUNC(ret, name, args) \
     typedef ret (WINAPI *PFN_##name##_PROC) args; \
     static PFN_##name##_PROC gl_##name = nullptr
 
 DECL_GL_FUNC(GLuint, CreateShader, (GLenum));
 DECL_GL_FUNC(void,   ShaderSource, (GLuint, GLsizei, const GLchar**, const GLint*));
 DECL_GL_FUNC(void,   CompileShader, (GLuint));
 DECL_GL_FUNC(void,   GetShaderiv, (GLuint, GLenum, GLint*));
 DECL_GL_FUNC(void,   GetShaderInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*));
 DECL_GL_FUNC(GLuint, CreateProgram, (void));
 DECL_GL_FUNC(void,   AttachShader, (GLuint, GLuint));
 DECL_GL_FUNC(void,   LinkProgram, (GLuint));
 DECL_GL_FUNC(void,   GetProgramiv, (GLuint, GLenum, GLint*));
 DECL_GL_FUNC(void,   GetProgramInfoLog, (GLuint, GLsizei, GLsizei*, GLchar*));
 DECL_GL_FUNC(void,   DeleteShader, (GLuint));
 DECL_GL_FUNC(void,   UseProgram, (GLuint));
 DECL_GL_FUNC(GLint,  GetUniformLocation, (GLuint, const GLchar*));
 DECL_GL_FUNC(void,   Uniform3f, (GLint, GLfloat, GLfloat, GLfloat));
 DECL_GL_FUNC(void,   Uniform1f, (GLint, GLfloat));
 DECL_GL_FUNC(void,   Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat));
 DECL_GL_FUNC(void,   GenVertexArrays, (GLsizei, GLuint*));
 DECL_GL_FUNC(void,   GenBuffers, (GLsizei, GLuint*));
 DECL_GL_FUNC(void,   BindVertexArray, (GLuint));
 DECL_GL_FUNC(void,   BindBuffer, (GLenum, GLuint));
 DECL_GL_FUNC(void,   BufferData, (GLenum, GLsizeiptr, const void*, GLenum));
 DECL_GL_FUNC(void,   VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*));
 DECL_GL_FUNC(void,   EnableVertexAttribArray, (GLuint));
 DECL_GL_FUNC(void,   DrawArrays, (GLenum, GLint, GLsizei));
 DECL_GL_FUNC(void,   DeleteVertexArrays, (GLsizei, const GLuint*));
 DECL_GL_FUNC(void,   DeleteBuffers, (GLsizei, const GLuint*));
 DECL_GL_FUNC(void,   DeleteProgram, (GLuint));
 
 #define LOAD_GL_FUNC(name) do { \
     gl_##name = (PFN_##name##_PROC)glfwGetProcAddress("gl" #name); \
     if (!gl_##name) { fprintf(stderr, "Failed to load gl" #name "\n"); return false; } \
 } while(0)
 
 static bool loadGLFunctions() {
     LOAD_GL_FUNC(CreateShader);
     LOAD_GL_FUNC(ShaderSource);
     LOAD_GL_FUNC(CompileShader);
     LOAD_GL_FUNC(GetShaderiv);
     LOAD_GL_FUNC(GetShaderInfoLog);
     LOAD_GL_FUNC(CreateProgram);
     LOAD_GL_FUNC(AttachShader);
     LOAD_GL_FUNC(LinkProgram);
     LOAD_GL_FUNC(GetProgramiv);
     LOAD_GL_FUNC(GetProgramInfoLog);
     LOAD_GL_FUNC(DeleteShader);
     LOAD_GL_FUNC(UseProgram);
     LOAD_GL_FUNC(GetUniformLocation);
     LOAD_GL_FUNC(Uniform3f);
     LOAD_GL_FUNC(Uniform1f);
     LOAD_GL_FUNC(Uniform4f);
     LOAD_GL_FUNC(GenVertexArrays);
     LOAD_GL_FUNC(GenBuffers);
     LOAD_GL_FUNC(BindVertexArray);
     LOAD_GL_FUNC(BindBuffer);
     LOAD_GL_FUNC(BufferData);
     LOAD_GL_FUNC(VertexAttribPointer);
     LOAD_GL_FUNC(EnableVertexAttribArray);
     LOAD_GL_FUNC(DrawArrays);
     LOAD_GL_FUNC(DeleteVertexArrays);
     LOAD_GL_FUNC(DeleteBuffers);
     LOAD_GL_FUNC(DeleteProgram);
     return true;
 }
 
 // =====================================================================
 // Shader helpers
 // =====================================================================
 
static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) {
        fprintf(stderr, "Error: Cannot open %s\n", path);
        return "";
    }
    std::stringstream ss;
     ss << f.rdbuf();
     return ss.str();
 }
 
 static GLuint compileShader(GLenum type, const std::string& source) {
    GLuint shader = gl_CreateShader(type);
    const char* src = source.c_str();
    gl_ShaderSource(shader, 1, &src, nullptr);
    gl_CompileShader(shader);
    GLint ok = 0;
    gl_GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    char log[4096];
    gl_GetShaderInfoLog(shader, sizeof(log), nullptr, log);
    if (log[0]) fprintf(stderr, "[%s] log: %s\n",
        type == GL_VERTEX_SHADER ? "vert" : "frag", log);
    if (!ok) {
        fprintf(stderr, "Shader compile ERROR:\n%s\n", log);
        gl_DeleteShader(shader);
        return 0;
    }
     return shader;
 }
 
 static GLuint createProgram(const std::string& vertSrc, const std::string& fragSrc) {
     GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
     if (!vs) return 0;
     GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
     if (!fs) { gl_DeleteShader(vs); return 0; }
     GLuint prog = gl_CreateProgram();
     gl_AttachShader(prog, vs);
     gl_AttachShader(prog, fs);
    gl_LinkProgram(prog);
    GLint ok = 0;
    gl_GetProgramiv(prog, GL_LINK_STATUS, &ok);
    char log[4096];
    gl_GetProgramInfoLog(prog, sizeof(log), nullptr, log);
    if (log[0]) fprintf(stderr, "Link log:\n%s\n", log);
    if (!ok) {
        fprintf(stderr, "Program link ERROR:\n%s\n", log);
        gl_DeleteProgram(prog);
        gl_DeleteShader(vs);
        gl_DeleteShader(fs);
         return 0;
     }
     gl_DeleteShader(vs);
     gl_DeleteShader(fs);
     return prog;
 }
 
 // =====================================================================
 // Config: set SIZE_MODE in blackhole.glsl to MODE_DEMO for showcase,
 // or MODE_POMODORO for perpetual growth (no idle detection in standalone)
 // =====================================================================
 
static bool buildFragmentShader(std::string& out)  {
    // Use simplified shader to avoid NVIDIA Cg compiler issues
    out = readFile("shaders/frag_simple.glsl");
    return !out.empty();
}
 
 // =====================================================================
 // Main
 // =====================================================================

// ---- Blackhole mode system ----
enum BlackholeMode { MODE_ALWAYS, MODE_IDLE, MODE_OFF };

static bool isIdle(DWORD thresholdMs) {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    if (!GetLastInputInfo(&lii)) return false;
    return (GetTickCount() - lii.dwTime) >= thresholdMs;
}

 
int main(int argc, char* argv[]) {
    // Parse mode from command line
    BlackholeMode bhMode = MODE_ALWAYS;
    int idleSec = 300; // default idle threshold: 5 min
    if (argc > 1) {
        if (strcmp(argv[1], "idle") == 0) bhMode = MODE_IDLE;
        else if (strcmp(argv[1], "off") == 0) bhMode = MODE_OFF;
    }
    if (argc > 2 && bhMode == MODE_IDLE) {
        idleSec = atoi(argv[2]);
        if (idleSec < 10) idleSec = 10;
    }
    if (bhMode == MODE_OFF) {
        fprintf(stderr, "Blackhole: MODE_OFF, exiting.\\n");
        return 0;
    }
    fprintf(stderr, "Blackhole: mode=%s idle=%ds\\n",
        bhMode == MODE_IDLE ? "idle" : "always", idleSec);

     // Initialize GLFW
     if (!glfwInit()) {
         fprintf(stderr, "Failed to initialize GLFW\n");
         return 1;
     }
 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
     glfwWindowHint(GLFW_DECORATED, GL_FALSE);
     glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
 
     // Get primary monitor video mode for sizing
     GLFWmonitor* monitor = glfwGetPrimaryMonitor();
     const GLFWvidmode* mode = glfwGetVideoMode(monitor);
     int winW = mode->width * 3 / 4;
     int winH = mode->height * 3 / 4;
 
     GLFWwindow* window = glfwCreateWindow(winW, winH, "Black Hole (ESC to exit)", nullptr, nullptr);
     if (!window) {
         fprintf(stderr, "Failed to create window\n");
         glfwTerminate();
         return 1;
     }

    // Desktop overlay: always-on-top + click-through
    {
        HWND hwnd = glfwGetWin32Window(window);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
    }

 
    glfwMakeContextCurrent(window);
    setbuf(stderr, NULL);
    glfwSwapInterval(1);  // VSync on

    fprintf(stderr, "OpenGL %s, GLSL %s\n",
        glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (!loadGLFunctions()) {
         glfwTerminate();
         return 1;
     }
 
     // Build and compile shaders
     std::string vertSrc = readFile("shaders/vert.glsl");
     if (vertSrc.empty()) { glfwTerminate(); return 1; }
     std::string fragSrc;
     if (!buildFragmentShader(fragSrc)) { glfwTerminate(); return 1; }
 
    GLuint program = createProgram(vertSrc, fragSrc);
    if (!program) {
        fprintf(stderr, "FATAL: Blackhole shader failed to compile/link.\n");
        glfwTerminate();
        return 1; }

     // Full-screen quad: two triangles
     float vertices[] = {
         -1.0f, -1.0f,
          1.0f, -1.0f,
         -1.0f,  1.0f,
          1.0f,  1.0f,
     };
     GLuint indices[] = { 0, 1, 2, 1, 3, 2 };
 
     GLuint vao, vbo, ebo;
     gl_GenVertexArrays(1, &vao);
     gl_GenBuffers(1, &vbo);
     gl_GenBuffers(1, &ebo);
 
     gl_BindVertexArray(vao);
     gl_BindBuffer(GL_ARRAY_BUFFER, vbo);
     gl_BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
     gl_BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
     gl_BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
     gl_VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
     gl_EnableVertexAttribArray(0);
     gl_BindVertexArray(0);
 
     // Uniform locations
     gl_UseProgram(program);
     GLint locResolution = gl_GetUniformLocation(program, "iResolution");
     GLint locTime       = gl_GetUniformLocation(program, "iTime");
     GLint locDate       = gl_GetUniformLocation(program, "iDate");
     gl_UseProgram(0);
 
     // Main loop
     double startTime = glfwGetTime();
     int frames = 0;
     double lastFpsTime = startTime;
     char title[128];
 
     while (!glfwWindowShouldClose(window)) {
         glfwPollEvents();
 
         // ESC to exit
         if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
             glfwSetWindowShouldClose(window, GL_TRUE);

        // Idle mode: auto show/hide based on user activity
        if (bhMode == MODE_IDLE) {
            if (isIdle((DWORD)idleSec * 1000)) {
                glfwShowWindow(window);
                glfwSetWindowOpacity(window, 1.0f);
            } else {
                glfwHideWindow(window);
                Sleep(250);
                continue;
            }
        } else if (bhMode == MODE_ALWAYS) {
            if (!glfwGetWindowAttrib(window, GLFW_VISIBLE))
                glfwShowWindow(window);
        }

 
         // Window size
         int fbW, fbH;
         glfwGetFramebufferSize(window, &fbW, &fbH);
         glViewport(0, 0, fbW, fbH);
 
         // Update uniforms
         double now = glfwGetTime();
         float t = (float)(now - startTime);
         // iDate.w = seconds since epoch (for pomodoro wall clock)
         float epochSec = (float)time(nullptr);
 
         gl_UseProgram(program);
         gl_Uniform3f(locResolution, (float)fbW, (float)fbH, 0.0f);
         gl_Uniform1f(locTime, t);
         gl_Uniform4f(locDate, 0.0f, 0.0f, 0.0f, epochSec);
 
         gl_BindVertexArray(vao);
         gl_DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         gl_BindVertexArray(0);
         gl_UseProgram(0);
 
         glfwSwapBuffers(window);
 
         // FPS counter in title bar
         frames++;
         if (now - lastFpsTime >= 1.0) {
             snprintf(title, sizeof(title), "Black Hole  [%d FPS]  (ESC to exit)", frames);
             glfwSetWindowTitle(window, title);
             frames = 0;
             lastFpsTime = now;
         }
     }
 
     // Cleanup
     gl_DeleteProgram(program);
     gl_DeleteVertexArrays(1, &vao);
     gl_DeleteBuffers(1, &vbo);
     gl_DeleteBuffers(1, &ebo);
     glfwTerminate();
     return 0;
 }

