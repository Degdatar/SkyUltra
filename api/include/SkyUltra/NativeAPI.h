#pragma once

#include <stddef.h>
#include <stdint.h>

/* Use backend-default cadence (CEF remains capped at 120 FPS). 0 keeps the 60 FPS API default. */
#define SKYULTRA_FPS_UNCAPPED UINT32_MAX

#if defined(_WIN32)
#define SKYULTRA_CALL __cdecl
#if defined(SKYULTRA_BUILDING_PLUGIN)
#define SKYULTRA_EXPORT __declspec(dllexport)
#else
#define SKYULTRA_EXPORT
#endif
#else
#define SKYULTRA_CALL
#define SKYULTRA_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t SkyUltraViewHandle;
typedef uint64_t SkyUltraModHandle;

typedef enum SkyUltraResult {
    SKYULTRA_OK = 0,
    SKYULTRA_INVALID_ARGUMENT = 1,
    SKYULTRA_NOT_INITIALIZED = 2,
    SKYULTRA_NOT_FOUND = 3,
    SKYULTRA_UNSUPPORTED = 4,
    SKYULTRA_INTERNAL_ERROR = 5,
    SKYULTRA_BUFFER_TOO_SMALL = 6,
    SKYULTRA_QUEUE_FULL = 7,
} SkyUltraResult;

typedef enum SkyUltraViewStateV1 {
    SKYULTRA_VIEW_PENDING = 0,
    SKYULTRA_VIEW_READY = 1,
    SKYULTRA_VIEW_FAILED = 2,
    SKYULTRA_VIEW_CLOSING = 3,
} SkyUltraViewStateV1;

typedef enum SkyUltraLayerV1 {
    SKYULTRA_LAYER_HUD = 0,
    SKYULTRA_LAYER_PANEL = 1,
    SKYULTRA_LAYER_POPUP = 2,
    SKYULTRA_LAYER_SYSTEM = 3,
} SkyUltraLayerV1;

typedef enum SkyUltraAnchorV1 {
    SKYULTRA_ANCHOR_TOP_LEFT = 0,
    SKYULTRA_ANCHOR_TOP_RIGHT = 1,
    SKYULTRA_ANCHOR_BOTTOM_LEFT = 2,
    SKYULTRA_ANCHOR_BOTTOM_RIGHT = 3,
    SKYULTRA_ANCHOR_CENTER = 4,
} SkyUltraAnchorV1;

typedef enum SkyUltraBrowserEngineV1 {
    SKYULTRA_BROWSER_ENGINE_DEFAULT = 0,
    SKYULTRA_BROWSER_ENGINE_ULTRALIGHT = 1,
    SKYULTRA_BROWSER_ENGINE_CEF = 2,
} SkyUltraBrowserEngineV1;

typedef struct SkyUltraRectV1 {
    float x;
    float y;
    float width;
    float height;
} SkyUltraRectV1;

typedef struct SkyUltraViewConfigV1 {
    uint32_t struct_size;
    const char* id_utf8;
    uint32_t id_length;
    const char* initial_url_utf8;
    uint32_t initial_url_length;
    uint32_t pixel_width;
    uint32_t pixel_height;
    SkyUltraRectV1 bounds;
    SkyUltraRectV1 clip;
    float opacity;
    float scale;
    int32_t z_index;
    /* 0 keeps 60 FPS; positive caps are honored. CEF never exceeds 120 FPS, including the sentinel. */
    uint32_t max_fps;
    uint32_t flags;
    uint32_t layer;
    uint32_t anchor;
    // Optional trailing field. DEFAULT uses the plugin's configured default;
    // ULTRALIGHT and CEF request that backend for this view. For manifest-backed
    // mods, manifest.browserEngine takes precedence when present. Read only
    // when struct_size includes this member. Original V1 callers use the default.
    uint32_t browser_engine;
} SkyUltraViewConfigV1;

enum {
    SKYULTRA_VIEW_VISIBLE = 1u << 0,
    SKYULTRA_VIEW_TRANSPARENT = 1u << 1,
    SKYULTRA_VIEW_CLICK_THROUGH = 1u << 2,
    SKYULTRA_VIEW_MODAL = 1u << 3,
    SKYULTRA_VIEW_ENABLE_JAVASCRIPT = 1u << 4,
    SKYULTRA_VIEW_ENABLE_GAMEPAD = 1u << 5,
};

typedef struct SkyUltraNativeAPI_V1 {
    uint32_t abi_version;
    uint32_t struct_size;

    SkyUltraResult(SKYULTRA_CALL* create_view)(
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    SkyUltraResult(SKYULTRA_CALL* get_view_state)(
        SkyUltraViewHandle view,
        SkyUltraViewStateV1* out_state);
    SkyUltraResult(SKYULTRA_CALL* destroy_view)(SkyUltraViewHandle view);
    SkyUltraResult(SKYULTRA_CALL* set_visible)(SkyUltraViewHandle view, uint8_t visible);
    SkyUltraResult(SKYULTRA_CALL* set_bounds)(SkyUltraViewHandle view, SkyUltraRectV1 bounds);
    SkyUltraResult(SKYULTRA_CALL* navigate)(
        SkyUltraViewHandle view,
        const char* url_utf8,
        uint32_t url_length);
    SkyUltraResult(SKYULTRA_CALL* send_event_json)(
        SkyUltraViewHandle view,
        const char* event_name_utf8,
        uint32_t event_name_length,
        const char* json_utf8,
        uint32_t json_length);
} SkyUltraNativeAPI_V1;

typedef enum SkyUltraPageRequestEventV2 {
    SKYULTRA_PAGE_REQUEST_V2 = 0,
    SKYULTRA_PAGE_REQUEST_CANCELLED_V2 = 1,
} SkyUltraPageRequestEventV2;

/*
 * Called on Skyrim's game thread. The UTF-8 strings are borrowed for the duration of the
 * callback. A request callback may retain copies and complete the request later from any thread.
 * Cancellation callbacks report AbortSignal cancellation while a request is pending, or view
 * destruction. A cancelled request must not be completed.
 */
typedef void(SKYULTRA_CALL* SkyUltraPageRequestCallbackV2)(
    void* user_data,
    SkyUltraViewHandle view,
    const char* request_id_utf8,
    uint32_t request_id_length,
    const char* name_utf8,
    uint32_t name_length,
    const char* params_json_utf8,
    uint32_t params_json_length,
    SkyUltraPageRequestEventV2 event_type);

/* Method names are UTF-8 byte spans copied during set_page_request_allowlist. */
typedef struct SkyUltraPageRequestMethodV2 {
    const char* name_utf8;
    uint32_t name_length;
} SkyUltraPageRequestMethodV2;

/* V2 preserves the V1 function layout and appends the asynchronous page-request API. */
typedef struct SkyUltraNativeAPI_V2 {
    uint32_t abi_version;
    uint32_t struct_size;

    SkyUltraResult(SKYULTRA_CALL* create_view)(
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    SkyUltraResult(SKYULTRA_CALL* get_view_state)(
        SkyUltraViewHandle view,
        SkyUltraViewStateV1* out_state);
    SkyUltraResult(SKYULTRA_CALL* destroy_view)(SkyUltraViewHandle view);
    SkyUltraResult(SKYULTRA_CALL* set_visible)(SkyUltraViewHandle view, uint8_t visible);
    SkyUltraResult(SKYULTRA_CALL* set_bounds)(SkyUltraViewHandle view, SkyUltraRectV1 bounds);
    SkyUltraResult(SKYULTRA_CALL* navigate)(
        SkyUltraViewHandle view,
        const char* url_utf8,
        uint32_t url_length);
    SkyUltraResult(SKYULTRA_CALL* send_event_json)(
        SkyUltraViewHandle view,
        const char* event_name_utf8,
        uint32_t event_name_length,
        const char* json_utf8,
        uint32_t json_length);

    /* Passing NULL unregisters the callback. Requests and cancellations use one callback. */
    SkyUltraResult(SKYULTRA_CALL* set_page_request_callback)(
        SkyUltraPageRequestCallbackV2 callback,
        void* user_data);
    /* May be called from any thread. SKYULTRA_OK means the completion was queued. */
    SkyUltraResult(SKYULTRA_CALL* complete_page_request)(
        SkyUltraViewHandle view,
        const char* request_id_utf8,
        uint32_t request_id_length,
        uint8_t success,
        const char* result_json_utf8,
        uint32_t result_json_length,
        const char* error_utf8,
        uint32_t error_length);
    /* Replaces the per-view allowlist. It defaults to empty; an empty list denies all methods.
     * The update affects new requests; requests already dispatched remain pending. */
    SkyUltraResult(SKYULTRA_CALL* set_page_request_allowlist)(
        SkyUltraViewHandle view,
        const SkyUltraPageRequestMethodV2* methods,
        uint32_t method_count);
} SkyUltraNativeAPI_V2;

/* V3 preserves V2's member order and adds manifest-scoped mod registration and storage. */
typedef struct SkyUltraNativeAPI_V3 {
    uint32_t abi_version;
    uint32_t struct_size;

    /* V2-compatible unscoped entry point; new integrations should use create_mod_view. */
    SkyUltraResult(SKYULTRA_CALL* create_view)(
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    SkyUltraResult(SKYULTRA_CALL* get_view_state)(
        SkyUltraViewHandle view,
        SkyUltraViewStateV1* out_state);
    SkyUltraResult(SKYULTRA_CALL* destroy_view)(SkyUltraViewHandle view);
    SkyUltraResult(SKYULTRA_CALL* set_visible)(SkyUltraViewHandle view, uint8_t visible);
    SkyUltraResult(SKYULTRA_CALL* set_bounds)(SkyUltraViewHandle view, SkyUltraRectV1 bounds);
    SkyUltraResult(SKYULTRA_CALL* navigate)(
        SkyUltraViewHandle view,
        const char* url_utf8,
        uint32_t url_length);
    SkyUltraResult(SKYULTRA_CALL* send_event_json)(
        SkyUltraViewHandle view,
        const char* event_name_utf8,
        uint32_t event_name_length,
        const char* json_utf8,
        uint32_t json_length);

    SkyUltraResult(SKYULTRA_CALL* set_page_request_callback)(
        SkyUltraPageRequestCallbackV2 callback,
        void* user_data);
    SkyUltraResult(SKYULTRA_CALL* complete_page_request)(
        SkyUltraViewHandle view,
        const char* request_id_utf8,
        uint32_t request_id_length,
        uint8_t success,
        const char* result_json_utf8,
        uint32_t result_json_length,
        const char* error_utf8,
        uint32_t error_length);
    SkyUltraResult(SKYULTRA_CALL* set_page_request_allowlist)(
        SkyUltraViewHandle view,
        const SkyUltraPageRequestMethodV2* methods,
        uint32_t method_count);

    /*
     * Loads a declared web identity from Data/SkyUltra/<id>/manifest.json.
     * ASCII case in mod_id is normalized to lowercase; the manifest ID remains
     * a lowercase slug, separate from its display name.
     */
    SkyUltraResult(SKYULTRA_CALL* register_mod)(
        const char* mod_id_utf8,
        uint32_t mod_id_length,
        SkyUltraModHandle* out_mod);
    /* Creates a view from the registered manifest's local entry page. initial_url must be empty. */
    SkyUltraResult(SKYULTRA_CALL* create_mod_view)(
        SkyUltraModHandle mod,
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    /* required_bytes includes the trailing NUL. Pass (NULL, 0) to query the required capacity. */
    SkyUltraResult(SKYULTRA_CALL* get_mod_storage_directory)(
        SkyUltraModHandle mod,
        char* utf8_buffer,
        uint32_t buffer_capacity,
        uint32_t* required_bytes);
} SkyUltraNativeAPI_V3;

/* V4 preserves V3's layout and adds a queued reload of a view's original URL. */
typedef struct SkyUltraNativeAPI_V4 {
    uint32_t abi_version;
    uint32_t struct_size;

    SkyUltraResult(SKYULTRA_CALL* create_view)(
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    SkyUltraResult(SKYULTRA_CALL* get_view_state)(
        SkyUltraViewHandle view,
        SkyUltraViewStateV1* out_state);
    SkyUltraResult(SKYULTRA_CALL* destroy_view)(SkyUltraViewHandle view);
    SkyUltraResult(SKYULTRA_CALL* set_visible)(SkyUltraViewHandle view, uint8_t visible);
    SkyUltraResult(SKYULTRA_CALL* set_bounds)(SkyUltraViewHandle view, SkyUltraRectV1 bounds);
    SkyUltraResult(SKYULTRA_CALL* navigate)(
        SkyUltraViewHandle view,
        const char* url_utf8,
        uint32_t url_length);
    SkyUltraResult(SKYULTRA_CALL* send_event_json)(
        SkyUltraViewHandle view,
        const char* event_name_utf8,
        uint32_t event_name_length,
        const char* json_utf8,
        uint32_t json_length);
    SkyUltraResult(SKYULTRA_CALL* set_page_request_callback)(
        SkyUltraPageRequestCallbackV2 callback,
        void* user_data);
    SkyUltraResult(SKYULTRA_CALL* complete_page_request)(
        SkyUltraViewHandle view,
        const char* request_id_utf8,
        uint32_t request_id_length,
        uint8_t success,
        const char* result_json_utf8,
        uint32_t result_json_length,
        const char* error_utf8,
        uint32_t error_length);
    SkyUltraResult(SKYULTRA_CALL* set_page_request_allowlist)(
        SkyUltraViewHandle view,
        const SkyUltraPageRequestMethodV2* methods,
        uint32_t method_count);
    SkyUltraResult(SKYULTRA_CALL* register_mod)(
        const char* mod_id_utf8,
        uint32_t mod_id_length,
        SkyUltraModHandle* out_mod);
    SkyUltraResult(SKYULTRA_CALL* create_mod_view)(
        SkyUltraModHandle mod,
        const SkyUltraViewConfigV1* config,
        SkyUltraViewHandle* out_view);
    SkyUltraResult(SKYULTRA_CALL* get_mod_storage_directory)(
        SkyUltraModHandle mod,
        char* utf8_buffer,
        uint32_t buffer_capacity,
        uint32_t* required_bytes);
    /* Queues a reload of the URL used when this view was created. */
    SkyUltraResult(SKYULTRA_CALL* reload_view)(SkyUltraViewHandle view);
} SkyUltraNativeAPI_V4;

#ifdef __cplusplus
static_assert(offsetof(SkyUltraNativeAPI_V2, create_view) ==
    offsetof(SkyUltraNativeAPI_V1, create_view));
static_assert(offsetof(SkyUltraNativeAPI_V2, get_view_state) ==
    offsetof(SkyUltraNativeAPI_V1, get_view_state));
static_assert(offsetof(SkyUltraNativeAPI_V2, destroy_view) ==
    offsetof(SkyUltraNativeAPI_V1, destroy_view));
static_assert(offsetof(SkyUltraNativeAPI_V2, set_visible) ==
    offsetof(SkyUltraNativeAPI_V1, set_visible));
static_assert(offsetof(SkyUltraNativeAPI_V2, set_bounds) ==
    offsetof(SkyUltraNativeAPI_V1, set_bounds));
static_assert(offsetof(SkyUltraNativeAPI_V2, navigate) ==
    offsetof(SkyUltraNativeAPI_V1, navigate));
static_assert(offsetof(SkyUltraNativeAPI_V2, send_event_json) ==
    offsetof(SkyUltraNativeAPI_V1, send_event_json));
static_assert(offsetof(SkyUltraNativeAPI_V2, set_page_request_callback) ==
    sizeof(SkyUltraNativeAPI_V1));
static_assert(offsetof(SkyUltraNativeAPI_V2, set_page_request_allowlist) ==
    offsetof(SkyUltraNativeAPI_V2, complete_page_request) +
        sizeof(((SkyUltraNativeAPI_V2*)nullptr)->complete_page_request));
static_assert(offsetof(SkyUltraNativeAPI_V3, create_view) ==
    offsetof(SkyUltraNativeAPI_V2, create_view));
static_assert(offsetof(SkyUltraNativeAPI_V3, set_page_request_callback) ==
    offsetof(SkyUltraNativeAPI_V2, set_page_request_callback));
static_assert(offsetof(SkyUltraNativeAPI_V3, set_page_request_allowlist) ==
    offsetof(SkyUltraNativeAPI_V2, set_page_request_allowlist));
static_assert(offsetof(SkyUltraNativeAPI_V3, register_mod) == sizeof(SkyUltraNativeAPI_V2));
static_assert(offsetof(SkyUltraNativeAPI_V4, create_view) ==
    offsetof(SkyUltraNativeAPI_V3, create_view));
static_assert(offsetof(SkyUltraNativeAPI_V4, register_mod) ==
    offsetof(SkyUltraNativeAPI_V3, register_mod));
static_assert(offsetof(SkyUltraNativeAPI_V4, reload_view) == sizeof(SkyUltraNativeAPI_V3));
#endif

/*
 * Obtain the immutable API table for ABI version 1. The table remains owned by SkyUltra.
 * View operations enqueue in call order and run on Skyrim's game thread. SKYULTRA_OK means
 * accepted into the queue; use get_view_state to observe asynchronous view creation.
 * send_event_json raises a window-level "skyultra:message" CustomEvent whose detail contains
 * the named event and parsed JSON payload.
 */
SKYULTRA_EXPORT const SkyUltraNativeAPI_V1* SKYULTRA_CALL SkyUltra_GetAPI_V1(void);

/* Obtain ABI version 2. Its V1 function members retain their original order and signatures. */
SKYULTRA_EXPORT const SkyUltraNativeAPI_V2* SKYULTRA_CALL SkyUltra_GetAPI_V2(void);

/*
 * Obtain ABI version 3. register_mod loads the manifest schema documented in
 * samples/mod-manifest. create_mod_view loads only its local entry page and
 * rejects a non-empty initial URL. V1/V2 entry points remain for compatibility.
 */
SKYULTRA_EXPORT const SkyUltraNativeAPI_V3* SKYULTRA_CALL SkyUltra_GetAPI_V3(void);

/* Obtain ABI version 4. V4 preserves all V3 members and adds explicit view reload. */
SKYULTRA_EXPORT const SkyUltraNativeAPI_V4* SKYULTRA_CALL SkyUltra_GetAPI_V4(void);

#ifdef __cplusplus
}
#endif
