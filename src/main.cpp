#include <unistd.h>
#include <iostream>
#include <math.h>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GLFW/glfw3.h>

#include "globals.h"
#include "file.h"
#include "vao.h"
#include "shader.h"


#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define POINTS_PER_TRIANGLE 500
#define POINTS_PER_SPHERA 2000

#define CAMERA_RPP 2*M_PI/1000.0 //resolution 1000px = 2PI

#define SGN(x)   (((x) < 0) ? (-1) : (1))
#define DEG_TO_RAD(x) (x * (M_PI / 180.0))
#define LESS_THAN(x, limit) ((x > limit) ? (limit) : (x))
#define GREATER_THAN(x, limit) ((x < limit) ? (limit) : (x))


using namespace std;


/**
 @brief Returns a title for the window
 Concatenate 'CUBE' with the description of the shader thats its been used
 @returns title
 */
const char* getTitleWindow()
{
    string multipass;
    if (MultipassEnabled)
        multipass = "MultiPass";
    else
        multipass = "SinglePass";
    title = "CUBE | " + listOfShaders[actualShader%listOfShaders.size()].getDescription() + " | " + multipass;
    return title.c_str();
}


/**
 @brief Gets a random point inside a triangle defined by its vertices
 http://parametricplayground.blogspot.com.es/2011/02/random-points-distributed-inside.html
 @param v1, v2, v3 vertices of an triangle
 @returns point
 */
glm::vec3 pickPoint(glm::vec3 v1, glm::vec3 v2, glm::vec3 v3)
{
    glm::vec3 point;
    
    double c, a, b;
    
    a = rand() / double(RAND_MAX);
    b = rand() / double(RAND_MAX);
    
    if (a + b > 1)
    {
        a = 1.0f - a;
        b = 1.0f - b;
    }
    c = 1.0f - a - b;
    
    point.x = (a * v1.x) + (b * v2.x) + (c * v3.x);
    point.y = (a * v1.y) + (b * v2.y) + (c * v3.y);
    point.z = (a * v1.z) + (b * v2.z) + (c * v3.z);
    
    return point;
}


/**
 Sample a model transforming it into a point cloud
 @param mesh model defined by GL_TRIANGLES
 @returns point cloud
 */
struct vao sampleMesh(struct vao *mesh)
{
    //Init
    struct vao sampledMesh;
    sampledMesh.numOfTriangles = 0;
    sampledMesh.numOfVertices = 0;
    
    int line;
    for (int i = 0; i < mesh->numOfTriangles; i++) {
        for (int j = 0; j < POINTS_PER_TRIANGLE; j++) {
            line = i*3;
            sampledMesh.vertices.push_back(pickPoint(mesh->vertices[line], mesh->vertices[line+1], mesh->vertices[line+2]));
            sampledMesh.colors.push_back(mesh->colors[line]);
            sampledMesh.normals.push_back(mesh->normals[i]);
            sampledMesh.numOfVertices++;
        }
    }
    
    sampledMesh.mode = GL_POINTS;
    
    return sampledMesh;
}


struct vao sampleSphere()
{
    double x, y, z;
    
    struct vao sampledSphere;
    sampledSphere.numOfTriangles = 0;
    sampledSphere.numOfVertices = 0;
    
    for (int i = 0; i < POINTS_PER_SPHERA; i++) {
        //x^2 + y^2 + z^2 = 1
        
        x = rand() / double(RAND_MAX) * 2 - 1;
        y = rand() / double(RAND_MAX) * 2 - 1;
        
        while (sqrt(pow(x, 2) + pow(y, 2)) > 1.0)
            y = rand() / double(RAND_MAX) * 2 - 1;
            
        z = sqrt(1 - pow(x, 2) - pow(y, 2));
    
        //+z
        sampledSphere.vertices.push_back(glm::vec3(x,y,z));
        sampledSphere.normals.push_back(glm::normalize(glm::vec3(x,y,z)));
        sampledSphere.colors.push_back(glm::vec3(0,0,1));
        
        //-z
        sampledSphere.vertices.push_back(glm::vec3(x,y,-z));
        sampledSphere.normals.push_back(glm::normalize(glm::vec3(x,y,-z)));
        sampledSphere.colors.push_back(glm::vec3(1,0,0));
        
        sampledSphere.numOfVertices = sampledSphere.numOfVertices + 2;
        
    }
    
    return sampledSphere;
}


void updateProjMatrix(GLFWwindow * window)
{
    float ratio;
    float fovy = 53.13f;
    float near = 0.1f;
    float far = 100.0f;
    
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    
    if (h == 0)
        h = 1;
    
    ratio = (1.0f * w) / h;
    projMatrix = glm::perspective(fovy, ratio, near, far);
    
    GLfloat top = (GLfloat) tan( 0.5f * DEG_TO_RAD(fovy)) * near;
    glUniformMatrix4fv(Shader::projMatrixLoc,  1, false, glm::value_ptr(projMatrix));
    glUniform1i(Shader::hViewportLoc, (GLint) h);
    glUniform1i(Shader::wViewportLoc, (GLint) w);
    glUniform1f(Shader::nearFrustumLoc, (GLfloat) near);
    glUniform1f(Shader::farFrustumLoc, (GLfloat) far);
    glUniform1f(Shader::topFrustumLoc, (GLfloat) -top );
    glUniform1f(Shader::bottomFrustumLoc, (GLfloat) top );
    glUniform1f(Shader::leftFrustumLoc, (GLfloat) -top * ratio);
    glUniform1f(Shader::rightFrustumLoc, (GLfloat) top * ratio);

}


void reshapeCallback(GLFWwindow * window, int w, int h)
{

    // set viewport to be the entire window
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    
    updateProjMatrix(window);
}



/**
 Reset camera position to origin.
 @returns void
 */
void updateCameraPosition()
{
    // Calculate the camera position using the distance and angles
    cameraEye.x = cameraDistance * -sinf(cameraAngleX) * cosf(cameraAngleY);
    cameraEye.y = cameraDistance * -sinf(cameraAngleY);
    cameraEye.z = -cameraDistance * cosf(cameraAngleX) * cosf(cameraAngleY);
}



void resetCameraPosition()
{
    cameraAngleX = 0;
    cameraAngleY = 0;
    updateCameraPosition();
}



void scrollCallback(GLFWwindow * window, double xoffset, double yoffset)
{
    cameraDistance -= yoffset;
    cameraDistance = GREATER_THAN(cameraDistance, 0.001f);
    
    updateCameraPosition();
}



void mousePosCallback(GLFWwindow * window, double x, double y)
{
    if (leftBtnPress == true) {
        cameraAngleX += (lastMouseX - x) * CAMERA_RPP;
        cameraAngleY += (lastMouseY - y) * CAMERA_RPP;
        
        //-PI/2 < cameraAngleY < PI/2
        cameraAngleY = LESS_THAN(cameraAngleY, M_PI/2.0f);
        cameraAngleY = GREATER_THAN(cameraAngleY, -M_PI/2.0f);
        
        updateCameraPosition();
    }
    
    lastMouseX = x;
    lastMouseY = y;

}



void mouseCallback(GLFWwindow * window, int btn, int action, int mods)
{
	if(btn == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_RELEASE)
            leftBtnPress = false;
        else if (action == GLFW_PRESS)
            leftBtnPress = true;
    }
}



void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    
    if (key == GLFW_KEY_UP) {
        radiusSplat += 0.001f;
        glUniform1f(Shader::radiusSplatLoc, radiusSplat);
    }
    
    if (key == GLFW_KEY_DOWN) {
        radiusSplat -= 0.001f;
        glUniform1f(Shader::radiusSplatLoc, radiusSplat);
    }
    
    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        resetCameraPosition();
    }
    
    if (key == GLFW_KEY_S && action == GLFW_PRESS) {
        actualShader++;
        listOfShaders[actualShader%listOfShaders.size()].compileShader();
        Shader::bindShader();
        updateProjMatrix(window);
        glfwSetWindowTitle(window, getTitleWindow());
    }
    
    if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        actualVAO++;
        displayVAO = &models[actualVAO%models.size()];
    }
    
    if (key == GLFW_KEY_O && action == GLFW_PRESS) {
        string pathToFile;
        cout << "Open File: ";
        cin >> pathToFile;
        struct vao VAO = loadCloud(pathToFile);
        if (VAO.numOfVertices != 0 || VAO.numOfTriangles != 0) {
            models.push_back(VAO);
            actualVAO = models.size() - 1;
            loadVAO(&models[models.size()-1]);
            displayVAO = &models[models.size()-1];
        }
    }
    
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        MultipassEnabled = !MultipassEnabled;
        
        if (!MultipassEnabled) {
            listOfShaders[actualShader%listOfShaders.size()].compileShader();
            Shader::bindShader();
            updateProjMatrix(window);
        }
        
        glfwSetWindowTitle(window, getTitleWindow());
    }
    
}



void display(GLFWwindow* window)
{

	glClearColor(86.f/255.f,136.f/255.f,199.f/255.f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    viewMatrix = glm::lookAt(cameraEye,
                             glm::vec3(0,0,0),
                             glm::cross(glm::cross(cameraEye, cameraUp), cameraEye));
    
    normalMatrix = glm::inverseTranspose(glm::mat3(viewMatrix));
    
    glBindVertexArray(displayVAO->vaoID);
    
    if (displayVAO != NULL) {
        
        if (!MultipassEnabled) {
            glUniformMatrix4fv(Shader::viewMatrixLoc,  1, false, glm::value_ptr(viewMatrix));
            glUniformMatrix3fv(Shader::normalMatrixLoc, 1, false, glm::value_ptr(normalMatrix));
            
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDrawArrays(displayVAO->mode, 0, displayVAO->numOfVertices);
        }
        else {
            //glDisable(GL_DEPTH_TEST);
            Shader shaderh = listOfShaders[actualShader%listOfShaders.size()];
            
            for (int i = 0; i < shaderh.getMultiPass().size(); i++) {
                shaderh.getMultiPass()[i].compileShader();
                Shader::bindShader();
                updateProjMatrix(window);
                
                glUniformMatrix4fv(Shader::viewMatrixLoc,  1, false, glm::value_ptr(viewMatrix));
                glUniformMatrix3fv(Shader::normalMatrixLoc, 1, false, glm::value_ptr(normalMatrix));
                
                switch (shaderh.getMultiPass()[i].getMode()) {
                    case shader::DEPTH_MASK:
                    {
                        glDepthMask(GL_TRUE);
                        glDrawArrays(displayVAO->mode, 0, displayVAO->numOfVertices);
                        break;
                    }
                    case shader::BLENDING:
                    {
                        glEnable(GL_BLEND);
                        glDepthMask(GL_FALSE);
                        glDepthFunc(GL_LEQUAL);
                        glDrawArrays(displayVAO->mode, 0, displayVAO->numOfVertices);
                        break;
                    }
                    case shader::NORMALIZATION:
                    {
//                        GLfloat vVertices[] = {0.0f, 0.5f, 0.0f,
//                            -0.5f, -0.5f, 0.0f,
//                            0.5f, -0.5f, 0.0f};
//                    
//                        // Load the vertex data
//                        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
//                        glEnableVertexAttribArray(0);
//                        glDrawArrays(GL_TRIANGLES, 0, 3);
                        break;
                    }
                    default:
                        break;
                }
                
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
                
            }
            
        }
    }
    
    glBindVertexArray(0);

    
}

int main(int argc, char **argv)
{
    GLFWwindow* window;
    
    /* Initialize the library */
    if (!glfwInit())
        return -1;
    
    /* create context */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "CUBE", NULL, NULL);
    glfwSetWindowTitle(window, getTitleWindow());
    
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    //Glew Init
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        cout << "glewInit failed, aborting." << endl;
        exit (1);
    }

    /*openGL configure*/
    glEnable(GL_PROGRAM_POINT_SIZE);
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT);
    glBlendFuncSeparateEXT(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
    
    cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;
    cout << "GLSL version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
    cout << "GLEW version: " << glewGetString(GLEW_VERSION) << endl;
    
    //init all models
    models.push_back(sampleMesh(&cubeMesh));
    models.push_back(sampleSphere());
    for (int i = 0; i < models.size(); i++) {
        loadVAO(&models[i]);
        
    }
    
    displayVAO = &models[0];
    
    listOfShaders[actualShader].compileShader();
    Shader::bindShader();
    updateProjMatrix(window);
    
    /*glfw Callbacks*/
    glfwSetKeyCallback(window, keyboardCallback);
    glfwSetWindowSizeCallback(window, reshapeCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseCallback);
    glfwSetCursorPosCallback(window, mousePosCallback);
    reshapeCallback(window, WINDOW_WIDTH, WINDOW_HEIGHT); //callback forced
    
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        display(window);
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        
        /* Poll for and process events */
        glfwPollEvents();
    }
    
    glfwTerminate();
    return 0;

}
