#include "glad/glad.h"

/* ── 函数指针定义 ──────────────────────────────────────────── */

PFNGLVIEWPORTPROC               glad_glViewport = 0;
PFNGLCLEARPROC                   glad_glClear = 0;
PFNGLCLEARCOLORPROC              glad_glClearColor = 0;
PFNGLENABLEPROC                  glad_glEnable = 0;
PFNGLDISABLEPROC                 glad_glDisable = 0;
PFNGLBLENDFUNCPROC               glad_glBlendFunc = 0;
PFNGLGETERRORPROC                glad_glGetError = 0;
PFNGLGETSTRINGPROC               glad_glGetString = 0;
PFNGLGETINTEGERVPROC             glad_glGetIntegerv = 0;
PFNGLGETFLOATVPROC               glad_glGetFloatv = 0;
PFNGLDEPTHFUNCPROC               glad_glDepthFunc = 0;
PFNGLDEPTHMASKPROC               glad_glDepthMask = 0;
PFNGLCULLFACEPROC                glad_glCullFace = 0;
PFNGLFRONTFACEPROC               glad_glFrontFace = 0;
PFNGLPOLYGONMODEPROC             glad_glPolygonMode = 0;
PFNGLLINEWIDTHPROC               glad_glLineWidth = 0;

PFNGLDRAWARRAYSPROC              glad_glDrawArrays = 0;
PFNGLDRAWELEMENTSPROC            glad_glDrawElements = 0;

PFNGLBINDTEXTUREPROC             glad_glBindTexture = 0;
PFNGLDELETETEXTURESPROC          glad_glDeleteTextures = 0;
PFNGLGENTEXTURESPROC             glad_glGenTextures = 0;
PFNGLTEXIMAGE2DPROC              glad_glTexImage2D = 0;
PFNGLTEXPARAMETERIPROC           glad_glTexParameteri = 0;
PFNGLTEXPARAMETERFPROC           glad_glTexParameterf = 0;
PFNGLTEXPARAMETERFVPROC          glad_glTexParameterfv = 0;

PFNGLGENVERTEXARRAYSPROC         glad_glGenVertexArrays = 0;
PFNGLDELETEVERTEXARRAYSPROC      glad_glDeleteVertexArrays = 0;
PFNGLBINDVERTEXARRAYPROC         glad_glBindVertexArray = 0;

PFNGLGENBUFFERSPROC              glad_glGenBuffers = 0;
PFNGLDELETEBUFFERSPROC           glad_glDeleteBuffers = 0;
PFNGLBINDBUFFERPROC              glad_glBindBuffer = 0;
PFNGLBUFFERDATAPROC              glad_glBufferData = 0;
PFNGLBUFFERSUBDATAPROC           glad_glBufferSubData = 0;

PFNGLENABLEVERTEXATTRIBARRAYPROC  glad_glEnableVertexAttribArray = 0;
PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray = 0;
PFNGLVERTEXATTRIBPOINTERPROC     glad_glVertexAttribPointer = 0;
PFNGLVERTEXATTRIBDIVISORPROC     glad_glVertexAttribDivisor = 0;
PFNGLDRAWARRAYSINSTANCEDPROC     glad_glDrawArraysInstanced = 0;

PFNGLCREATESHADERPROC            glad_glCreateShader = 0;
PFNGLDELETESHADERPROC            glad_glDeleteShader = 0;
PFNGLSHADERSOURCEPROC            glad_glShaderSource = 0;
PFNGLCOMPILESHADERPROC           glad_glCompileShader = 0;
PFNGLGETSHADERIVPROC             glad_glGetShaderiv = 0;
PFNGLGETSHADERINFOLOGPROC        glad_glGetShaderInfoLog = 0;

PFNGLCREATEPROGRAMPROC           glad_glCreateProgram = 0;
PFNGLDELETEPROGRAMPROC           glad_glDeleteProgram = 0;
PFNGLATTACHSHADERPROC            glad_glAttachShader = 0;
PFNGLLINKPROGRAMPROC             glad_glLinkProgram = 0;
PFNGLUSEPROGRAMPROC              glad_glUseProgram = 0;
PFNGLGETPROGRAMIVPROC            glad_glGetProgramiv = 0;
PFNGLGETPROGRAMINFOLOGPROC       glad_glGetProgramInfoLog = 0;
PFNGLGETUNIFORMLOCATIONPROC      glad_glGetUniformLocation = 0;

PFNGLUNIFORM1IPROC              glad_glUniform1i = 0;
PFNGLUNIFORM1FPROC              glad_glUniform1f = 0;
PFNGLUNIFORM2FPROC              glad_glUniform2f = 0;
PFNGLUNIFORM3FPROC              glad_glUniform3f = 0;
PFNGLUNIFORM4FPROC              glad_glUniform4f = 0;
PFNGLUNIFORMMATRIX4FVPROC       glad_glUniformMatrix4fv = 0;
PFNGLUNIFORMMATRIX3FVPROC       glad_glUniformMatrix3fv = 0;

PFNGLACTIVETEXTUREPROC          glad_glActiveTexture = 0;
PFNGLGENERATEMIPMAPROC          glad_glGenerateMipmap = 0;

PFNGLGENFRAMEBUFFERSPROC        glad_glGenFramebuffers = 0;
PFNGLDELETEFRAMEBUFFERSPROC     glad_glDeleteFramebuffers = 0;
PFNGLBINDFRAMEBUFFERPROC        glad_glBindFramebuffer = 0;
PFNGLFRAMEBUFFERTEXTURE2DPROC   glad_glFramebufferTexture2D = 0;
PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus = 0;
PFNGLDRAWBUFFERSPROC            glad_glDrawBuffers = 0;
PFNGLTEXSUBIMAGE2DPROC          glad_glTexSubImage2D = 0;

PFNGLPIXELSTOREIPROC            glad_glPixelStorei = 0;
PFNGLSCISSORPROC                glad_glScissor = 0;
PFNGLBLENDFUNCSEPARATEPROC     glad_glBlendFuncSeparate = 0;
PFNGLBLENDEQUATIONPROC         glad_glBlendEquation = 0;

PFNGLDEBUGMESSAGECALLBACKPROC   glad_glDebugMessageCallback = 0;

/* ── 加载实现 ──────────────────────────────────────────────── */

/* 使用 undef 来避免宏展开干扰字符串字面量 */
int gladLoadGL(GLADloadproc load) {
    int count = 0;

#define GLAD_LOAD(ptr, name) ptr = (typeof(ptr))load(name); if(ptr) count++

    GLAD_LOAD(glad_glViewport,              "glViewport");
    GLAD_LOAD(glad_glClear,                 "glClear");
    GLAD_LOAD(glad_glClearColor,            "glClearColor");
    GLAD_LOAD(glad_glEnable,                "glEnable");
    GLAD_LOAD(glad_glDisable,               "glDisable");
    GLAD_LOAD(glad_glBlendFunc,             "glBlendFunc");
    GLAD_LOAD(glad_glGetError,              "glGetError");
    GLAD_LOAD(glad_glGetString,             "glGetString");
    GLAD_LOAD(glad_glGetIntegerv,           "glGetIntegerv");
    GLAD_LOAD(glad_glGetFloatv,             "glGetFloatv");
    GLAD_LOAD(glad_glDepthFunc,             "glDepthFunc");
    GLAD_LOAD(glad_glDepthMask,             "glDepthMask");
    GLAD_LOAD(glad_glCullFace,              "glCullFace");
    GLAD_LOAD(glad_glFrontFace,             "glFrontFace");
    GLAD_LOAD(glad_glPolygonMode,           "glPolygonMode");
    GLAD_LOAD(glad_glLineWidth,             "glLineWidth");

    GLAD_LOAD(glad_glDrawArrays,            "glDrawArrays");
    GLAD_LOAD(glad_glDrawElements,          "glDrawElements");

    GLAD_LOAD(glad_glBindTexture,           "glBindTexture");
    GLAD_LOAD(glad_glDeleteTextures,        "glDeleteTextures");
    GLAD_LOAD(glad_glGenTextures,           "glGenTextures");
    GLAD_LOAD(glad_glTexImage2D,            "glTexImage2D");
    GLAD_LOAD(glad_glTexParameteri,         "glTexParameteri");
    GLAD_LOAD(glad_glTexParameterf,         "glTexParameterf");
    GLAD_LOAD(glad_glTexParameterfv,        "glTexParameterfv");

    GLAD_LOAD(glad_glGenVertexArrays,       "glGenVertexArrays");
    GLAD_LOAD(glad_glDeleteVertexArrays,    "glDeleteVertexArrays");
    GLAD_LOAD(glad_glBindVertexArray,       "glBindVertexArray");

    GLAD_LOAD(glad_glGenBuffers,            "glGenBuffers");
    GLAD_LOAD(glad_glDeleteBuffers,         "glDeleteBuffers");
    GLAD_LOAD(glad_glBindBuffer,            "glBindBuffer");
    GLAD_LOAD(glad_glBufferData,            "glBufferData");
    GLAD_LOAD(glad_glBufferSubData,         "glBufferSubData");

    GLAD_LOAD(glad_glEnableVertexAttribArray,  "glEnableVertexAttribArray");
    GLAD_LOAD(glad_glDisableVertexAttribArray, "glDisableVertexAttribArray");
    GLAD_LOAD(glad_glVertexAttribPointer,      "glVertexAttribPointer");
    GLAD_LOAD(glad_glVertexAttribDivisor,      "glVertexAttribDivisor");
    GLAD_LOAD(glad_glDrawArraysInstanced,      "glDrawArraysInstanced");

    GLAD_LOAD(glad_glCreateShader,          "glCreateShader");
    GLAD_LOAD(glad_glDeleteShader,          "glDeleteShader");
    GLAD_LOAD(glad_glShaderSource,          "glShaderSource");
    GLAD_LOAD(glad_glCompileShader,         "glCompileShader");
    GLAD_LOAD(glad_glGetShaderiv,           "glGetShaderiv");
    GLAD_LOAD(glad_glGetShaderInfoLog,      "glGetShaderInfoLog");

    GLAD_LOAD(glad_glCreateProgram,         "glCreateProgram");
    GLAD_LOAD(glad_glDeleteProgram,         "glDeleteProgram");
    GLAD_LOAD(glad_glAttachShader,          "glAttachShader");
    GLAD_LOAD(glad_glLinkProgram,           "glLinkProgram");
    GLAD_LOAD(glad_glUseProgram,            "glUseProgram");
    GLAD_LOAD(glad_glGetProgramiv,          "glGetProgramiv");
    GLAD_LOAD(glad_glGetProgramInfoLog,     "glGetProgramInfoLog");
    GLAD_LOAD(glad_glGetUniformLocation,    "glGetUniformLocation");

    GLAD_LOAD(glad_glUniform1i,             "glUniform1i");
    GLAD_LOAD(glad_glUniform1f,             "glUniform1f");
    GLAD_LOAD(glad_glUniform2f,             "glUniform2f");
    GLAD_LOAD(glad_glUniform3f,             "glUniform3f");
    GLAD_LOAD(glad_glUniform4f,             "glUniform4f");
    GLAD_LOAD(glad_glUniformMatrix4fv,      "glUniformMatrix4fv");
    GLAD_LOAD(glad_glUniformMatrix3fv,      "glUniformMatrix3fv");

    GLAD_LOAD(glad_glPixelStorei,           "glPixelStorei");
    GLAD_LOAD(glad_glScissor,               "glScissor");
    GLAD_LOAD(glad_glBlendFuncSeparate,     "glBlendFuncSeparate");
    GLAD_LOAD(glad_glBlendEquation,         "glBlendEquation");
    GLAD_LOAD(glad_glActiveTexture,         "glActiveTexture");
    GLAD_LOAD(glad_glGenerateMipmap,        "glGenerateMipmap");

    GLAD_LOAD(glad_glGenFramebuffers,       "glGenFramebuffers");
    GLAD_LOAD(glad_glDeleteFramebuffers,    "glDeleteFramebuffers");
    GLAD_LOAD(glad_glBindFramebuffer,       "glBindFramebuffer");
    GLAD_LOAD(glad_glFramebufferTexture2D,  "glFramebufferTexture2D");
    GLAD_LOAD(glad_glCheckFramebufferStatus,"glCheckFramebufferStatus");
    GLAD_LOAD(glad_glDrawBuffers,            "glDrawBuffers");
    GLAD_LOAD(glad_glTexSubImage2D,          "glTexSubImage2D");

    GLAD_LOAD(glad_glDebugMessageCallback,  "glDebugMessageCallback");

#undef GLAD_LOAD

    return count;
}
