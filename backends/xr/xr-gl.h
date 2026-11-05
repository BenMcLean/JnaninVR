/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef BACKENDS_XR_XR_GL_H
#define BACKENDS_XR_XR_GL_H

#include "common/scummsys.h"

#ifdef USE_JNANINVR

/**
 * OpenGL flavor used by the XR code. Headsets provide desktop OpenGL 4.x
 * or OpenGL ES 3.x, so framebuffer objects and blits are always assumed.
 */
#if defined(__ANDROID__) || (defined(USE_GLES_MODE) && USE_GLES_MODE != 0)
#define XR_GL_ES
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#endif

#ifdef XR_GL_ES
#include <GLES3/gl3.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#if defined(GL_APIENTRY)
#define XR_GL_CALL GL_APIENTRY
#elif defined(APIENTRY)
#define XR_GL_CALL APIENTRY
#else
#define XR_GL_CALL
#endif

namespace XR {

/** The GL entry points used by the XR code, loaded at runtime. */
struct GLFuncs {
	void (XR_GL_CALL *genFramebuffers)(GLsizei, GLuint *);
	void (XR_GL_CALL *deleteFramebuffers)(GLsizei, const GLuint *);
	void (XR_GL_CALL *bindFramebuffer)(GLenum, GLuint);
	void (XR_GL_CALL *framebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
	void (XR_GL_CALL *blitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
	void (XR_GL_CALL *genRenderbuffers)(GLsizei, GLuint *);
	void (XR_GL_CALL *bindRenderbuffer)(GLenum, GLuint);
	void (XR_GL_CALL *renderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
	void (XR_GL_CALL *framebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
	void (XR_GL_CALL *deleteRenderbuffers)(GLsizei, const GLuint *);
	void (XR_GL_CALL *deleteTextures)(GLsizei, const GLuint *);
	void (XR_GL_CALL *genTextures)(GLsizei, GLuint *);
	void (XR_GL_CALL *bindTexture)(GLenum, GLuint);
	void (XR_GL_CALL *texImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
	void (XR_GL_CALL *getIntegerv)(GLenum, GLint *);
	GLboolean (XR_GL_CALL *isEnabled)(GLenum);
	void (XR_GL_CALL *enable)(GLenum);
	void (XR_GL_CALL *disable)(GLenum);
	void (XR_GL_CALL *finish)();
	void (XR_GL_CALL *viewport)(GLint, GLint, GLsizei, GLsizei);
	void (XR_GL_CALL *clear)(GLbitfield);
	void (XR_GL_CALL *clearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
	void (XR_GL_CALL *depthMask)(GLboolean);
	void (XR_GL_CALL *colorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
	void (XR_GL_CALL *texParameteri)(GLenum, GLenum, GLint);
	GLuint (XR_GL_CALL *createShader)(GLenum);
	void (XR_GL_CALL *shaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
	void (XR_GL_CALL *compileShader)(GLuint);
	void (XR_GL_CALL *getShaderiv)(GLuint, GLenum, GLint *);
	void (XR_GL_CALL *deleteShader)(GLuint);
	GLuint (XR_GL_CALL *createProgram)();
	void (XR_GL_CALL *attachShader)(GLuint, GLuint);
	void (XR_GL_CALL *linkProgram)(GLuint);
	void (XR_GL_CALL *getProgramiv)(GLuint, GLenum, GLint *);
	void (XR_GL_CALL *deleteProgram)(GLuint);
	void (XR_GL_CALL *useProgram)(GLuint);
	GLint (XR_GL_CALL *getUniformLocation)(GLuint, const GLchar *);
	void (XR_GL_CALL *uniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
	void (XR_GL_CALL *uniform3fv)(GLint, GLsizei, const GLfloat *);
	void (XR_GL_CALL *uniform1i)(GLint, GLint);
	void (XR_GL_CALL *drawArrays)(GLenum, GLint, GLsizei);
	void (XR_GL_CALL *activeTexture)(GLenum);
	void (XR_GL_CALL *bindAttribLocation)(GLuint, GLuint, const GLchar *);
	void (XR_GL_CALL *bindBuffer)(GLenum, GLuint);
	void (XR_GL_CALL *enableVertexAttribArray)(GLuint);
	void (XR_GL_CALL *disableVertexAttribArray)(GLuint);
	void (XR_GL_CALL *vertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
	void (XR_GL_CALL *getVertexAttribiv)(GLuint, GLenum, GLint *);
};

/** Load all the entry points through the host. Returns false if one is missing. */
bool loadGLFuncs(GLFuncs &gl);

/** Saves the GL state touched while copying, and restores it on scope exit. */
class GLStateSaver {
public:
	GLStateSaver(GLFuncs &gl) : _gl(gl) {
		_gl.getIntegerv(GL_READ_FRAMEBUFFER_BINDING, &_read);
		_gl.getIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &_draw);
		_gl.getIntegerv(GL_TEXTURE_BINDING_2D, &_tex);
#ifdef GL_FRAMEBUFFER_SRGB
		_srgb = _gl.isEnabled(GL_FRAMEBUFFER_SRGB);
		_gl.disable(GL_FRAMEBUFFER_SRGB);
#endif
	}
	~GLStateSaver() {
#ifdef GL_FRAMEBUFFER_SRGB
		if (_srgb)
			_gl.enable(GL_FRAMEBUFFER_SRGB);
#endif
		_gl.bindTexture(GL_TEXTURE_2D, _tex);
		_gl.bindFramebuffer(GL_READ_FRAMEBUFFER, _read);
		_gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, _draw);
	}
private:
	GLFuncs &_gl;
	GLint _read, _draw, _tex;
	GLboolean _srgb = GL_FALSE;
};

/**
 * A framebuffer with a depth buffer that an engine renders one eye into.
 * The color texture comes from the backend and changes every frame.
 */
class EyeTarget {
public:
	EyeTarget();

	bool create(GLFuncs &gl);
	void resize(GLFuncs &gl, int width, int height);
	void destroy(GLFuncs &gl);

	/** Bind the target with a texture attached and clear it. The previous binding is kept for unbind(). */
	void bind(GLFuncs &gl, GLuint texture);
	void unbind(GLFuncs &gl);
	bool isBound() const { return _bound; }

private:
	GLuint _fbo, _depth;
	int _width, _height;
	GLint _savedFBO;
	GLint _savedViewport[4];
	bool _bound;
};

} // End of namespace XR

#endif // USE_JNANINVR

#endif
