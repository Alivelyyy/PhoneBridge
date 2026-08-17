#include "phonebridge/gui.h"
#include <gtk/gtk.h>
#include <cstdio>

namespace phonebridge {

struct Gui::Impl {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* deviceLabel = nullptr;
    GtkWidget* cameraLabel = nullptr;
    GtkWidget* audioLabel = nullptr;
    GtkWidget* fpsLabel = nullptr;
    GtkWidget* latencyLabel = nullptr;
    GtkWidget* framesLabel = nullptr;
    GtkWidget* droppedLabel = nullptr;
    GtkWidget* connectButton = nullptr;
    GtkWidget* startButton = nullptr;
    GtkWidget* stopButton = nullptr;
    GuiState state;
};

Gui::Gui() : impl_(new Impl()) {}

Gui::~Gui() {
    delete impl_;
}

static void onConnectClicked(GtkButton* button, gpointer data) {
    (void)button;
    auto* gui = static_cast<Gui*>(data);
    if (gui->onConnect) gui->onConnect();
}

static void onStartClicked(GtkButton* button, gpointer data) {
    (void)button;
    auto* gui = static_cast<Gui*>(data);
    if (gui->onStart) gui->onStart();
}

static void onStopClicked(GtkButton* button, gpointer data) {
    (void)button;
    auto* gui = static_cast<Gui*>(data);
    if (gui->onStop) gui->onStop();
}

static void activate(GtkApplication* app, gpointer data) {
    auto* gui = static_cast<Gui*>(data);
    auto* impl = gui->impl_;

    impl->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(impl->window), "PhoneBridge");
    gtk_window_set_default_size(GTK_WINDOW(impl->window), 380, 480);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(impl->window), vbox);

    GtkWidget* title = gtk_label_new("PhoneBridge");
    gtk_widget_add_css_class(title, "title");
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".title { font-size: 22px; font-weight: bold; }"
        ".stat-label { font-weight: bold; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    gtk_box_append(GTK_BOX(vbox), title);

    // Status section
    GtkWidget* statusFrame = gtk_frame_new("Connection");
    GtkWidget* statusGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(statusGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(statusGrid), 6);
    gtk_frame_set_child(GTK_FRAME(statusFrame), statusGrid);
    gtk_box_append(GTK_BOX(vbox), statusFrame);

    GtkWidget* s1 = gtk_label_new("USB:");
    gtk_widget_set_halign(s1, GTK_ALIGN_START);
    gtk_widget_add_css_class(s1, "stat-label");
    gtk_grid_attach(GTK_GRID(statusGrid), s1, 0, 0, 1, 1);
    impl->statusLabel = gtk_label_new("Disconnected");
    gtk_widget_set_halign(impl->statusLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), impl->statusLabel, 1, 0, 1, 1);

    GtkWidget* s2 = gtk_label_new("Device:");
    gtk_widget_set_halign(s2, GTK_ALIGN_START);
    gtk_widget_add_css_class(s2, "stat-label");
    gtk_grid_attach(GTK_GRID(statusGrid), s2, 0, 1, 1, 1);
    impl->deviceLabel = gtk_label_new("None");
    gtk_widget_set_halign(impl->deviceLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), impl->deviceLabel, 1, 1, 1, 1);

    // Stream section
    GtkWidget* streamFrame = gtk_frame_new("Stream");
    GtkWidget* streamGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(streamGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(streamGrid), 6);
    gtk_frame_set_child(GTK_FRAME(streamFrame), streamGrid);
    gtk_box_append(GTK_BOX(vbox), streamFrame);

    GtkWidget* c1 = gtk_label_new("Camera:");
    gtk_widget_set_halign(c1, GTK_ALIGN_START);
    gtk_widget_add_css_class(c1, "stat-label");
    gtk_grid_attach(GTK_GRID(streamGrid), c1, 0, 0, 1, 1);
    impl->cameraLabel = gtk_label_new("Stopped");
    gtk_widget_set_halign(impl->cameraLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), impl->cameraLabel, 1, 0, 1, 1);

    GtkWidget* c2 = gtk_label_new("Microphone:");
    gtk_widget_set_halign(c2, GTK_ALIGN_START);
    gtk_widget_add_css_class(c2, "stat-label");
    gtk_grid_attach(GTK_GRID(streamGrid), c2, 0, 1, 1, 1);
    impl->audioLabel = gtk_label_new("Stopped");
    gtk_widget_set_halign(impl->audioLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), impl->audioLabel, 1, 1, 1, 1);

    // Stats section
    GtkWidget* statsFrame = gtk_frame_new("Statistics");
    GtkWidget* statsGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(statsGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(statsGrid), 6);
    gtk_frame_set_child(GTK_FRAME(statsFrame), statsGrid);
    gtk_box_append(GTK_BOX(vbox), statsFrame);

    GtkWidget* st1 = gtk_label_new("Video FPS:");
    gtk_widget_set_halign(st1, GTK_ALIGN_START);
    gtk_widget_add_css_class(st1, "stat-label");
    gtk_grid_attach(GTK_GRID(statsGrid), st1, 0, 0, 1, 1);
    impl->fpsLabel = gtk_label_new("0");
    gtk_widget_set_halign(impl->fpsLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statsGrid), impl->fpsLabel, 1, 0, 1, 1);

    GtkWidget* st2 = gtk_label_new("Latency:");
    gtk_widget_set_halign(st2, GTK_ALIGN_START);
    gtk_widget_add_css_class(st2, "stat-label");
    gtk_grid_attach(GTK_GRID(statsGrid), st2, 0, 1, 1, 1);
    impl->latencyLabel = gtk_label_new("--");
    gtk_widget_set_halign(impl->latencyLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statsGrid), impl->latencyLabel, 1, 1, 1, 1);

    GtkWidget* st3 = gtk_label_new("Frames:");
    gtk_widget_set_halign(st3, GTK_ALIGN_START);
    gtk_widget_add_css_class(st3, "stat-label");
    gtk_grid_attach(GTK_GRID(statsGrid), st3, 0, 2, 1, 1);
    impl->framesLabel = gtk_label_new("0");
    gtk_widget_set_halign(impl->framesLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statsGrid), impl->framesLabel, 1, 2, 1, 1);

    GtkWidget* st4 = gtk_label_new("Dropped:");
    gtk_widget_set_halign(st4, GTK_ALIGN_START);
    gtk_widget_add_css_class(st4, "stat-label");
    gtk_grid_attach(GTK_GRID(statsGrid), st4, 0, 3, 1, 1);
    impl->droppedLabel = gtk_label_new("0");
    gtk_widget_set_halign(impl->droppedLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statsGrid), impl->droppedLabel, 1, 3, 1, 1);

    // Buttons
    GtkWidget* buttonBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(buttonBox, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), buttonBox);

    impl->connectButton = gtk_button_new_with_label("Connect");
    g_signal_connect(impl->connectButton, "clicked", G_CALLBACK(onConnectClicked), gui);
    gtk_box_append(GTK_BOX(buttonBox), impl->connectButton);

    impl->startButton = gtk_button_new_with_label("Start");
    gtk_widget_set_sensitive(impl->startButton, FALSE);
    g_signal_connect(impl->startButton, "clicked", G_CALLBACK(onStartClicked), gui);
    gtk_box_append(GTK_BOX(buttonBox), impl->startButton);

    impl->stopButton = gtk_button_new_with_label("Stop");
    gtk_widget_set_sensitive(impl->stopButton, FALSE);
    g_signal_connect(impl->stopButton, "clicked", G_CALLBACK(onStopClicked), gui);
    gtk_box_append(GTK_BOX(buttonBox), impl->stopButton);

    gtk_widget_set_visible(impl->window, TRUE);
}

bool Gui::initialize(int argc, char* argv[]) {
    (void)argc; (void)argv;
    impl_->app = gtk_application_new("com.phonebridge", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(impl_->app, "activate", G_CALLBACK(activate), this);
    return true;
}

void Gui::run() {
    g_application_run(G_APPLICATION(impl_->app), 0, nullptr);
    g_object_unref(impl_->app);
}

void Gui::quit() {
    if (impl_->window) {
        gtk_window_close(GTK_WINDOW(impl_->window));
    }
}

void Gui::updateState(const GuiState& state) {
    impl_->state = state;
    if (impl_->statusLabel) gtk_label_set_text(GTK_LABEL(impl_->statusLabel), state.status.c_str());
    if (impl_->deviceLabel) gtk_label_set_text(GTK_LABEL(impl_->deviceLabel), state.device.c_str());
    if (impl_->cameraLabel) gtk_label_set_text(GTK_LABEL(impl_->cameraLabel), state.camera.c_str());
    if (impl_->audioLabel) gtk_label_set_text(GTK_LABEL(impl_->audioLabel), state.microphone.c_str());

    char buf[64];
    snprintf(buf, sizeof(buf), "%d", state.videoFps);
    if (impl_->fpsLabel) gtk_label_set_text(GTK_LABEL(impl_->fpsLabel), buf);

    if (state.latencyMs > 0) {
        snprintf(buf, sizeof(buf), "%.1f ms", state.latencyMs);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    if (impl_->latencyLabel) gtk_label_set_text(GTK_LABEL(impl_->latencyLabel), buf);

    snprintf(buf, sizeof(buf), "%lu video, %lu audio",
             (unsigned long)state.videoFrames, (unsigned long)state.audioFrames);
    if (impl_->framesLabel) gtk_label_set_text(GTK_LABEL(impl_->framesLabel), buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)state.droppedFrames);
    if (impl_->droppedLabel) gtk_label_set_text(GTK_LABEL(impl_->droppedLabel), buf);

    bool connected = (state.status == "Connected");
    bool streaming = connected && state.videoFps > 0;
    gtk_widget_set_sensitive(impl_->connectButton, !connected);
    gtk_widget_set_sensitive(impl_->startButton, connected && !streaming);
    gtk_widget_set_sensitive(impl_->stopButton, streaming);
}

} // namespace phonebridge
