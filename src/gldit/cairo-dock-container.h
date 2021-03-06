/*
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __CAIRO_DOCK_CONTAINER__
#define  __CAIRO_DOCK_CONTAINER__

#include "gldi-config.h"
#include <glib.h>
#ifdef HAVE_GLX
#include <GL/glx.h>  // GLXContext
#endif
#ifdef HAVE_EGL
#include <EGL/egl.h>  // EGLContext, EGLSurface
#endif
#include "cairo-dock-struct.h"
#include "cairo-dock-manager.h"

G_BEGIN_DECLS

/**
*@file cairo-dock-container.h This class defines the Containers, that are classic or hardware accelerated animated windows, and exposes common functions, such as redrawing a part of a container or popping a menu on a container.
*
* A Container is a rectangular on-screen located surface, has the notion of orientation, can hold external datas, monitors the mouse position, and has its own animation loop.
*
* Docks, Desklets, Dialogs, and Flying-containers all derive from Containers.
*
*/

// manager
typedef struct _GldiContainersParam GldiContainersParam;
typedef struct _GldiContainerAttr GldiContainerAttr;

#ifndef _MANAGER_DEF_
extern GldiContainersParam myContainersParam;
extern GldiManager myContainersMgr;
extern GldiObjectManager myContainerObjectMgr;
#endif

#define CD_DOUBLE_CLICK_DELAY 250  // ms

// params
struct _GldiContainersParam{
	//gboolean bUseFakeTransparency;
	gint iGLAnimationDeltaT;
	gint iCairoAnimationDeltaT;
	};

struct _GldiContainerAttr {
	gboolean bNoOpengl;
};

/// signals
typedef enum {
	/// notification called when the menu is being built on a container. data : {Icon, GldiContainer, GtkMenu, gboolean*}
	NOTIFICATION_BUILD_CONTAINER_MENU = NB_NOTIFICATIONS_OBJECT,
	/// notification called when the menu is being built on an icon (possibly NULL). data : {Icon, GldiContainer, GtkMenu}
	NOTIFICATION_BUILD_ICON_MENU,
	/// notification called when use clicks on an icon data : {Icon, CairoDock, int}
	NOTIFICATION_CLICK_ICON,
	/// notification called when the user double-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_DOUBLE_CLICK_ICON,
	/// notification called when the user middle-clicks on an icon. data : {Icon, CairoDock}
	NOTIFICATION_MIDDLE_CLICK_ICON,
	/// notification called when the user scrolls on an icon. data : {Icon, CairoDock, int}
	NOTIFICATION_SCROLL_ICON,
	/// notification called when the mouse enters an icon. data : {Icon, CairoDock, gboolean*}
	NOTIFICATION_ENTER_ICON,
	/// notification called when the mouse enters a dock while dragging an object.
	NOTIFICATION_START_DRAG_DATA,
	/// notification called when something is dropped inside a container. data : {gchar*, Icon, double*, CairoDock}
	NOTIFICATION_DROP_DATA,
	/// notification called when the mouse has moved inside a container.
	NOTIFICATION_MOUSE_MOVED,
	/// notification called when a key is pressed in a container that has the focus.
	NOTIFICATION_KEY_PRESSED,
	/// notification called for the fast rendering loop on a container.
	NOTIFICATION_UPDATE,
	/// notification called for the slow rendering loop on a container.
	NOTIFICATION_UPDATE_SLOW,
	/// notification called when a container is rendered.
	NOTIFICATION_RENDER,
	NB_NOTIFICATIONS_CONTAINER
	} GldiContainerNotifications;


// factory
/// Main orientation of a container.
typedef enum {
	CAIRO_DOCK_VERTICAL = 0,
	CAIRO_DOCK_HORIZONTAL
	} CairoDockTypeHorizontality;

struct _GldiContainerInterface {
	gboolean (*animation_loop) (GldiContainer *pContainer);
	void (*setup_menu) (GldiContainer *pContainer, Icon *pIcon, GtkWidget *pMenu);
	void (*detach_icon) (GldiContainer *pContainer, Icon *pIcon);
	void (*insert_icon) (GldiContainer *pContainer, Icon *pIcon, gboolean bAnimateIcon);
	};

/// Definition of a Container, whom derive Dock, Desklet, Dialog and FlyingContainer. 
struct _GldiContainer {
	/// object.
	GldiObject object;
	/// External data.
	gpointer pDataSlot[CAIRO_DOCK_NB_DATA_SLOT];
	/// window of the container.
	GtkWidget *pWidget;
	/// size of the container.
	gint iWidth, iHeight;
	/// position of the container.
	gint iWindowPositionX, iWindowPositionY;
	/// TURE is the mouse is inside the container (including the possible sub-widgets).
	gboolean bInside;
	/// TRUE if the container is horizontal, FALSE if vertical
	CairoDockTypeHorizontality bIsHorizontal;
	/// TRUE if the container is oriented upwards, FALSE if downwards.
	gboolean bDirectionUp;
	/// Source ID of the animation loop.
	guint iSidGLAnimation;
	/// interval of time between 2 animation steps.
	gint iAnimationDeltaT;
	/// X position of the mouse in the container's system of reference.
	gint iMouseX;
	/// Y position of the mouse in the container's system of reference.
	gint iMouseY;
	/// zoom applied to the container's elements.
	gdouble fRatio;
	/// TRUE if the container has a reflection power.
	gboolean bUseReflect;
	#ifdef HAVE_GLX
	/// OpenGL context.
	GLXContext glContext;
	void *unused;  // keep ABI compatibility
	#elif defined(HAVE_EGL)
	/// OpenGL context.
	EGLContext glContext;
	/// EGL surface.
	EGLSurface eglSurface;
	#endif
	/// whether the GL context is an ortho or a perspective view.
	gboolean bPerspectiveView;
	/// TRUE if a slow animation is running.
	gboolean bKeepSlowAnimation;
	/// counter for the animation loop.
	gint iAnimationStep;
	GldiContainerInterface iface;
	
	gboolean bIgnoreNextReleaseEvent;
	gpointer reserved[4];
};


/// Definition of the Container backend. It defines some operations that should be, but are not, provided by GTK.
struct _GldiContainerManagerBackend {
	void (*reserve_space) (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);
	int (*get_current_desktop_index) (GldiContainer *pContainer);
	void (*move) (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY);
	gboolean (*is_active) (GldiContainer *pContainer);
	void (*present) (GldiContainer *pContainer);
};


/// Get the Container part of a pointer.
#define CAIRO_CONTAINER(p) ((GldiContainer *) (p))

/** Say if an object is a Container.
*@param obj the object.
*@return TRUE if the object is a Container.
*/
#define CAIRO_DOCK_IS_CONTAINER(obj) gldi_object_is_manager_child (GLDI_OBJECT(obj), &myContainerObjectMgr)

  /////////////
 // WINDOW //
///////////


void cairo_dock_set_containers_non_sticky (void);

void cairo_dock_disable_containers_opacity (void);

#define gldi_container_get_gdk_window(pContainer) gtk_widget_get_window ((pContainer)->pWidget)

#define gldi_container_get_Xid(pContainer) GDK_WINDOW_XID (gldi_container_get_gdk_window(pContainer))

#define gldi_container_is_visible(pContainer) gtk_widget_get_visible ((pContainer)->pWidget)

#define gldi_display_get_pointer(xptr, yptr) do {\
	GdkDeviceManager *_dm = gdk_display_get_device_manager (gdk_display_get_default());\
	GdkDevice *_dev = gdk_device_manager_get_client_pointer (_dm);\
	gdk_device_get_position (_dev, NULL, xptr, yptr); } while (0)

#define gldi_container_update_mouse_position(pContainer) do {\
	GdkDeviceManager *pManager = gdk_display_get_device_manager (gtk_widget_get_display (pContainer->pWidget)); \
	GdkDevice *pDevice = gdk_device_manager_get_client_pointer (pManager); \
	if ((pContainer)->bIsHorizontal) \
		gdk_window_get_device_position (gldi_container_get_gdk_window (pContainer), pDevice, &pContainer->iMouseX, &pContainer->iMouseY, NULL); \
	else \
		gdk_window_get_device_position (gldi_container_get_gdk_window (pContainer), pDevice, &pContainer->iMouseY, &pContainer->iMouseX, NULL); } while (0)


/** Reserve a space on the screen for a Container; other windows won't overlap this space when maximised.
*@param pContainer the container
*@param left 
*@param right 
*@param top 
*@param bottom 
*@param left_start_y 
*@param left_end_y 
*@param right_start_y 
*@param right_end_y 
*@param top_start_x 
*@param top_end_x 
*@param bottom_start_x 
*@param bottom_end_x 
*/
void gldi_container_reserve_space (GldiContainer *pContainer, int left, int right, int top, int bottom, int left_start_y, int left_end_y, int right_start_y, int right_end_y, int top_start_x, int top_end_x, int bottom_start_x, int bottom_end_x);

/** Get the desktop and viewports a Container is placed on.
*@param pContainer the container
*@return an index representing the desktop and viewports.
*/
int gldi_container_get_current_desktop_index (GldiContainer *pContainer);

/** Move a Container to a given desktop, viewport, and position (similar to gtk_window_move except that the position is defined on the whole desktop (made of all viewports); it's only useful if the Container is sticky).
*@param pContainer the container
*@param iNumDesktop desktop number
*@param iAbsolutePositionX horizontal position on the virtual screen
*@param iAbsolutePositionY vertical position on the virtual screen
*/
void gldi_container_move (GldiContainer *pContainer, int iNumDesktop, int iAbsolutePositionX, int iAbsolutePositionY);

/** Tell if a Container is the current active window (similar to gtk_window_is_active but actually works).
*@param pContainer the container
*@return TRUE if the Container is the current active window.
*/
gboolean gldi_container_is_active (GldiContainer *pContainer);

/** Show a Container and make it take the focus  (similar to gtk_window_present, but bypasses the WM focus steal prevention).
*@param pContainer the container
*/
void gldi_container_present (GldiContainer *pContainer);


void gldi_container_manager_register_backend (GldiContainerManagerBackend *pBackend);



  ////////////
 // REDRAW //
////////////

void cairo_dock_set_default_rgba_visual (GtkWidget *pWidget);

/** Clear and trigger the redraw of a Container.
*@param pContainer the Container to redraw.
*/
void cairo_dock_redraw_container (GldiContainer *pContainer);

/** Clear and trigger the redraw of a part of a container.
*@param pContainer the Container to redraw.
*@param pArea the zone to redraw.
*/
void cairo_dock_redraw_container_area (GldiContainer *pContainer, GdkRectangle *pArea);

/** Clear and trigger the redraw of an Icon. The drawing is not done immediately, but when the expose event is received.
*@param icon l'icone a retracer.
*/
void cairo_dock_redraw_icon (Icon *icon);


void cairo_dock_allow_widget_to_receive_data (GtkWidget *pWidget, GCallback pCallBack, gpointer data);

/** Enable a Container to accept drag-and-drops.
* @param pContainer a container.
* @param pCallBack the function that will be called when some data is received.
* @param data data passed to the callback.
*/
#define gldi_container_enable_drop(pContainer, pCallBack, data) cairo_dock_allow_widget_to_receive_data (pContainer->pWidget, pCallBack, data)

void gldi_container_disable_drop (GldiContainer *pContainer);

/** Notify everybody that a drop has just occured.
* @param cReceivedData the dropped data.
* @param pPointedIcon the icon which was pointed when the drop occured.
* @param fOrder the order of the icon if the drop occured on it, or LAST_ORDER if the drop occured between 2 icons.
* @param pContainer the container of the icon
*/
void gldi_container_notify_drop_data (GldiContainer *pContainer, gchar *cReceivedData, Icon *pPointedIcon, double fOrder);


gboolean cairo_dock_emit_signal_on_container (GldiContainer *pContainer, const gchar *cSignal);
gboolean cairo_dock_emit_leave_signal (GldiContainer *pContainer);
gboolean cairo_dock_emit_enter_signal (GldiContainer *pContainer);

/** Build the main menu of a Container.
*@param icon the icon that was left-clicked, or NULL if none.
*@param pContainer the container that was left-clicked.
*@return the menu.
*/
GtkWidget *gldi_container_build_menu (GldiContainer *pContainer, Icon *icon);


  /////////////////
 // INPUT SHAPE //
/////////////////

cairo_region_t *gldi_container_create_input_shape (GldiContainer *pContainer, int x, int y, int w, int h);

// Note: if gdkwindow->shape == NULL, setting a NULL shape will do nothing
#define gldi_container_set_input_shape(pContainer, pShape) \
gtk_widget_input_shape_combine_region ((pContainer)->pWidget, pShape)


void gldi_register_containers_manager (void);

G_END_DECLS
#endif
