#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <random>
#include <chrono>

//works well with square windows with relatively small sizes, helps to spawn multiple particles without performance drops
const int h = 800;
const int w = 800;

const int CELL_SIZE = 15;
const int GRID_WIDTH = w / CELL_SIZE;
const int GRID_HEIGHT = h / CELL_SIZE;

bool isPaused = false;
bool leftmousePressed = false;
bool rightmousePressed = false;

float colorChangeInterval = 0.01f;
float saturationLevel = 2.0f; //initial value
float speed = 7.0f; //initial value

double mouseX, mouseY;

enum CellType {
    EMPTY,
    SAND
};

struct Cell {
    CellType type;
    glm::vec4 color;

};

Cell grid[GRID_HEIGHT][GRID_WIDTH];
glm::vec4 currentColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

std::chrono::high_resolution_clock::time_point lastColorUpdateTime;


const char* vertexShaderSource = R"glsl(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * vec4(aPos, 0.0, 1.0);
    }
)glsl";

const char* fragmentShaderSource = R"glsl(
    #version 330 core
    out vec4 FragColor;
    uniform vec4 color;
    void main() {
        FragColor = color;
    }
)glsl";

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

unsigned int createShaderProgram() {
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void initializeGrid() {
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            grid[y][x] = { EMPTY, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) };
        }
    }
}

//add ( && grid[gridY - 1][gridX].type == EMPTY to all if statements to stop drawing on pre-existing sand)

void placeSand(int mouseX, int mouseY) {
    int gridX = mouseX / CELL_SIZE;
    int gridY = (GRID_HEIGHT - 1) - mouseY / CELL_SIZE;

    if (gridX < 0 || gridX > GRID_WIDTH || gridY < 0 || gridY > GRID_HEIGHT) {
        return;
    }
    if (gridX + 1 < GRID_WIDTH && gridY - 1 >= 0) {
        grid[gridY - 1][gridX] = { SAND, currentColor };
    }
}

void randomPlaceSand(int mouseX, int mouseY) {
    int gridX = mouseX / CELL_SIZE;
    int gridY = (GRID_HEIGHT - 1) - mouseY / CELL_SIZE;

    if (gridX < 0 || gridX >= GRID_WIDTH || gridY < 0 || gridY >= GRID_HEIGHT) {
        return;
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 5);
    int direction = dis(gen);

    if (direction == 1) {
        if (gridY - 1 >= 0) {
            grid[gridY + 3][gridX] = { SAND, currentColor };
        }
    }
    else if (direction == 2) {
        if (gridX - 1 >= 0 && gridY - 1 >= 0) {
            grid[gridY + 1][gridX - 2] = { SAND, currentColor };
        }
    }
    else if (direction == 3) {
        if (gridX + 1 < GRID_WIDTH && gridY - 1 >= 0) {
            grid[gridY + 1][gridX + 2] = { SAND, currentColor };
        }
    }
    else if (direction == 4) {
        if (gridX + 1 < GRID_WIDTH && gridY - 1 >= 0) {
            grid[gridY - 2][gridX - 1] = { SAND, currentColor };
        }
    }
    else if (direction == 5) {
        if (gridX + 1 < GRID_WIDTH && gridY - 1 >= 0) {
            grid[gridY - 2][gridX + 1] = { SAND, currentColor };
        }
    }
}

void updateSimulation() {
    if (!isPaused) {
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                if (grid[y][x].type == SAND) {
                    if (y - 1 >= 0 && grid[y - 1][x].type == EMPTY) {
                        grid[y - 1][x] = { SAND, grid[y][x].color };
                        grid[y][x] = { EMPTY, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) };
                    }
                    else if (x > 0 && y - 1 >= 0 && grid[y - 1][x - 1].type == EMPTY) {
                        grid[y - 1][x - 1] = { SAND, grid[y][x].color };
                        grid[y][x] = { EMPTY, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) };
                    }
                    else if (x < GRID_WIDTH - 1 && y - 1 >= 0 && grid[y - 1][x + 1].type == EMPTY) {
                        grid[y - 1][x + 1] = { SAND, grid[y][x].color };
                        grid[y][x] = { EMPTY, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f) };
                    }
                }
            }
        }
    }
}



unsigned int VAO = 0, VBO = 0;

void initializeRenderingResources() {
    if (VAO == 0 && VBO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }
}

void renderGrid(unsigned int shaderProgram, unsigned int projectionLoc, unsigned int colorLoc) {
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    float left = 0.0f;
    float right = GRID_WIDTH * CELL_SIZE;
    float bottom = 0.0f;
    float top = GRID_HEIGHT * CELL_SIZE;
    glm::mat4 projection = glm::ortho(left, right, bottom, top);

    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, &projection[0][0]);

    glBindVertexArray(VAO);

   
    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            if (grid[y][x].type == SAND) {
                glm::vec4 color = grid[y][x].color;

                
                float vertices[] = {
                    x * CELL_SIZE,         y * CELL_SIZE,
                    (x + 1) * CELL_SIZE,   y * CELL_SIZE,
                    (x + 1) * CELL_SIZE,   (y + 1) * CELL_SIZE,
                    x * CELL_SIZE,         (y + 1) * CELL_SIZE
                };

                
                glBindBuffer(GL_ARRAY_BUFFER, VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
              
                glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
          
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

glm::vec4 rgbToHsv(const glm::vec4& color) {
    float r = color.r;
    float g = color.g;
    float b = color.b;
    float cmax = std::max({ r, g, b });
    float cmin = std::min({ r, g, b });
    float diff = cmax - cmin;
    float h = 0.0f;
    float s = (cmax == 0) ? 0 : diff / cmax;
    float v = cmax;

    if (diff > 0) {
        if (cmax == r) {
            h = (g - b) / diff;
        }
        else if (cmax == g) {
            h = (b - r) / diff + 2;
        }
        else {
            h = (r - g) / diff + 4;
        }
        h *= 60;
        if (h < 0) h += 360;
    }
    return glm::vec4(h / 360.0f, s, v, color.a); // Normalized H value [0, 1]
}

glm::vec4 hsvToRgb(const glm::vec4& color) {
    float h = color.r * 360.0f;
    float s = color.g;
    float v = color.b;
    float c = v * s;
    float x = c * (1 - std::abs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (h < 60) {
        r = c; g = x; b = 0;
    }
    else if (h < 120) {
        r = x; g = c; b = 0;
    }
    else if (h < 180) {
        r = 0; g = c; b = x;
    }
    else if (h < 240) {
        r = 0; g = x; b = c;
    }
    else if (h < 300) {
        r = x; g = 0; b = c;
    }
    else {
        r = c; g = 0; b = x;
    }

    return glm::vec4(r + m, g + m, b + m, color.a);
}



void updateColor() {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    float elapsed = duration<float>(now - lastColorUpdateTime).count();

    if (elapsed >= colorChangeInterval) {
        lastColorUpdateTime = now;

        glm::vec4 hsv = rgbToHsv(currentColor);
        hsv.r += colorChangeInterval / speed;
        if (hsv.r > 1.0f) {
            hsv.r -= 1.0f;
        }
        currentColor = hsvToRgb(glm::vec4(hsv.r, saturationLevel, hsv.b, currentColor.a));
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    glfwGetCursorPos(window, &mouseX, &mouseY);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    currentColor = glm::vec4(dis(gen), dis(gen), dis(gen), 1.0f);

    if (action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            leftmousePressed = true;
            placeSand(static_cast<int>(mouseX), static_cast<int>(mouseY + 2));
            updateColor();
        }

        else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            rightmousePressed = true;
            randomPlaceSand(static_cast<int>(mouseX), static_cast<int>(mouseY + 2));
            updateColor();
        }
    }

    if (action == GLFW_RELEASE) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            leftmousePressed = false;
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            rightmousePressed = false;
        }
    }

}

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos) {
    if (leftmousePressed) {
        placeSand(static_cast<int>(xpos), static_cast<int>(ypos + 2));
    }
    else if (rightmousePressed) {
        randomPlaceSand(static_cast<int>(xpos), static_cast<int>(ypos + 2));
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        initializeGrid();
    }

    else if (key == GLFW_KEY_P && action == GLFW_RELEASE) { // Use 'P' key to toggle pause
        isPaused = !isPaused; // Toggle pause state
    }
}

int main() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);                     //version of the 
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);                     //opengl library
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(h, w, "falling sand", nullptr, nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    unsigned int shaderProgram = createShaderProgram();
    unsigned int projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    unsigned int colorLoc = glGetUniformLocation(shaderProgram, "color");

    initializeGrid();
    initializeRenderingResources();

    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetKeyCallback(window, key_callback);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImVec4 background_color = ImVec4(0.45f, 0.55f, 0.6f, 1.0f);


    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window)) {

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiIO& io = ImGui::GetIO();
        bool isMouseHandling = io.WantCaptureMouse;

        if (isMouseHandling) {
            leftmousePressed = false, rightmousePressed = false;
        }
        
        if (leftmousePressed || rightmousePressed) {
            updateColor();
        }
        
        updateSimulation();
        glClear(GL_COLOR_BUFFER_BIT);
        
        renderGrid(shaderProgram, projectionLoc, colorLoc);

        glfwPollEvents();

        ImVec2 initialWindowSize(640, 320);
        /*ImVec2 windowPos(100, 100);
        ImGui::SetWindowPos(windowPos);*/
        ImGui::SetNextWindowSize(initialWindowSize, ImGuiCond_FirstUseEver);

        ImGui::Begin("Properties");;
        ImGui::Text("any value except sand size can be changed (since its set during compile time)");
        ImGui::SliderFloat("lightness(lower is brighter)", &saturationLevel, 0.01f, 10.0f);
        ImGui::SliderFloat("color cycle speed", &speed, 0.1f, 12.0f);
        ImGui::SliderFloat("color change time", &colorChangeInterval, 0.01f, 10.0f);
        ImGui::ColorEdit3("background", (float*)&background_color);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();


        ImGui::Render();
        glClearColor(background_color.x * background_color.w, background_color.y * background_color.w, background_color.z * background_color.w, background_color.w);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
