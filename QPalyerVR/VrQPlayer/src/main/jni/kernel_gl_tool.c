#include "kernel_gl_tool.h"

#if( defined  _WIN32 )
#include <stdio.h>      /* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <math.h>
#else
#include <GLES3/gl3.h>

#include <stdio.h>      /* printf, scanf, NULL */
#include <stdlib.h>     /* malloc, free, rand */
#include <string.h>      /* printf, scanf, NULL */
#include <math.h>
#include "GL/gl_format.h"
#include "Render/Egl.h"
#endif

static unsigned int glprog_test_c = 0;
static unsigned int glprog_src_attr = 0;

static const char g_fshader_src_attr4[] =
        "precision highp float;"
        "uniform sampler2D s_texture;"
        "varying vec2 v_texcoord;"
        "void main(){"
        "    gl_FragColor = texture2D( s_texture, v_texcoord );"
        "}";


static const char g_vshader_attr3[] =
        "attribute vec3 vPosition;"
        "attribute vec2 vTeximage;"
        "varying vec2 v_texcoord;"
        "void main(){"
        "	 gl_Position = vec4( vPosition.x, vPosition.y, 1. , 1. );"
        "    v_texcoord = vTeximage;"
        "}";



#define ANGDSP (0.125)


////////////////////////////////////////////////////////////////////////////////////////////////
static GLuint PV_LoadShaderGL(GLenum type, const char *shaderSrc);
// OpenGL-objects are NOT objects in an OOP-sense !
/* @brief OpenGL Object, Program Object */
int KERGL_ProgramObject(GLuint *programObject_ou, const char * vertexName, const char * fragmentName)
{
    GLuint vertexShader, fragmentShader, programObject;
    GLint linked;
    // Load the vertex/ fragment shaders
    vertexShader = PV_LoadShaderGL(GL_VERTEX_SHADER, vertexName);
    fragmentShader = PV_LoadShaderGL(GL_FRAGMENT_SHADER, fragmentName);
    // Create the program object
    programObject = glCreateProgram();
    if (programObject == 0) return GL_FALSE;
    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    // Link the program
    glLinkProgram(programObject);
    // Check the link status
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint infoLen = 0;
        glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char *)malloc(infoLen * sizeof(char));
            glGetProgramInfoLog(programObject, infoLen, NULL, infoLog);

            printf("Error linking program:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteProgram(programObject);
        return GL_FALSE;
    }
    *programObject_ou = programObject;
    return GL_TRUE;
}

//----------------------------------------------------------------------------------------------
/* @brief OpenGL Load, Load Shader*/
static GLuint PV_LoadShaderGL(GLenum type, const char *shaderSrc)
{
    GLuint shader;
    GLint compiled;
    // Create the shader object
    shader = glCreateShader(type);
    if (shader == 0) return 0;
    // Load the shader source
    glShaderSource(shader, 1, &shaderSrc, NULL);
    // Compile the shader
    glCompileShader(shader);
    // Check the compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1)
        {
            char* infoLog = (char *)malloc(infoLen * sizeof(char));
            glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
            printf("Error Compiling Shader:\n%s\n", infoLog);
            free(infoLog);
        }
        glDeleteShader(shader);
        return GL_FALSE;
    }
    return shader;
}

/* @brief List the attribute variables of the openGL Shader program */
void KERGL_ListAttributes(GLuint program)
{
    GLint i, j;
    GLint count;
    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)
    const GLsizei bufSize = 64; // maximum name length
    GLchar name[64]; // variable name in GLSL
    GLsizei length; // name length
    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &count);
    printf("Active Attributes: %d\n", count);
    for (i = 0; i < count; i++)
    {
        glGetActiveAttrib(program, (GLuint)i, bufSize, &length, &size, &type, name);
        j = glGetAttribLocation(program, name);
        printf("Attribute #%d Type: %u Name: %s\n", j, type, name);
    }
}
/* @brief List the uniform variables of the openGL Shader program */
void KERGL_ListUniforms(GLuint program)
{
    GLint i, j;
    GLint count;
    GLint size; // size of the variable
    GLenum type; // type of the variable (float, vec3 or mat4, etc)
    const GLsizei bufSize = 64; // maximum name length
    GLchar name[64]; // variable name in GLSL
    GLsizei length; // name length
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &count);
    printf("Active Uniforms: %d\n", count);
    for (i = 0; i < count; i++)
    {
        glGetActiveUniform(program, (GLuint)i, bufSize, &length, &size, &type, name);
        j = glGetUniformLocation(program, name);
        printf("Uniform #%d Type: %u Name: %s\n", j, type, name);
    }
}
//--------------------------------------------------------------------------------------------------
int KERGL_Canvas_Create(KERGL_Canvas *can, int w, int h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, can->handle_fb);
    int succ = glCheckFramebufferStatus(GL_FRAMEBUFFER);

   // if (can->size_w != w || can->size_h != h ||succ!=GL_FRAMEBUFFER_COMPLETE)
    if( !glIsTexture( can->handle_tx ))
    {
       glDeleteTextures( 1, &can->handle_tx );

        can->size_w = w;
        can->size_h = h;
        glGenFramebuffers(1, &can->handle_fb);
        glGenTextures(1, &can->handle_tx);
        glBindTexture(GL_TEXTURE_2D, can->handle_tx);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, can->size_w, can->size_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, can->size_w, can->size_h, 0, GL_RGBA, GL_FLOAT, NULL);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        // bind the frame buffer
        glBindFramebuffer(GL_FRAMEBUFFER, can->handle_fb);
        // specify texture as color attachment
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, can->handle_tx, 0);
        // check for framebuffer complete
        // int succ = glCheckFramebufferStatus(GL_FRAMEBUFFER);
       // if (can->ptr != 0)  free(can->ptr);


        //can->ptr = malloc(4 * can->size_w * can->size_h);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, can->handle_fb);
     succ = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    assert ( succ ==GL_FRAMEBUFFER_COMPLETE);
    return succ;
}


//--------------------------------------------------------------------------------------------------
static const char g_fshader_src_test_c[] =
#if( defined  _WIN32 )
        "precision mediump float;"
        "uniform sampler2D s_texture;"
#else
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float;"
        "uniform samplerExternalOES s_texture;"
#endif
        "varying vec3 oTexCoord;"
        "void main(){"
        "    gl_FragColor = texture2D( s_texture, vec2( oTexCoord.x, oTexCoord.y) );"
        "}";


//==================================================================================================
//
//==================================================================================================
static unsigned int PV_Draw_OESCopy(unsigned int handle_texture)
{
    unsigned int handle_prog = glprog_test_c;

    const float triangle_vertices[] =
            {
                    -1, -1, 0.0f,
                    -1, 1, 0.0f,
                    1, 1, 0.0f,
                    1, -1, 0.0f
            };

    const float b = 0.00;
    const float gSgxRender1x1_tex_coords[] = {
            0.0f + b, 0.0f + b, 0.0f,
            0.0f + b, 1.0f - b, 0.0f,
            1.0f - b, 1.0f - b, 0.0f,
            1.0f - b, 0.0f + b, 0.0f
    };
    glUseProgram(handle_prog);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(glGetAttribLocation(handle_prog, "vPosition"));
    glEnableVertexAttribArray(glGetAttribLocation(handle_prog, "vTeximage"));

    glVertexAttribPointer(glGetAttribLocation(handle_prog, "vPosition"), 3, GL_FLOAT, GL_FALSE, 0, (const void*)triangle_vertices);
    glVertexAttribPointer(glGetAttribLocation(handle_prog, "vTeximage"), 3, GL_FLOAT, GL_FALSE, 0, (const void*)gSgxRender1x1_tex_coords);
    glUniform1i(glGetUniformLocation(handle_prog, "s_texture"), 0);

    //
    glActiveTexture(GL_TEXTURE0);

    glBindTexture(GL_TEXTURE_EXTERNAL_OES, handle_texture);
    //

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    return 1;
}

void Tool_Func_Copy(KERGL_Canvas * dst_Canvas, unsigned int tex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, dst_Canvas->handle_fb);
    glViewport(0, 0, dst_Canvas->size_w, dst_Canvas->size_h);
    PV_Draw_OESCopy(tex );
}


static const char g_vshader_test[] =
        "attribute vec3 vPosition;"
        "attribute vec3 vTeximage;"
        "varying vec3 oTexCoord;"
        "void main(){"
        "	 gl_Position = vec4( vPosition.x, vPosition.y, 1. , 1. );"
        "    oTexCoord = vTeximage;"
        "}";

int KERGL_TOOL()
{


    int succ = 1;
    succ = KERGL_ProgramObject(&glprog_test_c, g_vshader_test, g_fshader_src_test_c);
    if (!succ)  return 0;
    return succ;
}