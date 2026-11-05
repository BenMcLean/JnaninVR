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

#ifdef USE_OPENXR

#include "common/array.h"
#include "common/debug.h"
#include "common/str.h"
#include "common/textconsole.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "backends/xr/openxr-platform.h"
#include "backends/xr/xr-backend.h"

namespace XR {

namespace {

enum { kEyeCount = 2 };

struct Session {
	GLFuncs *gl;

	XrInstance instance;
	XrSystemId systemId;
	XrSession session;
	XrSpace space;
	XrSpace viewSpace;
	bool sessionRunning;
	int64_t chainFormat;

	// Swapchain carrying the theatre screen to the compositor
	XrSwapchain swapchain;
	SwapchainImage *images;
	uint32 imageCount;
	GLuint chainFBO;
	int chainW, chainH;

	// Per eye swapchains (stereo mode)
	XrSwapchain eyeChain[kEyeCount];
	SwapchainImage *eyeImages[kEyeCount];
	uint32 eyeImageCount[kEyeCount];
	int eyeW, eyeH;

	XrFrameState frameState;
	XrView views[kEyeCount];

	XrActionSet actionSet;
	XrAction aimAction, selectAction, menuAction, turnAction, turnClickAction, inventoryAction;
	XrPath handPath[kHandCount];
	XrSpace aimSpace[kHandCount];

	uint32 periodMs;

	Session() {
		memset(this, 0, sizeof(*this));
		instance = XR_NULL_HANDLE;
		session = XR_NULL_HANDLE;
		space = XR_NULL_HANDLE;
		viewSpace = XR_NULL_HANDLE;
		swapchain = XR_NULL_HANDLE;
		for (int e = 0; e < kEyeCount; e++)
			eyeChain[e] = XR_NULL_HANDLE;
		actionSet = XR_NULL_HANDLE;
		aimAction = selectAction = menuAction = turnAction = turnClickAction = inventoryAction = XR_NULL_HANDLE;
		for (int h = 0; h < kHandCount; h++) {
			handPath[h] = XR_NULL_PATH;
			aimSpace[h] = XR_NULL_HANDLE;
		}
		periodMs = 11;
	}
};

template<class T>
T xrStruct(XrStructureType type) {
	T s;
	memset(&s, 0, sizeof(s));
	s.type = type;
	return s;
}

bool check(XrResult result, const char *what) {
	if (XR_FAILED(result)) {
		warning("OpenXR: %s failed (%d)", what, (int)result);
		return false;
	}
	return true;
}

#define XR_TRY(x) do { if (!check((x), #x)) return false; } while (0)

Quat toQuat(const XrQuaternionf &q) {
	const Quat r = { q.x, q.y, q.z, q.w };
	return r;
}

XrPosef toXrPose(const Pose &pose) {
	XrPosef p;
	p.orientation.x = pose.orientation.x;
	p.orientation.y = pose.orientation.y;
	p.orientation.z = pose.orientation.z;
	p.orientation.w = pose.orientation.w;
	p.position.x = pose.position[0];
	p.position.y = pose.position[1];
	p.position.z = pose.position[2];
	return p;
}

enum { kBothHands, kLeftOnly, kRightOnly };

struct BindingDef {
	int action;
	const char *suffix;
	int hands;
};

enum { kActionAim, kActionSelect, kActionMenu, kActionTurn, kActionTurnClick, kActionInventory, kActionCount };

bool suggestBindings(Session &c, const XrAction *actions, const char *profileName, const BindingDef *defs, uint count, bool required) {
	XrPath profile;
	XR_TRY(xrStringToPath(c.instance, profileName, &profile));

	static const char *const kHands[kHandCount] = { "/user/hand/left", "/user/hand/right" };
	Common::Array<XrActionSuggestedBinding> bindings;
	for (uint i = 0; i < count; i++) {
		for (int h = 0; h < kHandCount; h++) {
			if ((defs[i].hands == kLeftOnly && h != kHandLeft) || (defs[i].hands == kRightOnly && h != kHandRight))
				continue;
			XrActionSuggestedBinding binding;
			binding.action = actions[defs[i].action];
			const Common::String path = Common::String(kHands[h]) + defs[i].suffix;
			XR_TRY(xrStringToPath(c.instance, path.c_str(), &binding.binding));
			bindings.push_back(binding);
		}
	}

	XrInteractionProfileSuggestedBinding suggest = xrStruct<XrInteractionProfileSuggestedBinding>(XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING);
	suggest.interactionProfile = profile;
	suggest.countSuggestedBindings = bindings.size();
	suggest.suggestedBindings = bindings.data();
	const XrResult result = xrSuggestInteractionProfileBindings(c.instance, &suggest);
	if (XR_FAILED(result)) {
		if (required)
			return check(result, profileName);
		debug(1, "OpenXR: bindings for %s not accepted (%d)", profileName, (int)result);
	}
	return true;
}

bool createActions(Session &c) {
	XR_TRY(xrStringToPath(c.instance, "/user/hand/left", &c.handPath[kHandLeft]));
	XR_TRY(xrStringToPath(c.instance, "/user/hand/right", &c.handPath[kHandRight]));

	XrActionSetCreateInfo setInfo = xrStruct<XrActionSetCreateInfo>(XR_TYPE_ACTION_SET_CREATE_INFO);
	strcpy(setInfo.actionSetName, "gameplay");
	strcpy(setInfo.localizedActionSetName, "Gameplay");
	XR_TRY(xrCreateActionSet(c.instance, &setInfo, &c.actionSet));

	struct { XrActionType type; const char *name; } defs[kActionCount] = {
		{ XR_ACTION_TYPE_POSE_INPUT, "aim" },
		{ XR_ACTION_TYPE_BOOLEAN_INPUT, "select" },
		{ XR_ACTION_TYPE_BOOLEAN_INPUT, "menu" },
		{ XR_ACTION_TYPE_FLOAT_INPUT, "turn" },
		{ XR_ACTION_TYPE_BOOLEAN_INPUT, "turn_click" },
		{ XR_ACTION_TYPE_BOOLEAN_INPUT, "inventory" }
	};
	XrAction actions[kActionCount];
	for (int i = 0; i < kActionCount; i++) {
		XrActionCreateInfo info = xrStruct<XrActionCreateInfo>(XR_TYPE_ACTION_CREATE_INFO);
		info.actionType = defs[i].type;
		strcpy(info.actionName, defs[i].name);
		strcpy(info.localizedActionName, defs[i].name);
		info.countSubactionPaths = kHandCount;
		info.subactionPaths = c.handPath;
		XR_TRY(xrCreateAction(c.actionSet, &info, &actions[i]));
	}
	c.aimAction = actions[kActionAim];
	c.selectAction = actions[kActionSelect];
	c.menuAction = actions[kActionMenu];
	c.turnAction = actions[kActionTurn];
	c.turnClickAction = actions[kActionTurnClick];
	c.inventoryAction = actions[kActionInventory];

	// Standard Khronos interaction profiles; the runtime maps the connected device
	static const BindingDef simple[] = {
		{ kActionAim, "/input/aim/pose", kBothHands },
		{ kActionSelect, "/input/select/click", kBothHands },
		{ kActionMenu, "/input/menu/click", kBothHands }
	};
	static const BindingDef thumbstickMenuLeft[] = {
		{ kActionAim, "/input/aim/pose", kBothHands },
		{ kActionSelect, "/input/trigger/value", kBothHands },
		{ kActionMenu, "/input/menu/click", kLeftOnly },
		{ kActionMenu, "/input/b/click", kRightOnly },
		{ kActionInventory, "/input/x/click", kLeftOnly },
		{ kActionInventory, "/input/a/click", kRightOnly },
		{ kActionTurn, "/input/thumbstick/x", kBothHands },
		{ kActionTurnClick, "/input/thumbstick/click", kBothHands }
	};
	static const BindingDef thumbstickMenuB[] = {
		{ kActionAim, "/input/aim/pose", kBothHands },
		{ kActionSelect, "/input/trigger/value", kBothHands },
		{ kActionMenu, "/input/b/click", kBothHands },
		{ kActionInventory, "/input/a/click", kBothHands },
		{ kActionTurn, "/input/thumbstick/x", kBothHands },
		{ kActionTurnClick, "/input/thumbstick/click", kBothHands }
	};

	return suggestBindings(c, actions, "/interaction_profiles/khr/simple_controller", simple, ARRAYSIZE(simple), true) &&
	       suggestBindings(c, actions, "/interaction_profiles/oculus/touch_controller", thumbstickMenuLeft, ARRAYSIZE(thumbstickMenuLeft), false) &&
	       suggestBindings(c, actions, "/interaction_profiles/valve/index_controller", thumbstickMenuB, ARRAYSIZE(thumbstickMenuB), false);
}

bool createSession(Session &c) {
	Common::Array<const char *> extensions;
	Platform::getExtensions(extensions);
	XrInstanceCreateInfo instanceInfo = xrStruct<XrInstanceCreateInfo>(XR_TYPE_INSTANCE_CREATE_INFO);
	strcpy(instanceInfo.applicationInfo.applicationName, "ScummVM");
	instanceInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
	instanceInfo.next = Platform::getInstanceNext();
	instanceInfo.enabledExtensionCount = extensions.size();
	instanceInfo.enabledExtensionNames = extensions.data();
	XR_TRY(xrCreateInstance(&instanceInfo, &c.instance));

	XrSystemGetInfo systemInfo = xrStruct<XrSystemGetInfo>(XR_TYPE_SYSTEM_GET_INFO);
	systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XR_TRY(xrGetSystem(c.instance, &systemInfo, &c.systemId));

	if (!Platform::createSession(c.instance, c.systemId, c.session)) {
		warning("OpenXR: could not create a session for the GL context");
		return false;
	}

	XrReferenceSpaceCreateInfo spaceInfo = xrStruct<XrReferenceSpaceCreateInfo>(XR_TYPE_REFERENCE_SPACE_CREATE_INFO);
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
	XR_TRY(xrCreateReferenceSpace(c.session, &spaceInfo, &c.space));
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	XR_TRY(xrCreateReferenceSpace(c.session, &spaceInfo, &c.viewSpace));

	XrViewConfigurationView viewConfigs[kEyeCount];
	for (int e = 0; e < kEyeCount; e++)
		viewConfigs[e] = xrStruct<XrViewConfigurationView>(XR_TYPE_VIEW_CONFIGURATION_VIEW);
	uint32_t viewCount = 0;
	XR_TRY(xrEnumerateViewConfigurationViews(c.instance, c.systemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, kEyeCount, &viewCount, viewConfigs));
	c.eyeW = viewConfigs[0].recommendedImageRectWidth;
	c.eyeH = viewConfigs[0].recommendedImageRectHeight;

	uint32_t formatCount = 0;
	XR_TRY(xrEnumerateSwapchainFormats(c.session, 0, &formatCount, nullptr));
	int64_t *formats = new int64_t[formatCount];
	xrEnumerateSwapchainFormats(c.session, formatCount, &formatCount, formats);
	c.chainFormat = 0;
	for (uint32_t i = 0; i < formatCount; i++) {
		if (formats[i] == GL_SRGB8_ALPHA8)
			c.chainFormat = formats[i];
	}
	for (uint32_t i = 0; i < formatCount && !c.chainFormat; i++) {
		if (formats[i] == GL_RGBA8)
			c.chainFormat = formats[i];
	}
	delete[] formats;
	if (!c.chainFormat) {
		warning("OpenXR: no usable swapchain format");
		return false;
	}

	if (!createActions(c))
		return false;

	XrSessionActionSetsAttachInfo attachInfo = xrStruct<XrSessionActionSetsAttachInfo>(XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO);
	attachInfo.countActionSets = 1;
	attachInfo.actionSets = &c.actionSet;
	XR_TRY(xrAttachSessionActionSets(c.session, &attachInfo));

	for (int h = 0; h < kHandCount; h++) {
		XrActionSpaceCreateInfo info = xrStruct<XrActionSpaceCreateInfo>(XR_TYPE_ACTION_SPACE_CREATE_INFO);
		info.action = c.aimAction;
		info.subactionPath = c.handPath[h];
		info.poseInActionSpace.orientation.w = 1.0f;
		XR_TRY(xrCreateActionSpace(c.session, &info, &c.aimSpace[h]));
	}

	return true;
}

void destroySwapchain(XrSwapchain &chain, SwapchainImage *&images, uint32 &count) {
	if (chain != XR_NULL_HANDLE) {
		xrDestroySwapchain(chain);
		chain = XR_NULL_HANDLE;
	}
	delete[] images;
	images = nullptr;
	count = 0;
}

bool createSwapchain(Session &c, int w, int h, XrSwapchain &chain, SwapchainImage *&images, uint32 &count) {
	destroySwapchain(chain, images, count);

	XrSwapchainCreateInfo info = xrStruct<XrSwapchainCreateInfo>(XR_TYPE_SWAPCHAIN_CREATE_INFO);
	info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
	info.format = c.chainFormat;
	info.sampleCount = 1;
	info.width = w;
	info.height = h;
	info.faceCount = 1;
	info.arraySize = 1;
	info.mipCount = 1;
	XR_TRY(xrCreateSwapchain(c.session, &info, &chain));

	XR_TRY(xrEnumerateSwapchainImages(chain, 0, &count, nullptr));
	images = new SwapchainImage[count];
	for (uint32 i = 0; i < count; i++)
		images[i] = xrStruct<SwapchainImage>(kSwapchainImageType);
	XR_TRY(xrEnumerateSwapchainImages(chain, count, &count, (XrSwapchainImageBaseHeader *)images));
	return true;
}

/** Read the head, the controllers and the actions. */
void locateInput(Session &c, XrTime time, Frame &frame) {
	XrSpaceLocation headLocation = xrStruct<XrSpaceLocation>(XR_TYPE_SPACE_LOCATION);
	const XrSpaceLocationFlags needed = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
	if (XR_SUCCEEDED(xrLocateSpace(c.viewSpace, c.space, time, &headLocation)) &&
	    (headLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
		frame.head.orientation = toQuat(headLocation.pose.orientation);
		frame.head.position[0] = headLocation.pose.position.x;
		frame.head.position[1] = headLocation.pose.position.y;
		frame.head.position[2] = headLocation.pose.position.z;
		frame.headValid = true;
	}

	XrActiveActionSet active = { c.actionSet, XR_NULL_PATH };
	XrActionsSyncInfo syncInfo = xrStruct<XrActionsSyncInfo>(XR_TYPE_ACTIONS_SYNC_INFO);
	syncInfo.countActiveActionSets = 1;
	syncInfo.activeActionSets = &active;
	if (XR_FAILED(xrSyncActions(c.session, &syncInfo)))
		return;

	InputState &input = frame.input;
	input.valid = true;

	for (int h = 0; h < kHandCount; h++) {
		XrActionStateGetInfo getInfo = xrStruct<XrActionStateGetInfo>(XR_TYPE_ACTION_STATE_GET_INFO);
		getInfo.subactionPath = c.handPath[h];
		XrActionStateBoolean state = xrStruct<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);

		getInfo.action = c.selectAction;
		input.select[h] = XR_SUCCEEDED(xrGetActionStateBoolean(c.session, &getInfo, &state)) && state.currentState;

		getInfo.action = c.menuAction;
		state = xrStruct<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
		input.menu[h] = XR_SUCCEEDED(xrGetActionStateBoolean(c.session, &getInfo, &state)) && state.currentState;

		getInfo.action = c.inventoryAction;
		state = xrStruct<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
		if (XR_SUCCEEDED(xrGetActionStateBoolean(c.session, &getInfo, &state)) && state.changedSinceLastSync && state.currentState)
			input.inventoryPressed = true;

		XrActionStateFloat axis = xrStruct<XrActionStateFloat>(XR_TYPE_ACTION_STATE_FLOAT);
		getInfo.action = c.turnAction;
		if (XR_SUCCEEDED(xrGetActionStateFloat(c.session, &getInfo, &axis)) && axis.isActive && fabsf(axis.currentState) > fabsf(input.stick))
			input.stick = axis.currentState;

		state = xrStruct<XrActionStateBoolean>(XR_TYPE_ACTION_STATE_BOOLEAN);
		getInfo.action = c.turnClickAction;
		if (XR_SUCCEEDED(xrGetActionStateBoolean(c.session, &getInfo, &state)) && state.changedSinceLastSync && state.currentState)
			input.stickClicked = true;

		XrSpaceLocation location = xrStruct<XrSpaceLocation>(XR_TYPE_SPACE_LOCATION);
		if (XR_SUCCEEDED(xrLocateSpace(c.aimSpace[h], c.space, time, &location)) &&
		    (location.locationFlags & needed) == needed) {
			forwardVector(toQuat(location.pose.orientation), input.aimDirection[h][0], input.aimDirection[h][1], input.aimDirection[h][2]);
			input.aimOrigin[h][0] = location.pose.position.x;
			input.aimOrigin[h][1] = location.pose.position.y;
			input.aimOrigin[h][2] = location.pose.position.z;
			input.aimValid[h] = true;
		}
	}
}

bool locateViews(Session &c, Frame &frame) {
	XrViewLocateInfo locateInfo = xrStruct<XrViewLocateInfo>(XR_TYPE_VIEW_LOCATE_INFO);
	locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	locateInfo.displayTime = c.frameState.predictedDisplayTime;
	locateInfo.space = c.space;
	XrViewState viewState = xrStruct<XrViewState>(XR_TYPE_VIEW_STATE);
	for (int e = 0; e < kEyeCount; e++)
		c.views[e] = xrStruct<XrView>(XR_TYPE_VIEW);
	uint32_t count = 0;
	XR_TRY(xrLocateViews(c.session, &locateInfo, &viewState, kEyeCount, &count, c.views));
	if (count != kEyeCount || !(viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT))
		return true;

	for (int e = 0; e < kEyeCount; e++) {
		frame.eyeOrientation[e] = toQuat(c.views[e].pose.orientation);
		frame.eyeFov[e].tanLeft = tanf(c.views[e].fov.angleLeft);
		frame.eyeFov[e].tanRight = tanf(c.views[e].fov.angleRight);
		frame.eyeFov[e].tanDown = tanf(c.views[e].fov.angleDown);
		frame.eyeFov[e].tanUp = tanf(c.views[e].fov.angleUp);
	}
	frame.viewsValid = true;
	return true;
}

} // End of anonymous namespace

class OpenXRBackend : public Backend {
public:
	bool init(GLFuncs &gl) override;
	void shutdown() override;

	bool pollEvents() override;
	bool isRunning() const override { return _c.sessionRunning; }
	uint32 framePeriodMs() const override { return _c.periodMs; }

	bool beginFrame(Frame &frame) override;
	bool endFrame(const Frame &frame, SubmitMode mode, const Theatre &theatre) override;

	bool prepareEyes() override;
	void getEyeSize(int &width, int &height) const override { width = _c.eyeW; height = _c.eyeH; }
	bool acquireEye(int eye, GLuint &texture) override;
	bool releaseEye(int eye) override;

private:
	Session _c;
};

bool OpenXRBackend::init(GLFuncs &gl) {
	_c.gl = &gl;
	if (!gl.blitFramebuffer) {
		warning("OpenXR: needs OpenGL with framebuffer blits");
		return false;
	}
	if (!Platform::initLoader() || !Platform::init()) {
		warning("OpenXR: could not get the window");
		return false;
	}
	if (!createSession(_c))
		return false;
	gl.genFramebuffers(1, &_c.chainFBO);
	return true;
}

bool OpenXRBackend::pollEvents() {
	Session &c = _c;
	XrEventDataBuffer event = xrStruct<XrEventDataBuffer>(XR_TYPE_EVENT_DATA_BUFFER);
	while (xrPollEvent(c.instance, &event) == XR_SUCCESS) {
		if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
			const XrEventDataSessionStateChanged *changed = (const XrEventDataSessionStateChanged *)&event;
			debug(1, "OpenXR: session state %d", (int)changed->state);
			switch (changed->state) {
			case XR_SESSION_STATE_READY: {
				XrSessionBeginInfo beginInfo = xrStruct<XrSessionBeginInfo>(XR_TYPE_SESSION_BEGIN_INFO);
				beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
				if (check(xrBeginSession(c.session, &beginInfo), "xrBeginSession"))
					c.sessionRunning = true;
				break;
			}
			case XR_SESSION_STATE_STOPPING:
				xrEndSession(c.session);
				c.sessionRunning = false;
				break;
			case XR_SESSION_STATE_EXITING:
			case XR_SESSION_STATE_LOSS_PENDING:
				c.sessionRunning = false;
				return false;
			default:
				break;
			}
		}
		if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
			for (int h = 0; h < kHandCount; h++) {
				XrInteractionProfileState profile = xrStruct<XrInteractionProfileState>(XR_TYPE_INTERACTION_PROFILE_STATE);
				if (XR_SUCCEEDED(xrGetCurrentInteractionProfile(c.session, c.handPath[h], &profile)) && profile.interactionProfile != XR_NULL_PATH) {
					char name[XR_MAX_PATH_LENGTH];
					uint32_t length = 0;
					if (XR_SUCCEEDED(xrPathToString(c.instance, profile.interactionProfile, sizeof(name), &length, name)))
						debug(1, "OpenXR: hand %d uses the interaction profile %s", h, name);
				}
			}
		}
		event = xrStruct<XrEventDataBuffer>(XR_TYPE_EVENT_DATA_BUFFER);
	}
	return true;
}

bool OpenXRBackend::beginFrame(Frame &frame) {
	Session &c = _c;
	c.frameState = xrStruct<XrFrameState>(XR_TYPE_FRAME_STATE);
	XR_TRY(xrWaitFrame(c.session, nullptr, &c.frameState));
	if (c.frameState.predictedDisplayPeriod > 0)
		c.periodMs = (uint32)(c.frameState.predictedDisplayPeriod / 1000000);
	XR_TRY(xrBeginFrame(c.session, nullptr));

	frame.shouldRender = c.frameState.shouldRender;
	locateInput(c, c.frameState.predictedDisplayTime, frame);
	return locateViews(c, frame);
}

bool OpenXRBackend::endFrame(const Frame &frame, SubmitMode mode, const Theatre &theatre) {
	Session &c = _c;
	GLFuncs &gl = *c.gl;

	const XrCompositionLayerBaseHeader *layers[1];
	uint32_t layerCount = 0;
	XrCompositionLayerQuad quad = xrStruct<XrCompositionLayerQuad>(XR_TYPE_COMPOSITION_LAYER_QUAD);
	XrCompositionLayerProjection projection = xrStruct<XrCompositionLayerProjection>(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
	XrCompositionLayerProjectionView projectionViews[kEyeCount];

	if (mode == kSubmitStereo) {
		for (int e = 0; e < kEyeCount; e++) {
			projectionViews[e] = xrStruct<XrCompositionLayerProjectionView>(XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW);
			projectionViews[e].pose = c.views[e].pose;
			projectionViews[e].fov = c.views[e].fov;
			projectionViews[e].subImage.swapchain = c.eyeChain[e];
			projectionViews[e].subImage.imageRect.extent.width = c.eyeW;
			projectionViews[e].subImage.imageRect.extent.height = c.eyeH;
		}
		projection.space = c.space;
		projection.viewCount = kEyeCount;
		projection.views = projectionViews;
		layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&projection;
	} else if (mode == kSubmitTheatre) {
		const int w = theatre.width(), h = theatre.height();
		if (c.swapchain == XR_NULL_HANDLE || c.chainW != w || c.chainH != h) {
			if (!createSwapchain(c, w, h, c.swapchain, c.images, c.imageCount))
				return false;
			c.chainW = w;
			c.chainH = h;
		}

		uint32_t index;
		XrSwapchainImageAcquireInfo acquireInfo = xrStruct<XrSwapchainImageAcquireInfo>(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO);
		XR_TRY(xrAcquireSwapchainImage(c.swapchain, &acquireInfo, &index));
		XrSwapchainImageWaitInfo waitInfo = xrStruct<XrSwapchainImageWaitInfo>(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
		waitInfo.timeout = XR_INFINITE_DURATION;
		XR_TRY(xrWaitSwapchainImage(c.swapchain, &waitInfo));

		{
			GLStateSaver saver(gl);
			gl.bindFramebuffer(GL_READ_FRAMEBUFFER, theatre.framebuffer());
			gl.bindFramebuffer(GL_DRAW_FRAMEBUFFER, c.chainFBO);
			gl.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c.images[index].image, 0);
			gl.blitFramebuffer(0, 0, w, h, 0, 0, c.chainW, c.chainH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			gl.framebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
			gl.finish();
		}

		XrSwapchainImageReleaseInfo releaseInfo = xrStruct<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
		XR_TRY(xrReleaseSwapchainImage(c.swapchain, &releaseInfo));

		quad.space = c.space;
		quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
		quad.subImage.swapchain = c.swapchain;
		quad.subImage.imageRect.extent.width = c.chainW;
		quad.subImage.imageRect.extent.height = c.chainH;
		quad.pose = toXrPose(theatre.pose());
		quad.size.width = theatre.screenWidth();
		quad.size.height = theatre.screenHeight();
		layers[layerCount++] = (const XrCompositionLayerBaseHeader *)&quad;
	}

	XrFrameEndInfo endInfo = xrStruct<XrFrameEndInfo>(XR_TYPE_FRAME_END_INFO);
	endInfo.displayTime = c.frameState.predictedDisplayTime;
	endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endInfo.layerCount = layerCount;
	endInfo.layers = layers;
	XR_TRY(xrEndFrame(c.session, &endInfo));
	return true;
}

bool OpenXRBackend::prepareEyes() {
	if (_c.eyeChain[0] != XR_NULL_HANDLE)
		return true;

	for (int e = 0; e < kEyeCount; e++) {
		if (!createSwapchain(_c, _c.eyeW, _c.eyeH, _c.eyeChain[e], _c.eyeImages[e], _c.eyeImageCount[e]))
			return false;
	}
	return true;
}

bool OpenXRBackend::acquireEye(int eye, GLuint &texture) {
	Session &c = _c;

	uint32_t index;
	XrSwapchainImageAcquireInfo acquireInfo = xrStruct<XrSwapchainImageAcquireInfo>(XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO);
	if (!check(xrAcquireSwapchainImage(c.eyeChain[eye], &acquireInfo, &index), "xrAcquireSwapchainImage"))
		return false;
	XrSwapchainImageWaitInfo waitInfo = xrStruct<XrSwapchainImageWaitInfo>(XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO);
	waitInfo.timeout = XR_INFINITE_DURATION;
	if (!check(xrWaitSwapchainImage(c.eyeChain[eye], &waitInfo), "xrWaitSwapchainImage"))
		return false;

	texture = c.eyeImages[eye][index].image;
	return true;
}

bool OpenXRBackend::releaseEye(int eye) {
	XrSwapchainImageReleaseInfo releaseInfo = xrStruct<XrSwapchainImageReleaseInfo>(XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO);
	return check(xrReleaseSwapchainImage(_c.eyeChain[eye], &releaseInfo), "xrReleaseSwapchainImage");
}

void OpenXRBackend::shutdown() {
	Session &c = _c;

	// The GL context is still current
	if (c.gl && c.gl->deleteFramebuffers && c.chainFBO)
		c.gl->deleteFramebuffers(1, &c.chainFBO);
	c.chainFBO = 0;

	destroySwapchain(c.swapchain, c.images, c.imageCount);
	for (int e = 0; e < kEyeCount; e++)
		destroySwapchain(c.eyeChain[e], c.eyeImages[e], c.eyeImageCount[e]);
	for (int h = 0; h < kHandCount; h++) {
		if (c.aimSpace[h] != XR_NULL_HANDLE)
			xrDestroySpace(c.aimSpace[h]);
	}
	if (c.actionSet != XR_NULL_HANDLE)
		xrDestroyActionSet(c.actionSet);
	if (c.viewSpace != XR_NULL_HANDLE)
		xrDestroySpace(c.viewSpace);
	if (c.space != XR_NULL_HANDLE)
		xrDestroySpace(c.space);
	if (c.session != XR_NULL_HANDLE)
		xrDestroySession(c.session);
	if (c.instance != XR_NULL_HANDLE)
		xrDestroyInstance(c.instance);
	c = Session();
}

Backend *createOpenXRBackend() {
	return new OpenXRBackend();
}

} // End of namespace XR

#endif // USE_OPENXR
