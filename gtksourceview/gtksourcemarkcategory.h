#ifndef __GTK_SOURCE_MARK_CATEGORY_H__
#define __GTK_SOURCE_MARK_CATEGORY_H__

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcemark.h>

G_BEGIN_DECLS

#define GTK_TYPE_SOURCE_MARK_CATEGORY			(gtk_source_mark_category_get_type ())
#define GTK_SOURCE_MARK_CATEGORY(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_MARK_CATEGORY, GtkSourceMarkCategory))
#define GTK_SOURCE_MARK_CATEGORY_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOURCE_MARK_CATEGORY, GtkSourceMarkCategory const))
#define GTK_SOURCE_MARK_CATEGORY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SOURCE_MARK_CATEGORY, GtkSourceMarkCategoryClass))
#define GTK_IS_SOURCE_MARK_CATEGORY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SOURCE_MARK_CATEGORY))
#define GTK_IS_SOURCE_MARK_CATEGORY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SOURCE_MARK_CATEGORY))
#define GTK_SOURCE_MARK_CATEGORY_GET_CLASS(obj)		(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SOURCE_MARK_CATEGORY, GtkSourceMarkCategoryClass))

typedef struct _GtkSourceMarkCategory		GtkSourceMarkCategory;
typedef struct _GtkSourceMarkCategoryClass	GtkSourceMarkCategoryClass;
typedef struct _GtkSourceMarkCategoryPrivate	GtkSourceMarkCategoryPrivate;

struct _GtkSourceMarkCategory
{
	/*< private >*/
	GObject parent;

	GtkSourceMarkCategoryPrivate *priv;

	/*< public >*/
};

struct _GtkSourceMarkCategoryClass
{
	/*< private >*/
	GObjectClass parent_class;

	/*< public >*/
};

GType gtk_source_mark_category_get_type (void) G_GNUC_CONST;

GtkSourceMarkCategory *gtk_source_mark_category_new (const gchar *id);

const gchar     *gtk_source_mark_category_get_id              (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_background      (GtkSourceMarkCategory *category,
                                                               const GdkColor        *background);

gboolean         gtk_source_mark_category_get_background      (GtkSourceMarkCategory *category,
                                                               GdkColor              *background);

void             gtk_source_mark_category_set_priority        (GtkSourceMarkCategory *category,
                                                               gint                   priority);

gint             gtk_source_mark_category_get_priority        (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_stock_id        (GtkSourceMarkCategory *category,
                                                               const gchar           *stock_id);
const gchar     *gtk_source_mark_category_get_stock_id        (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_stock_detail    (GtkSourceMarkCategory *category,
                                                               const gchar           *stock_detail);
const gchar     *gtk_source_mark_category_get_stock_detail    (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_icon_name       (GtkSourceMarkCategory *category,
                                                               const gchar           *icon_name);
const gchar     *gtk_source_mark_category_get_stock_icon_name (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_gicon            (GtkSourceMarkCategory *category,
                                                                GIcon                 *gicon);
GIcon           *gtk_source_mark_category_get_gicon            (GtkSourceMarkCategory *category);

void             gtk_source_mark_category_set_pixbuf           (GtkSourceMarkCategory *category,
                                                                const GdkPixbuf       *pixbuf);
const GdkPixbuf *gtk_source_mark_category_get_pixbuf           (GtkSourceMarkCategory *category);

const GdkPixbuf *gtk_source_mark_category_render_icon          (GtkSourceMarkCategory *category,
                                                                GtkWidget             *widget,
                                                                gint                   size);

gchar           *gtk_source_mark_category_get_tooltip_text     (GtkSourceMarkCategory *category,
                                                                GtkSourceMark         *mark);

gchar           *gtk_source_mark_category_get_tooltip_markup   (GtkSourceMarkCategory *category,
                                                                GtkSourceMark         *mark);

G_END_DECLS

#endif /* __GTK_SOURCE_MARK_CATEGORY_H__ */
