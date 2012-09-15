/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** QGL.H
*/

#ifndef __QGL_H__
#define __QGL_H__

QGL_EXTERN	qboolean QGL_Init( const char *dllname );
QGL_EXTERN	void     QGL_Shutdown( void );
QGL_EXTERN	void	 *qglGetProcAddress( const GLchar* );

//typedef int GLintptrARB;
//typedef int GLsizeiptrARB;
#endif // __QGL_H__

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef WINAPI
#define WINAPI
#endif


#ifndef __linux__
/*
** extension constants
*/
#define GL_POINT_SIZE_MIN_EXT				0x8126
#define GL_POINT_SIZE_MAX_EXT				0x8127
#define GL_POINT_FADE_THRESHOLD_SIZE_EXT	0x8128
#define GL_DISTANCE_ATTENUATION_EXT			0x8129

#define GL_TEXTURE0_ARB						0x84C0
#define GL_TEXTURE1_ARB						0x84C1
#endif //n _linux

#define GL_TEXTURE0_SGIS					0x835E
#define GL_TEXTURE1_SGIS					0x835F

#ifndef GL_MAX_TEXTURE_UNITS
#define GL_MAX_TEXTURE_UNITS				0x84E2
#endif

#ifndef GL_POLYGON_OFFSET
#define GL_POLYGON_OFFSET					0x8037

#endif /* GL_POLYGON_OFFSET */

#ifndef GL_NV_texture_rectangle
#define GL_NV_texture_rectangle
#define GL_TEXTURE_RECTANGLE_NV				0x84F5
#define GL_TEXTURE_BINDING_RECTANGLE_NV		0x84F6
#define GL_PROXY_TEXTURE_RECTANGLE_NV		0x84F7
#define GL_MAX_RECTANGLE_TEXTURE_SIZE_NV	0x84F8
#endif

#ifndef GL_EXT_texture_filter_anisotropic
#define GL_EXT_texture_filter_anisotropic
#define GL_TEXTURE_MAX_ANISOTROPY_EXT		0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT	0x84FF
#endif

/* GL_ARB_texture_compression */
#ifndef GL_ARB_texture_compression
#define GL_ARB_texture_compression

#define GL_COMPRESSED_ALPHA_ARB								0x84E9
#define GL_COMPRESSED_LUMINANCE_ARB							0x84EA
#define GL_COMPRESSED_LUMINANCE_ALPHA_ARB					0x84EB
#define GL_COMPRESSED_INTENSITY_ARB							0x84EC
#define GL_COMPRESSED_RGB_ARB								0x84ED
#define GL_COMPRESSED_RGBA_ARB								0x84EE
#define GL_TEXTURE_COMPRESSION_HINT_ARB						0x84EF
#define GL_TEXTURE_IMAGE_SIZE_ARB							0x86A0
#define GL_TEXTURE_COMPRESSED_ARB							0x86A1
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB				0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS_ARB					0x86A3
#endif /* GL_ARB_texture_compression */

#ifndef GL_SGIS_generate_mipmap
#define GL_SGIS_generate_mipmap 1
#define GL_GENERATE_MIPMAP_SGIS           0x8191
#define GL_GENERATE_MIPMAP_HINT_SGIS      0x8192
#endif

#ifndef QGL_FUNC
#define QGL_FUNC(type, name, params)
#define UNDEF_QGL_FUNC
#endif

QGL_FUNC(void, glAlphaFunc, (GLenum func, GLclampf ref))
QGL_FUNC(void, glArrayElement, (GLint i))
QGL_FUNC(void, glBegin, (GLenum mode))
QGL_FUNC(void, glBindTexture, (GLenum target, GLuint texture))
QGL_FUNC(void, glBlendFunc, (GLenum sfactor, GLenum dfactor))
QGL_FUNC(void, glClear, (GLbitfield mask))
QGL_FUNC(void, glClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha))
QGL_FUNC(void, glClearDepth, (GLclampd depth))
QGL_FUNC(void, glClearStencil, (GLint s))
QGL_FUNC(void, glClipPlane, (GLenum plane, const GLdouble *equation))
QGL_FUNC(void, glColor3f, (GLfloat red, GLfloat green, GLfloat blue))
QGL_FUNC(void, glColor3fv, (const GLfloat *v))
QGL_FUNC(void, glColor3ubv, (const GLubyte *v))
QGL_FUNC(void, glColor4f, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha))
QGL_FUNC(void, glColor4fv, (const GLfloat *v))
QGL_FUNC(void, glColor4ubv, (const GLubyte *v))
QGL_FUNC(void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha))
QGL_FUNC(void, glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
QGL_FUNC(void, glCullFace, (GLenum mode))
QGL_FUNC(void, glDeleteTextures, (GLsizei n, const GLuint *textures))
QGL_FUNC(void, glDepthFunc, (GLenum func))
QGL_FUNC(void, glDepthMask, (GLboolean flag))
QGL_FUNC(void, glDepthRange, (GLclampd zNear, GLclampd zFar))
QGL_FUNC(void, glDisable, (GLenum cap))
QGL_FUNC(void, glDisableClientState, (GLenum array))
QGL_FUNC(void, glDrawBuffer, (GLenum mode))
QGL_FUNC(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices))
QGL_FUNC(void, glEnable, (GLenum cap))
QGL_FUNC(void, glEnableClientState, (GLenum array))
QGL_FUNC(void, glEnd, (void))
QGL_FUNC(void, glFinish, (void))
QGL_FUNC(void, glFlush, (void))
QGL_FUNC(void, glFrontFace, (GLenum mode))
QGL_FUNC(GLenum, glGetError, (void))
QGL_FUNC(void, glGetFloatv, (GLenum pname, GLfloat *params))
QGL_FUNC(void, glGetIntegerv, (GLenum pname, GLint *params))
QGL_FUNC(const GLubyte *, glGetString, (GLenum name))
QGL_FUNC(void, glLoadIdentity, (void))
QGL_FUNC(void, glLoadMatrixf, (const GLfloat *m))
QGL_FUNC(void, glMatrixMode, (GLenum mode))
QGL_FUNC(void, glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *pointer))
QGL_FUNC(void, glOrtho, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar))
QGL_FUNC(void, glPolygonMode, (GLenum face, GLenum mode))
QGL_FUNC(void, glPolygonOffset, (GLfloat factor, GLfloat units))
QGL_FUNC(void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels))
QGL_FUNC(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height))
QGL_FUNC(void, glShadeModel, (GLenum mode))
QGL_FUNC(void, glStencilFunc, (GLenum func, GLint ref, GLuint mask))
QGL_FUNC(void, glStencilMask, (GLuint mask))
QGL_FUNC(void, glStencilOp, (GLenum fail, GLenum zfail, GLenum zpass))
QGL_FUNC(void, glTexCoord2f, (GLfloat s, GLfloat t))
QGL_FUNC(void, glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
//QGL_FUNC(void, glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params))
QGL_FUNC(void, glTexEnvi, (GLenum target, GLenum pname, GLint param))
//QGL_FUNC(void, glTexEnvf, (GLenum target, GLenum pname, GLfloat param))
//QGL_FUNC(void, glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params))
//QGL_FUNC(void, glTexGeni, (GLenum coord, GLenum pname, GLint param))
QGL_FUNC(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels))
QGL_FUNC(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))
QGL_FUNC(void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param))
QGL_FUNC(void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels))
QGL_FUNC(void, glVertex2f, (GLfloat x, GLfloat y))
QGL_FUNC(void, glVertex3f, (GLfloat x, GLfloat y, GLfloat z))
QGL_FUNC(void, glVertex3fv, (const GLfloat *v))
QGL_FUNC(void, glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *pointer))
QGL_FUNC(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))
QGL_FUNC(void, glTranslatef, (GLfloat x, GLfloat y, GLfloat z))
QGL_FUNC(void, glRotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z))
QGL_FUNC(void, glTexCoord2i, (GLint s, GLint t))
QGL_FUNC(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))
QGL_FUNC(void, glPointSize, (GLfloat size))
QGL_FUNC(void, glFogf, (GLenum pname, GLfloat param))
QGL_FUNC(void, glFogfv, (GLenum pname, const GLfloat *params))
QGL_FUNC(void, glFogi, (GLenum pname, GLint param))
QGL_FUNC(void, glFogiv, (GLenum pname, const GLint *params))
QGL_FUNC(void, glGenTextures, (GLsizei n, GLuint *textures))
QGL_FUNC(void, glCopyTexImage2D, (GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border))
QGL_FUNC(void, glCopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset , GLint yoffset , GLint x , GLint y , GLsizei width , GLsizei height))
QGL_FUNC(void, glHint, (GLenum target, GLenum mode))
QGL_FUNC(void, glTexCoord2fv, (const GLfloat *v))
QGL_FUNC(void, glVertex2i, (GLint x, GLint y))
QGL_FUNC(void, glPopMatrix, (void))
QGL_FUNC(void, glPushMatrix, (void))
QGL_FUNC(void, glLineStipple, (GLint factor, GLushort pattern))
QGL_FUNC(void, glLineWidth, (GLfloat width))
#ifdef UNDEF_QGL_FUNC
#undef QGL_FUNC
#undef UNDEF_QGL_FUNC
#endif

#ifndef QGL_EXT
#define QGL_EXT(type, name, params)
#define UNDEF_QGL_EXT
#endif
QGL_EXT(void, glLockArraysEXT, (int , int ))
QGL_EXT(void, glUnlockArraysEXT, (void))
QGL_EXT(void, glSelectTextureSGIS, (GLenum ))
QGL_EXT(void, glActiveTextureARB, (GLenum ))
QGL_EXT(void, glClientActiveTextureARB, (GLenum ))
QGL_EXT(void, glDrawRangeElementsEXT, (GLenum, GLuint, GLuint, GLsizei, GLenum, const GLvoid *))
QGL_EXT(void, glPointParameterfEXT, (GLenum param, GLfloat value))
QGL_EXT(void, glPointParameterfvEXT, (GLenum param, const GLfloat *value))
QGL_EXT(void, glMTexCoord2fSGIS, (GLenum, GLfloat, GLfloat ))
QGL_EXT(void, glMTexCoord2fvSGIS, (GLenum, GLfloat * ))
QGL_EXT(void, glSelectTextureSGIS, (GLenum ))
#ifdef UNDEF_QGL_EXT
#undef QGL_EXT
#undef UNDEF_QGL_EXT
#endif

// WGL Functions
#ifndef QGL_WGL
#define QGL_WGL(type, name, params)
#define UNDEF_QGL_WGL
#endif
QGL_WGL(int, wglChoosePixelFormat, (HDC, CONST PIXELFORMATDESCRIPTOR *))
QGL_WGL(int, wglDescribePixelFormat, (HDC, int, UINT, LPPIXELFORMATDESCRIPTOR))
QGL_WGL(BOOL, wglSetPixelFormat, (HDC, int, CONST PIXELFORMATDESCRIPTOR *))
QGL_WGL(BOOL, wglSwapBuffers, (HDC))
QGL_WGL(HGLRC, wglCreateContext, (HDC))
QGL_WGL(BOOL, wglDeleteContext, (HGLRC))
QGL_WGL(BOOL, wglMakeCurrent, (HDC, HGLRC))
QGL_WGL(PROC, wglGetProcAddress, (LPCSTR))
#ifdef UNDEF_QGL_WGL
#undef QGL_WGL
#undef UNDEF_QGL_WGL
#endif

// WGL_EXT Functions
#ifndef QGL_WGL_EXT
#define QGL_WGL_EXT(type, name, params)
#define UNDEF_QGL_WGL_EXT
#endif
QGL_WGL_EXT(BOOL, wglSwapIntervalEXT, (int interval))
//QGL_WGL_EXT(BOOL, wglGetDeviceGammaRamp3DFX, (HDC, WORD *))
//QGL_WGL_EXT(BOOL, wglSetDeviceGammaRamp3DFX, (HDC, WORD *))
#ifdef UNDEF_QGL_WGL_EXT
#undef QGL_WGL_EXT
#undef UNDEF_QGL_WGL_EXT
#endif

// GLX Functions
#ifndef QGL_GLX
#define QGL_GLX(type, name, params)
#define UNDEF_GLX
#endif
#ifndef USE_SDL
QGL_GLX(XVisualInfo *, glXChooseVisual, (Display *dpy, int screen, int *attribList))
QGL_GLX(GLXContext, glXCreateContext, (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct))
QGL_GLX(void, glXDestroyContext, (Display *dpy, GLXContext ctx))
QGL_GLX(Bool, glXMakeCurrent, (Display *dpy, GLXDrawable drawable, GLXContext ctx))
QGL_GLX(Bool, glXCopyContext, (Display *dpy, GLXContext src, GLXContext dst, GLuint mask))
QGL_GLX(Bool, glXSwapBuffers, (Display *dpy, GLXDrawable drawable))
QGL_GLX(int, glXGetConfig, (Display *dpy, XVisualInfo *vis, int attrib, int *value))
//QGL_GLX(void *, glXGetProcAddressARB, (const GLubyte *procName))
#endif
#ifdef UNDEF_GLX
#undef QGL_GLX
#undef UNDEF_GLX
#endif

// GLX_EXT Functions
