#ifndef _KERNEL_GL_TOOL_H_
#define _KERNEL_GL_TOOL_H_
#ifdef __cplusplus
extern "C" {
#endif
#if( defined  _WIN32 )
#include "include/GLEW/glew.h"
	#include "include/GLFW/glfw3.h"
	#pragma comment (lib, "opengl32.lib")     /* link Microsoft OpenGL lib   */
	#pragma comment( lib, "glew32.lib" )
	#pragma comment( lib, "glfw3.lib" )
	#define GLFW_INCLUDE_NONE
	#define GLFW_DLL
#else
#include "GL/gl_format.h"
#endif

// OpenGL-objects are NOT objects in an OOP-sense !
/* @brief OpenGL Object, Program Object */
int KERGL_ProgramObject( GLuint *programObject_ou,  const char * vertexName, const char * fragmentName );
/* @brief OpenGL Object, Texture Object */
void KERGL_TextureObject( GLuint *textureObject_ou );
/* @brief OpenGL Load, Load Texture*/
void KERGL_LoadTexture_RGB( GLuint textureObject_ou,int level, unsigned char *ptr,  int width, int height );
#if( defined  _WIN32 )
void KERGL_LoadTexture_BGR( GLuint textureObject_ou,int level, unsigned char *ptr,  int width, int height );
#endif
/* @brief */
void KERGL_ListAttributes( GLuint programObject );
/* @brief */
void KERGL_ListUniforms( GLuint program);
//=====================================================================
typedef struct {
    int size_w;
    int size_h;
    int stride;
    unsigned int handle_fb;
    unsigned int handle_tx;
    int status;
    int mipmap;
    unsigned char *ptr;
}KERGL_Canvas;

int KERGL_Canvas_Create(KERGL_Canvas *can, int w, int h);
//========================================================================
int KERGL_TOOL();
void Tool_Func_Copy(KERGL_Canvas * dst_Canvas, unsigned int tex);

//========================================================================

#ifdef __cplusplus
}
#endif
#endif
