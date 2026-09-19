#include "gflai_scenes.h"
#include "gflai_tx.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_light.h>
#include <gui/gui.h>
#include <input/input.h>

#define TAG "GflaiNeon"

#define BRIGHTNESS_MAX     15
#define BRIGHTNESS_DEFAULT 15
/* The Flipper's own LED mirrors the band; full scale is dazzling up close. */
#define LED_SCALE 10

typedef struct {
    uint8_t scene;
    uint8_t brightness;
    bool run;
    bool quit;
    uint32_t tick;
    uint8_t r, g, b;
    bool tx_phase;
    bool radio_failed;
    GflaiTxStatus radio_status;
} GflaiModel;

typedef struct {
    GflaiModel model;
    FuriMutex* mutex;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;
    FuriThread* worker;
    GflaiTx* tx;
} GflaiApp;

static void model_lock(GflaiApp* app) {
    furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
}

static void model_unlock(GflaiApp* app) {
    furi_check(furi_mutex_release(app->mutex) == FuriStatusOk);
}

/* ---------------------------------------------------------------- drawing */

static void draw_bar(Canvas* canvas, const char* label, int y, uint8_t value, uint8_t max) {
    const int x = 22;
    const int w = 103;
    const int h = 6;

    canvas_draw_str(canvas, 2, y + 5, label);
    canvas_draw_frame(canvas, x, y, w, h);

    int fill = ((w - 2) * value) / max;
    if(fill > 0) canvas_draw_box(canvas, x + 1, y + 1, fill, h - 2);
}

static void gflai_draw(Canvas* canvas, void* context) {
    GflaiApp* app = context;

    model_lock(app);
    GflaiModel m = app->model;
    model_unlock(app);

    canvas_clear(canvas);

    canvas_draw_box(canvas, 0, 0, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 9, "GFLAI NEON");

    canvas_set_font(canvas, FontSecondary);
    if(m.radio_failed) {
        canvas_draw_str_aligned(canvas, 125, 6, AlignRight, AlignCenter, "ERR");
    } else if(m.run) {
        canvas_draw_str_aligned(canvas, 125, 6, AlignRight, AlignCenter, "RUN");
        if(m.tx_phase) canvas_draw_box(canvas, 100, 3, 5, 5);
    } else {
        canvas_draw_str_aligned(canvas, 125, 6, AlignRight, AlignCenter, "IDLE");
    }
    canvas_set_color(canvas, ColorBlack);

    if(m.radio_failed) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "Radio unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, 44, AlignCenter, AlignCenter, gflai_tx_status_text(m.radio_status));
        return;
    }

    const GflaiScene* scene = &gflai_scenes[m.scene];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 22, scene->name);

    canvas_set_font(canvas, FontSecondary);
    char counter[12];
    snprintf(counter, sizeof(counter), "%u/%u", m.scene + 1u, (unsigned)gflai_scene_count);
    canvas_draw_str_aligned(canvas, 125, 19, AlignRight, AlignCenter, counter);

    canvas_draw_str(canvas, 3, 31, "<>scene  ^v bri  OK:go");

    draw_bar(canvas, "R", 34, m.r, BRIGHTNESS_MAX);
    draw_bar(canvas, "G", 41, m.g, BRIGHTNESS_MAX);
    draw_bar(canvas, "B", 48, m.b, BRIGHTNESS_MAX);
    draw_bar(canvas, "BRI", 56, m.brightness, BRIGHTNESS_MAX);
}

static void gflai_input(InputEvent* event, void* context) {
    GflaiApp* app = context;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

/* ----------------------------------------------------------------- worker */

static void led_mirror(uint8_t r, uint8_t g, uint8_t b) {
    furi_hal_light_set(LightRed, (uint8_t)(r * LED_SCALE));
    furi_hal_light_set(LightGreen, (uint8_t)(g * LED_SCALE));
    furi_hal_light_set(LightBlue, (uint8_t)(b * LED_SCALE));
}

static int32_t gflai_worker(void* context) {
    GflaiApp* app = context;

    GflaiTxStatus status = gflai_tx_open(app->tx);
    if(status != GflaiTxOk) {
        FURI_LOG_E(TAG, "radio open failed: %d", status);
        model_lock(app);
        app->model.radio_failed = true;
        app->model.radio_status = status;
        app->model.run = false;
        model_unlock(app);
        view_port_update(app->view_port);
        return 0;
    }

    uint32_t deadline = furi_get_tick();

    while(true) {
        model_lock(app);
        bool quit = app->model.quit;
        bool run = app->model.run;
        uint8_t scene_index = app->model.scene;
        uint8_t brightness = app->model.brightness;
        uint32_t tick = app->model.tick;
        model_unlock(app);

        if(quit) break;

        if(!run) {
            furi_delay_ms(40);
            deadline = furi_get_tick();
            continue;
        }

        const GflaiScene* scene = &gflai_scenes[scene_index];
        uint8_t r, g, b;
        gflai_scene_render(scene, tick, brightness, &r, &g, &b);

        gflai_tx_send(app->tx, r, g, b);
        led_mirror(r, g, b);

        model_lock(app);
        app->model.tick++;
        app->model.r = r;
        app->model.g = g;
        app->model.b = b;
        app->model.tx_phase = !app->model.tx_phase;
        model_unlock(app);
        view_port_update(app->view_port);

        deadline += furi_ms_to_ticks(scene->step_ms);
        uint32_t now = furi_get_tick();
        if(deadline > now) {
            furi_delay_tick(deadline - now);
        } else {
            /* A block outlasts the requested step; just run back to back. */
            deadline = now;
        }
    }

    gflai_tx_close(app->tx);
    led_mirror(0, 0, 0);
    return 0;
}

/* -------------------------------------------------------------- app plumbing */

static void scene_step(GflaiApp* app, int delta) {
    model_lock(app);
    int next = (int)app->model.scene + delta;
    if(next < 0) next = gflai_scene_count - 1;
    if(next >= gflai_scene_count) next = 0;
    app->model.scene = (uint8_t)next;
    /* Restart the animation so a scene always begins at its first frame. */
    app->model.tick = 0;
    model_unlock(app);
}

static void brightness_step(GflaiApp* app, int delta) {
    model_lock(app);
    int next = (int)app->model.brightness + delta;
    if(next < 0) next = 0;
    if(next > BRIGHTNESS_MAX) next = BRIGHTNESS_MAX;
    app->model.brightness = (uint8_t)next;
    model_unlock(app);
}

static GflaiApp* gflai_app_alloc(void) {
    GflaiApp* app = malloc(sizeof(GflaiApp));

    app->model = (GflaiModel){
        .scene = 0,
        .brightness = BRIGHTNESS_DEFAULT,
        .run = false,
        .quit = false,
        .tick = 0,
        .r = 0,
        .g = 0,
        .b = 0,
        .tx_phase = false,
        .radio_failed = false,
        .radio_status = GflaiTxOk,
    };

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->tx = gflai_tx_alloc();

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, gflai_draw, app);
    view_port_input_callback_set(app->view_port, gflai_input, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->worker = furi_thread_alloc_ex("GflaiTxWorker", 2048, gflai_worker, app);
    furi_thread_start(app->worker);

    return app;
}

static void gflai_app_free(GflaiApp* app) {
    model_lock(app);
    app->model.quit = true;
    model_unlock(app);
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);

    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);

    gflai_tx_free(app->tx);
    furi_message_queue_free(app->input_queue);
    furi_mutex_free(app->mutex);
    free(app);
}

int32_t gflai_neon_app(void* p) {
    UNUSED(p);

    GflaiApp* app = gflai_app_alloc();
    InputEvent event;
    bool exit = false;

    while(!exit) {
        if(furi_message_queue_get(app->input_queue, &event, 200) != FuriStatusOk) continue;

        bool pressed = (event.type == InputTypeShort) || (event.type == InputTypeRepeat);

        switch(event.key) {
        case InputKeyBack:
            if(event.type == InputTypeShort || event.type == InputTypeLong) exit = true;
            break;

        case InputKeyLeft:
            if(pressed) scene_step(app, -1);
            break;

        case InputKeyRight:
            if(pressed) scene_step(app, +1);
            break;

        case InputKeyUp:
            if(pressed) brightness_step(app, +1);
            break;

        case InputKeyDown:
            if(pressed) brightness_step(app, -1);
            break;

        case InputKeyOk:
            if(event.type == InputTypeShort) {
                model_lock(app);
                if(!app->model.radio_failed) app->model.run = !app->model.run;
                model_unlock(app);
            } else if(event.type == InputTypeLong) {
                /* Panic blackout: jump to the last scene and make sure it goes out. */
                model_lock(app);
                if(!app->model.radio_failed) {
                    app->model.scene = gflai_scene_count - 1;
                    app->model.tick = 0;
                    app->model.run = true;
                }
                model_unlock(app);
            }
            break;

        default:
            break;
        }

        view_port_update(app->view_port);
    }

    gflai_app_free(app);
    return 0;
}
