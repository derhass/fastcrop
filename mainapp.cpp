#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "codec.h"
#include "controller.h"
#include "render.h"
#include "util.h"

#ifdef WITH_IMGUI
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"
#endif

#include "codec_stb_image.h"
#ifdef WITH_LIBJPEG
#include "codec_libjpeg.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int baseMods = (GLFW_MOD_CONTROL | GLFW_MOD_SHIFT | GLFW_MOD_ALT);

/****************************************************************************
 * DATA STRUCTURES                                                          *
 ****************************************************************************/

#define APP_TITLE "fastcrop"

/* OpenGL debug output error level */
typedef enum {
	DEBUG_OUTPUT_DISABLED=0,
	DEBUG_OUTPUT_ERRORS_ONLY,
	DEBUG_OUTPUT_ALL
} DebugOutputLevel;

/* AppConfig: application configuration, controllable via command line arguments*/
struct AppConfig {
	int posx;
	int posy;
	int width;
	int height;
	bool decorated;
	bool fullscreen;
	unsigned int frameCount;
	DebugOutputLevel debugOutputLevel;
	bool debugOutputSynchronous;
	float colorBackground[4];
	bool withGUI;

	AppConfig() :
		posx(100),
		posy(100),
		width(1920),
		height(1080),
		decorated(true),
		fullscreen(false),
		frameCount(0),
#ifdef NDEBUG
		debugOutputLevel(DEBUG_OUTPUT_DISABLED),
#else
		debugOutputLevel(DEBUG_OUTPUT_ERRORS_ONLY),
#endif
		debugOutputSynchronous(false),
		colorBackground{0.0f,0.0f,0.0f,0.0f},
#ifdef WITH_IMGUI
		withGUI(true)
#else
		withGUI(false)
#endif
	{
	}

};

/* MainApp: We encapsulate all of our application state in this struct.
 * We use a single instance of this object (in main), and set a pointer to
 * this as the user-defined pointer for GLFW windows. That way, we have access
 * to this data structure without ever using global variables.
 */
typedef struct TMainApp {
	/* the window and related state */
	GLFWwindow *win;
	AppConfig* cfg;
	int winWidth, winHeight;
	double winToPixel[2];
	unsigned int flags;

	/* timing */
	double timeCur, timeDelta;
	double avg_frametime;
	double avg_fps;
	unsigned int frame;

	/* input state */
	double mousePosWin[2];
	double mousePosPixel[2];
	int modifiers;

	// some gl limits
	int maxGlTextureSize;
	int maxGlSize;

	CCodecs     codecs;
	CCodecSettings codecSettings;
	CController controller;
	CRenderer renderer;

	TMainApp() :
		controller(codecs, codecSettings, codecSettings)
	{}
} MainApp;

/* flags */
#define APP_HAVE_GLFW	0x1	/* we have called glfwInit() and should terminate it */
#define APP_HAVE_GL	0x2	/* we have a valid GL context */
#define APP_HAVE_IMGUI	0x4	/* we have Dear ImGui initialized */

/****************************************************************************
 * SETTING UP THE GL STATE                                                  *
 ****************************************************************************/

/* debug callback of the GL */
static void APIENTRY
debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
			  GLsizei length, const GLchar *message, const GLvoid* userParam)
{
	/* we pass a pointer to our application config to the callback as userParam */
	const AppConfig *cfg=(const AppConfig*)userParam;

	switch(type) {
		case GL_DEBUG_TYPE_ERROR:
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
			if (cfg->debugOutputLevel >= DEBUG_OUTPUT_ERRORS_ONLY) {
				util::warn("GLDEBUG: %s %s %s [0x%x]: %s",
					util::translateDebugSourceEnum(source),
					util::translateDebugTypeEnum(type),
					util::translateDebugSeverityEnum(severity),
					id, message);
			}
			break;
		default:
			if (cfg->debugOutputLevel >= DEBUG_OUTPUT_ALL) {
				util::warn("GLDEBUG: %s %s %s [0x%x]: %s",
					util::translateDebugSourceEnum(source),
					util::translateDebugTypeEnum(type),
					util::translateDebugSeverityEnum(severity),
					id, message);
			}
	}
}

/* Initialize the global OpenGL state. This is called once after the context
 * is created. */
static void initGLState(MainApp* app, const AppConfig& cfg)
{
	util::printGLInfo();
	//listGLExtensions();

	if (cfg.debugOutputLevel > DEBUG_OUTPUT_DISABLED) {
		if (GLAD_GL_VERSION_4_3) {
			util::info("enabling GL debug output [via OpenGL >= 4.3]");
			glDebugMessageCallback(debugCallback,&cfg);
			glEnable(GL_DEBUG_OUTPUT);
			if (cfg.debugOutputSynchronous) {
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			} else {
				glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			}
		} else if (GLAD_GL_KHR_debug) {
			util::info("enabling GL debug output [via GL_KHR_debug]");
			glDebugMessageCallback(debugCallback,&cfg);
			glEnable(GL_DEBUG_OUTPUT);
			if (cfg.debugOutputSynchronous) {
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			} else {
				glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			}
		} else if (GLAD_GL_ARB_debug_output) {
			util::info("enabling GL debug output [via GL_ARB_debug_output]");
			glDebugMessageCallbackARB(debugCallback,&cfg);
			if (cfg.debugOutputSynchronous) {
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
			} else {
				glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
			}
		} else {
			util::warn("GL debug output requested, but not supported by the context");
		}
	}

	glDepthFunc(GL_LESS);
	glClearDepth(1.0f);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	app->maxGlTextureSize = 4096;
	GLint maxViewport[2] = { 4096, 4096};
	GLint maxFB[2] = {4096, 4096};
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &app->maxGlTextureSize);
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxViewport);
	glGetIntegerv(GL_MAX_FRAMEBUFFER_WIDTH, &maxFB[0]);
	glGetIntegerv(GL_MAX_FRAMEBUFFER_HEIGHT, &maxFB[1]);
	util::info("GL limits: tex size: %d, viewport: %dx%d, framebuffer: %dx%d",
			app->maxGlTextureSize, maxViewport[0], maxViewport[1], maxFB[0], maxFB[1]);
	app->maxGlSize = app->maxGlTextureSize;
	if (maxViewport[0] < app->maxGlSize) {
		app->maxGlSize = maxViewport[0];
	}
	if (maxViewport[1] < app->maxGlSize) {
		app->maxGlSize = maxViewport[1];
	}
	if (maxFB[0] < app->maxGlSize) {
		app->maxGlSize = maxFB[0];
	}
	if (maxFB[1] < app->maxGlSize) {
		app->maxGlSize = maxFB[1];
	}
	util::info("GL limits: tex size: %d, viewport: %dx%d, framebuffer: %dx%d, using limt: %d",
			app->maxGlTextureSize, maxViewport[0], maxViewport[1], maxFB[0], maxFB[1], app->maxGlSize);

	if (!app->controller.initGL()) {
		util::warn("GL controller failed to initialize");
	}
	if (!app->renderer.initGL()) {
		util::warn("GL renderer failed to initialize");
	}
	app->renderer.invalidateImageState();
}

/****************************************************************************
 * WINDOW-RELATED CALLBACKS                                                 *
 ****************************************************************************/

static bool isOurInput(MainApp *app, bool mouse)
{
	bool inputAllowed = true;

#ifdef WITH_IMGUI
	if (app->cfg->withGUI) {
		ImGuiIO& io = ImGui::GetIO();
		if (mouse) {
			if (io.WantCaptureMouse) {
				inputAllowed = false;
			}
		} else {
			if (io.WantCaptureKeyboard) {
				inputAllowed = false;
			}
		}
	}
#endif
	return inputAllowed;
}


/* This function is registered as the framebuffer size callback for GLFW,
 * so GLFW will call this whenever the window is resized. */
static void callback_Resize(GLFWwindow *win, int w, int h)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	util::info("new framebuffer size: %dx%d pixels",w,h);

	/* store curent size for later use in the main loop */
	const TWindowState ws = app->controller.getWindowState();
	if (w != ws.dims[0] || h != ws.dims[1]) {
		app->controller.setWindowSize(w,h);
		app->renderer.invalidateWindowState();
	}

	app->winToPixel[0] = (double)ws.dims[0] / (double)app->winWidth;
	app->winToPixel[1] = (double)ws.dims[1] / (double)app->winHeight;
}

/* This function is registered as the window size callback for GLFW,
 * so GLFW will call this whenever the window is resized. */
static void callback_WinResize(GLFWwindow *win, int w, int h)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	util::info("new window size: %dx%d units",w,h);

	/* store curent size for later use in the main loop */
	app->winWidth=w;
	app->winHeight=h;

	const TWindowState ws = app->controller.getWindowState();

	app->winToPixel[0] = (double)ws.dims[0] / (double)app->winWidth;
	app->winToPixel[1] = (double)ws.dims[1] / (double)app->winHeight;
}

/* This function is registered as the keyboard callback for GLFW, so GLFW
 * will call this whenever a key is pressed. */
static void callback_Keyboard(GLFWwindow *win, int key, int scancode, int action, int mods)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	if (isOurInput(app, false)) {
		AppConfig *cfg = app->cfg;
		(void)cfg;
		if (action == GLFW_PRESS) {
			switch(key) {
				case GLFW_KEY_ESCAPE:
					glfwSetWindowShouldClose(win, 1);
					break;
				case GLFW_KEY_1:
					app->controller.setCropAspect(1.0f, 1.0f);
					app->renderer.invalidateCropState();
					break;
				case GLFW_KEY_2:
					app->controller.setCropAspect(2.0f, 3.0f);
					app->renderer.invalidateCropState();
					break;
				case GLFW_KEY_3:
					app->controller.setCropAspect(3.0f, 2.0f);
					app->renderer.invalidateCropState();
					break;
				case GLFW_KEY_8:
					app->controller.setCropAspect(16.0f, 10.0f);
					app->renderer.invalidateCropState();
					break;
				case GLFW_KEY_9:
					app->controller.setCropAspect(16.0f, 9.0f);
					app->renderer.invalidateCropState();
					break;
				case GLFW_KEY_LEFT_CONTROL:
				case GLFW_KEY_RIGHT_CONTROL:
					app->modifiers |= GLFW_MOD_CONTROL;
					break;
				case GLFW_KEY_LEFT_SHIFT:
				case GLFW_KEY_RIGHT_SHIFT:
					app->modifiers |= GLFW_MOD_SHIFT;
					break;
				case GLFW_KEY_LEFT_ALT:
				case GLFW_KEY_RIGHT_ALT:
					app->modifiers |= GLFW_MOD_ALT;
					break;
			}
		} else if (action == GLFW_RELEASE) {
			switch(key) {
				case GLFW_KEY_LEFT_CONTROL:
				case GLFW_KEY_RIGHT_CONTROL:
					app->modifiers &= ~GLFW_MOD_CONTROL;
					break;
				case GLFW_KEY_LEFT_SHIFT:
				case GLFW_KEY_RIGHT_SHIFT:
					app->modifiers &= ~GLFW_MOD_SHIFT;
					break;
				case GLFW_KEY_LEFT_ALT:
				case GLFW_KEY_RIGHT_ALT:
					app->modifiers &= ~GLFW_MOD_ALT;
					break;
			}
		}
	}
}

/* registered mouse position callback */
static void callback_mouse_position(GLFWwindow* win, double xpos, double ypos)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	if (isOurInput(app, true)) {
		bool moved = ((xpos != app->mousePosWin[0]) || (ypos != app->mousePosWin[1]));
		if (moved) {
			app->mousePosWin[0] = xpos;
			app->mousePosWin[1] = ypos;
			app->mousePosPixel[0] = app->mousePosWin[0] * app->winToPixel[0];
			app->mousePosPixel[1] = app->mousePosWin[1] * app->winToPixel[1];

			if (glfwGetMouseButton(win,GLFW_MOUSE_BUTTON_LEFT)) {
				if (app->controller.doDragCrop(app->mousePosPixel)) {
					app->renderer.invalidateCropState();
				}
			}
		}
	}
}

/* registered mouse button callback */
static void callback_mouse_button(GLFWwindow* win, int button, int action, int mods)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	if (isOurInput(app, true)) {
		if (action == GLFW_PRESS) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				if (!(mods & baseMods)) {
					app->controller.beginDragCrop(app->mousePosPixel);
				}
			}
		} else if (action == GLFW_RELEASE) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				if (!(mods & baseMods)) {
					if (!app->controller.endDragCrop()) {
						// normal click
					}
				}

			}

		}
	}
}

/* registered scroll wheel callback */
static void callback_scroll(GLFWwindow *win, double x, double y)
{
	MainApp *app=(MainApp*)glfwGetWindowUserPointer(win);
	if (isOurInput(app, true)) {
		if (y > 0.1) {
			if (app->modifiers & GLFW_MOD_CONTROL) {
				app->controller.adjustZoom((float)(y * sqrt(2.0)));
				app->renderer.invalidateDisplayState();
			} else if (!(app->modifiers & baseMods)) {
				app->controller.adjustCropScale((float)(y * sqrt(2.0)));
				app->renderer.invalidateCropState();
			}
		} else if ( y < -0.1) {
			if (app->modifiers & GLFW_MOD_CONTROL) {
				app->controller.adjustZoom((float)(-1.0 / (sqrt(2.0) * y)));
				app->renderer.invalidateDisplayState();
			} else if (!(app->modifiers & baseMods)) {
				app->controller.adjustCropScale((float)(-1.0 / (sqrt(2.0) * y)));
				app->renderer.invalidateCropState();
			}
		}
	}
}

/****************************************************************************
 * GLOBAL INITIALIZATION AND CLEANUP                                        *
 ****************************************************************************/

/* Initialize the Application.
 * This will initialize the app object, create a windows and OpenGL context
 * (via GLFW), initialize the GL function pointers via GLEW and initialize
 * the cube.
 * Returns true if successfull or false if an error occured. */
bool initMainApp(MainApp *app, AppConfig& cfg)
{
	int w, h, x, y;
	bool debugCtx=(cfg.debugOutputLevel > DEBUG_OUTPUT_DISABLED);

	/* Initialize the app structure */
	app->win=NULL;
	app->cfg=&cfg;
	app->flags=0;
	app->avg_frametime=-1.0;
	app->avg_fps=-1.0;
	app->frame = 0;

	/* initialize GLFW library */
	util::info("initializing GLFW");
	if (!glfwInit()) {
		util::warn("Failed to initialze GLFW");
		return false;
	}

	app->flags |= APP_HAVE_GLFW;

	/* request a OpenGL 4.6 core profile context */
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, (debugCtx)?GL_TRUE:GL_FALSE);

	GLFWmonitor *monitor = NULL;
	x = cfg.posx;
	y = cfg.posy;
	w = cfg.width;
	h = cfg.height;

	if (cfg.fullscreen) {
		monitor = glfwGetPrimaryMonitor();
	}
	if (monitor) {
		glfwGetMonitorPos(monitor, &x, &y);
		const GLFWvidmode *v = glfwGetVideoMode(monitor);
		if (v) {
			w = v->width;
			h = v->height;
			util::info("Primary monitor: %dx%d @(%d,%d)", w, h, x, y);
		}
		else {
			util::warn("Failed to query current video mode!");
		}
	}

	if (!cfg.decorated) {
		glfwWindowHint(GLFW_DECORATED, GL_FALSE);
	}

	/* create the window and the gl context */
	util::info("creating window and OpenGL context");
	app->win=glfwCreateWindow(w, h, APP_TITLE, monitor, NULL);
	if (!app->win) {
		util::warn("failed to get window with OpenGL 4.5 core context");
		return false;
	}

	app->controller.setWindowSize(w,h);
	app->winWidth = w;
	app->winHeight = h;
	app->winToPixel[0] = 1.0f;
	app->winToPixel[1] = 1.0f;
	app->mousePosWin[0] = 0.0f;
	app->mousePosWin[1] = 0.0f;
	app->mousePosPixel[0] = 0.0f;
	app->mousePosPixel[1] = 0.0f;
	app->modifiers = 0;

	if (!monitor) {
		glfwSetWindowPos(app->win, x, y);
	}

	/* store a pointer to our application context in GLFW's window data.
	 * This allows us to access our data from within the callbacks */
	glfwSetWindowUserPointer(app->win, app);
	/* register our callbacks */
	glfwSetFramebufferSizeCallback(app->win, callback_Resize);
	glfwSetWindowSizeCallback(app->win, callback_WinResize);
	glfwSetKeyCallback(app->win, callback_Keyboard);
	glfwSetCursorPosCallback(app->win, callback_mouse_position);
	glfwSetMouseButtonCallback(app->win, callback_mouse_button);
	glfwSetScrollCallback(app->win, callback_scroll);

	/* make the context the current context (of the current thread) */
	glfwMakeContextCurrent(app->win);

	/* ask the driver to enable synchronizing the buffer swaps to the
	 * VBLANK of the display. Depending on the driver and the user's
	 * setting, this may have no effect. But we can try... */
	glfwSwapInterval(1);

	/* initialize glad,
	 * this will load all OpenGL function pointers
	 */
	util::info("initializing glad");
	if (!gladLoadGL(glfwGetProcAddress)) {
		util::warn("failed to intialize glad GL extension loader");
		return false;
	}

	if (!GLAD_GL_VERSION_4_5) {
		util::warn("failed to load at least GL 4.5 functions via GLAD");
		return false;
	}

	app->flags |= APP_HAVE_GL;

	if (cfg.withGUI) {
#ifdef WITH_IMGUI
		/* initialize imgui */
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = NULL;
		io.LogFilename = NULL;

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(app->win, true);
		ImGui_ImplOpenGL3_Init();
		app->flags |= APP_HAVE_IMGUI;
#else
		util::warn("GUI requested but Dear ImGui not compiled in!");
#endif
	}

	/* initialize the GL context */
	initGLState(app, cfg);

	/* initialize the timer */
	app->timeCur=glfwGetTime();

	return true;
}

/* Clean up: destroy everything the cube app still holds */
static void destroyMainApp(MainApp *app)
{
	if (app->flags & APP_HAVE_GLFW) {
		if (app->win) {
			if (app->flags & APP_HAVE_GL) {
				app->renderer.dropGL();
				app->controller.dropGL();
				/* shut down imgui */
#ifdef WITH_IMGUI
				if (app->flags & APP_HAVE_IMGUI) {
					ImGui_ImplOpenGL3_Shutdown();
					ImGui_ImplGlfw_Shutdown();
					ImGui::DestroyContext();				
				}
#endif
			}
			glfwDestroyWindow(app->win);
		}
		glfwTerminate();
	}
}

/****************************************************************************
 * DRAWING FUNCTION                                                         *
 ****************************************************************************/

/* This draws the complete scene for a single eye */
static void
drawScene(MainApp *app, AppConfig& cfg)
{
	/* set the viewport (might have changed since last iteration) */
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	const TWindowState& ws = app->controller.getWindowState();
	glViewport(0, 0, ws.dims[0], ws.dims[1]);

	glClearColor(cfg.colorBackground[0], cfg.colorBackground[1], cfg.colorBackground[2], cfg.colorBackground[3]);
	glClear(GL_COLOR_BUFFER_BIT); /* clear the buffers */

	const CImageEntity& cur = app->controller.getCurrent();
	app->renderer.render(cur, app->controller);


#ifdef WITH_IMGUI
	if (app->flags & APP_HAVE_IMGUI) {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		/*
		ImGui::SetNextWindowPos(ImVec2(widthOffset, heightOffset));
		ImGui::SetNextWindowSize(ImVec2(newWidth, newHeight));
		*/
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
#endif
}


/* The main drawing function. This is responsible for drawing the next frame,
 * it is called in a loop as long as the application runs */
static bool
displayFunc(MainApp *app, AppConfig& cfg)
{
	// Render an animation frame
	drawScene(app, cfg);

	/* finished with drawing, swap FRONT and BACK buffers to show what we
	 * have rendered */
	glfwSwapBuffers(app->win);

	/* In DEBUG builds, we also check for GL errors in the display
	 * function, to make sure no GL error goes unnoticed. */
	GL_ERROR_DBG("display function");
	return true;
}

/****************************************************************************
 * MAIN LOOP                                                                *
 ****************************************************************************/

/* The main loop of the application. This will call the display function
 *  until the application is closed. This function also keeps timing
 *  statistics. */
static void mainLoop(MainApp *app, AppConfig& cfg)
{
	unsigned int frame=0;
	double start_time=glfwGetTime();
	double last_time=start_time;

	util::info("entering main loop");
	while (!glfwWindowShouldClose(app->win)) {
		/* update the current time and time delta to last frame */
		double now=glfwGetTime();
		app->timeDelta = now - app->timeCur;
		app->timeCur = now;

		/* update FPS estimate at most once every second */
		double elapsed = app->timeCur - last_time;
		if (elapsed >= 1.0) {
			char WinTitle[80];
			app->avg_frametime=1000.0 * elapsed/(double)frame;
			app->avg_fps=(double)frame/elapsed;
			last_time=app->timeCur;
			frame=0;
			/* update window title */
			mysnprintf(WinTitle, sizeof(WinTitle), APP_TITLE "   /// AVG: %4.2fms/frame (%.1ffps)", app->avg_frametime, app->avg_fps);
			glfwSetWindowTitle(app->win, WinTitle);
			util::info("frame time: %4.2fms/frame (%.1ffps)",app->avg_frametime, app->avg_fps);
		}

		/* This is needed for GLFW event handling. This function
		 * will call the registered callback functions to forward
		 * the events to us. */
		glfwPollEvents();

		/* call the display function */
		if (!displayFunc(app, cfg)) {
			break;
		}
		app->frame++;
		frame++;
		if (cfg.frameCount && app->frame >= cfg.frameCount) {
			break;
		}
	}
	util::info("left main loop\n%u frames rendered in %.1fs seconds == %.1ffps",
		app->frame,(app->timeCur-start_time),
		(double)app->frame/(app->timeCur-start_time) );
}

/****************************************************************************
 * SIMPLE COMMAND LINE PARSER                                               *
 ****************************************************************************/

void parseCommandlineArgs(AppConfig& cfg, MainApp& app, int argc, char**argv)
{
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--fullscreen")) {
			cfg.fullscreen = true;
			cfg.decorated = false;
		} else if (!strcmp(argv[i], "--undecorated")) {
			cfg.decorated = false;
		} else if (!strcmp(argv[i], "--gl-debug-sync")) {
			cfg.debugOutputSynchronous = true;
		} else if (!strcmp(argv[i], "--no-gui")) {
			cfg.withGUI = false;
		} else if (!strcmp(argv[i], "--with-gui")) {
			cfg.withGUI = true;
		} else {
			bool unhandled = false;
			if (i + 1 < argc) {
				if (!strcmp(argv[i], "--width")) {
					cfg.width = (int)strtol(argv[++i], NULL, 10);
				} else if (!strcmp(argv[i], "--height")) {
					cfg.height = (int)strtol(argv[++i], NULL, 10);
				} else if (!strcmp(argv[i], "--x")) {
					cfg.posx = (int)strtol(argv[++i], NULL, 10);
				} else if (!strcmp(argv[i], "--y")) {
					cfg.posy = (int)strtol(argv[++i], NULL, 10);
				} else if (!strcmp(argv[i], "--frameCount")) {
					cfg.frameCount = (unsigned)strtoul(argv[++i], NULL, 10);
				} else if (!strcmp(argv[i], "--gl-debug-level")) {
					cfg.debugOutputLevel = (DebugOutputLevel)strtoul(argv[++i], NULL, 10);
				} else {
					unhandled = true;
				}
			} else {
				unhandled = true;
			}
			if (unhandled) {
				// TODO: add as file/dir
			}
		}
	}
}

/****************************************************************************
 * PROGRAM ENTRY POINT                                                      *
 ****************************************************************************/

#ifdef WIN32
#define REALMAIN main_utf8
#else
#define REALMAIN main
#endif

int REALMAIN (int argc, char **argv)
{
	AppConfig cfg;	/* the generic configuration */
	MainApp app;	/* the cube application stata stucture */
#ifdef WITH_IMGUI
	/*
	filedialog::CFileDialogTracks fileDialog(app.animCtrl);
	filedialog::CFileDialogSelectDir dirDialog;
	*/
#endif

#ifdef WITH_LIBJPEG
	app.codecs.registerCodec(codecLibjpeg);
#endif
	app.codecs.registerCodec(codecSTBImageLoad);
	app.codecs.registerCodec(codecSTBImageWrite);

	parseCommandlineArgs(cfg, app, argc, argv);

	/// TODO XXX XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx
	app.controller.addFile("test.jpg");

	if (initMainApp(&app, cfg)) {
#ifdef WITH_IMGUI
		/*
		app.fileDialog = &fileDialog;
		app.dirDialog = &dirDialog;
		*/
#endif
		/* initialization succeeded, enter the main loop */
		mainLoop(&app, cfg);
	}
	/* clean everything up */
	destroyMainApp(&app);

	return 0;
}

#ifdef WIN32
int wmain(int argc, wchar_t **argv)
{
	std::vector<std::string> args_utf8;
	std::vector<char*> argv_utf8;
	int i;

	for (i = 0; i < argc; i++) {
		args_utf8.push_back(util::wideToUtf8(std::wstring(argv[i])));
	}
	for (i = 0; i < argc; i++) {
		argv_utf8.push_back((char*)args_utf8[i].c_str());
	}
	return REALMAIN(argc, argv_utf8.data());
}
#endif // WIN32
