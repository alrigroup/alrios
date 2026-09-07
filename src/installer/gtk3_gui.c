/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "gtk3_gui.h"

#ifdef HAS_GTK3
#include <gtk/gtk.h>

typedef struct {
    installer_config_t *cfg;
    GtkWidget *window;
    GtkWidget *entry_dest;
    GtkWidget *chk_path;
    GtkWidget *chk_gcc;
    GtkWidget *chk_node;
    GtkWidget *chk_python;
    GtkWidget *progress_bar;
    GtkWidget *lbl_status;
    GtkWidget *btn_install;
    GtkWidget *text_view;
    GtkTextBuffer *text_buffer;
} gtk_app_state_t;

static gtk_app_state_t g_gui_state;

static void gtk_on_log(const char *msg, void *user_data) {
    (void)user_data;
    if (!g_gui_state.text_buffer) return;
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(g_gui_state.text_buffer, &end);
    gtk_text_buffer_insert(g_gui_state.text_buffer, &end, msg, -1);
    while (gtk_events_pending()) gtk_main_iteration();
}

static void gtk_on_progress(int step, int total, int pct, const char *name, void *user_data) {
    (void)user_data;
    if (g_gui_state.progress_bar) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_gui_state.progress_bar), pct / 100.0);
    }
    if (g_gui_state.lbl_status) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[%d/%d] %s (%d%%)", step, total, name, pct);
        gtk_label_set_text(GTK_LABEL(g_gui_state.lbl_status), buf);
    }
    while (gtk_events_pending()) gtk_main_iteration();
}

static void gtk_on_complete(int success, const char *msg, void *user_data) {
    (void)user_data;
    if (g_gui_state.lbl_status) {
        gtk_label_set_text(GTK_LABEL(g_gui_state.lbl_status), msg);
    }
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(g_gui_state.window),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        success ? GTK_MESSAGE_INFO : GTK_MESSAGE_ERROR,
        GTK_BUTTONS_OK,
        "%s", msg
    );
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void on_browse_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Selecionar Pasta de Instalacao",
        GTK_WINDOW(g_gui_state.window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancelar", GTK_RESPONSE_CANCEL,
        "_Selecionar", GTK_RESPONSE_ACCEPT,
        NULL
    );
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            gtk_entry_set_text(GTK_ENTRY(g_gui_state.entry_dest), filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_install_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    gtk_widget_set_sensitive(g_gui_state.btn_install, FALSE);

    /* Atualizar configuracao a partir dos campos */
    const char *text = gtk_entry_get_text(GTK_ENTRY(g_gui_state.entry_dest));
    if (text && text[0]) {
        strncpy(g_gui_state.cfg->dest_dir, text, sizeof(g_gui_state.cfg->dest_dir) - 1);
    }
    g_gui_state.cfg->add_to_path = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_path));
    g_gui_state.cfg->install_gcc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_gcc));
    g_gui_state.cfg->install_node = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_node));
    g_gui_state.cfg->install_python = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_python));

    installer_callbacks_t cb = {
        .on_log = gtk_on_log,
        .on_progress = gtk_on_progress,
        .on_complete = gtk_on_complete
    };

    installer_run(g_gui_state.cfg, &cb, NULL);
    gtk_widget_set_sensitive(g_gui_state.btn_install, TRUE);
}

int run_gtk3_installer(int argc, char **argv, installer_config_t *cfg) {
    if (!gtk_init_check(&argc, &argv)) {
        return -1; /* Display nao disponivel -> fallback para CLI */
    }

    g_gui_state.cfg = cfg;
    if (!cfg->dest_dir[0]) {
        installer_get_default_paths(cfg->dest_dir, sizeof(cfg->dest_dir), NULL);
    }

    /* Janela Principal */
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_gui_state.window = win;
    gtk_window_set_title(GTK_WINDOW(win), "ALRIOS — Instalador Oficial");
    gtk_window_set_default_size(GTK_WINDOW(win), 580, 480);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 16);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* Titulo */
    GtkWidget *lbl_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_title), "<span size='xx-large' weight='bold'>ALRIOS</span>\n<span size='medium' color='#555'>Sovereign Operating System & Microkernel Ecosystem</span>");
    gtk_label_set_xalign(GTK_LABEL(lbl_title), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_title, FALSE, FALSE, 0);

    /* Destino */
    GtkWidget *lbl_dest = gtk_label_new("Pasta de Instalacao:");
    gtk_label_set_xalign(GTK_LABEL(lbl_dest), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), lbl_dest, FALSE, FALSE, 0);

    GtkWidget *hbox_dest = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_gui_state.entry_dest = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_gui_state.entry_dest), cfg->dest_dir);
    gtk_box_pack_start(GTK_BOX(hbox_dest), g_gui_state.entry_dest, TRUE, TRUE, 0);

    GtkWidget *btn_browse = gtk_button_new_with_label("Procurar...");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_dest), btn_browse, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_dest, FALSE, FALSE, 0);

    /* Opcoes */
    GtkWidget *frame_opts = gtk_frame_new("Opcoes de Ambiente & Toolchains");
    GtkWidget *box_opts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box_opts), 8);
    gtk_container_add(GTK_CONTAINER(frame_opts), box_opts);

    g_gui_state.chk_path = gtk_check_button_new_with_label("Registrar comandos globais no PATH do sistema (/usr/local/bin)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_path), cfg->add_to_path);
    gtk_box_pack_start(GTK_BOX(box_opts), g_gui_state.chk_path, FALSE, FALSE, 0);

    g_gui_state.chk_gcc = gtk_check_button_new_with_label("Auto-instalar Compilador GCC / C/C++ se ausente");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_gcc), cfg->install_gcc);
    gtk_box_pack_start(GTK_BOX(box_opts), g_gui_state.chk_gcc, FALSE, FALSE, 0);

    g_gui_state.chk_node = gtk_check_button_new_with_label("Auto-instalar Runtime Node.js");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_node), cfg->install_node);
    gtk_box_pack_start(GTK_BOX(box_opts), g_gui_state.chk_node, FALSE, FALSE, 0);

    g_gui_state.chk_python = gtk_check_button_new_with_label("Auto-instalar Runtime Python");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_gui_state.chk_python), cfg->install_python);
    gtk_box_pack_start(GTK_BOX(box_opts), g_gui_state.chk_python, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), frame_opts, FALSE, FALSE, 0);

    /* Barra de Progresso e Status */
    g_gui_state.lbl_status = gtk_label_new("Pronto para instalar.");
    gtk_label_set_xalign(GTK_LABEL(g_gui_state.lbl_status), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), g_gui_state.lbl_status, FALSE, FALSE, 0);

    g_gui_state.progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), g_gui_state.progress_bar, FALSE, FALSE, 0);

    /* Log Output */
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scrolled, -1, 100);
    g_gui_state.text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_gui_state.text_view), FALSE);
    g_gui_state.text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_gui_state.text_view));
    gtk_container_add(GTK_CONTAINER(scrolled), g_gui_state.text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);

    /* Botoes de Acao */
    GtkWidget *hbox_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *btn_close = gtk_button_new_with_label("Fechar");
    g_signal_connect(btn_close, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_actions), btn_close, FALSE, FALSE, 0);

    g_gui_state.btn_install = gtk_button_new_with_label("Instalar ALRIOS");
    g_signal_connect(g_gui_state.btn_install, "clicked", G_CALLBACK(on_install_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_actions), g_gui_state.btn_install, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_actions, FALSE, FALSE, 0);

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}

#else
int run_gtk3_installer(int argc, char **argv, installer_config_t *cfg) {
    (void)argc; (void)argv; (void)cfg;
    return -1;
}
#endif
