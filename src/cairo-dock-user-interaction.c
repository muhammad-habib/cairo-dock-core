/**
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

#include "cairo-dock-animations.h"
#include "cairo-dock-icon-facility.h"
#include "cairo-dock-applications-manager.h"
#include "cairo-dock-application-facility.h"
#include "cairo-dock-launcher-manager.h"  // gldi_launcher_add_new
#include "cairo-dock-utils.h"  // cairo_dock_launch_command_full
#include "cairo-dock-stack-icon-manager.h"
#include "cairo-dock-separator-manager.h"
#include "cairo-dock-applet-manager.h"
#include "cairo-dock-class-icon-manager.h"
#include "cairo-dock-dock-facility.h"
#include "cairo-dock-desklet-factory.h"
#include "cairo-dock-dialog-factory.h"
#include "cairo-dock-themes-manager.h"  // cairo_dock_update_conf_file
#include "cairo-dock-file-manager.h"  // cairo_dock_copy_file
#include "cairo-dock-log.h"
#include "cairo-dock-keyfile-utilities.h"
#include "cairo-dock-dock-manager.h"
#include "cairo-dock-keybinder.h"
#include "cairo-dock-animations.h"
#include "cairo-dock-class-manager.h"
#include "cairo-dock-desktop-manager.h"
#include "cairo-dock-windows-manager.h"
#include "cairo-dock-gui-backend.h"
#include "cairo-dock-dbus.h"
#include "cairo-dock-user-interaction.h"

extern gboolean g_bLocked;
extern gchar *g_cConfFile;
extern gchar *g_cCurrentIconsPath;

static int _compare_zorder (Icon *icon1, Icon *icon2)  // classe par z-order decroissant.
{
	if (icon1->pAppli->iStackOrder < icon2->pAppli->iStackOrder)
		return -1;
	else if (icon1->pAppli->iStackOrder > icon2->pAppli->iStackOrder)
		return 1;
	else
		return 0;
}
static void _cairo_dock_hide_show_in_class_subdock (Icon *icon)
{
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
	// if the appli has the focus, we hide all the windows, else we show them all
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli != NULL && pIcon->pAppli == gldi_windows_get_active ())
		{
			break;
		}
	}
	
	if (ic != NULL)  // one of the windows of the appli has the focus -> hide.
	{
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli != NULL && ! pIcon->pAppli->bIsHidden)
			{
				gldi_window_minimize (pIcon->pAppli);
			}
		}
	}
	else  // on montre tout, dans l'ordre du z-order.
	{
		GList *pZOrderList = NULL;
		for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (pIcon->pAppli != NULL)
				pZOrderList = g_list_insert_sorted (pZOrderList, pIcon, (GCompareFunc) _compare_zorder);
		}
		
		int iNumDesktop, iViewPortX, iViewPortY;
		gldi_desktop_get_current (&iNumDesktop, &iViewPortX, &iViewPortY);
		
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (gldi_window_is_on_desktop (pIcon->pAppli, iNumDesktop, iViewPortX, iViewPortY))
				break;
		}
		if (pZOrderList && ic == NULL)  // no window on the current desktop -> take the first desktop
		{
			pIcon = pZOrderList->data;
			iNumDesktop = pIcon->pAppli->iNumDesktop;
			iViewPortX = pIcon->pAppli->iViewPortX;
			iViewPortY = pIcon->pAppli->iViewPortY;
		}
		
		for (ic = pZOrderList; ic != NULL; ic = ic->next)
		{
			pIcon = ic->data;
			if (gldi_window_is_on_desktop (pIcon->pAppli, iNumDesktop, iViewPortX, iViewPortY))
				gldi_window_show (pIcon->pAppli);
		}
		g_list_free (pZOrderList);
	}
}

static void _cairo_dock_show_prev_next_in_subdock (Icon *icon, gboolean bNext)
{
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
	GldiWindowActor *pActiveAppli = gldi_windows_get_active ();
	GList *ic;
	Icon *pIcon;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli == pActiveAppli)
			break;
	}
	if (ic == NULL)
		ic = icon->pSubDock->icons;
	
	GList *ic2 = ic;
	do
	{
		ic2 = (bNext ? cairo_dock_get_next_element (ic2, icon->pSubDock->icons) : cairo_dock_get_previous_element (ic2, icon->pSubDock->icons));
		pIcon = ic2->data;
		if (CAIRO_DOCK_IS_APPLI (pIcon))
		{
			gldi_window_show (pIcon->pAppli);
			break;
		}
	} while (ic2 != ic);
}

static void _cairo_dock_close_all_in_class_subdock (Icon *icon)
{
	if (icon->pSubDock == NULL || icon->pSubDock->icons == NULL)
		return;
	Icon *pIcon;
	GList *ic;
	for (ic = icon->pSubDock->icons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli != NULL)
		{
			gldi_window_close (pIcon->pAppli);
		}
	}
}

static void _show_all_windows (GList *pIcons)
{
	Icon *pIcon;
	GList *ic;
	for (ic = pIcons; ic != NULL; ic = ic->next)
	{
		pIcon = ic->data;
		if (pIcon->pAppli != NULL && pIcon->pAppli->bIsHidden)  // a window is hidden...
		{
			gldi_window_show (pIcon->pAppli);
		}
	}
}

static gboolean _launch_icon_command (Icon *icon, CairoDock *pDock, gboolean bForce)
{
	if (icon->cCommand == NULL)
		return GLDI_NOTIFICATION_LET_PASS;
	
	if (pDock->iRefCount != 0)  // let the applets handle their own sub-icons.
	{
		Icon *pMainIcon = cairo_dock_search_icon_pointing_on_dock (pDock, NULL);
		if (CAIRO_DOCK_IS_APPLET (pMainIcon))
			return GLDI_NOTIFICATION_LET_PASS;
	}

	// do not launch it twice (avoid wrong double click)
	// => if we want 2 apps, we have to use Shift + Click
	if (! bForce && icon->iSidOpeningTimeout != 0)
		return GLDI_NOTIFICATION_INTERCEPT;

	gboolean bSuccess = FALSE;
	if (*icon->cCommand == '<')  // shortkey
	{
		bSuccess = cairo_dock_trigger_shortkey (icon->cCommand);
		if (!bSuccess)
			bSuccess = cairo_dock_launch_command_with_opening_animation (icon);
	}
	else  // normal command
	{
		bSuccess = cairo_dock_launch_command_with_opening_animation (icon);
		if (! bSuccess)
			bSuccess = cairo_dock_trigger_shortkey (icon->cCommand);
	}
	if (! bSuccess)
	{
		gldi_icon_request_animation (icon, "blink", 1);  // 1 blink if fail.
	}
	return GLDI_NOTIFICATION_INTERCEPT;
}
gboolean cairo_dock_notification_click_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, GldiContainer *pContainer, guint iButtonState)
{
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	// shit/ctrl + click on an icon that is linked to a program => re-launch this program.
	if (iButtonState & (GDK_SHIFT_MASK | GDK_CONTROL_MASK))  // shit or ctrl + click
	{
		if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
		|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon))
		{
			return _launch_icon_command (icon, pDock, TRUE);
		}
		return GLDI_NOTIFICATION_LET_PASS;
	}
	
	// scale on an icon holding a class sub-dock.
	if (CAIRO_DOCK_IS_MULTI_APPLI(icon))
	{
		if (myTaskbarParam.bPresentClassOnClick // if we want to use this feature
		&& (!myDocksParam.bShowSubDockOnClick  // if sub-docks are shown on mouse over
			|| gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock)))  // or this sub-dock is already visible
		&& gldi_desktop_present_class (icon->cClass)) // we use the scale plugin if it's possible
		{
			_show_all_windows (icon->pSubDock->icons); // show all windows
			// in case the dock is visible or about to be visible, hide it, as it would confuse the user to have both.
			cairo_dock_emit_leave_signal (CAIRO_CONTAINER (icon->pSubDock));
			return GLDI_NOTIFICATION_INTERCEPT;
		}
	}
	
	// else handle sub-docks showing on click, applis and launchers (not applets).
	if (icon->pSubDock != NULL && (myDocksParam.bShowSubDockOnClick || !gldi_container_is_visible (CAIRO_CONTAINER (icon->pSubDock))))  // icon pointing to a sub-dock with either "sub-dock activation on click" option enabled, or sub-dock not visible -> open the sub-dock
	{
		cairo_dock_show_subdock (icon, pDock);
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon))  // icon holding an appli, but not being an applet -> show/hide the window.
	{
		GldiWindowActor *pAppli = icon->pAppli;
		if (gldi_windows_get_active () == pAppli && myTaskbarParam.bMinimizeOnClick && ! pAppli->bIsHidden && gldi_window_is_on_current_desktop (pAppli))  // ne marche que si le dock est une fenêtre de type 'dock', sinon il prend le focus.
			gldi_window_minimize (pAppli);
		else
			gldi_window_show (pAppli);
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon))  // icon holding a class sub-dock -> show/hide the windows of the class.
	{
		if (! myDocksParam.bShowSubDockOnClick)
		{
			_cairo_dock_hide_show_in_class_subdock (icon);
		}
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon))  // finally, launcher being none of the previous cases -> launch the command
	{
		return _launch_icon_command (icon, pDock, FALSE);
	}
	else
	{
		cd_debug ("no action here");
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_middle_click_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, GldiContainer *pContainer)
{
	if (icon == NULL || ! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	
	if (CAIRO_DOCK_IS_APPLI (icon) && ! CAIRO_DOCK_IS_APPLET (icon) && myTaskbarParam.iActionOnMiddleClick != 0)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case 1:  // close
				gldi_window_close (icon->pAppli);
			break;
			case 2:  // minimise
				if (! icon->pAppli->bIsHidden)
				{
					gldi_window_minimize (icon->pAppli);
				}
			break;
			case 3:  // launch new
				if (icon->cCommand != NULL)
				{
					gldi_object_notify (pDock, NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
				}
			break;
		}
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	else if (CAIRO_DOCK_IS_MULTI_APPLI (icon) && myTaskbarParam.iActionOnMiddleClick != 0)
	{
		switch (myTaskbarParam.iActionOnMiddleClick)
		{
			case 1:  // close
				_cairo_dock_close_all_in_class_subdock (icon);
			break;
			case 2:  // minimise
				_cairo_dock_hide_show_in_class_subdock (icon);
			break;
			case 3:  // launch new
				if (icon->cCommand != NULL)
				{
					gldi_object_notify (CAIRO_CONTAINER (pDock), NOTIFICATION_CLICK_ICON, icon, pDock, GDK_SHIFT_MASK);  // on emule un shift+clic gauche .
				}
			break;
		}
		
		return GLDI_NOTIFICATION_INTERCEPT;
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_scroll_icon (G_GNUC_UNUSED gpointer pUserData, Icon *icon, G_GNUC_UNUSED GldiContainer *pContainer, int iDirection)
{
	if (CAIRO_DOCK_IS_MULTI_APPLI (icon) || CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // on emule un alt+tab sur la liste des applis du sous-dock.
	{
		_cairo_dock_show_prev_next_in_subdock (icon, iDirection == GDK_SCROLL_DOWN);
	}
	else if (CAIRO_DOCK_IS_APPLI (icon) && icon->cClass != NULL)
	{
		Icon *pNextIcon = cairo_dock_get_prev_next_classmate_icon (icon, iDirection == GDK_SCROLL_DOWN);
		if (pNextIcon != NULL)
			gldi_window_show (pNextIcon->pAppli);
	}
	return GLDI_NOTIFICATION_LET_PASS;
}


gboolean cairo_dock_notification_drop_data (G_GNUC_UNUSED gpointer pUserData, const gchar *cReceivedData, Icon *icon, double fOrder, GldiContainer *pContainer)
{
	cd_debug ("take the drop");
	if (! CAIRO_DOCK_IS_DOCK (pContainer))
		return GLDI_NOTIFICATION_LET_PASS;
	
	CairoDock *pDock = CAIRO_DOCK (pContainer);
	CairoDock *pReceivingDock = pDock;
	if (g_str_has_suffix (cReceivedData, ".desktop"))  // .desktop -> add a new launcher if dropped on or amongst launchers. 
	{
		cd_debug (" dropped a .desktop");
		if (! myTaskbarParam.bMixLauncherAppli && CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon))
			return GLDI_NOTIFICATION_LET_PASS;
		cd_debug (" add it");
		if (fOrder == CAIRO_DOCK_LAST_ORDER && CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon) && icon->pSubDock != NULL)  // drop onto a container icon.
		{
			pReceivingDock = icon->pSubDock;  // -> add into the pointed sub-dock.
		}
	}
	else  // file.
	{
		if (icon != NULL && fOrder == CAIRO_DOCK_LAST_ORDER)  // dropped on an icon
		{
			if (CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (icon))  // sub-dock -> propagate to the sub-dock.
			{
				pReceivingDock = icon->pSubDock;
			}
			else if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_APPLI (icon)
			|| CAIRO_DOCK_ICON_TYPE_IS_CLASS_CONTAINER (icon)) // launcher/appli -> fire the command with this file.
			{
				if (icon->cCommand == NULL)
					return GLDI_NOTIFICATION_LET_PASS;
				gchar *cPath = NULL;
				if (strncmp (cReceivedData, "file://", 7) == 0)  // tous les programmes ne gerent pas les URI; pour parer au cas ou il ne le gererait pas, dans le cas d'un fichier local, on convertit en un chemin
				{
					cPath = g_filename_from_uri (cReceivedData, NULL, NULL);
				}
				gchar *cCommand = g_strdup_printf ("%s \"%s\"", icon->cCommand, cPath ? cPath : cReceivedData);
				cd_message ("will open the file with the command '%s'...", cCommand);
				g_spawn_command_line_async (cCommand, NULL);
				g_free (cPath);
				g_free (cCommand);
				gldi_icon_request_animation (icon, "blink", 2);
				return GLDI_NOTIFICATION_INTERCEPT;
			}
			else  // skip any other case.
			{
				return GLDI_NOTIFICATION_LET_PASS;
			}
		}  // else: dropped between 2 icons -> try to add it (for instance a script).
	}

	if (g_bLocked || myDocksParam.bLockAll)
		return GLDI_NOTIFICATION_LET_PASS;
	
	Icon *pNewIcon = gldi_launcher_add_new (cReceivedData, pReceivingDock, fOrder);
	
	return (pNewIcon ? GLDI_NOTIFICATION_INTERCEPT : GLDI_NOTIFICATION_LET_PASS);
}


void cairo_dock_set_custom_icon_on_appli (const gchar *cFilePath, Icon *icon, GldiContainer *pContainer)
{
	g_return_if_fail (CAIRO_DOCK_IS_APPLI (icon) && cFilePath != NULL);
	gchar *ext = strrchr (cFilePath, '.');
	if (!ext)
		return;
	cd_debug ("%s (%s - %s)", __func__, cFilePath, icon->cFileName);
	if ((strcmp (ext, ".png") == 0 || strcmp (ext, ".svg") == 0) && !myDocksParam.bLockAll) // && ! myDocksParam.bLockIcons) // or if we have to hide the option...
	{
		if (!myTaskbarParam.bOverWriteXIcons)
		{
			myTaskbarParam.bOverWriteXIcons = TRUE;
			cairo_dock_update_conf_file (g_cConfFile,
				G_TYPE_BOOLEAN, "TaskBar", "overwrite xicon", myTaskbarParam.bOverWriteXIcons,
				G_TYPE_INVALID);
			gldi_dialog_show_temporary_with_default_icon (_("The option 'overwrite X icons' has been automatically enabled in the config.\nIt is located in the 'Taskbar' module."), icon, pContainer, 6000);
		}
		
		gchar *cPath = NULL;
		if (strncmp (cFilePath, "file://", 7) == 0)
		{
			cPath = g_filename_from_uri (cFilePath, NULL, NULL);
		}
		
		const gchar *cClassIcon = cairo_dock_get_class_icon (icon->cClass);
		if (cClassIcon == NULL)
			cClassIcon = icon->cClass;
		
		gchar *cDestPath = g_strdup_printf ("%s/%s%s", g_cCurrentIconsPath, cClassIcon, ext);
		cairo_dock_copy_file (cPath?cPath:cFilePath, cDestPath);
		g_free (cDestPath);
		g_free (cPath);
		
		cairo_dock_reload_icon_image (icon, pContainer);
		cairo_dock_redraw_icon (icon);
	}
}


gboolean cairo_dock_notification_configure_desklet (G_GNUC_UNUSED gpointer pUserData, CairoDesklet *pDesklet)
{
	//g_print ("desklet %s configured\n", pDesklet->pIcon?pDesklet->pIcon->cName:"unknown");
	cairo_dock_gui_update_desklet_params (pDesklet);
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_moved (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s moved\n", pIcon?pIcon->cName:"unknown");
	
	if (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| (CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_inserted (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s inserted (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return GLDI_NOTIFICATION_LET_PASS;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_icon_removed (G_GNUC_UNUSED gpointer pUserData, Icon *pIcon, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("icon %s removed (%.2f)\n", pIcon?pIcon->cName:"unknown", pIcon->fInsertRemoveFactor);
	//if (pIcon->fInsertRemoveFactor == 0)
	//	return GLDI_NOTIFICATION_LET_PASS;
	
	if ( ( (CAIRO_DOCK_ICON_TYPE_IS_LAUNCHER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_CONTAINER (pIcon)
	|| CAIRO_DOCK_ICON_TYPE_IS_SEPARATOR (pIcon)) && pIcon->cDesktopFileName)
	|| CAIRO_DOCK_ICON_TYPE_IS_APPLET (pIcon))
		cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_desklet_added_removed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED CairoDesklet *pDesklet)
{
	//Icon *pIcon = pDesklet->pIcon;
	//g_print ("desklet %s removed\n", pIcon?pIcon->cName:"unknown");
	
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_dock_destroyed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED CairoDock *pDock)
{
	//g_print ("dock destroyed\n");
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_activated (G_GNUC_UNUSED gpointer pUserData, const gchar *cModuleName, G_GNUC_UNUSED gboolean bActivated)
{
	//g_print ("module %s (de)activated (%d)\n", cModuleName, bActivated);
	cairo_dock_gui_trigger_update_module_state (cModuleName);
	
	cairo_dock_gui_trigger_reload_items ();  // for plug-ins that don't have an applet, like Cairo-Pinguin.
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_registered (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED const gchar *cModuleName, G_GNUC_UNUSED gboolean bRegistered)
{
	//g_print ("module %s (un)registered (%d)\n", cModuleName, bRegistered);
	cairo_dock_gui_trigger_update_modules_list ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_module_detached (G_GNUC_UNUSED gpointer pUserData, GldiModuleInstance *pInstance, gboolean bIsDetached)
{
	//g_print ("module %s (de)tached (%d)\n", pInstance->pModule->pVisitCard->cModuleName, bIsDetached);
	cairo_dock_gui_trigger_update_module_container (pInstance, bIsDetached);
	
	cairo_dock_gui_trigger_reload_items ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}

gboolean cairo_dock_notification_shortkey_added_removed_changed (G_GNUC_UNUSED gpointer pUserData, G_GNUC_UNUSED GldiShortkey *pShortkey)
{
	cairo_dock_gui_trigger_reload_shortkeys ();
	
	return GLDI_NOTIFICATION_LET_PASS;
}
