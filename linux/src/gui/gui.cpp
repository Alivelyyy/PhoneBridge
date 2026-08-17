#include "phonebridge/gui.h"
#include <gtk/gtk.h>

namespace phonebridge {

struct Gui::Impl {
    GtkApplication* app = nullptr;
    GtkWidget* window = nullptr;
    GtkWidget* statusLabel = nullptr;
    GtkWidget* deviceLabel = nullptr;
    GtkWidget* cameraLabel = nullptr;
    GtkWidget* audioLabel = nullptr;
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
    auto* gui = static_cast<Gui*>(data);
    if (gui->onConnect) gui->onConnect();
}

static void onStartClicked(GtkButton* button, gpointer data) {
    auto* gui = static_cast<Gui*>(data);
    if (gui->onStart) gui->onStart();
}

static void onStopClicked(GtkButton* button, gpointer data) {
    auto* gui = static_cast<Gui*>(data);
    if (gui->onStop) gui->onStop();
}

static void activate(GtkApplication* app, gpointer data) {
    auto* gui = static_cast<Gui*>(data);
    auto* impl = gui->impl_;

    impl->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(impl->window), "PhoneBridge");
    gtk_window_set_default_size(GTK_WINDOW(impl->window), 350, 400);

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 24);
    gtk_widget_set_margin_bottom(vbox, 24);
    gtk_window_set_child(GTK_WINDOW(impl->window), vbox);

    // Title
    GtkWidget* title = gtk_label_new("PhoneBridge");
    gtk_widget_add_css_class(title, "title");
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css, ".title { font-size: 24px; font-weight: bold; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    gtk_box_append(GTK_BOX(vbox), title);

    // Status section
    GtkWidget* statusFrame = gtk_frame_new("Status");
    GtkWidget* statusGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(statusGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(statusGrid), 8);
    gtk_frame_set_child(GTK_FRAME(statusFrame), statusGrid);
    gtk_box_append(GTK_BOX(vbox), statusFrame);

    GtkWidget* statusName = gtk_label_new("USB:");
    gtk_widget_set_halign(statusName, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), statusName, 0, 0, 1, 1);

    impl->statusLabel = gtk_label_new("Disconnected");
    gtk_widget_set_halign(impl->statusLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), impl->statusLabel, 1, 0, 1, 1);

    GtkWidget* deviceName = gtk_label_new("Device:");
    gtk_widget_set_halign(deviceName, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), deviceName, 0, 1, 1, 1);

    impl->deviceLabel = gtk_label_new("None");
    gtk_widget_set_halign(impl->deviceLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(statusGrid), impl->deviceLabel, 1, 1, 1, 1);

    // Stream section
    GtkWidget* streamFrame = gtk_frame_new("Stream");
    GtkWidget* streamGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(streamGrid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(streamGrid), 8);
    gtk_frame_set_child(GTK_FRAME(streamFrame), streamGrid);
    gtk_box_append(GTK_BOX(vbox), streamFrame);

    GtkWidget* cameraName = gtk_label_new("Camera:");
    gtk_widget_set_halign(cameraName, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), cameraName, 0, 0, 1, 1);

    impl->cameraLabel = gtk_label_new("Stopped");
    gtk_widget_set_halign(impl->cameraLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), impl->cameraLabel, 1, 0, 1, 1);

    GtkWidget* audioName = gtk_label_new("Microphone:");
    gtk_widget_set_halign(audioName, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), audioName, 0, 1, 1, 1);

    impl->audioLabel = gtk_label_new("Stopped");
    gtk_widget_set_halign(impl->audioLabel, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(streamGrid), impl->audioLabel, 1, 1, 1, 1);

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
    impl_->app = gtk_application_new("com.phonebridge", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(impl_->app, "activate", G_CALLBACK(activate), this);
    return true;
}

void Gui::run() {
    int status = g_application_run(G_APPLICATION(impl_->app), 0, nullptr);
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

    bool connected = (state.status == "Connected");
    bool streaming = connected && state.camera != "Stopped" && state.camera != "Ready";
    gtk_widget_set_sensitive(impl_->connectButton, !connected);
    gtk_widget_set_sensitive(impl_->startButton, connected && !streaming);
    gtk_widget_set_sensitive(impl_->stopButton, streaming);
}

} // namespace phonebridge
