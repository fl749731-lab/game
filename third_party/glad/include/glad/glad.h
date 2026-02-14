#ifndef __glad_h_
#define __glad_h_

/*
 * GLAD - OpenGL Loader (手写精简版)
 * 
 * 重要: 必须在 GLFW/glfw3.h 之前包含此头文件!
 * 此头文件会阻止系统 GL/gl.h 被引入，从而避免符号冲突。
 */

/* 阻止系统 GL 头文件 */
#define __gl_h_
#define __GL_H__
#define _GL_H
#define __gltypes_h_
#define __REGAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* ── 平台定义 ────────────────────────────────────────────── */
#ifdef _WIN32
    #define APIENTRY __stdcall
    #define WINGDIAPI __declspec(dllimport)
#else
    #define APIENTRY
#endif

/* ── OpenGL 类型定义 ─────────────────────────────────────── */
#include <stddef.h>
#include <stdint.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef int8_t GLbyte;
typedef uint8_t GLubyte;
typedef int16_t GLshort;
typedef uint16_t GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLclampx;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef struct __GLsync *GLsync;

/* ── OpenGL 常量 ─────────────────────────────────────────── */
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_NONE                           0

/* Data types */
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_DOUBLE                         0x140A

/* Primitives */
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

/* Clear buffer bits */
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

/* Enable/Disable */
#define GL_DEPTH_TEST                     0x0B71
#define GL_BLEND                          0x0BE2
#define GL_CULL_FACE                      0x0B44
#define GL_SCISSOR_TEST                   0x0C11
#define GL_STENCIL_TEST                   0x0B90
#define GL_MULTISAMPLE                    0x809D

/* Depth function */
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

/* Face culling */
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408
#define GL_CW                             0x0900
#define GL_CCW                            0x0901

/* Polygon mode */
#define GL_POINT                          0x1B00
#define GL_LINE                           0x1B01
#define GL_FILL                           0x1B02

/* Blend factors */
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305

/* Errors */
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505

/* GetString */
#define GL_VENDOR                         0x1F00
#define GL_RENDERER                       0x1F01
#define GL_VERSION                        0x1F02
#define GL_SHADING_LANGUAGE_VERSION       0x8B8C

/* Shader types */
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_GEOMETRY_SHADER                0x8DD9

/* Shader status */
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_ACTIVE_UNIFORMS                0x8B86

/* Buffer objects */
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8

/* Textures */
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_LINEAR                         0x2601
#define GL_NEAREST                        0x2600
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_RED                            0x1903
#define GL_RG                             0x8227
#define GL_R8                             0x8229
#define GL_RGB8                           0x8051
#define GL_RGBA8                          0x8058
#define GL_DEPTH_COMPONENT                0x1902
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32F             0x8CAC

/* HDR / Float texture formats */
#define GL_R16F                           0x822D
#define GL_RG16F                          0x822F
#define GL_RGB16F                         0x881B
#define GL_RGBA16F                        0x881A
#define GL_R32F                           0x822E
#define GL_RGB32F                         0x8815
#define GL_RGBA32F                        0x8814
#define GL_COLOR_ATTACHMENT1              0x8CE1
#define GL_CLAMP_TO_BORDER                0x812D
#define GL_TEXTURE_BORDER_COLOR           0x1004

/* Cubemap */
#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X    0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X    0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y    0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y    0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z    0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z    0x851A
#define GL_TEXTURE_WRAP_R                 0x8072
#define GL_TEXTURE_MAX_LEVEL              0x813D

/* Framebuffer */
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5

/* Viewport */
#define GL_VIEWPORT                       0x0BA2
#define GL_MAX_TEXTURE_SIZE               0x0D33

/* Debug */
#define GL_DEBUG_OUTPUT                    0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B

/* Debug callback type */
typedef void (APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length, const GLchar *message,
    const void *userParam);

/* ── OpenGL 函数声明 (全部通过函数指针) ───────────────────── */

/* 所有 GL 函数统一用函数指针，包括 GL 1.1 的。
 * 我们通过 #define 阻止了系统 gl.h，所以不会冲突。 */

/* Core state (GL 1.0/1.1) */
typedef void   (APIENTRY *PFNGLVIEWPORTPROC)(GLint, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *PFNGLCLEARPROC)(GLbitfield);
typedef void   (APIENTRY *PFNGLCLEARCOLORPROC)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFNGLENABLEPROC)(GLenum);
typedef void   (APIENTRY *PFNGLDISABLEPROC)(GLenum);
typedef void   (APIENTRY *PFNGLBLENDFUNCPROC)(GLenum, GLenum);
typedef GLenum (APIENTRY *PFNGLGETERRORPROC)(void);
typedef const GLubyte* (APIENTRY *PFNGLGETSTRINGPROC)(GLenum);
typedef void   (APIENTRY *PFNGLGETINTEGERVPROC)(GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETFLOATVPROC)(GLenum, GLfloat*);
typedef void   (APIENTRY *PFNGLDEPTHFUNCPROC)(GLenum);
typedef void   (APIENTRY *PFNGLDEPTHMASKPROC)(GLboolean);
typedef void   (APIENTRY *PFNGLCULLFACEPROC)(GLenum);
typedef void   (APIENTRY *PFNGLFRONTFACEPROC)(GLenum);
typedef void   (APIENTRY *PFNGLPOLYGONMODEPROC)(GLenum, GLenum);
typedef void   (APIENTRY *PFNGLLINEWIDTHPROC)(GLfloat);
typedef void   (APIENTRY *PFNGLDRAWARRAYSPROC)(GLenum, GLint, GLsizei);
typedef void   (APIENTRY *PFNGLDRAWELEMENTSPROC)(GLenum, GLsizei, GLenum, const void*);
typedef void   (APIENTRY *PFNGLBINDTEXTUREPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLDELETETEXTURESPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLGENTEXTURESPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLTEXIMAGE2DPROC)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
typedef void   (APIENTRY *PFNGLTEXPARAMETERIPROC)(GLenum, GLenum, GLint);
typedef void   (APIENTRY *PFNGLTEXPARAMETERFPROC)(GLenum, GLenum, GLfloat);
typedef void   (APIENTRY *PFNGLTEXPARAMETERFVPROC)(GLenum, GLenum, const GLfloat*);

/* VAO (GL 3.0+) */
typedef void   (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);

/* VBO/IBO (GL 2.0+) */
typedef void   (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum, GLintptr, GLsizeiptr, const void*);

/* Vertex attribs (GL 2.0+) */
typedef void   (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void   (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef void   (APIENTRY *PFNGLVERTEXATTRIBDIVISORPROC)(GLuint, GLuint);

/* Instanced rendering (GL 3.1+) */
typedef void   (APIENTRY *PFNGLDRAWARRAYSINSTANCEDPROC)(GLenum, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *PFNGLDRAWELEMENTSINSTANCEDPROC)(GLenum, GLsizei, GLenum, const void*, GLsizei);

/* Shaders (GL 2.0+) */
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void   (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef void   (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const GLchar *const*, const GLint*);
typedef void   (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);

/* Programs (GL 2.0+) */
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void   (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void   (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef void   (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void   (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLint  (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const GLchar*);

/* Uniforms (GL 2.0+) */
typedef void   (APIENTRY *PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void   (APIENTRY *PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void   (APIENTRY *PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
typedef void   (APIENTRY *PFNGLUNIFORM3FPROC)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFNGLUNIFORM4FPROC)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (APIENTRY *PFNGLUNIFORMMATRIX3FVPROC)(GLint, GLsizei, GLboolean, const GLfloat*);

/* Pixel store (GL 1.0+) */
typedef void   (APIENTRY *PFNGLPIXELSTOREIPROC)(GLenum, GLint);
typedef void   (APIENTRY *PFNGLSCISSORPROC)(GLint, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);
typedef void   (APIENTRY *PFNGLBLENDEQUATIONPROC)(GLenum);

/* Textures extended (GL 1.3+) */
typedef void   (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void   (APIENTRY *PFNGLGENERATEMIPMAPROC)(GLenum);

/* Framebuffer (GL 3.0+) */
typedef void   (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum (APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum);
typedef void   (APIENTRY *PFNGLDRAWBUFFERSPROC)(GLsizei, const GLenum*);
typedef void   (APIENTRY *PFNGLTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);

/* Renderbuffer (GL 3.0+) */
typedef void   (APIENTRY *PFNGLGENRENDERBUFFERSPROC)(GLsizei, GLuint*);
typedef void   (APIENTRY *PFNGLDELETERENDERBUFFERSPROC)(GLsizei, const GLuint*);
typedef void   (APIENTRY *PFNGLBINDRENDERBUFFERPROC)(GLenum, GLuint);
typedef void   (APIENTRY *PFNGLRENDERBUFFERSTORAGEPROC)(GLenum, GLenum, GLsizei, GLsizei);
typedef void   (APIENTRY *PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum, GLenum, GLenum, GLuint);

/* Vertex attribs integer (GL 3.0+) — for bone IDs */
typedef void   (APIENTRY *PFNGLVERTEXATTRIBIPOINTERPROC)(GLuint, GLint, GLenum, GLsizei, const void*);

/* Debug (GL 4.3+) */
typedef void   (APIENTRY *PFNGLDEBUGMESSAGECALLBACKPROC)(GLDEBUGPROC, const void*);

/* ── 全局函数指针 ────────────────────────────────────────── */

extern PFNGLVIEWPORTPROC               glad_glViewport;
extern PFNGLCLEARPROC                   glad_glClear;
extern PFNGLCLEARCOLORPROC              glad_glClearColor;
extern PFNGLENABLEPROC                  glad_glEnable;
extern PFNGLDISABLEPROC                 glad_glDisable;
extern PFNGLBLENDFUNCPROC               glad_glBlendFunc;
extern PFNGLGETERRORPROC                glad_glGetError;
extern PFNGLGETSTRINGPROC               glad_glGetString;
extern PFNGLGETINTEGERVPROC             glad_glGetIntegerv;
extern PFNGLGETFLOATVPROC               glad_glGetFloatv;
extern PFNGLDEPTHFUNCPROC               glad_glDepthFunc;
extern PFNGLDEPTHMASKPROC               glad_glDepthMask;
extern PFNGLCULLFACEPROC                glad_glCullFace;
extern PFNGLFRONTFACEPROC               glad_glFrontFace;
extern PFNGLPOLYGONMODEPROC             glad_glPolygonMode;
extern PFNGLLINEWIDTHPROC               glad_glLineWidth;

extern PFNGLDRAWARRAYSPROC              glad_glDrawArrays;
extern PFNGLDRAWELEMENTSPROC            glad_glDrawElements;

extern PFNGLBINDTEXTUREPROC             glad_glBindTexture;
extern PFNGLDELETETEXTURESPROC          glad_glDeleteTextures;
extern PFNGLGENTEXTURESPROC             glad_glGenTextures;
extern PFNGLTEXIMAGE2DPROC              glad_glTexImage2D;
extern PFNGLTEXPARAMETERIPROC           glad_glTexParameteri;
extern PFNGLTEXPARAMETERFPROC           glad_glTexParameterf;
extern PFNGLTEXPARAMETERFVPROC          glad_glTexParameterfv;

extern PFNGLGENVERTEXARRAYSPROC         glad_glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC      glad_glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC         glad_glBindVertexArray;

extern PFNGLGENBUFFERSPROC              glad_glGenBuffers;
extern PFNGLDELETEBUFFERSPROC           glad_glDeleteBuffers;
extern PFNGLBINDBUFFERPROC              glad_glBindBuffer;
extern PFNGLBUFFERDATAPROC              glad_glBufferData;
extern PFNGLBUFFERSUBDATAPROC           glad_glBufferSubData;

extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glad_glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC     glad_glVertexAttribPointer;
extern PFNGLVERTEXATTRIBDIVISORPROC     glad_glVertexAttribDivisor;

extern PFNGLDRAWARRAYSINSTANCEDPROC     glad_glDrawArraysInstanced;
extern PFNGLDRAWELEMENTSINSTANCEDPROC   glad_glDrawElementsInstanced;

extern PFNGLCREATESHADERPROC            glad_glCreateShader;
extern PFNGLDELETESHADERPROC            glad_glDeleteShader;
extern PFNGLSHADERSOURCEPROC            glad_glShaderSource;
extern PFNGLCOMPILESHADERPROC           glad_glCompileShader;
extern PFNGLGETSHADERIVPROC             glad_glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC        glad_glGetShaderInfoLog;

extern PFNGLCREATEPROGRAMPROC           glad_glCreateProgram;
extern PFNGLDELETEPROGRAMPROC           glad_glDeleteProgram;
extern PFNGLATTACHSHADERPROC            glad_glAttachShader;
extern PFNGLLINKPROGRAMPROC             glad_glLinkProgram;
extern PFNGLUSEPROGRAMPROC              glad_glUseProgram;
extern PFNGLGETPROGRAMIVPROC            glad_glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC       glad_glGetProgramInfoLog;
extern PFNGLGETUNIFORMLOCATIONPROC      glad_glGetUniformLocation;

extern PFNGLUNIFORM1IPROC              glad_glUniform1i;
extern PFNGLUNIFORM1FPROC              glad_glUniform1f;
extern PFNGLUNIFORM2FPROC              glad_glUniform2f;
extern PFNGLUNIFORM3FPROC              glad_glUniform3f;
extern PFNGLUNIFORM4FPROC              glad_glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC       glad_glUniformMatrix4fv;
extern PFNGLUNIFORMMATRIX3FVPROC       glad_glUniformMatrix3fv;

extern PFNGLPIXELSTOREIPROC            glad_glPixelStorei;
extern PFNGLSCISSORPROC                glad_glScissor;
extern PFNGLBLENDFUNCSEPARATEPROC      glad_glBlendFuncSeparate;
extern PFNGLBLENDEQUATIONPROC          glad_glBlendEquation;

extern PFNGLACTIVETEXTUREPROC          glad_glActiveTexture;
extern PFNGLGENERATEMIPMAPROC          glad_glGenerateMipmap;

extern PFNGLGENFRAMEBUFFERSPROC        glad_glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC     glad_glDeleteFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC        glad_glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC   glad_glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
extern PFNGLDRAWBUFFERSPROC            glad_glDrawBuffers;
extern PFNGLTEXSUBIMAGE2DPROC          glad_glTexSubImage2D;

extern PFNGLGENRENDERBUFFERSPROC       glad_glGenRenderbuffers;
extern PFNGLDELETERENDERBUFFERSPROC    glad_glDeleteRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC       glad_glBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC    glad_glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;

extern PFNGLVERTEXATTRIBIPOINTERPROC   glad_glVertexAttribIPointer;

extern PFNGLDEBUGMESSAGECALLBACKPROC   glad_glDebugMessageCallback;

/* ── 用宏将 glXxx 映射到 glad_glXxx ─────────────────────── */

#define glViewport              glad_glViewport
#define glClear                 glad_glClear
#define glClearColor            glad_glClearColor
#define glEnable                glad_glEnable
#define glDisable               glad_glDisable
#define glBlendFunc             glad_glBlendFunc
#define glGetError              glad_glGetError
#define glGetString             glad_glGetString
#define glGetIntegerv           glad_glGetIntegerv
#define glGetFloatv             glad_glGetFloatv
#define glDepthFunc             glad_glDepthFunc
#define glDepthMask             glad_glDepthMask
#define glCullFace              glad_glCullFace
#define glFrontFace             glad_glFrontFace
#define glPolygonMode           glad_glPolygonMode
#define glLineWidth             glad_glLineWidth
#define glDrawArrays            glad_glDrawArrays
#define glDrawElements          glad_glDrawElements
#define glBindTexture           glad_glBindTexture
#define glDeleteTextures        glad_glDeleteTextures
#define glGenTextures           glad_glGenTextures
#define glTexImage2D            glad_glTexImage2D
#define glTexParameteri         glad_glTexParameteri
#define glTexParameterf         glad_glTexParameterf
#define glTexParameterfv        glad_glTexParameterfv
#define glGenVertexArrays       glad_glGenVertexArrays
#define glDeleteVertexArrays    glad_glDeleteVertexArrays
#define glBindVertexArray       glad_glBindVertexArray
#define glGenBuffers            glad_glGenBuffers
#define glDeleteBuffers         glad_glDeleteBuffers
#define glBindBuffer            glad_glBindBuffer
#define glBufferData            glad_glBufferData
#define glBufferSubData         glad_glBufferSubData
#define glEnableVertexAttribArray  glad_glEnableVertexAttribArray
#define glDisableVertexAttribArray glad_glDisableVertexAttribArray
#define glVertexAttribPointer   glad_glVertexAttribPointer
#define glVertexAttribDivisor   glad_glVertexAttribDivisor
#define glDrawArraysInstanced   glad_glDrawArraysInstanced
#define glDrawElementsInstanced glad_glDrawElementsInstanced
#define glCreateShader          glad_glCreateShader
#define glDeleteShader          glad_glDeleteShader
#define glShaderSource          glad_glShaderSource
#define glCompileShader         glad_glCompileShader
#define glGetShaderiv           glad_glGetShaderiv
#define glGetShaderInfoLog      glad_glGetShaderInfoLog
#define glCreateProgram         glad_glCreateProgram
#define glDeleteProgram         glad_glDeleteProgram
#define glAttachShader          glad_glAttachShader
#define glLinkProgram           glad_glLinkProgram
#define glUseProgram            glad_glUseProgram
#define glGetProgramiv          glad_glGetProgramiv
#define glGetProgramInfoLog     glad_glGetProgramInfoLog
#define glGetUniformLocation    glad_glGetUniformLocation
#define glUniform1i             glad_glUniform1i
#define glUniform1f             glad_glUniform1f
#define glUniform2f             glad_glUniform2f
#define glUniform3f             glad_glUniform3f
#define glUniform4f             glad_glUniform4f
#define glUniformMatrix4fv      glad_glUniformMatrix4fv
#define glUniformMatrix3fv      glad_glUniformMatrix3fv
#define glPixelStorei           glad_glPixelStorei
#define glScissor               glad_glScissor
#define glBlendFuncSeparate     glad_glBlendFuncSeparate
#define glBlendEquation         glad_glBlendEquation
#define glActiveTexture         glad_glActiveTexture
#define glGenerateMipmap        glad_glGenerateMipmap
#define glGenFramebuffers       glad_glGenFramebuffers
#define glDeleteFramebuffers    glad_glDeleteFramebuffers
#define glBindFramebuffer       glad_glBindFramebuffer
#define glFramebufferTexture2D  glad_glFramebufferTexture2D
#define glCheckFramebufferStatus glad_glCheckFramebufferStatus
#define glDrawBuffers           glad_glDrawBuffers
#define glTexSubImage2D         glad_glTexSubImage2D
#define glGenRenderbuffers      glad_glGenRenderbuffers
#define glDeleteRenderbuffers   glad_glDeleteRenderbuffers
#define glBindRenderbuffer      glad_glBindRenderbuffer
#define glRenderbufferStorage   glad_glRenderbufferStorage
#define glFramebufferRenderbuffer glad_glFramebufferRenderbuffer
#define glVertexAttribIPointer  glad_glVertexAttribIPointer
#define glDebugMessageCallback  glad_glDebugMessageCallback

/* ── 加载函数 ────────────────────────────────────────────── */

typedef void* (*GLADloadproc)(const char *name);

/**
 * 使用给定的加载函数加载所有 OpenGL 函数指针。
 * @param load  函数加载器 (通常是 glfwGetProcAddress)
 * @return      成功加载的函数数量; 0 表示失败
 */
int gladLoadGL(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif /* __glad_h_ */
