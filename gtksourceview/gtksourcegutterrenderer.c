/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*-
 * gtksourcegutterrenderer.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2010 - Jesse van den Kieboom
 *
 * GtkSourceView is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * GtkSourceView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "gtksourcegutterrenderer.h"
#include "gtksourceview-marshal.h"
#include "gtksourceview-typebuiltins.h"
#include "gtksourceview-i18n.h"

#define GTK_SOURCE_GUTTER_RENDERER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_GUTTER_RENDERER, GtkSourceGutterRendererPrivate))

enum
{
	ACTIVATE,
	SIZE_CHANGED,
	QUEUE_DRAW,
	QUERY_TOOLTIP,
	QUERY_DATA,
	QUERY_ACTIVATABLE,
	NUM_SIGNALS
};

struct _GtkSourceGutterRendererPrivate
{
	GtkTextView *view;
	GtkTextWindowType window_type;

	gint xpad;
	gint ypad;

	gfloat xalign;
	gfloat yalign;

	gint fixed_width;
	gint fixed_height;
	GtkSourceGutterRendererAlignmentMode alignment_mode;

	guint visible : 1;
};

static guint signals[NUM_SIGNALS] = {0,};

G_DEFINE_ABSTRACT_TYPE (GtkSourceGutterRenderer,
                        gtk_source_gutter_renderer,
                        G_TYPE_INITIALLY_UNOWNED)

enum
{
	PROP_0,
	PROP_VISIBLE,
	PROP_XPAD,
	PROP_YPAD,
	PROP_XALIGN,
	PROP_YALIGN,
	PROP_VIEW,
	PROP_ALIGNMENT_MODE,
	PROP_WINDOW_TYPE,
	PROP_FIXED_WIDTH,
	PROP_FIXED_HEIGHT,
};

static void
gtk_source_gutter_renderer_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_gutter_renderer_parent_class)->finalize (object);
}

static gboolean
set_visible (GtkSourceGutterRenderer *renderer,
             gboolean                 visible)
{
	if (renderer->priv->visible == visible)
	{
		return FALSE;
	}

	renderer->priv->visible = visible;
	g_object_notify (G_OBJECT (renderer), "visible");

	gtk_source_gutter_renderer_queue_draw (renderer);

	return TRUE;
}


static gboolean
set_padding (GtkSourceGutterRenderer *renderer,
             gint                    *field,
             gint                     padding,
             const gchar             *name)
{
	if (*field == padding || padding < 0)
	{
		return FALSE;
	}

	*field = padding;
	g_object_notify (G_OBJECT (renderer), name);

	return TRUE;
}

static gboolean
set_xpad (GtkSourceGutterRenderer *renderer,
          gint                     xpad)
{
	return set_padding (renderer,
	                    &renderer->priv->xpad,
	                    xpad,
	                    "xpad");
}

static gboolean
set_ypad (GtkSourceGutterRenderer *renderer,
          gint                     ypad)
{
	return set_padding (renderer,
	                    &renderer->priv->ypad,
	                    ypad,
	                    "ypad");
}

static gboolean
set_alignment (GtkSourceGutterRenderer *renderer,
               gfloat                  *field,
               gfloat                   align,
               const gchar             *name,
               gboolean                 emit)
{
	if (*field == align || align < 0)
	{
		return FALSE;
	}

	*field = align;
	g_object_notify (G_OBJECT (renderer), name);

	if (emit)
	{
		gtk_source_gutter_renderer_queue_draw (renderer);
	}

	return TRUE;
}

static gboolean
set_xalign (GtkSourceGutterRenderer *renderer,
            gfloat                   xalign,
            gboolean                 emit)
{
	return set_alignment (renderer,
	                      &renderer->priv->xalign,
	                      xalign,
	                      "xalign",
	                      emit);
}

static gboolean
set_yalign (GtkSourceGutterRenderer *renderer,
            gfloat                   yalign,
            gboolean                 emit)
{
	return set_alignment (renderer,
	                      &renderer->priv->yalign,
	                      yalign,
	                      "yalign",
	                      emit);
}

static void
set_alignment_mode (GtkSourceGutterRenderer              *renderer,
                    GtkSourceGutterRendererAlignmentMode  mode)
{
	if (renderer->priv->alignment_mode == mode)
	{
		return;
	}

	renderer->priv->alignment_mode = mode;
	g_object_notify (G_OBJECT (renderer), "alignment-mode");

	gtk_source_gutter_renderer_queue_draw (renderer);
}

static gboolean
set_fixed_size (GtkSourceGutterRenderer *renderer,
                gint                    *field,
                const gchar             *name,
                gint                     value,
                gboolean                 emit)
{
	if (*field == value)
	{
		return FALSE;
	}

	*field = value;
	g_object_notify (G_OBJECT (renderer), name);

	if (emit)
	{
		gtk_source_gutter_renderer_size_changed (renderer);
	}

	return TRUE;
}

static void
gtk_source_gutter_renderer_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
	GtkSourceGutterRenderer *self = GTK_SOURCE_GUTTER_RENDERER (object);

	switch (prop_id)
	{
		case PROP_VISIBLE:
			set_visible (self, g_value_get_boolean (value));
			break;
		case PROP_XPAD:
			set_xpad (self, g_value_get_int (value));
			break;
		case PROP_YPAD:
			set_ypad (self, g_value_get_int (value));
			break;
		case PROP_XALIGN:
			set_xalign (self, g_value_get_float (value), TRUE);
			break;
		case PROP_YALIGN:
			set_yalign (self, g_value_get_float (value), TRUE);
			break;
		case PROP_ALIGNMENT_MODE:
			set_alignment_mode (self, g_value_get_enum (value));
			break;
		case PROP_VIEW:
			self->priv->view = g_value_get_object (value);
			break;
		case PROP_WINDOW_TYPE:
			self->priv->window_type = g_value_get_enum (value);
			break;
		case PROP_FIXED_WIDTH:
			set_fixed_size (self,
			                &self->priv->fixed_width,
			                "fixed-width",
			                g_value_get_int (value),
			                TRUE);
			break;
		case PROP_FIXED_HEIGHT:
			set_fixed_size (self,
			                &self->priv->fixed_height,
			                "fixed-height",
			                g_value_get_int (value),
			                TRUE);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
	GtkSourceGutterRenderer *self = GTK_SOURCE_GUTTER_RENDERER (object);

	switch (prop_id)
	{
		case PROP_VISIBLE:
			g_value_set_boolean (value, self->priv->visible);
			break;
		case PROP_XPAD:
			g_value_set_int (value, self->priv->xpad);
			break;
		case PROP_YPAD:
			g_value_set_int (value, self->priv->ypad);
			break;
		case PROP_XALIGN:
			g_value_set_float (value, self->priv->xalign);
			break;
		case PROP_YALIGN:
			g_value_set_float (value, self->priv->yalign);
			break;
		case PROP_VIEW:
			g_value_set_object (value, self->priv->view);
			break;
		case PROP_ALIGNMENT_MODE:
			g_value_set_enum (value, self->priv->alignment_mode);
			break;
		case PROP_WINDOW_TYPE:
			g_value_set_enum (value, self->priv->window_type);
			break;
		case PROP_FIXED_WIDTH:
			g_value_set_int (value, self->priv->fixed_width);
			break;
			break;
		case PROP_FIXED_HEIGHT:
			g_value_set_int (value, self->priv->fixed_height);
			break;
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
renderer_get_size_impl (GtkSourceGutterRenderer *renderer,
                        gint                    *width,
                        gint                    *height)
{
	if (width)
	{
		*width = renderer->priv->fixed_width;
	}

	if (height)
	{
		*height = renderer->priv->fixed_height;
	}
}

static void
gtk_source_gutter_renderer_class_init (GtkSourceGutterRendererClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_gutter_renderer_finalize;

	object_class->get_property = gtk_source_gutter_renderer_get_property;
	object_class->set_property = gtk_source_gutter_renderer_set_property;

	klass->get_size = renderer_get_size_impl;

	g_type_class_add_private (object_class, sizeof (GtkSourceGutterRendererPrivate));

	/**
	 * GtkSourceGutterRenderer:visible:
	 *
	 * The visibility of the renderer.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_VISIBLE,
	                                 g_param_spec_boolean ("visible",
	                                                       _("Visible"),
	                                                       _("Visible"),
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer:xpad:
	 *
	 * The x-padding of the renderer.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_XPAD,
	                                 g_param_spec_int ("xpad",
	                                                   _("X Padding"),
	                                                   _("The x-padding"),
	                                                   -1,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer:ypad:
	 *
	 * The y-padding of the renderer.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_YPAD,
	                                 g_param_spec_int ("ypad",
	                                                   _("Y Padding"),
	                                                   _("The y-padding"),
	                                                   -1,
	                                                   G_MAXINT,
	                                                   0,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer:xalign:
	 *
	 * The x-alignment of the renderer.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_XALIGN,
	                                 g_param_spec_float ("xalign",
	                                                     _("X Alignment"),
	                                                     _("The x-alignment"),
	                                                     -1,
	                                                     1,
	                                                     0,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer:yalign:
	 *
	 * The y-alignment of the renderer.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_YALIGN,
	                                 g_param_spec_float ("yalign",
	                                                     _("Y Alignment"),
	                                                     _("The y-alignment"),
	                                                     -1,
	                                                     1,
	                                                     0,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer::activate:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @iter: a #GtkTextIter
	 * @area: a #GdkRectangle
	 * @event: the event that caused the activation
	 *
	 * The ::activate signal is emitted when the renderer is
	 * activated.
	 *
	 */
	signals[ACTIVATE] =
		g_signal_new ("activate",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, activate),
		              NULL,
		              NULL,
		              _gtksourceview_marshal_VOID__BOXED_BOXED_BOXED,
		              G_TYPE_NONE,
		              3,
		              GTK_TYPE_TEXT_ITER,
		              GDK_TYPE_RECTANGLE,
		              GDK_TYPE_EVENT);

	/**
	 * GtkSourceGutterRenderer::size-changed:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 *
	 * The ::size-changed signal is emitted when the renderer has
	 * changed its size. Use #gtk_source_gutter_renderer_size_changed
	 * to emit this signal from an implementation of the
	 * #GtkSourceGutterRenderer interface.
	 *
	 */
	signals[SIZE_CHANGED] =
		g_signal_new ("size-changed",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, size_changed),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	/**
	 * GtkSourceGutterRenderer::queue-draw:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 *
	 * The ::queue-draw signal is emitted when the renderer needs
	 * to be redrawn. Use #gtk_source_gutter_renderer_queue_draw
	 * to emit this signal from an implementation of the
	 * #GtkSourceGutterRenderer interface.
	 *
	 */
	signals[QUEUE_DRAW] =
		g_signal_new ("queue-draw",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, queue_draw),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	/**
	 * GtkSourceGutterRenderer::query-tooltip:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @iter: a #GtkTextIter
	 * @area: a #GdkRectangle
	 * @x: the x position (in window coordinates)
	 * @y: the y position (in window coordinates)
	 * @tooltip: a #GtkTooltip
	 *
	 * The ::query-tooltip signal is emitted when the renderer can
	 * show a tooltip.
	 *
	 */
	signals[QUERY_TOOLTIP] =
		g_signal_new ("query-tooltip",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, query_tooltip),
		              g_signal_accumulator_true_handled,
		              NULL,
		              _gtksourceview_marshal_BOOLEAN__BOXED_BOXED_INT_INT_OBJECT,
		              G_TYPE_BOOLEAN,
		              5,
		              GTK_TYPE_TEXT_ITER,
		              GDK_TYPE_RECTANGLE,
		              G_TYPE_INT,
		              G_TYPE_INT,
		              GTK_TYPE_TOOLTIP);

	/**
	 * GtkSourceGutterRenderer::query-data:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @start: a #GtkTextIter
	 * @end: a #GtkTextIter
	 * @state: the renderer state
	 *
	 * The ::query-data signal is emitted when the renderer needs
	 * to be filled with data just before a cell is drawn. This can
	 * be used by general renderer implementations to allow render
	 * data to be filled in externally.
	 *
	 */
	signals[QUERY_DATA] =
		g_signal_new ("query-data",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, query_data),
		              NULL,
		              NULL,
		              _gtksourceview_marshal_VOID__BOXED_BOXED_FLAGS,
		              G_TYPE_NONE,
		              3,
		              GTK_TYPE_TEXT_ITER,
		              GTK_TYPE_TEXT_ITER,
		              GTK_TYPE_SOURCE_GUTTER_RENDERER_STATE);

	/**
	 * GtkSourceGutterRenderer::query-activatable:
	 * @renderer: the #GtkSourceGutterRenderer who emits the signal
	 * @iter: a #GtkTextIter
	 * @area: a #GdkRectangle
	 * @event: the #GdkEvent that is causing the activatable query
	 *
	 * The ::query-activatable signal is emitted when the renderer
	 * can possibly be activated.
	 *
	 */
	signals[QUERY_ACTIVATABLE] =
		g_signal_new ("query-activatable",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (GtkSourceGutterRendererClass, query_activatable),
		              g_signal_accumulator_true_handled,
		              NULL,
		              _gtksourceview_marshal_BOOLEAN__BOXED_BOXED_BOXED,
		              G_TYPE_BOOLEAN,
		              3,
		              GTK_TYPE_TEXT_ITER,
		              GDK_TYPE_RECTANGLE,
		              GDK_TYPE_EVENT);

	/**
	 * GtkSourceGutterRenderer:view:
	 *
	 * The view on which the renderer is placed.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      _("The View"),
	                                                      _("The view"),
	                                                      GTK_TYPE_TEXT_VIEW,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceGutterRenderer:alignment-mode:
	 *
	 * The alignment mode of the renderer. This can be used to indicate
	 * that in the case a cell spans multiple lines (due to text wrapping)
	 * the alignment should work on either the full cell, the first line
	 * or the last line.
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_ALIGNMENT_MODE,
	                                 g_param_spec_enum ("alignment-mode",
	                                                    _("Alignment Mode"),
	                                                    _("The alignment mode"),
	                                                    GTK_TYPE_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE,
	                                                    GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GtkSourceGutterRenderer:window-type:
	 *
	 * The window type of the view on which the renderer is placed (left,
	 * or right).
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_WINDOW_TYPE,
	                                 g_param_spec_enum ("window-type",
	                                                    _("Window Type"),
	                                                    _("The window type"),
	                                                    GTK_TYPE_TEXT_WINDOW_TYPE,
	                                                    GTK_TEXT_WINDOW_PRIVATE,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_FIXED_WIDTH,
	                                 g_param_spec_int ("fixed-width",
	                                                   _("Fixed Width"),
	                                                   _("The fixed width"),
	                                                   -1,
	                                                   G_MAXINT,
	                                                   -1,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_FIXED_HEIGHT,
	                                 g_param_spec_int ("fixed-height",
	                                                   _("Fixed Height"),
	                                                   _("The fixed height"),
	                                                   -1,
	                                                   G_MAXINT,
	                                                   -1,
	                                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
gtk_source_gutter_renderer_init (GtkSourceGutterRenderer *self)
{
	self->priv = GTK_SOURCE_GUTTER_RENDERER_GET_PRIVATE (self);
}

/**
 * gtk_source_gutter_renderer_begin:
 * @renderer: a #GtkSourceGutterRenderer
 * @cr: a #cairo_t
 * @background_area: a #GdkRectangle
 * @cell_area: a #GdkRectangle
 * @start: a #GtkTextIter
 * @end: a #GtkTextIter
 *
 * Called when drawing a region begins. The region to be drawn is indicated
 * by @start and @end. The purpose is to allow the implementation to precompute
 * some state before the ::draw method is called for each cell.
 *
 **/
void
gtk_source_gutter_renderer_begin (GtkSourceGutterRenderer *renderer,
                                  cairo_t                 *cr,
                                  const GdkRectangle      *background_area,
                                  const GdkRectangle      *cell_area,
                                  GtkTextIter             *start,
                                  GtkTextIter             *end)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));
	g_return_if_fail (cr != NULL);
	g_return_if_fail (background_area != NULL);
	g_return_if_fail (cell_area != NULL);
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	if (GTK_SOURCE_GUTTER_RENDERER_CLASS (G_OBJECT_GET_CLASS (renderer))->begin)
	{
		GTK_SOURCE_GUTTER_RENDERER_CLASS (
			G_OBJECT_GET_CLASS (renderer))->begin (renderer,
			                                       cr,
			                                       background_area,
			                                       cell_area,
			                                       start,
			                                       end);
	}
}

/**
 * gtk_source_gutter_renderer_draw:
 * @renderer: a #GtkSourceGutterRenderer
 * @cr: the cairo render context
 * @background_area: a #GdkRectangle indicating the total area to be drawn
 * @cell_area: a #GdkRectangle indicating the area to draw content
 * @start: a #GtkTextIter
 * @end: a #GtkTextIter
 * @state: a #GtkSourceGutterRendererState
 *
 * Main renderering method. Implementations should implement this method to
 * draw onto the cairo context. The @background_area indicates total area of
 * the cell (without padding or margin) to be drawn. The @cell_area indicates
 * the area where content can be drawn (text, images, etc).
 *
 * The @state argument indicates the current state of the renderer and should
 * be taken into account to properly draw the different possible states
 * (cursor, prelit, selected) if appropriate.
 *
 **/
void
gtk_source_gutter_renderer_draw (GtkSourceGutterRenderer      *renderer,
                                 cairo_t                      *cr,
                                 const GdkRectangle           *background_area,
                                 const GdkRectangle           *cell_area,
                                 GtkTextIter                  *start,
                                 GtkTextIter                  *end,
                                 GtkSourceGutterRendererState  state)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));
	g_return_if_fail (cr != NULL);
	g_return_if_fail (background_area != NULL);
	g_return_if_fail (cell_area != NULL);
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	if (GTK_SOURCE_GUTTER_RENDERER_CLASS (G_OBJECT_GET_CLASS (renderer))->draw)
	{
		GTK_SOURCE_GUTTER_RENDERER_CLASS (
			G_OBJECT_GET_CLASS (renderer))->draw (renderer,
			                                      cr,
			                                      background_area,
			                                      cell_area,
			                                      start,
			                                      end,
			                                      state);
	}
}

/**
 * gtk_source_gutter_renderer_end:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Called when drawing a region of lines has ended.
 *
 **/
void
gtk_source_gutter_renderer_end (GtkSourceGutterRenderer *renderer)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (GTK_SOURCE_GUTTER_RENDERER_CLASS (G_OBJECT_GET_CLASS (renderer))->end)
	{
		GTK_SOURCE_GUTTER_RENDERER_CLASS (G_OBJECT_GET_CLASS (renderer))->end (renderer);
	}
}

/**
 * gtk_source_gutter_renderer_query_activatable:
 * @renderer: a #GtkSourceGutterRenderer
 * @iter: a #GtkTextIter at the start of the line to be activated
 * @area: a #GdkRectangle of the cell area to be activated
 * @event: the event that triggered the query
 *
 * Get whether the renderer is activatable at the location in @event. This is
 * called from #GtkSourceGutter to determine whether a renderer is activatable
 * using the mouse pointer.
 *
 * Returns: %TRUE if the renderer can be activated, %FALSE otherwise
 *
 **/
gboolean
gtk_source_gutter_renderer_query_activatable (GtkSourceGutterRenderer *renderer,
                                              GtkTextIter             *iter,
                                              const GdkRectangle      *area,
                                              GdkEvent                *event)
{
	gboolean ret;

	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (area != NULL, FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	ret = FALSE;

	g_signal_emit (renderer,
	               signals[QUERY_ACTIVATABLE],
	               0,
	               iter,
	               area,
	               event,
	               &ret);

	return ret;
}

/**
 * gtk_source_gutter_renderer_get_size:
 * @renderer: a #GtkSourceGutterRenderer
 * @width: (out caller-allocates) (allow-none): return value for the requested width of the renderer
 * @height: (out caller-allocates) (allow-none): return value for the requested height of the renderer
 *
 * Get the required size of the renderer. The @width and @height parameters
 * can be %NULL depending on whether the renderer is packed in a top/bottom or
 * left/right gutter.
 *
 **/
void
gtk_source_gutter_renderer_get_size (GtkSourceGutterRenderer *renderer,
                                     gint                    *width,
                                     gint                    *height)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (GTK_SOURCE_GUTTER_RENDERER_CLASS (G_OBJECT_GET_CLASS (renderer))->get_size)
	{
		GTK_SOURCE_GUTTER_RENDERER_CLASS (
			G_OBJECT_GET_CLASS (renderer))->get_size (renderer,
			                                          width,
			                                          height);
	}
	else
	{
		if (width)
		{
			*width = 0;
		}

		if (height)
		{
			*height = 0;
		}
	}
}

/**
 * gtk_source_gutter_renderer_activate:
 * @renderer: a #GtkSourceGutterRenderer
 * @iter: a #GtkTextIter at the start of the line where the renderer is activated
 * @area: a #GdkRectangle of the cell area where the renderer is activated
 * @event: the event that triggered the activation
 *
 * Emits the ::activate signal of the renderer. This is called from
 * #GtkSourceGutter and should never have to be called manually.
 *
 */
void
gtk_source_gutter_renderer_activate (GtkSourceGutterRenderer *renderer,
                                     GtkTextIter             *iter,
                                     const GdkRectangle      *area,
                                     GdkEvent                *event)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (area != NULL);
	g_return_if_fail (event != NULL);

	g_signal_emit (renderer, signals[ACTIVATE], 0, iter, area, event);
}

/**
 * gtk_source_gutter_renderer_size_changed:
 * @renderer: a #GtkSourceGutterRenderer
 * 
 * Emits the ::size-changed signal of the renderer. Call this from an
 * implementation to inform about a size change of the renderer.
 *
 **/
void
gtk_source_gutter_renderer_size_changed (GtkSourceGutterRenderer *renderer)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	g_signal_emit (renderer, signals[SIZE_CHANGED], 0);
}

/**
 * gtk_source_gutter_renderer_queue_draw:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Emits the ::queue-draw signal of the renderer. Call this from an
 * implementation to inform that the renderer has changed such that it needs
 * to redraw.
 *
 **/
void
gtk_source_gutter_renderer_queue_draw (GtkSourceGutterRenderer *renderer)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	g_signal_emit (renderer, signals[QUEUE_DRAW], 0);
}

/**
 * gtk_source_gutter_renderer_query_tooltip:
 * @renderer: a #GtkSourceGutterRenderer.
 * @iter: a #GtkTextIter.
 * @area: a #GdkRectangle.
 * @x: The x position of the tooltip.
 * @y: The y position of the tooltip.
 * @tooltip: a #GtkTooltip.
 *
 * Emits the ::query-tooltip signal. This function is called from
 * #GtkSourceGutter. Implementations can override the default signal handler
 * or can connect to the signal externally.
 *
 * Returns: %TRUE if the tooltip has been set, %FALSE otherwise
 *
 **/
gboolean
gtk_source_gutter_renderer_query_tooltip (GtkSourceGutterRenderer *renderer,
                                          GtkTextIter             *iter,
                                          const GdkRectangle      *area,
                                          gint                     x,
                                          gint                     y,
                                          GtkTooltip              *tooltip)
{
	gboolean ret;

	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (area != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TOOLTIP (tooltip), FALSE);

	ret = FALSE;

	g_signal_emit (renderer,
	               signals[QUERY_TOOLTIP],
	               0,
	               iter,
	               area,
	               x,
	               y,
	               tooltip,
	               &ret);

	return ret;
}

/**
 * gtk_source_gutter_renderer_query_data:
 * @renderer: a #GtkSourceGutterRenderer.
 * @start: a #GtkTextIter.
 * @end: a #GtkTextIter.
 * @state: a #GtkSourceGutterRendererState.
 *
 * Emit the ::query-data signal. This function is called to query for data
 * just before rendering a cell. This is called from the #GtkSourceGutter.
 * Implementations can override the default signal handler or can connect
 * a signal handler externally to the ::query-data signal.
 *
 **/
void
gtk_source_gutter_renderer_query_data (GtkSourceGutterRenderer      *renderer,
                                       GtkTextIter                  *start,
                                       GtkTextIter                  *end,
                                       GtkSourceGutterRendererState  state)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));
	g_return_if_fail (start != NULL);
	g_return_if_fail (end != NULL);

	g_signal_emit (renderer, signals[QUERY_DATA], 0, start, end, state);
}

/**
 * gtk_source_gutter_renderer_set_visible:
 * @renderer: a #GtkSourceGutterRenderer
 * @visible: the visibility
 *
 * Set whether the gutter renderer is visible.
 *
 **/
void
gtk_source_gutter_renderer_set_visible (GtkSourceGutterRenderer *renderer,
                                        gboolean                 visible)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (visible != renderer->priv->visible)
	{
		renderer->priv->visible = visible;

		g_object_notify (G_OBJECT (renderer), "visible");

		gtk_source_gutter_renderer_queue_draw (renderer);
	}
}

/**
 * gtk_source_gutter_renderer_get_visible:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get whether the gutter renderer is visible.
 *
 * Returns: %TRUE if the renderer is visible, %FALSE otherwise
 *
 **/
gboolean
gtk_source_gutter_renderer_get_visible (GtkSourceGutterRenderer *renderer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), FALSE);

	return renderer->priv->visible;
}

/**
 * gtk_source_gutter_renderer_set_padding:
 * @renderer: a #GtkSourceGutterRenderer
 * @xpad: the x-padding
 * @ypad: the y-padding
 *
 * Set the padding of the gutter renderer. Both @xpad and @ypad can be
 * -1, which means the values will not be changed (this allows changing only
 * one of the values).
 *
 **/
void
gtk_source_gutter_renderer_set_padding (GtkSourceGutterRenderer *renderer,
                                        gint                     xpad,
                                        gint                     ypad)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	set_xpad (renderer, xpad);
	set_ypad (renderer, ypad);
}

/**
 * gtk_source_gutter_renderer_get_padding:
 * @renderer: a #GtkSourceGutterRenderer
 * @xpad: (out caller-allocates) (allow-none): return location for the x-padding (can be %NULL)
 * @ypad: (out caller-allocates) (allow-none): return location for the y-padding (can be %NULL)
 *
 * Get the x-padding and y-padding of the gutter renderer.
 *
 **/
void
gtk_source_gutter_renderer_get_padding (GtkSourceGutterRenderer *renderer,
                                        gint                    *xpad,
                                        gint                    *ypad)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (xpad)
	{
		*xpad = renderer->priv->xpad;
	}

	if (ypad)
	{
		*ypad = renderer->priv->ypad;
	}
}

/**
 * gtk_source_gutter_renderer_set_alignment:
 * @renderer: a #GtkSourceGutterRenderer
 * @xalign: the x-alignment
 * @yalign: the y-alignment
 *
 * Set the alignment of the gutter renderer. Both @xalign and @yalign can be
 * -1, which means the values will not be changed (this allows changing only
 * one of the values).
 *
 **/
void
gtk_source_gutter_renderer_set_alignment (GtkSourceGutterRenderer *renderer,
                                          gfloat                   xalign,
                                          gfloat                   yalign)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (set_xalign (renderer, xalign, FALSE) ||
	    set_yalign (renderer, yalign, FALSE))
	{
		gtk_source_gutter_renderer_queue_draw (renderer);
		return;
	}
}

/**
 * gtk_source_gutter_renderer_get_alignment:
 * @renderer: a #GtkSourceGutterRenderer
 * @xalign: (out caller-allocates) (allow-none): return location for the x-alignment (can be %NULL)
 * @yalign: (out caller-allocates) (allow-none): return location for the y-alignment (can be %NULL)
 *
 * Get the x-alignment and y-alignment of the gutter renderer.
 *
 **/
void
gtk_source_gutter_renderer_get_alignment (GtkSourceGutterRenderer *renderer,
                                          gfloat                  *xalign,
                                          gfloat                  *yalign)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (xalign)
	{
		*xalign = renderer->priv->xalign;
	}

	if (yalign)
	{
		*yalign = renderer->priv->yalign;
	}
}

/**
 * gtk_source_gutter_renderer_set_alignment_mode:
 * @renderer: a #GtkSourceGutterRenderer
 * @mode: a #GtkSourceGutterRendererAlignmentMode
 *
 * Set the alignment mode. The alignment mode describes the manner in which the
 * renderer is aligned (see :xalign and :yalign).
 *
 **/
void
gtk_source_gutter_renderer_set_alignment_mode (GtkSourceGutterRenderer              *renderer,
                                               GtkSourceGutterRendererAlignmentMode  mode)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	set_alignment_mode (renderer, mode);
}

/**
 * gtk_source_gutter_renderer_get_alignment_mode:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get the alignment mode. The alignment mode describes the manner in which the
 * renderer is aligned (see :xalign and :yalign).
 *
 * Returns: a #GtkSourceGutterRendererAlignmentMode
 *
 **/
GtkSourceGutterRendererAlignmentMode
gtk_source_gutter_renderer_get_alignment_mode (GtkSourceGutterRenderer *renderer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), 0);

	return renderer->priv->alignment_mode;
}

/**
 * gtk_source_gutter_renderer_get_window_type:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get the #GtkTextWindowType associated with the gutter renderer.
 *
 * Returns: a #GtkTextWindowType
 *
 **/
GtkTextWindowType
gtk_source_gutter_renderer_get_window_type (GtkSourceGutterRenderer *renderer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), GTK_TEXT_WINDOW_PRIVATE);

	return renderer->priv->window_type;
}

/**
 * gtk_source_gutter_renderer_get_view:
 * @renderer: a #GtkSourceGutterRenderer
 *
 * Get the view associated to the gutter renderer
 *
 * Returns: (transfer none): a #GtkTextView
 *
 **/
GtkTextView *
gtk_source_gutter_renderer_get_view (GtkSourceGutterRenderer *renderer)
{
	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer), NULL);

	return renderer->priv->view;
}

/**
 * gtk_source_gutter_renderer_get_fixed_size:
 * @renderer: a #GtkSourceGutterRenderer
 * @width: (out caller-allocates) (allow-none): return value for the fixed width
 * @height: (out caller-allocates) (allow-none): return value for the fixed height
 *
 * Get the fixed size of the renderer. If @width or @height is smaller than
 * 0, then the renderer does not have a fixed size.
 *
 **/
void
gtk_source_gutter_renderer_get_fixed_size (GtkSourceGutterRenderer *renderer,
                                           gint                    *width,
                                           gint                    *height)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (width)
	{
		*width = renderer->priv->fixed_width;
	}

	if (height)
	{
		*height = renderer->priv->fixed_height;
	}
}

/**
 * gtk_source_gutter_renderer_set_fixed_size:
 * @renderer: a #GtkSourceGutterRenderer
 * @width: the fixed width, or -1
 * @height: the fixed height, or -1
 *
 * Sets the fixed size of the renderer. A value of -1 specifies that the size
 * is to be determined dynamically.
 *
 **/
void
gtk_source_gutter_renderer_set_fixed_size (GtkSourceGutterRenderer *renderer,
                                           gint                     width,
                                           gint                     height)
{
	gboolean changed_width;
	gboolean changed_height;

	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	changed_width = set_fixed_size (renderer,
	                                &renderer->priv->fixed_width,
	                                "fixed-width",
	                                width,
	                                FALSE);

	changed_height = set_fixed_size (renderer,
	                                 &renderer->priv->fixed_height,
	                                 "fixed-height",
	                                 height,
	                                 FALSE);

	if (changed_width || changed_height)
	{
		gtk_source_gutter_renderer_size_changed (renderer);
	}
}
