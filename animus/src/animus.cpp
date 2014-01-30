#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <time.h>
#include <sstream>

#include <vector>

#include "glew/glew.h"
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "GL/glfw.h"
#include "stb/stb_image.h"
#include "imgui/imgui.h"
#include "imgui/imguiRenderGL3.h"

#include "glm/glm.hpp"
#include "glm/vec3.hpp" // glm::vec3
#include "glm/vec4.hpp" // glm::vec4, glm::ivec4
#include "glm/mat4x4.hpp" // glm::mat4
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // glm::value_ptr

#include "CameraTools.hpp"
#include "ShaderTools.hpp"
#include "IMGUITools.hpp"
#include "SoundTools.hpp"

#ifndef DEBUG_PRINT
#define DEBUG_PRINT 1
#endif

#if DEBUG_PRINT == 0
#define debug_print(FORMAT, ...) ((void)0)
#else
#ifdef _MSC_VER
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)
#else
#define debug_print(FORMAT, ...) \
    fprintf(stderr, "%s() in %s, line %i: " FORMAT "\n", \
        __func__, __FILE__, __LINE__, __VA_ARGS__)
#endif
#endif
   
#define SPECTRUM_SIZE 512

int main( int argc, char **argv )
{

    SoundManager soundManager;
    unsigned int idSound1 = soundManager.createSound("sounds/sound1.wav");
    soundManager.playSound(idSound1);
    float spectrum[SPECTRUM_SIZE];


    int width = 1920, height = 1080;
    float widthf = (float) width, heightf = (float) height;
    double t;
    float fps = 0.f;

    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }

    // Force core profile on Mac OSX
#ifdef __APPLE__
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
    glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
    glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    // Open a window and create its OpenGL context
    if( !glfwOpenWindow( width, height, 0,0,0,0, 24, 0, GLFW_FULLSCREEN ) )
    {
        fprintf( stderr, "Failed to open GLFW window\n" );

        glfwTerminate();
        exit( EXIT_FAILURE );
    }
glfwEnable( GLFW_MOUSE_CURSOR );
    glfwSetWindowTitle( "Animus" );


    // Core profile is flagged as experimental in glew
#ifdef __APPLE__
    glewExperimental = GL_TRUE;
#endif
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
          /* Problem: glewInit failed, something is seriously wrong. */
          fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
          exit( EXIT_FAILURE );
    }

    // Ensure we can capture the escape key being pressed below
    glfwEnable( GLFW_STICKY_KEYS );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );
    GLenum glerr = GL_NO_ERROR;
    glerr = glGetError();

    if (!imguiRenderGLInit(DroidSans_ttf, DroidSans_ttf_len))
    {
        fprintf(stderr, "Could not init GUI renderer.\n");
        exit(EXIT_FAILURE);
    }

    // Init viewer structures
    Camera camera;
    camera_defaults(camera);
    GUIStates guiStates;
    init_gui_states(guiStates);



    // Load images and upload textures
    GLuint textures[3];
    glGenTextures(3, textures);
    int x;
    int y;
    int comp; 
    unsigned char * diffuse = stbi_load("textures/spnza_bricks_a_diff.tga", &x, &y, &comp, 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, diffuse);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    fprintf(stderr, "Diffuse %dx%d:%d\n", x, y, comp);
    unsigned char * spec = stbi_load("textures/spnza_bricks_a_spec.tga", &x, &y, &comp, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, x, y, 0, GL_RED, GL_UNSIGNED_BYTE, spec);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    fprintf(stderr, "Spec %dx%d:%d\n", x, y, comp);

    // Try to load and compile shader
    int status;
    ShaderGLSL gbuffer_shader;
    const char * shaderFileGBuffer = "src/2_gbuffer.glsl";
    //int status = load_shader_from_file(gbuffer_shader, shaderFileGBuffer, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER | ShaderGLSL::GEOMETRY_SHADER);
    status = load_shader_from_file(gbuffer_shader, shaderFileGBuffer, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 )
    {
        fprintf(stderr, "Error on loading  %s\n", shaderFileGBuffer);
        exit( EXIT_FAILURE );
    }    

    // Compute locations for gbuffer_shader
    GLuint gbuffer_projectionLocation = glGetUniformLocation(gbuffer_shader.program, "Projection");
    GLuint gbuffer_viewLocation = glGetUniformLocation(gbuffer_shader.program, "View");
    GLuint gbuffer_objectLocation = glGetUniformLocation(gbuffer_shader.program, "Object");
    
    GLuint gbuffer_diffuseLocation = glGetUniformLocation(gbuffer_shader.program, "Diffuse");
    GLuint gbuffer_specLocation = glGetUniformLocation(gbuffer_shader.program, "Spec");

    // Animate
    GLuint gbuffer_timeLocation = glGetUniformLocation(gbuffer_shader.program, "Time");
    GLuint gbuffer_instanceCountLocation = glGetUniformLocation(gbuffer_shader.program, "InstanceCount");
    GLuint gbuffer_amplitudeLocation = glGetUniformLocation(gbuffer_shader.program, "Amplitude");
    GLuint gbuffer_spaceBetweenCubesLocation = glGetUniformLocation(gbuffer_shader.program, "SpaceBetweenCubes");
    GLuint gbuffer_spectrumOffsetLocation = glGetUniformLocation(gbuffer_shader.program, "SpectrumOffset");

    // Load Blit shader
    ShaderGLSL blit_shader;
    const char * shaderFileBlit = "src/2_blit.glsl";
    //int status = load_shader_from_file(blit_shader, shaderFileBlit, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER | ShaderGLSL::GEOMETRY_SHADER);
    status = load_shader_from_file(blit_shader, shaderFileBlit, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 )
    {
        fprintf(stderr, "Error on loading  %s\n", shaderFileBlit);
        exit( EXIT_FAILURE );
    }    

    // Compute locations for blit_shader
    GLuint blit_tex1Location = glGetUniformLocation(blit_shader.program, "Texture1");

    // Load light accumulation shader
    ShaderGLSL lighting_shader;
    const char * shaderFileLighting = "src/2_light.glsl";
    //int status = load_shader_from_file(lighting_shader, shaderFileLighting, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER | ShaderGLSL::GEOMETRY_SHADER);
    status = load_shader_from_file(lighting_shader, shaderFileLighting, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 )
    {
        fprintf(stderr, "Error on loading  %s\n", shaderFileLighting);
        exit( EXIT_FAILURE );
    }    
    // Compute locations for lighting_shader
    GLuint lighting_materialLocation = glGetUniformLocation(lighting_shader.program, "Material");
    GLuint lighting_normalLocation = glGetUniformLocation(lighting_shader.program, "Normal");
    GLuint lighting_depthLocation = glGetUniformLocation(lighting_shader.program, "Depth");
    GLuint lighting_timeLocation = glGetUniformLocation(lighting_shader.program, "Time");
    GLuint lighting_inverseViewProjectionLocation = glGetUniformLocation(lighting_shader.program, "InverseViewProjection");
    GLuint lighting_cameraPositionLocation = glGetUniformLocation(lighting_shader.program, "CameraPosition");
    GLuint lighting_lightPositionLocation = glGetUniformLocation(lighting_shader.program, "LightPosition");
    GLuint lighting_lightDiffuseColorLocation = glGetUniformLocation(lighting_shader.program, "LightDiffuseColor");
    GLuint lighting_lightSpecularColorLocation = glGetUniformLocation(lighting_shader.program, "LightSpecColor");
    GLuint lighting_lightIntensityLocation = glGetUniformLocation(lighting_shader.program, "LightIntensity");
    GLuint lighting_projectionLocation = glGetUniformLocation(lighting_shader.program, "Projection");
    

    // Load geometry
    int   cube_triangleCount = 12;
    int   cube_triangleList[] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 19, 17, 20, 21, 22, 23, 24, 25, 26, };
    float cube_uvs[] = {0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f, 1.f, 1.f, 0.f,  1.f, 0.f,  1.f, 1.f,  0.f, 1.f,  1.f, 1.f,  0.f, 0.f, 0.f, 0.f, 1.f, 1.f,  1.f, 0.f,  };
    float cube_vertices[] = {-0.5, -0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, -0.5, -0.5, 0.5, -0.5, -0.5, -0.5, 0.5, -0.5, 0.5, 0.5 };
    float cube_normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, -1, 0, 0, };
    int   plane_triangleCount = 2;
    int   plane_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float plane_uvs[] = {0.f, 0.f, 0.f, 10.f, 10.f, 0.f, 10.f, 10.f};
    float plane_vertices[] = {-50.0, -1.0, 50.0, 50.0, -1.0, 50.0, -50.0, -1.0, -50.0, 50.0, -1.0, -50.0};
    float plane_normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
    int   quad_triangleCount = 2;
    int   quad_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float quad_vertices[] =  {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    // Vertex Array Object
    GLuint vao[3];
    glGenVertexArrays(3, vao);

    // Vertex Buffer Objects
    GLuint vbo[12];
    glGenBuffers(12, vbo);

    // Cube
    glBindVertexArray(vao[0]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_triangleList), cube_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_normals), cube_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[3]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_uvs), cube_uvs, GL_STATIC_DRAW);

    // Plane
    glBindVertexArray(vao[1]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[4]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[5]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
    // Bind normals and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[6]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // Bind uv coords and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[7]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    // Quad
    glBindVertexArray(vao[2]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[8]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_triangleList), quad_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[9]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Unbind everything. Potentially illegal on some implementations
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Init frame buffers
    GLuint gbufferFbo;
    GLuint gbufferTextures[3];
    GLuint gbufferDrawBuffers[2];
    glGenTextures(3, gbufferTextures);

    // Create color texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth texture
    glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create Framebuffer Object
    glGenFramebuffers(1, &gbufferFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);

    // Attach textures to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, gbufferTextures[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 , GL_TEXTURE_2D, gbufferTextures[1], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gbufferTextures[2], 0);
    gbufferDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    gbufferDrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    // Note : la profondeur se remplit automatiquement.

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building framebuffer\n");
        exit( EXIT_FAILURE );
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Light data
    srand (time(NULL));

    std::vector<glm::vec3> pointLightsPositions;
    std::vector<glm::vec3> pointLightsDiffuseColors;
    std::vector<glm::vec3> pointLightsSpecularColors;
    std::vector<float> pointLightsIntensities;
    std::vector<bool> pointLightCollapses;

    float numPointLights = 0.f;

    // Animation Data
    float timeStep = 0.1f;
    float timeSpend = 0.f;
    float nbCubeInstance = 9.f;
    float amplitude = 0.2f;
    float spaceBetweenCubes = 0.f;

    bool showDeferrefTextures = false;
    int showUI = true;

    int spectrumStepLoop = 0;

    do
    {

        t = glfwGetTime();

        // Mouse states
        int leftButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_LEFT );
        int rightButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_RIGHT );
        int middleButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_MIDDLE );

        if( leftButton == GLFW_PRESS )
            guiStates.turnLock = true;
        else
            guiStates.turnLock = false;

        if( rightButton == GLFW_PRESS )
            guiStates.zoomLock = true;
        else
            guiStates.zoomLock = false;

        if( middleButton == GLFW_PRESS )
            guiStates.panLock = true;
        else
            guiStates.panLock = false;

        int spacePressed = glfwGetKey(GLFW_KEY_SPACE);
        if(spacePressed)
        {
            showUI = !showUI;
        }

        // Camera movements
        int altPressed = glfwGetKey(GLFW_KEY_LSHIFT);
        if (!altPressed && (leftButton == GLFW_PRESS || rightButton == GLFW_PRESS || middleButton == GLFW_PRESS))
        {
            int x; int y;
            glfwGetMousePos(&x, &y);
            guiStates.lockPositionX = x;
            guiStates.lockPositionY = y;
        }
        if (altPressed == GLFW_PRESS)
        {
            int mousex; int mousey;
            glfwGetMousePos(&mousex, &mousey);
            int diffLockPositionX = mousex - guiStates.lockPositionX;
            int diffLockPositionY = mousey - guiStates.lockPositionY;
            if (guiStates.zoomLock)
            {
                float zoomDir = 0.0;
                if (diffLockPositionX > 0)
                    zoomDir = -1.f;
                else if (diffLockPositionX < 0 )
                    zoomDir = 1.f;
                camera_zoom(camera, zoomDir * GUIStates::MOUSE_ZOOM_SPEED);
            }
            else if (guiStates.turnLock)
            {
                camera_turn(camera, diffLockPositionY * GUIStates::MOUSE_TURN_SPEED,
                            diffLockPositionX * GUIStates::MOUSE_TURN_SPEED);

            }
            else if (guiStates.panLock)
            {
                camera_pan(camera, diffLockPositionX * GUIStates::MOUSE_PAN_SPEED,
                            diffLockPositionY * GUIStates::MOUSE_PAN_SPEED);
            }
            guiStates.lockPositionX = mousex;
            guiStates.lockPositionY = mousey;
        }
  
        // Get camera matrices
        glm::mat4 projection = glm::perspective(45.0f, widthf / heightf, 0.1f, 100.f); 
        glm::mat4 worldToView = glm::lookAt(camera.eye, camera.o, camera.up);
        glm::mat4 objectToWorld;
        glm::mat4 worldToScreen = projection * worldToView;
        glm::mat4 screenToWorld = glm::transpose(glm::inverse(worldToScreen));

        timeSpend += timeStep;
        if(timeSpend > 2*3.1415)
        {
            timeSpend = 0;
        }

        // Viewport 
        glViewport( 0, 0, width, height);

        // Default states
        glEnable(GL_DEPTH_TEST);

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Bind gbuffer shader
        glUseProgram(gbuffer_shader.program);
        
        // Upload uniforms
        glUniformMatrix4fv(gbuffer_projectionLocation, 1, 0, glm::value_ptr(projection));
        glUniformMatrix4fv(gbuffer_viewLocation, 1, 0, glm::value_ptr(worldToView));
        glUniformMatrix4fv(gbuffer_objectLocation, 1, 0, glm::value_ptr(objectToWorld));
        glUniform1f(gbuffer_timeLocation, t);
        glUniform1i(gbuffer_diffuseLocation, 0);
        glUniform1i(gbuffer_specLocation, 1);
        glUniform1f(gbuffer_timeLocation, timeSpend);
        glUniform1i(gbuffer_instanceCountLocation, (int)nbCubeInstance);
        glUniform1f(gbuffer_amplitudeLocation, amplitude);
        glUniform1f(gbuffer_spaceBetweenCubesLocation, spaceBetweenCubes);
        
        //
        // Remplissage du buffer personnalisé
        //

        // Bind personnal buffer
        glBindFramebuffer(GL_FRAMEBUFFER, gbufferFbo);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Activer la liste des buffers dans lesquels dessiner
        glDrawBuffers(2, gbufferDrawBuffers);
        
        // Rendu : Bind textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        // Rendu : Dessin de la scène directement dans le personnal buffer bindé
        glBindVertexArray(vao[0]);
        
        // Call glDrawElementsInstanced for each "bar"
        if(spectrumStepLoop < 3) { spectrumStepLoop++; }
        else { spectrumStepLoop = 0; }
        soundManager.getSpectrum(idSound1, spectrum, SPECTRUM_SIZE);
        for(size_t i = 0; i < SPECTRUM_SIZE; i++)
        {
            int barHeight = 1 + (int)(spectrum[i] * 1000);
            glUniform1i(gbuffer_spectrumOffsetLocation, i);
            glDrawElementsInstanced(GL_TRIANGLES, cube_triangleCount * 3, GL_UNSIGNED_INT, (void*)0, barHeight);    
        }
        
        //glBindVertexArray(vao[1]);
        //glDrawElements(GL_TRIANGLES, plane_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

         // Debind personnal buffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        //
        // Rendu de la scène avec plusieurs lumières
        //

        glUseProgram(lighting_shader.program);

        glUniformMatrix4fv(lighting_projectionLocation, 1, 0, glm::value_ptr(projection));
        glUniform1i(lighting_materialLocation, 0);
        glUniform1i(lighting_normalLocation, 1);
        glUniform1i(lighting_depthLocation, 2);
        glUniform3fv(lighting_cameraPositionLocation, 1, glm::value_ptr(camera.eye));
        glUniformMatrix4fv(lighting_inverseViewProjectionLocation, 1, 0, glm::value_ptr(screenToWorld));
        glUniform1f(lighting_timeLocation, t);

        // On bin les trois textures du framebuffer pour les utiliser dans le shader
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);        
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);    
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);

        // On dessine un quad pour chaque lumière

        glViewport( 0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glBindVertexArray(vao[2]);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        for (unsigned int i = 0; i < numPointLights; ++i)
        {
            glm::vec3 pos = pointLightsPositions[i];
            glm::vec3 diff = pointLightsDiffuseColors[i];
            glm::vec3 spec = pointLightsSpecularColors[i];
            float lightPosition[3] = {pos.x, pos.y, pos.z};
            float lightDiffuseColor[3] = {diff.r, diff.g, diff.b};
            float lightSpecColor[3] = {spec.r, spec.g, spec.b};
            glUniform3fv(lighting_lightPositionLocation, 1, lightPosition);
            glUniform3fv(lighting_lightDiffuseColorLocation, 1, lightDiffuseColor);
            glUniform3fv(lighting_lightSpecularColorLocation, 1, lightSpecColor);
            glUniform1f(lighting_lightIntensityLocation, pointLightsIntensities[i]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);
        }

        glDisable(GL_BLEND);

        //
        // Dessin des textures remplies du FrameBuffer
        //

        if(showDeferrefTextures)
        {

           glDisable(GL_DEPTH_TEST);

            glUseProgram(blit_shader.program);        
            glUniform1i(blit_tex1Location, 0); // Le shader utilisera la texture bindée sur l'unité de texture 0
            glActiveTexture(GL_TEXTURE0); // On active le bonne unité de texture. Les futurs bind se feront dessus. Donc le shader prendra la texture actuellement bindée.

            // Quad 1/3
            glViewport(0, 0, width/3, height/4);
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[0]);
            glBindVertexArray(vao[2]); // Pour dessiner le quad
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

            // Quad 2/3
            glViewport(width/3, 0, width/3, height/4);
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[1]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

            // Quad 3/3
            glViewport(2*width/3, 0, width/3, height/4);
            glBindTexture(GL_TEXTURE_2D, gbufferTextures[2]);
            glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

        }

#if 1

        if(showUI)
        {

            // Draw UI
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glViewport(0, 0, width, height);

            unsigned char mbut = 0;
            int mscroll = 0;
            int mousex; int mousey;
            glfwGetMousePos(&mousex, &mousey);
            mousey = height - mousey;

            if( leftButton == GLFW_PRESS )
                mbut |= IMGUI_MBUT_LEFT;

            imguiBeginFrame(mousex, mousey, mbut, mscroll);
            int logScroll = 0;
            char lineBuffer[512];
            imguiBeginScrollArea("Deferred Shading", 0, height/4, 200, 3*height/4, &logScroll);
            sprintf(lineBuffer, "FPS %f", fps);
            imguiLabel(lineBuffer);

            imguiSeparatorLine();

            imguiSlider("Speed", &timeStep, 0.0, 0.8, 0.01);
            imguiSlider("Nb cubes", &nbCubeInstance, 1, 10000, 1);
            int but33 = imguiButton("3x3");
            int but55 = imguiButton("5x5");
            int but77 = imguiButton("7x7");
            int but99 = imguiButton("9x9");
            if(but33) { nbCubeInstance = 9; }
            if(but55) { nbCubeInstance = 25; }
            if(but77) { nbCubeInstance = 49; }
            if(but99) { nbCubeInstance = 81; }
            imguiSlider("Amplitude", &amplitude, 0, 3, 0.1);
            imguiSlider("Space Between Cubes", &spaceBetweenCubes, 0, 1, 0.01);

            imguiSeparatorLine();
                int dfs = imguiButton("Show Deferred Shading");
                if(dfs) { showDeferrefTextures = !showDeferrefTextures; }
            imguiSeparatorLine();

            sprintf(lineBuffer, "%1.0f PointLights", numPointLights);
            imguiLabel(lineBuffer);
            int button_addPL = imguiButton("Add PointLight");
            if(button_addPL)
            {
                pointLightsPositions.push_back(glm::vec3(((numPointLights/8)*10+10)*cos(numPointLights*M_PI/4), 2.25, ((numPointLights/8)*10+10)*sin(numPointLights*M_PI/4)));
                pointLightsDiffuseColors.push_back(glm::vec3(cos(numPointLights/10), sin(numPointLights/10), 0/*1- cos(numPointLights/10)*/));
                pointLightsSpecularColors.push_back(glm::vec3(1, 1, 1));
                pointLightsIntensities.push_back(1.f);
                pointLightCollapses.push_back(false);
                numPointLights++;
            }

            for (unsigned int i = 0; i < numPointLights; ++i)
            {
                std::ostringstream ss;
                ss << (i+1);
                std::string s(ss.str());
                int toggle = imguiCollapse("Point Light", s.c_str(), pointLightCollapses[i]);
                if(pointLightCollapses[i])
                {
                    imguiIndent();
                        imguiSlider("Intensity", &pointLightsIntensities[i], 0, 10, 0.1);
                        imguiLabel("Position :");
                        imguiIndent();
                            imguiSlider("x", &pointLightsPositions[i].x, -10, 10, 0.01);
                            imguiSlider("y", &pointLightsPositions[i].y, -10, 10, 0.01);
                            imguiSlider("z", &pointLightsPositions[i].z, -10, 10, 0.01);
                        imguiUnindent();
                        imguiLabel("Diffuse :");
                        imguiIndent();
                            imguiSlider("r", &pointLightsDiffuseColors[i].x, 0, 1, 0.01);
                            imguiSlider("g", &pointLightsDiffuseColors[i].y, 0, 1, 0.01);
                            imguiSlider("b", &pointLightsDiffuseColors[i].z, 0, 1, 0.01);
                        imguiUnindent();
                        imguiLabel("Specular :");
                        imguiIndent();
                            imguiSlider("r", &pointLightsSpecularColors[i].x, 0, 1, 0.01);
                            imguiSlider("g", &pointLightsSpecularColors[i].y, 0, 1, 0.01);
                            imguiSlider("b", &pointLightsSpecularColors[i].z, 0, 1, 0.01);
                        imguiUnindent();
                        int removeLight = imguiButton("Remove");
                        if(removeLight)
                        {
                            pointLightsPositions.erase(pointLightsPositions.begin() + i);
                            pointLightsDiffuseColors.erase(pointLightsDiffuseColors.begin() + i);
                            pointLightsSpecularColors.erase(pointLightsSpecularColors.begin() + i);
                            pointLightsIntensities.erase(pointLightsIntensities.begin() + i);
                            pointLightCollapses.erase(pointLightCollapses.begin() + i);
                            numPointLights--;
                        }
                    imguiUnindent();
                }
                if(toggle) { pointLightCollapses[i] = !pointLightCollapses[i]; }
            }

            imguiSeparatorLine();


            imguiEndScrollArea();
            imguiEndFrame();
            imguiRenderGLDraw(width, height); 

            glDisable(GL_BLEND);
        }
        
#endif
        
        // Check for errors
        GLenum err = glGetError();
        if(err != GL_NO_ERROR)
        {
            fprintf(stderr, "OpenGL Error : %s\n", gluErrorString(err));
            
        }

        // Swap buffers
        glfwSwapBuffers();
        double newTime = glfwGetTime();
        fps = 1.f/ (newTime - t);
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey( GLFW_KEY_ESC ) != GLFW_PRESS &&
           glfwGetWindowParam( GLFW_OPENED ) );

    // Clean UI
    imguiRenderGLDestroy();

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    exit( EXIT_SUCCESS );
}