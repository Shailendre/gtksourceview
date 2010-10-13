#include "gtksourcegutterrenderertext.h"

#define GTK_SOURCE_GUTTER_RENDERER_TEXT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), GTK_TYPE_SOURCE_GUTTER_RENDERER_TEXT, GtkSourceGutterRendererTextPrivate))

struct _GtkSourceGutterRendererTextPrivate
{
	gchar *text;

	gchar *measure_text;

	gint width;
	gint height;

	PangoLayout *cached_layout;
	PangoAttribute *fg_attr;
	PangoAttrList *cached_attr_list;

	guint measure_is_markup : 1;
	guint is_markup : 1;
};

G_DEFINE_TYPE (GtkSourceGutterRendererText, gtk_source_gutter_renderer_text, GTK_TYPE_SOURCE_GUTTER_RENDERER)

enum
{
	PROP_0,
	PROP_MARKUP,
	PROP_TEXT,
};

static void
create_layout (GtkSourceGutterRendererText *renderer,
               GtkWidget                   *widget)
{
	PangoLayout *layout;
	PangoAttribute *attr;
	GdkColor color;
	GtkStyle *style;
	PangoAttrList *attr_list;

	layout = gtk_widget_create_pango_layout (widget, NULL);

	style = gtk_widget_get_style (widget);
	color = style->fg[GTK_STATE_NORMAL];

	attr = pango_attr_foreground_new (color.red, color.green, color.blue);

	attr->start_index = 0;
	attr->end_index = G_MAXINT;

	attr_list = pango_attr_list_new ();
	pango_attr_list_insert (attr_list, attr);

	renderer->priv->fg_attr = attr;
	renderer->priv->cached_layout = layout;
	renderer->priv->cached_attr_list = attr_list;
}

static void
gutter_renderer_text_begin (GtkSourceGutterRenderer      *renderer,
                            cairo_t                      *cr,
                            const GdkRectangle           *background_area,
                            const GdkRectangle           *cell_area,
                            GtkTextIter                  *start,
                            GtkTextIter                  *end)
{
	GtkSourceGutterRendererText *text;

	text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);

	create_layout (text, GTK_WIDGET (gtk_source_gutter_renderer_get_view (renderer)));
}

static void
center_on (GtkSourceGutterRenderer *renderer,
           const GdkRectangle      *cell_area,
           GtkTextIter             *iter,
           gint                     width,
           gint                     height,
           gfloat                   xalign,
           gfloat                   yalign,
           gint                    *x,
           gint                    *y)
{
	GdkRectangle location;
	GtkTextView *view;

	view = gtk_source_gutter_renderer_get_view (renderer);

	gtk_text_view_get_iter_location (view, iter, &location);

	*x = cell_area->x + (cell_area->width - width) * xalign;
	*y = cell_area->y + (location.height - height) * yalign;
}

static void
gutter_renderer_text_draw (GtkSourceGutterRenderer      *renderer,
                           cairo_t                      *cr,
                           const GdkRectangle           *background_area,
                           const GdkRectangle           *cell_area,
                           GtkTextIter                  *start,
                           GtkTextIter                  *end,
                           GtkSourceGutterRendererState  state)
{
	GtkSourceGutterRendererText *text;
	gint width;
	gint height;
	PangoAttrList *attr_list;
	gfloat xalign;
	gfloat yalign;
	GtkSourceGutterRendererAlignmentMode mode;
	gint x;
	gint y;
	GtkTextView *view;

	text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);
	view = gtk_source_gutter_renderer_get_view (renderer);

	if (text->priv->is_markup)
	{
		pango_layout_set_markup (text->priv->cached_layout,
		                         text->priv->text,
		                         -1);
	}
	else
	{
		pango_layout_set_text (text->priv->cached_layout,
		                       text->priv->text,
		                       -1);
	}

	attr_list = pango_layout_get_attributes (text->priv->cached_layout);

	if (!attr_list)
	{
		pango_layout_set_attributes (text->priv->cached_layout,
		                             pango_attr_list_copy (text->priv->cached_attr_list));
	}
	else
	{
		pango_attr_list_insert (attr_list,
		                        pango_attribute_copy (text->priv->fg_attr));
	}

	pango_layout_get_size (text->priv->cached_layout, &width, &height);

	width /= PANGO_SCALE;
	height /= PANGO_SCALE;

	gtk_source_gutter_renderer_get_alignment (renderer,
	                                          &xalign,
	                                          &yalign);

	mode = gtk_source_gutter_renderer_get_alignment_mode (renderer);

	switch (mode)
	{
		case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_CELL:
			x = cell_area->x + (cell_area->width - width) * xalign;
			y = cell_area->y + (cell_area->height - height) * yalign;
		break;
		case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST:
			center_on (renderer,
			           cell_area,
			           start,
			           width,
			           height,
			           xalign,
			           yalign,
			           &x,
			           &y);
		break;
		case GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_LAST:
			center_on (renderer,
			           cell_area,
			           end,
			           width,
			           height,
			           xalign,
			           yalign,
			           &x,
			           &y);
		break;
	}

	gtk_paint_layout (gtk_widget_get_style (GTK_WIDGET (view)),
	                  cr,
	                  gtk_widget_get_state (GTK_WIDGET (view)),
	                  TRUE,
	                  GTK_WIDGET (view),
	                  "gtksourcegutterrenderertext",
	                  x,
	                  y,
	                  text->priv->cached_layout);
}

static void
gutter_renderer_text_end (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererText *text;

	text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);

	g_object_unref (text->priv->cached_layout);
	text->priv->cached_layout = NULL;

	pango_attr_list_unref (text->priv->cached_attr_list);
	text->priv->cached_attr_list = NULL;

	text->priv->fg_attr = NULL;
}

static void
gutter_renderer_text_get_size (GtkSourceGutterRenderer *renderer,
                               cairo_t                 *cr,
                               gint                    *width,
                               gint                    *height)
{
	GtkSourceGutterRendererText *text;

	text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);

	if (!width && !height)
	{
		return;
	}

	if (text->priv->width < 0 || text->priv->height < 0)
	{
		PangoLayout *layout;

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (gtk_source_gutter_renderer_get_view (renderer)),
		                                         NULL);

		if (text->priv->measure_is_markup)
		{
			pango_layout_set_markup (layout,
			                         text->priv->measure_text,
			                         -1);
		}
		else
		{
			pango_layout_set_text (layout,
			                       text->priv->measure_text,
			                       -1);
		}

		pango_layout_get_size (layout,
		                       &text->priv->width,
		                       &text->priv->height);

		text->priv->width /= PANGO_SCALE;
		text->priv->height /= PANGO_SCALE;

		g_object_unref (layout);
	}

	if (width)
	{
		*width = text->priv->width;
	}

	if (height)
	{
		*height = text->priv->height;
	}
}

static void
gutter_renderer_text_size_changed (GtkSourceGutterRenderer *renderer)
{
	GtkSourceGutterRendererText *text;

	text = GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer);

	g_free (text->priv->measure_text);

	text->priv->width = -1;
	text->priv->height = -1;

	text->priv->measure_text = g_strdup (text->priv->text);
	text->priv->measure_is_markup = text->priv->is_markup;
}

static void
gtk_source_gutter_renderer_text_finalize (GObject *object)
{
	GtkSourceGutterRendererText *renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);

	g_free (renderer->priv->text);

	G_OBJECT_CLASS (gtk_source_gutter_renderer_text_parent_class)->finalize (object);
}

static void
set_text (GtkSourceGutterRendererText *renderer,
          const gchar                 *text,
          gint                         length,
          gboolean                     is_markup)
{
	g_free (renderer->priv->text);
	g_free (renderer->priv->measure_text);

	renderer->priv->measure_text = NULL;

	renderer->priv->text = length >= 0 ? g_strndup (text, length) : g_strdup (text);
	renderer->priv->is_markup = is_markup;
}

static void
gtk_source_gutter_renderer_text_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
	GtkSourceGutterRendererText *renderer;

	renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);

	switch (prop_id)
	{
		case PROP_MARKUP:
			set_text (renderer, g_value_get_string (value), -1, TRUE);
			break;
		case PROP_TEXT:
			set_text (renderer, g_value_get_string (value), -1, FALSE);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_text_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
	GtkSourceGutterRendererText *renderer;

	renderer = GTK_SOURCE_GUTTER_RENDERER_TEXT (object);

	switch (prop_id)
	{
		case PROP_MARKUP:
			g_value_set_string (value, renderer->priv->is_markup ? renderer->priv->text : NULL);
			break;
		case PROP_TEXT:
			g_value_set_string (value, !renderer->priv->is_markup ? renderer->priv->text : NULL);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
gtk_source_gutter_renderer_text_class_init (GtkSourceGutterRendererTextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);

	object_class->finalize = gtk_source_gutter_renderer_text_finalize;

	object_class->get_property = gtk_source_gutter_renderer_text_get_property;
	object_class->set_property = gtk_source_gutter_renderer_text_set_property;

	renderer_class->begin = gutter_renderer_text_begin;
	renderer_class->draw = gutter_renderer_text_draw;
	renderer_class->end = gutter_renderer_text_end;

	renderer_class->get_size = gutter_renderer_text_get_size;
	renderer_class->size_changed = gutter_renderer_text_size_changed;

	g_type_class_add_private (object_class, sizeof (GtkSourceGutterRendererTextPrivate));

	g_object_class_install_property (object_class,
	                                 PROP_MARKUP,
	                                 g_param_spec_string ("markup",
	                                                      "Markup",
	                                                      "Markup",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_TEXT,
	                                 g_param_spec_string ("text",
	                                                      "Text",
	                                                      "Text",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
gtk_source_gutter_renderer_text_init (GtkSourceGutterRendererText *self)
{
	self->priv = GTK_SOURCE_GUTTER_RENDERER_TEXT_GET_PRIVATE (self);

	self->priv->width = -1;
	self->priv->height = -1;
	self->priv->is_markup = TRUE;
}

/**
 * gtk_source_gutter_renderer_text_new:
 *
 * Create a new #GtkSourceGutterRendererText.
 *
 * Returns: (transfer full): A #GtkSourceGutterRenderer
 *
 **/
GtkSourceGutterRenderer *
gtk_source_gutter_renderer_text_new ()
{
	return g_object_new (GTK_TYPE_SOURCE_GUTTER_RENDERER_TEXT, NULL);
}

void
gtk_source_gutter_renderer_text_set_markup (GtkSourceGutterRendererText *renderer,
                                            const gchar                 *markup,
                                            gint                         length)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER_TEXT (renderer));

	set_text (renderer, markup, length, TRUE);
}

void
gtk_source_gutter_renderer_text_set_text (GtkSourceGutterRendererText *renderer,
                                          const gchar                 *text,
                                          gint                         length)
{
	g_return_if_fail (GTK_IS_SOURCE_GUTTER_RENDERER_TEXT (renderer));

	set_text (renderer, text, length, FALSE);
}
