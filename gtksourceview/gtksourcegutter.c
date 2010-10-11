/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; coding: utf-8 -*- /
 * gtksourcegutter.c
 * This file is part of GtkSourceView
 *
 * Copyright (C) 2009 - Jesse van den Kieboom
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

#include "gtksourcegutter-private.h"
#include "gtksourceview.h"
#include "gtksourceview-i18n.h"
#include "gtksourceview-marshal.h"
#include "gtksourcegutterrenderer.h"

/**
 * SECTION:gutter
 * @Short_description: Gutter object for #GtkSourceView
 * @Title: GtkSourceGutter
 * @See_also:#GtkSourceView
 *
 * The #GtkSourceGutter object represents the left and right gutters of the text
 * view. It is used by #GtkSourceView to draw the line numbers and category
 * marks that might be present on a line. By packing additional #GtkSourceGutterRenderer
 * objects in the gutter, you can extend the gutter with your own custom 
 * drawings.
 *
 * The gutter works very much the same way as cells rendered in a #GtkTreeView.
 * The concept is similar, with the exception that the gutter does not have an
 * underlying #GtkTreeModel. The builtin line number renderer is at position
 * #GTK_SOURCE_VIEW_GUTTER_POSITION_LINES (-30) and the marks renderer is at
 * #GTK_SOURCE_VIEW_GUTTER_POSITION_MARKS (-20). You can use these values to
 * position custom renderers accordingly.
 *
 */

#define GTK_SOURCE_GUTTER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_GUTTER, GtkSourceGutterPrivate))

/* Properties */
enum
{
	PROP_0,
	PROP_VIEW,
	PROP_WINDOW_TYPE
};

typedef struct
{
	GtkSourceGutterRenderer *renderer;
	GtkSourceGutterRendererState state;
	gint position;

	guint queue_draw_handler;
	guint size_changed_handler;
} Renderer;

enum
{
	DRAW,
	MOTION_NOTIFY_EVENT,
	BUTTON_PRESS_EVENT,
	ENTER_NOTIFY_EVENT,
	LEAVE_NOTIFY_EVENT,
	QUERY_TOOLTIP_EVENT,
	LAST_EXTERNAL_SIGNAL
};

struct _GtkSourceGutterPrivate
{
	GtkSourceView *view;
	GtkTextWindowType window_type;
	gint size;
	GList *renderers;
	guint signals[LAST_EXTERNAL_SIGNAL];
};

G_DEFINE_TYPE (GtkSourceGutter, gtk_source_gutter, G_TYPE_OBJECT)

static gboolean on_view_draw (GtkSourceView   *view,
                              cairo_t         *cr,
                              GtkSourceGutter *gutter);

static gboolean on_view_motion_notify_event (GtkSourceView   *view,
                                             GdkEventMotion  *event,
                                             GtkSourceGutter *gutter);


static gboolean on_view_enter_notify_event (GtkSourceView    *view,
                                            GdkEventCrossing *event,
                                            GtkSourceGutter  *gutter);

static gboolean on_view_leave_notify_event (GtkSourceView    *view,
                                            GdkEventCrossing *event,
                                            GtkSourceGutter  *gutter);

static gboolean on_view_button_press_event (GtkSourceView    *view,
                                            GdkEventButton   *event,
                                            GtkSourceGutter  *gutter);

static gboolean on_view_query_tooltip (GtkSourceView   *view,
                                       gint             x,
                                       gint             y,
                                       gboolean         keyboard_mode,
                                       GtkTooltip      *tooltip,
                                       GtkSourceGutter *gutter);

static void do_redraw (GtkSourceGutter *gutter);
static void revalidate_size (GtkSourceGutter *gutter);

static void
view_notify (GtkSourceGutter *gutter,
             gpointer         where_the_object_was)
{
	gutter->priv->view = NULL;
}

static void
on_renderer_size_changed (GtkSourceGutterRenderer *renderer,
                          GtkSourceGutter         *gutter)
{
	revalidate_size (gutter);
}

static void
on_renderer_queue_draw (GtkSourceGutterRenderer *renderer,
                        GtkSourceGutter         *gutter)
{
	do_redraw (gutter);
}

static Renderer *
renderer_new (GtkSourceGutter         *gutter,
              GtkSourceGutterRenderer *renderer,
              gint                     position)
{
	Renderer *ret = g_slice_new0 (Renderer);

	if (G_IS_INITIALLY_UNOWNED (renderer))
	{
		ret->renderer = g_object_ref_sink (renderer);
	}
	else
	{
		ret->renderer = g_object_ref (renderer);
	}

	ret->state = GTK_SOURCE_GUTTER_RENDERER_STATE_NORMAL;
	ret->position = position;

	g_signal_connect (renderer,
	                  "size-changed",
	                  G_CALLBACK (on_renderer_size_changed),
	                  gutter);

	g_signal_connect (renderer,
	                  "queue-draw",
	                  G_CALLBACK (on_renderer_queue_draw),
	                  gutter);

	return ret;
}

static void
renderer_free (Renderer *renderer)
{
	g_signal_handler_disconnect (renderer->renderer,
	                             renderer->queue_draw_handler);

	g_signal_handler_disconnect (renderer->renderer,
	                             renderer->size_changed_handler);

	g_object_unref (renderer->renderer);
	g_slice_free (Renderer, renderer);
}

static void
gtk_source_gutter_finalize (GObject *object)
{
	G_OBJECT_CLASS (gtk_source_gutter_parent_class)->finalize (object);
}

static void
gtk_source_gutter_dispose (GObject *object)
{
	GtkSourceGutter *gutter = GTK_SOURCE_GUTTER (object);
	gint i;

	g_list_foreach (gutter->priv->renderers, (GFunc)renderer_free, NULL);
	g_list_free (gutter->priv->renderers);

	if (gutter->priv->view)
	{
		for (i = 0; i < LAST_EXTERNAL_SIGNAL; ++i)
		{
			g_signal_handler_disconnect (gutter->priv->view,
				                     gutter->priv->signals[i]);
		}

		g_object_weak_unref (G_OBJECT (gutter->priv->view),
		                     (GWeakNotify)view_notify,
		                     gutter);

		gutter->priv->view = NULL;
	}

	gutter->priv->renderers = NULL;
}

static void
gtk_source_gutter_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
	GtkSourceGutter *self = GTK_SOURCE_GUTTER (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			g_value_set_object (value, self->priv->view);
		break;
		case PROP_WINDOW_TYPE:
			g_value_set_enum (value, self->priv->window_type);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
set_view (GtkSourceGutter *gutter,
          GtkSourceView   *view)
{
	gutter->priv->view = view;

	gutter->priv->size = -1;

	g_object_weak_ref (G_OBJECT (view),
	                   (GWeakNotify)view_notify,
	                   gutter);

	gutter->priv->signals[DRAW] =
		g_signal_connect (view,
		                  "draw",
		                  G_CALLBACK (on_view_draw),
		                  gutter);

	gutter->priv->signals[MOTION_NOTIFY_EVENT] =
		g_signal_connect (view,
		                  "motion-notify-event",
		                  G_CALLBACK (on_view_motion_notify_event),
		                  gutter);

	gutter->priv->signals[ENTER_NOTIFY_EVENT] =
		g_signal_connect (view,
		                  "enter-notify-event",
		                  G_CALLBACK (on_view_enter_notify_event),
		                  gutter);

	gutter->priv->signals[LEAVE_NOTIFY_EVENT] =
		g_signal_connect (view,
		                  "leave-notify-event",
		                  G_CALLBACK (on_view_leave_notify_event),
		                  gutter);

	gutter->priv->signals[BUTTON_PRESS_EVENT] =
		g_signal_connect (view,
		                  "button-press-event",
		                  G_CALLBACK (on_view_button_press_event),
		                  gutter);

	gutter->priv->signals[QUERY_TOOLTIP_EVENT] =
		g_signal_connect (view,
		                  "query-tooltip",
		                  G_CALLBACK (on_view_query_tooltip),
		                  gutter);
}

static void
do_redraw (GtkSourceGutter *gutter)
{
	GdkWindow *window;

	window = gtk_text_view_get_window (GTK_TEXT_VIEW (gutter->priv->view),
	                                   gutter->priv->window_type);

	if (window)
	{
		gdk_window_invalidate_rect (window, NULL, FALSE);
	}
}

static gint
calculate_size (GtkSourceGutter  *gutter,
                Renderer         *renderer)
{
	gint width = 0;

	gtk_source_gutter_renderer_get_size (renderer->renderer, &width, NULL);

	return width;
}

static gint
calculate_sizes (GtkSourceGutter  *gutter,
                 GArray           *sizes)
{
	GList *item;
	gint total_width = 0;

	/* Calculate size */
	for (item = gutter->priv->renderers; item; item = g_list_next (item))
	{
		Renderer *renderer = (Renderer *)item->data;
		gint width;

		width = calculate_size (gutter, renderer);

		if (sizes)
		{
			g_array_append_val (sizes, width);
		}

		total_width += width;
	}

	return total_width;
}

static void
revalidate_size (GtkSourceGutter *gutter)
{
	GdkWindow *window;

	window = gtk_source_gutter_get_window (gutter);

	gint width = calculate_sizes (gutter, NULL);

	gtk_text_view_set_border_window_size (GTK_TEXT_VIEW (gutter->priv->view),
	                                      gutter->priv->window_type,
	                                      width);
}

static void
gtk_source_gutter_set_property (GObject       *object,
                                guint          prop_id,
                                const GValue  *value,
                                GParamSpec    *pspec)
{
	GtkSourceGutter *self = GTK_SOURCE_GUTTER (object);

	switch (prop_id)
	{
		case PROP_VIEW:
			set_view (self, GTK_SOURCE_VIEW (g_value_get_object (value)));

		break;
		case PROP_WINDOW_TYPE:
			self->priv->window_type = g_value_get_enum (value);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gtk_source_gutter_class_init (GtkSourceGutterClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = gtk_source_gutter_set_property;
	object_class->get_property = gtk_source_gutter_get_property;

	object_class->finalize = gtk_source_gutter_finalize;
	object_class->dispose = gtk_source_gutter_dispose;

	/**
	 * GtkSourceGutter:view:
	 *
	 * The #GtkSourceView of the gutter
	 */
	g_object_class_install_property (object_class,
	                                 PROP_VIEW,
	                                 g_param_spec_object ("view",
	                                                      _("View"),
	                                                      _("The gutters' GtkSourceView"),
	                                                      GTK_TYPE_SOURCE_VIEW,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/**
	 * GtkSourceGutter:window-type:
	 *
	 * The text window type on which the window is placed
	 */
	g_object_class_install_property (object_class,
	                                 PROP_WINDOW_TYPE,
	                                 g_param_spec_enum ("window_type",
	                                                    _("Window Type"),
	                                                    _("The gutters text window type"),
	                                                    GTK_TYPE_TEXT_WINDOW_TYPE,
	                                                    0,
	                                                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof(GtkSourceGutterPrivate));
}

static void
gtk_source_gutter_init (GtkSourceGutter *self)
{
	self->priv = GTK_SOURCE_GUTTER_GET_PRIVATE (self);

	self->priv->size = -1;
}

static gint
sort_by_position (Renderer *r1,
                  Renderer *r2,
                  gpointer  data)
{
	if (r1->position < r2->position)
	{
		return -1;
	}
	else if (r1->position > r2->position)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static void
append_renderer (GtkSourceGutter *gutter,
                 Renderer        *renderer)
{
	gutter->priv->renderers =
		g_list_insert_sorted_with_data (gutter->priv->renderers,
		                                renderer,
		                                (GCompareDataFunc)sort_by_position,
		                                NULL);

	revalidate_size (gutter);
}

GtkSourceGutter *
gtk_source_gutter_new (GtkSourceView     *view,
                       GtkTextWindowType  type)
{
	return g_object_new (GTK_TYPE_SOURCE_GUTTER,
	                     "view", view,
	                     "window_type", type,
	                     NULL);
}

/* Public API */

/**
 * gtk_source_gutter_get_window:
 * @gutter: a #GtkSourceGutter.
 *
 * Get the #GdkWindow of the gutter. The window will only be available when the
 * gutter has at least one, non-zero width, cell renderer packed.
 *
 * Returns: (transfer none): the #GdkWindow of the gutter, or %NULL
 * if the gutter has no window.
 *
 * Since: 2.8
 */
GdkWindow *
gtk_source_gutter_get_window (GtkSourceGutter *gutter)
{
	g_return_val_if_fail (GTK_IS_SOURCE_GUTTER (gutter), NULL);
	g_return_val_if_fail (gutter->priv->view != NULL, NULL);

	return gtk_text_view_get_window (GTK_TEXT_VIEW (gutter->priv->view),
	                                 gutter->priv->window_type);
}

/**
 * gtk_source_gutter_insert:
 * @gutter: a #GtkSourceGutter.
 * @renderer: a #GtkSourceGutterRenderer.
 * @position: the renderers position.
 *
 * Inserts @renderer into @gutter at @position.
 *
 * Since: 2.8
 */
void
gtk_source_gutter_insert (GtkSourceGutter         *gutter,
                          GtkSourceGutterRenderer *renderer,
                          gint                     position)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER (gutter));
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	append_renderer (gutter, renderer_new (gutter, renderer, position));
}

static gboolean
renderer_find (GtkSourceGutter          *gutter,
               GtkSourceGutterRenderer  *renderer,
               Renderer                **ret,
               GList                   **retlist)
{
	GList *list;

	for (list = gutter->priv->renderers; list; list = g_list_next (list))
	{
		*ret = list->data;

		if ((*ret)->renderer == renderer)
		{
			if (retlist)
			{
				*retlist = list;
			}

			return TRUE;
		}
	}

	return FALSE;
}


/**
 * gtk_source_gutter_reorder:
 * @gutter: a #GtkSourceGutterRenderer.
 * @renderer: a #GtkCellRenderer.
 * @position: the new renderer position.
 *
 * Reorders @renderer in @gutter to new @position.
 *
 * Since: 2.8
 */
void
gtk_source_gutter_reorder (GtkSourceGutter         *gutter,
                           GtkSourceGutterRenderer *renderer,
                           gint                     position)
{
	Renderer *ret;
	GList *retlist;

	g_return_if_fail (GTK_IS_SOURCE_GUTTER (gutter));
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (renderer_find (gutter, renderer, &ret, &retlist))
	{
		gutter->priv->renderers =
			g_list_remove_link (gutter->priv->renderers,
			                    retlist);

		ret->position = position;
		append_renderer (gutter, ret);
	}
}

/**
 * gtk_source_gutter_remove:
 * @gutter: a #GtkSourceGutter.
 * @renderer: a #GtkSourceGutterRenderer.
 *
 * Removes @renderer from @gutter.
 *
 * Since: 2.8
 */
void
gtk_source_gutter_remove (GtkSourceGutter         *gutter,
                          GtkSourceGutterRenderer *renderer)
{
	Renderer *ret;
	GList *retlist;

	g_return_if_fail (GTK_IS_SOURCE_GUTTER (gutter));
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER (renderer));

	if (renderer_find (gutter, renderer, &ret, &retlist))
	{
		gutter->priv->renderers =
			g_list_remove_link (gutter->priv->renderers,
			                    retlist);

		revalidate_size (gutter);
		renderer_free (ret);
	}
}

/**
 * gtk_source_gutter_queue_draw:
 * @gutter: a #GtkSourceGutter.
 *
 * Invalidates the drawable area of the gutter. You can use this to force a
 * redraw of the gutter if something has changed and needs to be redrawn.
 *
 * Since: 2.8
 */
void
gtk_source_gutter_queue_draw (GtkSourceGutter *gutter)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER (gutter));

	do_redraw (gutter);
}

/* This function is taken from gtk+/tests/testtext.c */
static gint
get_lines (GtkTextView  *text_view,
           gint          first_y,
           gint          last_y,
           GArray       *buffer_coords,
           GArray       *line_heights,
           GArray       *numbers,
           gint         *countp,
           GtkTextIter  *start,
           GtkTextIter  *end)
{
	GtkTextIter iter;
	gint count;
	gint size;
	gint last_line_num = -1;
	gint total_height = 0;

	g_array_set_size (buffer_coords, 0);
	g_array_set_size (numbers, 0);

	if (line_heights != NULL)
	{
		g_array_set_size (line_heights, 0);
	}

	/* Get iter at first y */
	gtk_text_view_get_line_at_y (text_view, &iter, first_y, NULL);

	/* For each iter, get its location and add it to the arrays.
	 * Stop when we pass last_y */
	count = 0;
	size = 0;

	*start = iter;

	while (!gtk_text_iter_is_end (&iter))
	{
		gint y, height;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		g_array_append_val (buffer_coords, y);

		if (line_heights)
		{
			g_array_append_val (line_heights, height);
		}

		total_height += height;

		last_line_num = gtk_text_iter_get_line (&iter);
		g_array_append_val (numbers, last_line_num);

		++count;

		if ((y + height) >= last_y)
		{
			break;
		}

		gtk_text_iter_forward_line (&iter);
	}

	if (gtk_text_iter_is_end (&iter))
	{
		gint y, height;
		gint line_num;

		gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);

		line_num = gtk_text_iter_get_line (&iter);

		if (line_num != last_line_num)
		{
			g_array_append_val (buffer_coords, y);

			if (line_heights)
			{
				g_array_append_val (line_heights, height);
			}

			total_height += height;

			g_array_append_val (numbers, line_num);
			++count;
		}
	}

	*countp = count;

	if (count == 0)
	{
		gint y = 0;
		gint n = 0;
		gint height;

		*countp = 1;

		g_array_append_val (buffer_coords, y);
		g_array_append_val (numbers, n);

		if (line_heights)
		{
			gtk_text_view_get_line_yrange (text_view, &iter, &y, &height);
			g_array_append_val (line_heights, height);

			total_height += height;
		}
	}

	*end = iter;

	return total_height;
}

static gboolean
on_view_draw (GtkSourceView   *view,
              cairo_t         *cr,
              GtkSourceGutter *gutter)
{
	GdkWindow *window;
	GtkTextView *text_view;
	GArray *sizes;
	GdkRectangle clip;
	gint size;
	gint x, y;
	gint y1, y2;
	GArray *numbers;
	GArray *pixels;
	GArray *heights;
	GtkTextIter cur;
	gint cur_line;
	gint count;
	gint i;
	GList *item;
	GtkTextIter start;
	GtkTextIter end;
	GtkTextBuffer *buffer;
	GdkRectangle rect;

	window = gtk_source_gutter_get_window (gutter);

	if (window == NULL || !gtk_cairo_should_draw_window (cr, window))
	{
		return FALSE;
	}

	gtk_cairo_transform_to_window (cr, GTK_WIDGET (view), window);

	text_view = GTK_TEXT_VIEW (view);

	if (!gdk_cairo_get_clip_rectangle (cr, &clip))
	{
		return FALSE;
	}

	buffer = gtk_text_view_get_buffer (text_view);

	gdk_window_get_pointer (window, &x, &y, NULL);

	y1 = clip.y;
	y2 = y1 + clip.height;

	/* get the extents of the line printing */
	gtk_text_view_window_to_buffer_coords (text_view,
	                                       gutter->priv->window_type,
	                                       0,
	                                       y1,
	                                       NULL,
	                                       &y1);

	gtk_text_view_window_to_buffer_coords (text_view,
	                                       gutter->priv->window_type,
	                                       0,
	                                       y2,
	                                       NULL,
	                                       &y2);

	numbers = g_array_new (FALSE, FALSE, sizeof (gint));
	pixels = g_array_new (FALSE, FALSE, sizeof (gint));
	heights = g_array_new (FALSE, FALSE, sizeof (gint));
	sizes = g_array_new (FALSE, FALSE, sizeof (gint));

	size = calculate_sizes (gutter, sizes);

	i = 0;
	x = 0;

	rect.x = 0;
	rect.height = get_lines (text_view, y1, y2, pixels, heights, numbers, &count, &start, &end);

	gtk_text_view_buffer_to_window_coords (text_view,
	                                       gutter->priv->window_type,
	                                       0,
	                                       g_array_index (pixels, gint, 0),
	                                       NULL,
	                                       &rect.y);

	item = gutter->priv->renderers;

	while (item)
	{
		Renderer *renderer = item->data;

		rect.width = g_array_index (sizes, gint, i);

		cairo_save (cr);

		gdk_cairo_rectangle (cr, &rect);
		cairo_clip (cr);

		gtk_source_gutter_renderer_begin (renderer->renderer,
		                                  cr,
		                                  &rect,
		                                  &rect,
		                                  &start,
		                                  &end);

		cairo_restore (cr);

		rect.x += rect.width;

		item = g_list_next (item);
	}

	gtk_text_buffer_get_iter_at_mark (buffer,
	                                  &cur,
	                                  gtk_text_buffer_get_insert (buffer));

	cur_line = gtk_text_iter_get_line (&cur);

	for (i = 0; i < count; ++i)
	{
		gint pos;
		gint line_to_paint;
		GdkRectangle cell_area;
		gint idx = 0;

		end = start;

		if (!gtk_text_iter_ends_line (&end))
		{
			gtk_text_iter_forward_to_line_end (&end);
		}

		gtk_text_view_buffer_to_window_coords (text_view,
		                                       gutter->priv->window_type,
		                                       0,
		                                       g_array_index (pixels, gint, i),
		                                       NULL,
		                                       &pos);

		line_to_paint = g_array_index (numbers, gint, i);

		cell_area.x = 0;
		cell_area.y = pos;
		cell_area.height = g_array_index (heights, gint, i);

		for (item = gutter->priv->renderers; item; item = g_list_next (item))
		{
			Renderer *renderer;
			gint width;
			GtkSourceGutterRendererState state;

			renderer = item->data;
			width = g_array_index (sizes, gint, idx++);
			state = renderer->state;

			cell_area.width = width;

			if (line_to_paint == cur_line)
			{
				state |= GTK_SOURCE_GUTTER_RENDERER_STATE_CURSOR;
			}

			cairo_save (cr);

			gdk_cairo_rectangle (cr, &cell_area);
			cairo_clip (cr);

			/* Call render with correct area */
			gtk_source_gutter_renderer_draw (renderer->renderer,
			                                 cr,
			                                 &cell_area,
			                                 &cell_area,
			                                 &start,
			                                 &end,
			                                 state);

			cairo_restore (cr);

			cell_area.x += cell_area.width;
		}

		gtk_text_iter_forward_line (&start);
	}

	for (item = gutter->priv->renderers; item; item = g_list_next (item))
	{
		Renderer *renderer = item->data;

		gtk_source_gutter_renderer_end (renderer->renderer);
	}

	g_array_free (numbers, TRUE);
	g_array_free (pixels, TRUE);
	g_array_free (heights, TRUE);

	g_array_free (sizes, TRUE);

	return FALSE;
}

static Renderer *
renderer_at_x (GtkSourceGutter *gutter,
               gint             x,
               gint            *start,
               gint            *width)
{
	GList *item;

	for (item = gutter->priv->renderers; item; item = g_list_next (item))
	{
		Renderer *renderer = item->data;

		*width = calculate_size (gutter, renderer);

		if (x >= *start && x < *start + *width)
		{
			return renderer;
		}

		*start += *width;
	}

	return NULL;
}

static gboolean
redraw_for_window (GtkSourceGutter *gutter,
                   GdkEventAny     *event,
                   gboolean         act_on_window,
                   gint             x,
                   gint             y)
{
	Renderer *at_x;
	GList *item;
	gboolean redraw;

	if (event->window != gtk_source_gutter_get_window (gutter) && act_on_window)
	{
		return FALSE;
	}

	at_x = renderer_at_x (gutter, x, NULL, NULL);
	redraw = FALSE;

	for (item = gutter->priv->renderers; item; item = g_list_next (item))
	{
		Renderer *renderer = item->data;
		GtkSourceGutterRendererState old_state = renderer->state;

		if (renderer != at_x)
		{
			renderer->state &= ~GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT;
		}
		else
		{
			renderer->state |= GTK_SOURCE_GUTTER_RENDERER_STATE_PRELIT;
		}

		redraw |= (renderer->state != old_state);
	}

	if (redraw)
	{
		do_redraw (gutter);
	}

	return FALSE;
}

static gboolean
on_view_motion_notify_event (GtkSourceView    *view,
                             GdkEventMotion   *event,
                             GtkSourceGutter  *gutter)
{
	return redraw_for_window (gutter,
	                          (GdkEventAny *)event,
	                          TRUE,
	                          (gint)event->x,
	                          (gint)event->y);
}

static gboolean
on_view_enter_notify_event (GtkSourceView     *view,
                            GdkEventCrossing  *event,
                            GtkSourceGutter   *gutter)
{
	return redraw_for_window (gutter,
	                          (GdkEventAny *)event,
	                          TRUE,
	                          (gint)event->x,
	                          (gint)event->y);
}

static gboolean
on_view_leave_notify_event (GtkSourceView     *view,
                            GdkEventCrossing  *event,
                            GtkSourceGutter   *gutter)
{
	return redraw_for_window (gutter,
	                          (GdkEventAny *)event,
	                          FALSE,
	                          (gint)event->x,
	                          (gint)event->y);
}

static void
get_renderer_rect (GtkSourceGutter *gutter,
                   Renderer        *renderer,
                   GtkTextIter     *iter,
                   gint             line,
                   GdkRectangle    *rectangle)
{
	GList *item;
	gint y;

	item = gutter->priv->renderers;
	rectangle->x = 0;

	while (item && item->data != renderer)
	{
		rectangle->x += calculate_size (gutter, item->data);
		item = g_list_next (item);
	}

	gtk_text_view_get_line_yrange (GTK_TEXT_VIEW (gutter->priv->view),
	                               iter,
	                               &y,
	                               &rectangle->height);

	rectangle->width = calculate_size (gutter, renderer);

	gtk_text_view_buffer_to_window_coords (GTK_TEXT_VIEW (gutter->priv->view),
	                                       gutter->priv->window_type,
	                                       0,
	                                       y,
	                                       NULL,
	                                       &rectangle->y);
}

static gboolean
on_view_button_press_event (GtkSourceView    *view,
                            GdkEventButton   *event,
                            GtkSourceGutter  *gutter)
{
	Renderer *renderer;
	gint yline;
	GtkTextIter line_iter;
	gint y_buf;
	gint start = 0;
	gint width = 0;
	GdkRectangle rect;

	if (event->window != gtk_source_gutter_get_window (gutter))
	{
		return FALSE;
	}

	/* Check cell renderer */
	renderer = renderer_at_x (gutter, event->x, &start, &width);

	if (!renderer)
	{
		return FALSE;
	}

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view),
	                                       gutter->priv->window_type,
	                                       event->x, event->y,
	                                       NULL, &y_buf);

	gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view),
	                             &line_iter,
	                             y_buf,
	                             &yline);

	if (yline > y_buf)
	{
		return FALSE;
	}

	get_renderer_rect (gutter, renderer, &line_iter, yline, &rect);

	if (gtk_source_gutter_renderer_get_activatable (renderer->renderer,
	                                                &line_iter,
	                                                &rect,
	                                                event->x,
	                                                event->y))
	{
		gtk_source_gutter_renderer_activate (renderer->renderer,
		                                     &line_iter,
		                                     &rect,
		                                     event->x,
		                                     event->y);

		return TRUE;
	}

	return FALSE;
}

static gboolean
on_view_query_tooltip (GtkSourceView   *view,
                       gint             x,
                       gint             y,
                       gboolean         keyboard_mode,
                       GtkTooltip      *tooltip,
                       GtkSourceGutter *gutter)
{
	GtkTextView *text_view = GTK_TEXT_VIEW (view);
	Renderer *renderer;
	gint start = 0;
	gint width = 0;
	gint y_buf;
	gint yline;
	GtkTextIter line_iter;
	GdkRectangle rect;

	if (keyboard_mode)
	{
		return FALSE;
	}

	/* Check cell renderer */
	renderer = renderer_at_x (gutter, x, &start, &width);

	if (!renderer)
	{
		return FALSE;
	}

	gtk_text_view_window_to_buffer_coords (text_view,
	                                       gutter->priv->window_type,
	                                       x, y,
	                                       NULL, &y_buf);

	gtk_text_view_get_line_at_y (GTK_TEXT_VIEW (view),
	                             &line_iter,
	                             y_buf,
	                             &yline);

	if (yline > y_buf)
	{
		return FALSE;
	}

	get_renderer_rect (gutter, renderer, &line_iter, yline, &rect);

	return gtk_source_gutter_renderer_query_tooltip (renderer->renderer,
	                                                 &line_iter,
	                                                 &rect,
	                                                 x,
	                                                 y,
	                                                 tooltip);
}

/* vi:ts=8 */
