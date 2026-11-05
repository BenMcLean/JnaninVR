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

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#ifdef USE_JNANINVR

#include "backends/xr/xr-gl.h"
#include "backends/xr/xr-host.h"

namespace XR {

namespace {

template<class T>
bool loadGL(T &fn, const char *name) {
	fn = (T)Host::getProcAddress(name);
	return fn != nullptr;
}

} // End of anonymous namespace

bool loadGLFuncs(GLFuncs &gl) {
	return loadGL(gl.genFramebuffers, "glGenFramebuffers") &&
	       loadGL(gl.deleteFramebuffers, "glDeleteFramebuffers") &&
	       loadGL(gl.bindFramebuffer, "glBindFramebuffer") &&
	       loadGL(gl.framebufferTexture2D, "glFramebufferTexture2D") &&
	       loadGL(gl.blitFramebuffer, "glBlitFramebuffer") &&
	       loadGL(gl.genRenderbuffers, "glGenRenderbuffers") &&
	       loadGL(gl.bindRenderbuffer, "glBindRenderbuffer") &&
	       loadGL(gl.renderbufferStorage, "glRenderbufferStorage") &&
	       loadGL(gl.framebufferRenderbuffer, "glFramebufferRenderbuffer") &&
	       loadGL(gl.deleteRenderbuffers, "glDeleteRenderbuffers") &&
	       loadGL(gl.deleteTextures, "glDeleteTextures") &&
	       loadGL(gl.genTextures, "glGenTextures") &&
	       loadGL(gl.bindTexture, "glBindTexture") &&
	       loadGL(gl.texImage2D, "glTexImage2D") &&
	       loadGL(gl.getIntegerv, "glGetIntegerv") &&
	       loadGL(gl.isEnabled, "glIsEnabled") &&
	       loadGL(gl.enable, "glEnable") &&
	       loadGL(gl.disable, "glDisable") &&
	       loadGL(gl.finish, "glFinish") &&
	       loadGL(gl.viewport, "glViewport") &&
	       loadGL(gl.clear, "glClear") &&
	       loadGL(gl.clearColor, "glClearColor") &&
	       loadGL(gl.depthMask, "glDepthMask") &&
	       loadGL(gl.colorMask, "glColorMask") &&
	       loadGL(gl.texParameteri, "glTexParameteri") &&
	       loadGL(gl.createShader, "glCreateShader") &&
	       loadGL(gl.shaderSource, "glShaderSource") &&
	       loadGL(gl.compileShader, "glCompileShader") &&
	       loadGL(gl.getShaderiv, "glGetShaderiv") &&
	       loadGL(gl.deleteShader, "glDeleteShader") &&
	       loadGL(gl.createProgram, "glCreateProgram") &&
	       loadGL(gl.attachShader, "glAttachShader") &&
	       loadGL(gl.linkProgram, "glLinkProgram") &&
	       loadGL(gl.getProgramiv, "glGetProgramiv") &&
	       loadGL(gl.deleteProgram, "glDeleteProgram") &&
	       loadGL(gl.useProgram, "glUseProgram") &&
	       loadGL(gl.getUniformLocation, "glGetUniformLocation") &&
	       loadGL(gl.uniformMatrix4fv, "glUniformMatrix4fv") &&
	       loadGL(gl.uniform3fv, "glUniform3fv") &&
	       loadGL(gl.uniform1i, "glUniform1i") &&
	       loadGL(gl.drawArrays, "glDrawArrays") &&
	       loadGL(gl.activeTexture, "glActiveTexture") &&
	       loadGL(gl.bindAttribLocation, "glBindAttribLocation") &&
	       loadGL(gl.bindBuffer, "glBindBuffer") &&
	       loadGL(gl.enableVertexAttribArray, "glEnableVertexAttribArray") &&
	       loadGL(gl.disableVertexAttribArray, "glDisableVertexAttribArray") &&
	       loadGL(gl.vertexAttribPointer, "glVertexAttribPointer") &&
	       loadGL(gl.getVertexAttribiv, "glGetVertexAttribiv");
}

EyeTarget::EyeTarget() : _fbo(0), _depth(0), _width(0), _height(0), _savedFBO(0), _bound(false) {
	_savedViewport[0] = _savedViewport[1] = _savedViewport[2] = _savedViewport[3] = 0;
}

bool EyeTarget::create(GLFuncs &gl) {
	gl.genFramebuffers(1, &_fbo);
	return _fbo != 0;
}

void EyeTarget::resize(GLFuncs &gl, int width, int height) {
	GLStateSaver saver(gl);
	GLint renderbuffer;
	gl.getIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
	if (_depth)
		gl.deleteRenderbuffers(1, &_depth);
	gl.genRenderbuffers(1, &_depth);
	gl.bindRenderbuffer(GL_RENDERBUFFER, _depth);
	gl.renderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
	gl.bindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
	gl.bindFramebuffer(GL_FRAMEBUFFER, _fbo);
	gl.framebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depth);
	_width = width;
	_height = height;
}

void EyeTarget::destroy(GLFuncs &gl) {
	if (_fbo)
		gl.deleteFramebuffers(1, &_fbo);
	if (_depth)
		gl.deleteRenderbuffers(1, &_depth);
	_fbo = _depth = 0;
}

void EyeTarget::bind(GLFuncs &gl, GLuint texture) {
	gl.getIntegerv(GL_FRAMEBUFFER_BINDING, &_savedFBO);
	gl.getIntegerv(GL_VIEWPORT, _savedViewport);

	gl.bindFramebuffer(GL_FRAMEBUFFER, _fbo);
	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	gl.viewport(0, 0, _width, _height);
	gl.colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	gl.depthMask(GL_TRUE);
	gl.clearColor(0.0f, 0.0f, 0.0f, 1.0f);
	gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	_bound = true;
}

void EyeTarget::unbind(GLFuncs &gl) {
	if (!_bound)
		return;

	gl.framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	gl.bindFramebuffer(GL_FRAMEBUFFER, _savedFBO);
	gl.viewport(_savedViewport[0], _savedViewport[1], _savedViewport[2], _savedViewport[3]);
	_bound = false;
}

} // End of namespace XR

#endif // USE_JNANINVR
