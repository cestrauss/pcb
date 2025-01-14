/*!
 * \file src/hid/gtk3/gtk3-drc-window.c
 *
 * \brief Provides a DRC dialog window for the GTK3 UI.
 *
 * \note This file is copied from the PCB GTK-2 port and modified to
 * comply with GTK-3.
 *
 * \copyright (C) 2021 PCB Contributors.
 *
 * <hr>
 *
 * <h1><b>Copyright.</b></h1>\n
 *
 * PCB, interactive printed circuit board design
 * Copyright (C) 1994,1995,1996 Thomas Nau
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contact addresses for paper mail and Email:
 * Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 * Thomas.Nau@rz.uni-ulm.de
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "error.h"
#include "search.h"
#include "draw.h"
#include "drc/drc_object.h"
#include "drc/drc_violation.h"
#include "find.h"
#include "flags.h"
#include "object_list.h"
#include "pcb-printf.h"
#include "undo.h"
#include "set.h"
#include "gtk3-main.h"
#include "gtk3-drc-window.h"


#ifdef HAVE_LIBDMALLOC
#include <dmalloc.h>
#endif


#define VIOLATION_PIXMAP_PIXEL_SIZE 100
#define VIOLATION_PIXMAP_PIXEL_BORDER 5
#define VIOLATION_PIXMAP_PCB_SIZE MIL_TO_COORD (100)


static GtkWidget *drc_window;
static GtkWidget *drc_list;
static GtkListStore *drc_list_model = NULL;
static int num_violations = 0;


/*!
 * \brief Remember user window resizes.
 */
static gint
drc_window_configure_event_cb (GtkWidget *widget, GdkEventConfigure *ev, gpointer data)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);
  ghidgui->drc_window_width = allocation.width;
  ghidgui->drc_window_height = allocation.height;
  ghidgui->config_modified = TRUE;
  return FALSE;
}


static void
drc_close_cb (gpointer data)
{
  gtk_widget_destroy (drc_window);
  drc_window = NULL;
}


static void
drc_refresh_cb (gpointer data)
{
  hid_actionl ("DRC", NULL);
}


static void
drc_destroy_cb (GtkWidget * widget, gpointer data)
{
  drc_window = NULL;
}


enum
{
  DRC_VIOLATION_NUM_COL = 0,
  DRC_VIOLATION_OBJ_COL,
  NUM_DRC_COLUMNS
};


static void
selection_changed_cb (GtkTreeSelection *selection, gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GhidDrcViolation *gviolation;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    if (ClearFlagOnAllObjects (FOUNDFLAG, true))
    {
      IncrementUndoSerialNumber ();
      Draw ();
    }

    return;
  }

  /* Check the selected node has children, if so; return. */
  if (gtk_tree_model_iter_has_child (model, &iter))
    return;

  gtk_tree_model_get (model, &iter, DRC_VIOLATION_OBJ_COL, &gviolation, -1);

  ClearFlagOnAllObjects (FOUNDFLAG, true);

  if (gviolation == NULL)
    return;

  set_flag_on_violating_objects(gviolation->v, FOUNDFLAG);
  IncrementUndoSerialNumber ();
  Draw ();
  CenterDisplay (gviolation->v->x, gviolation->v->y, false);
}


static void
row_activated_cb (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkTreeIter iter;
  GhidDrcViolation *gviolation;

  gtk_tree_model_get_iter (model, &iter, path);

  gtk_tree_model_get (model, &iter, DRC_VIOLATION_OBJ_COL, &gviolation, -1);

  if (gviolation == NULL)
    return;

  CenterDisplay (gviolation->v->x, gviolation->v->y, true);
  gtk_window_present (GTK_WINDOW (gport->top_window));
}


enum
{
  PROP_TITLE = 1,
  PROP_EXPLANATION,
  PROP_X_COORD,
  PROP_Y_COORD,
  PROP_ANGLE,
  PROP_HAVE_MEASURED,
  PROP_MEASURED_VALUE,
  PROP_REQUIRED_VALUE,
  PROP_OBJECT_LIST,
  PROP_PIXMAP
};


static GObjectClass *ghid_drc_violation_parent_class = NULL;


/*!
 * \brief GObject finalise handler.
 *
 * Just before the GhidDrcViolation GObject is finalized, free our
 * allocated data, and then chain up to the parent's finalize handler.
 *
 * \param [in] widget  The GObject being finalized.
 */
static void
ghid_drc_violation_finalize (GObject * object)
{
  GhidDrcViolation *gviolation = GHID_DRC_VIOLATION (object);

  pcb_drc_violation_free(gviolation->v);
  
  if (gviolation->pixmap != NULL)
    g_object_unref (gviolation->pixmap);

  G_OBJECT_CLASS (ghid_drc_violation_parent_class)->finalize (object);
}


typedef struct ghid_drc_object_list
{
  int count;
  long int *id_list;
  int *type_list;
} ghid_drc_object_list;


/*!
 * \brief GObject property setter function.
 *
 * Setter function for GhidDrcViolation's GObject properties,
 * "settings-name" and "toplevel".
 *
 * \param [in]  object       The GObject whose properties we are setting.
 * \param [in]  property_id  The numeric id. under which the property was
 *                           registered with g_object_class_install_property().
 * \param [in]  value        The GValue the property is being set from.
 * \param [in]  pspec        A GParamSpec describing the property being set.
 */
static void
ghid_drc_violation_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
  GhidDrcViolation *gviolation = GHID_DRC_VIOLATION (object);
  object_list *obj_list;

  switch (property_id)
  {
    case PROP_TITLE:
      if (gviolation->v->title) g_free (gviolation->v->title);
      gviolation->v->title = g_value_dup_string (value);
      break;

    case PROP_EXPLANATION:
      if (gviolation->v->explanation) g_free (gviolation->v->explanation);
      gviolation->v->explanation = g_value_dup_string (value);
      break;

    case PROP_X_COORD:
      gviolation->v->x = g_value_get_int (value);
      break;

    case PROP_Y_COORD:
      gviolation->v->y = g_value_get_int (value);
      break;

    case PROP_ANGLE:
      gviolation->v->angle = g_value_get_double (value);
      break;

    case PROP_HAVE_MEASURED:
      gviolation->v->have_measured = g_value_get_boolean (value);
      break;

    case PROP_MEASURED_VALUE:
      gviolation->v->measured_value = g_value_get_int (value);
      break;

    case PROP_REQUIRED_VALUE:
      gviolation->v->required_value = g_value_get_int (value);
      break;

    case PROP_OBJECT_LIST:
      if (gviolation->v->objects) object_list_delete(gviolation->v->objects);
      obj_list = (object_list *)g_value_get_pointer (value);
      gviolation->v->objects = object_list_duplicate(obj_list);
      break;

    case PROP_PIXMAP:
      if (gviolation->pixmap)
        g_object_unref (gviolation->pixmap); /* Frees our old reference. */

      gviolation->pixmap = (GdkPixbuf *) g_value_dup_object (value); /* Takes a new reference. */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
  }
}


/*! \brief GObject property getter function
 *
 * Getter function for GhidDrcViolation's GObject properties,
 * "settings-name" and "toplevel".
 *
 *  \param [in]  object       The GObject whose properties we are getting
 *  \param [in]  property_id  The numeric id. under which the property was
 *                            registered with g_object_class_install_property()
 *  \param [out] value        The GValue in which to return the value of the property
 *  \param [in]  pspec        A GParamSpec describing the property being got
 */
static void
ghid_drc_violation_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }

}


/*!
 * \brief GType class initialiser for GhidDrcViolation.
 *
 * GType class initialiser for GhidDrcViolation. We override our parent
 * virtual class methods as needed and register our GObject properties.
 *
 * \param [in]  klass       The GhidDrcViolationClass we are initialising
 */
static void
ghid_drc_violation_class_init (GhidViolationRendererClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ghid_drc_violation_finalize;
  gobject_class->set_property = ghid_drc_violation_set_property;
  gobject_class->get_property = ghid_drc_violation_get_property;

  ghid_drc_violation_parent_class = (GObjectClass *)g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", "", "", "", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_EXPLANATION,
                                   g_param_spec_string ("explanation", "", "", "", G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_X_COORD,
                                   g_param_spec_int ("x-coord", "", "", G_MININT, G_MAXINT, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_Y_COORD,
                                   g_param_spec_int ("y-coord", "", "", G_MININT, G_MAXINT, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_ANGLE,
                                   g_param_spec_double ("angle", "", "", G_MININT, G_MAXINT, 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_HAVE_MEASURED,
                                   g_param_spec_boolean ("have-measured", "", "", 0, G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_MEASURED_VALUE,
                                   g_param_spec_int ("measured-value", "", "", G_MININT, G_MAXINT, 0., G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_REQUIRED_VALUE,
                                   g_param_spec_int ("required-value", "", "", G_MININT, G_MAXINT, 0., G_PARAM_WRITABLE));

  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_LIST,
                                   g_param_spec_pointer ("object-list", "", "", G_PARAM_WRITABLE));

  /*! \todo GDK_TYPE_DRAWABLE is deprecated in GTK3. */
  g_object_class_install_property (gobject_class,
                                   PROP_PIXMAP,
                                   g_param_spec_object ("pixmap", "", "", GDK_TYPE_PIXBUF, G_PARAM_WRITABLE));
}


/*!
 * \brief Function to retrieve GhidViolationRenderer's GType identifier.
 *
 * Function to retrieve GhidViolationRenderer's GType identifier.
 * Upon first call, this registers the GhidViolationRenderer in the GType system.
 * Subsequently it returns the saved value from its first execution.
 *
 * \return the GType identifier associated with GhidViolationRenderer.
 */
GType
ghid_drc_violation_get_type ()
{
  static GType ghid_drc_violation_type = 0;

  if (!ghid_drc_violation_type)
  {
    static const GTypeInfo ghid_drc_violation_info =
    {
      sizeof (GhidDrcViolationClass),
      NULL, /* base_init. */
      NULL, /* base_finalize. */
      (GClassInitFunc) ghid_drc_violation_class_init,
      NULL, /* class_finalize. */
      NULL, /* class_data. */
      sizeof (GhidDrcViolation),
      0, /* n_preallocs. */
      NULL, /* instance_init. */
    };

    ghid_drc_violation_type = g_type_register_static (G_TYPE_OBJECT, "GhidDrcViolation", &ghid_drc_violation_info, (GTypeFlags)0);
  }

  return ghid_drc_violation_type;
}


GhidDrcViolation *ghid_drc_violation_new (DrcViolationType *violation, GdkPixbuf *pixmap)
{

  GhidDrcViolation * gv = (GhidDrcViolation *) g_object_new (GHID_TYPE_DRC_VIOLATION, NULL);

  gv->v = (DrcViolationType*) malloc(sizeof(DrcViolationType));

  g_object_set (gv,
               "title",            violation->title,
               "explanation",      violation->explanation,
               "x-coord",          violation->x,
               "y-coord",          violation->y,
               "angle",            violation->angle,
               "have-measured",    violation->have_measured,
               "measured-value",   violation->measured_value,
               "required-value",   violation->required_value,
               "object-list",      violation->objects,
               "pixmap",           pixmap,
               NULL);

  return gv;
}


enum
{
  PROP_VIOLATION = 1,
};


static GObjectClass *ghid_violation_renderer_parent_class = NULL;


/*!
 * \brief GObject finalise handler.
 *
 * Just before the GhidViolationRenderer GObject is finalized, free our
 * allocated data, and then chain up to the parent's finalize handler.
 *
 * \param [in] widget  The GObject being finalized.
 */
static void
ghid_violation_renderer_finalize (GObject * object)
{
  GhidViolationRenderer *renderer = GHID_VIOLATION_RENDERER (object);

  if (renderer->violation != NULL)
    g_object_unref (renderer->violation);

  G_OBJECT_CLASS (ghid_violation_renderer_parent_class)->finalize (object);
}


/*!
 * \brief GObject property setter function.
 *
 * Setter function for GhidViolationRenderer's GObject properties,
 * "settings-name" and "toplevel".
 *
 * \param [in]  object       The GObject whose properties we are setting.
 * \param [in]  property_id  The numeric id. under which the property was
 *                           registered with g_object_class_install_property().
 * \param [in]  value        The GValue the property is being set from.
 * \param [in]  pspec        A GParamSpec describing the property being set.
 */
static void
ghid_violation_renderer_set_property (GObject * object, guint property_id, const GValue * value, GParamSpec * pspec)
{
  GhidViolationRenderer *renderer = GHID_VIOLATION_RENDERER (object);
  char *markup;

  switch (property_id)
  {
    case PROP_VIOLATION:
      if (renderer->violation != NULL)
        g_object_unref (renderer->violation);

      renderer->violation = (GhidDrcViolation *)g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
  }

  if (renderer->violation == NULL)
    return;

  if (renderer->violation->v->have_measured)
  {
    markup = pcb_g_strdup_printf (_("%m+<b>%s (%$mS)</b>\n"
                                    "<span size='1024'> </span>\n"
                                    "<small>"
                                    "<i>%s</i>\n"
                                    "<span size='5120'> </span>\n"
                                    "Required: %$mS"
                                    "</small>"),
                                    Settings.grid_unit->allow,
                                    renderer->violation->v->title,
                                    renderer->violation->v->measured_value,
                                    renderer->violation->v->explanation,
                                    renderer->violation->v->required_value);
  }
  else
  {
    markup = pcb_g_strdup_printf (_("%m+<b>%s</b>\n"
                                    "<span size='1024'> </span>\n"
                                    "<small>"
                                    "<i>%s</i>\n"
                                    "<span size='5120'> </span>\n"
                                    "Required: %$mS"
                                    "</small>"),
                                    Settings.grid_unit->allow,
                                    renderer->violation->v->title,
                                    renderer->violation->v->explanation,
                                    renderer->violation->v->required_value);
  }

  g_object_set (object, "markup", markup, NULL);
  g_free (markup);
}


/*!
 * \brief GObject property getter function.
 *
 * Getter function for GhidViolationRenderer's GObject properties.
 *
 * \param [in]  object       The GObject whose properties we are getting.
 * \param [in]  property_id  The numeric id. under which the property was
 *                           registered with g_object_class_install_property().
 * \param [out] value        The GValue in which to return the value of the property.
 * \param [in]  pspec        A GParamSpec describing the property being got.
 */
static void
ghid_violation_renderer_get_property (GObject * object, guint property_id, GValue * value, GParamSpec * pspec)
{
  switch (property_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }

}


static void
ghid_violation_renderer_get_size
 (GtkCellRenderer *cell,
  GtkWidget *widget,
  GdkRectangle *cell_area,
  gint *x_offset,
  gint *y_offset,
  gint *width,
  gint *height)
{
  GTK_CELL_RENDERER_CLASS (ghid_violation_renderer_parent_class)->get_size
   (cell,
    widget,
    cell_area,
    x_offset,
    y_offset,
    width,
    height);

  if (width != NULL)
    *width += VIOLATION_PIXMAP_PIXEL_SIZE;

  if (height != NULL)
    *height = MAX (*height, VIOLATION_PIXMAP_PIXEL_SIZE);
}


static void
ghid_violation_renderer_render
(
  GtkCellRenderer *cell,
  cairo_t *window, /*! \note Used to be a GdkDrawable. */
  GtkWidget *widget,
  const struct _cairo_rectangle_int *background_area, /*! \note Used to be a GdkRectangle. */
  const struct _cairo_rectangle_int *cell_area, /*! \note Used to be a GdkRectangle. */
  const struct _cairo_rectangle_int *expose_area, /*! \note Used to be a GdkRectangle. */
  GtkCellRendererState flags
)
{
  cairo_t *mydrawable; /*! \note Used to be a GdkDrawable. */

  /*! \todo gtk_widget_get_style is deprecated in GTK3. */
//  GtkStyle *style = gtk_widget_get_style (widget);

  GhidViolationRenderer *renderer = GHID_VIOLATION_RENDERER (cell);
  GhidDrcViolation *gviolation = renderer->violation;
  int pixmap_size = VIOLATION_PIXMAP_PIXEL_SIZE - 2 * VIOLATION_PIXMAP_PIXEL_BORDER;

/*! \todo Set width  of cell_area (this used to be a GdkRectangle. */
//  cell_area->width -= VIOLATION_PIXMAP_PIXEL_SIZE;

  GTK_CELL_RENDERER_CLASS (ghid_violation_renderer_parent_class)->render
   (cell,
    window, /*! \todo Casts a warning. */
    widget,
    background_area,
    cell_area,
    (GtkCellRendererState) expose_area, /*! \todo Casts an error. */
    flags);

  if (gviolation == NULL)
    return;

  if (gviolation->pixmap == NULL)
    {
      GdkPixbuf *pixmap = ghid_render_pixmap (gviolation->v->x,
                                              gviolation->v->y,
                                              VIOLATION_PIXMAP_PCB_SIZE / pixmap_size,
                                              pixmap_size,
                                              pixmap_size,
                                              gdk_visual_get_depth (window));

      g_object_set (gviolation, "pixmap", pixmap, NULL);
      g_object_unref (pixmap);
    }

  if (gviolation->pixmap == NULL)
    return;

  /*! \todo gdk_draw_drawable is deprecated in GTK3. */
//  mydrawable = GDK_DRAWABLE (gviolation->pixmap);

  /*! \todo gdk_draw_drawable is deprecated in GTK3. */
/*  gdk_draw_drawable (window,
                     style->fg_gc[gtk_widget_get_state (widget)],
                     mydrawable,
                     0, 0,
                     cell_area->x + cell_area->width + VIOLATION_PIXMAP_PIXEL_BORDER,
                     cell_area->y + VIOLATION_PIXMAP_PIXEL_BORDER,
                     -1, -1);
*/
}


/*!
 * \brief GType class initialiser for GhidViolationRenderer.
 *
 * GType class initialiser for GhidViolationRenderer. We override our parent
 * virtual class methods as needed and register our GObject properties.
 *
 * \param [in]  klass       The GhidViolationRendererClass we are initialising.
 */
static void
ghid_violation_renderer_class_init (GhidViolationRendererClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cellrenderer_class = GTK_CELL_RENDERER_CLASS (klass);

  gobject_class->finalize = ghid_violation_renderer_finalize;
  gobject_class->set_property = ghid_violation_renderer_set_property;
  gobject_class->get_property = ghid_violation_renderer_get_property;

  cellrenderer_class->get_size = ghid_violation_renderer_get_size;
  cellrenderer_class->render = ghid_violation_renderer_render;

  ghid_violation_renderer_parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  g_object_class_install_property (gobject_class,
    PROP_VIOLATION, g_param_spec_object ("violation", "", "", GHID_TYPE_DRC_VIOLATION, G_PARAM_WRITABLE));
}


/*!
 * \brief Function to retrieve GhidViolationRenderer's GType identifier.
 *
 * Function to retrieve GhidViolationRenderer's GType identifier.
 * Upon first call, this registers the GhidViolationRenderer in the GType system.
 * Subsequently it returns the saved value from its first execution.
 *
 * \return the GType identifier associated with GhidViolationRenderer.
 */
GType
ghid_violation_renderer_get_type ()
{
  static GType ghid_violation_renderer_type = 0;

  if (!ghid_violation_renderer_type)
  {
    static const GTypeInfo ghid_violation_renderer_info =
    {
      sizeof (GhidViolationRendererClass),
      NULL, /* base_init. */
      NULL, /* base_finalize. */
      (GClassInitFunc) ghid_violation_renderer_class_init,
      NULL, /* class_finalize. */
      NULL, /* class_data. */
      sizeof (GhidViolationRenderer),
      0, /* n_preallocs. */
      NULL, /* instance_init. */
    };

    ghid_violation_renderer_type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT, "GhidViolationRenderer", &ghid_violation_renderer_info, (GTypeFlags)0);
  }

  return ghid_violation_renderer_type;
}


/*!
 * \brief Convenience function to create a new violation renderer.
 *
 * Convenience function which creates a GhidViolationRenderer.
 *
 * \return  The GhidViolationRenderer created.
 */
GtkCellRenderer *
ghid_violation_renderer_new (void)
{
  GhidViolationRenderer *renderer;

  renderer = (GhidViolationRenderer *) g_object_new (GHID_TYPE_VIOLATION_RENDERER, "ypad", 6, NULL);

  return GTK_CELL_RENDERER (renderer);
}


void
ghid_drc_window_show (gboolean raise)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *scrolled_window;
  GtkCellRenderer *violation_renderer;

  if (drc_window)
  {
    if (raise)
      gtk_window_present(GTK_WINDOW(drc_window));

    return;
  }

  drc_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (drc_window), "destroy", G_CALLBACK (drc_destroy_cb), NULL);
  g_signal_connect (G_OBJECT (drc_window), "configure_event", G_CALLBACK (drc_window_configure_event_cb), NULL);
  gtk_window_set_title (GTK_WINDOW (drc_window), _("PCB DRC"));
  gtk_window_set_wmclass (GTK_WINDOW (drc_window), "PCB_DRC", "PCB");
  gtk_window_resize (GTK_WINDOW (drc_window), ghidgui->drc_window_width, ghidgui->drc_window_height);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (drc_window), vbox);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_set_spacing (GTK_BOX (vbox), 6);

  drc_list_model = gtk_list_store_new (NUM_DRC_COLUMNS,
                                       G_TYPE_INT,      /* DRC_VIOLATION_NUM_COL */
                                       G_TYPE_OBJECT);  /* DRC_VIOLATION_OBJ_COL */

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (vbox),
                      scrolled_window,
                      TRUE, /* EXPAND */
                      TRUE, /* FILL */
                      0 /* PADDING */);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  drc_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (drc_list_model));
  gtk_container_add (GTK_CONTAINER (scrolled_window), drc_list);

  gtk_widget_set_tooltip_text (drc_list,
                               "Single-click to locate the violation,\n"
                               "double-click to also warp the mouse\n"
                               "pointer there.");

  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (drc_list), TRUE);
  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (drc_list)), "changed", G_CALLBACK (selection_changed_cb), NULL);
  g_signal_connect (drc_list, "row-activated", G_CALLBACK (row_activated_cb), NULL);

  violation_renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (drc_list),
                                               -1, /* Aappend. */
                                               _("No."), /* Title. */
                                               violation_renderer,
                                               "text", DRC_VIOLATION_NUM_COL,
                                               NULL);

  violation_renderer = ghid_violation_renderer_new ();
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (drc_list),
                                               -1, /* Append. */
                                               _("Violation details"), /* Title. */
                                               violation_renderer,
                                               "violation", DRC_VIOLATION_OBJ_COL,
                                               NULL);

  hbox = gtk_hbutton_box_new ();
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  gtk_box_set_spacing (GTK_BOX (hbox), 6);

  button = gtk_button_new_from_stock (GTK_STOCK_REFRESH);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (drc_refresh_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (drc_close_cb), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  gtk_widget_realize (drc_window);

  if (Settings.AutoPlace)
    gtk_window_move (GTK_WINDOW (drc_window), 10, 10);

  gtk_widget_show_all (drc_window);
}


void ghid_drc_window_append_violation (DrcViolationType *violation)
{
  GhidDrcViolation *violation_obj;
  GtkTreeIter iter;

  /* Ensure the required structures are setup. */
  ghid_drc_window_show (FALSE);

  num_violations++;

  violation_obj = ghid_drc_violation_new (violation,
                                          NULL /* Pixmap. */);

  gtk_list_store_append (drc_list_model, &iter);
  gtk_list_store_set (drc_list_model, &iter,
                      DRC_VIOLATION_NUM_COL, num_violations,
                      DRC_VIOLATION_OBJ_COL, violation_obj,
                      -1);

  /* The list store takes its own reference. */
  g_object_unref (violation_obj);
}


void ghid_drc_window_reset_message (void)
{
  if (drc_list_model != NULL)
    gtk_list_store_clear (drc_list_model);
 
  num_violations = 0;
}


int ghid_drc_window_throw_dialog (void)
{
  ghid_drc_window_show (TRUE);
  return 1;
}
