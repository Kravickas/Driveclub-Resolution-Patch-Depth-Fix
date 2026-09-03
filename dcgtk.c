/* The window, for Linux and for macOS. The same program underneath as... */

#include <gtk/gtk.h>
#include <string.h>

int dcfix_sweep(const char *dir, void (*say)(const char *));
int dcfix_already(void);

static GtkWidget *g_view, *g_pick, *g_go;
static GtkTextBuffer *g_buf;
static char g_dir[2048];

static void put(const char *s)
{
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(g_buf, &end);
    gtk_text_buffer_insert(g_buf, &end, s, -1);
    while (gtk_events_pending()) gtk_main_iteration();
}

static void say(const char *s) { put(s); }

static void clear(void)
{
    gtk_text_buffer_set_text(g_buf, "", -1);
}

static void on_pick(GtkWidget *w, gpointer data)
{
    GtkWidget *dlg;
    (void)w; (void)data;
    dlg = gtk_file_chooser_dialog_new("The game patch folder",
                                      GTK_WINDOW(gtk_widget_get_toplevel(g_pick)),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                      "_Cancel", GTK_RESPONSE_CANCEL,
                                      "_Choose", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *p = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        if (p) {
            g_strlcpy(g_dir, p, sizeof g_dir);
            g_free(p);
            gtk_button_set_label(GTK_BUTTON(g_pick), g_dir);
            gtk_widget_set_sensitive(g_go, TRUE);
            clear();
            put("Ready. Press Patch.\n");
        }
    }
    gtk_widget_destroy(dlg);
}

static void on_go(GtkWidget *w, gpointer data)
{
    int n;
    (void)w; (void)data;
    if (!g_dir[0]) return;
    clear();
    gtk_widget_set_sensitive(g_go, FALSE);
    gtk_widget_set_sensitive(g_pick, FALSE);
    n = dcfix_sweep(g_dir, say);
    if (n > 0) {
        put("\nPATCHED SUCCESSFULLY!\n");
        put("Delete the shader cache before you play.\n");
    } else if (dcfix_already()) {
        put("\nALREADY PATCHED!\n");
    } else {
        put("\nNOTHING TO PATCH! Point it at your game patch folder,\n");
        put("the one holding global.rpk.\n");
    }
    gtk_widget_set_sensitive(g_go, TRUE);
    gtk_widget_set_sensitive(g_pick, TRUE);
}

int main(int argc, char **argv)
{
    GtkWidget *win, *box, *scroll;

    gtk_init(&argc, &argv);

    win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "DC Res Patch Depth Fix");
    gtk_window_set_default_size(GTK_WINDOW(win), 620, 360);
    gtk_container_set_border_width(GTK_CONTAINER(win), 10);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(win), box);

    g_pick = gtk_button_new_with_label("Select game patch folder...");
    g_signal_connect(g_pick, "clicked", G_CALLBACK(on_pick), NULL);
    gtk_box_pack_start(GTK_BOX(box), g_pick, FALSE, FALSE, 0);

    g_go = gtk_button_new_with_label("Patch");
    gtk_widget_set_sensitive(g_go, FALSE);
    g_signal_connect(g_go, "clicked", G_CALLBACK(on_go), NULL);
    gtk_box_pack_start(GTK_BOX(box), g_go, FALSE, FALSE, 0);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    g_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(g_view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(g_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(g_view), 8);
    g_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_view));
    gtk_container_add(GTK_CONTAINER(scroll), g_view);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    put("The game tests depth against a fixed 960 x 540, which is wrong at\n");
    put("any other resolution and drops parts of the scene.\n\n");
    put("Pick your game's patch folder, the one holding global.rpk.\n");

    if (argc > 1) {
        g_strlcpy(g_dir, argv[1], sizeof g_dir);
        gtk_button_set_label(GTK_BUTTON(g_pick), g_dir);
        gtk_widget_set_sensitive(g_go, TRUE);
    }

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
