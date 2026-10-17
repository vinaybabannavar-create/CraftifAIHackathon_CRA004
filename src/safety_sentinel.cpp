#include <gst/gst.h>
#include <nvdsmeta.h>
#include <gstnvdsmeta.h>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cmath>

static constexpr float ZONE_X1 = 0.40f;
static constexpr float ZONE_Y1 = 0.35f;
static constexpr float ZONE_X2 = 0.70f;
static constexpr float ZONE_Y2 = 0.90f;

static GstPadProbeReturn safety_metadata_probe(
    GstPad *, GstPadProbeInfo *info, gpointer) {
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) return GST_PAD_PROBE_OK;

    NvDsBatchMeta *batch = gst_buffer_get_nvds_batch_meta(buffer);
    if (!batch) return GST_PAD_PROBE_OK;

    static std::unordered_map<guint64, guint> last_seen_frame;
    static std::ofstream log("incidents.jsonl", std::ios::app);
    constexpr guint GRACE_FRAMES = 12;

    for (NvDsMetaList *f = batch->frame_meta_list; f; f = f->next) {
        auto *frame = static_cast<NvDsFrameMeta *>(f->data);

        for (NvDsMetaList *n = frame->obj_meta_list; n; n = n->next) {
            auto *obj = static_cast<NvDsObjectMeta *>(n->data);
            if (obj->class_id != 2 || obj->object_id == UNTRACKED_OBJECT_ID)
                continue;
            if (obj->confidence < 0.40f)
                continue;

            float w = frame->source_frame_width;
            float h = frame->source_frame_height;
            if (w <= 0 || h <= 0) continue;

            float x = obj->rect_params.left + obj->rect_params.width / 2.0f;
            float y = obj->rect_params.top + obj->rect_params.height / 2.0f;
            bool inside = x / w >= ZONE_X1 && x / w <= ZONE_X2 &&
                          y / h >= ZONE_Y1 && y / h <= ZONE_Y2;

            auto it = last_seen_frame.find(obj->object_id);
            bool recently_inside = it != last_seen_frame.end() &&
                static_cast<guint>(frame->frame_num) <= it->second + GRACE_FRAMES;

            if (inside) {
                if (!recently_inside) {
                    log << "{\"event\":\"restricted_zone_entry\","
                        << "\"frame_num\":" << frame->frame_num << ","
                        << "\"track_id\":" << obj->object_id << ","
                        << "\"confidence\":" << obj->confidence
                        << ",\"bbox\":{\"left\":" << obj->rect_params.left
                        << ",\"top\":" << obj->rect_params.top
                        << ",\"width\":" << obj->rect_params.width
                        << ",\"height\":" << obj->rect_params.height << "}}"
                        << std::endl;
                    g_print("SAFETY ALERT: person entered restricted zone\n");
                }
                last_seen_frame[obj->object_id] =
                    static_cast<guint>(frame->frame_num);
            } else if (it != last_seen_frame.end() &&
                       static_cast<guint>(frame->frame_num) >
                           it->second + GRACE_FRAMES) {
                last_seen_frame.erase(it);
            }
        }
    }
    return GST_PAD_PROBE_OK;
}

static GstElement *make(GstElement *pipeline, const char *factory,
                        const char *name) {
    GstElement *element = gst_element_factory_make(factory, name);
    if (!element) {
        std::cerr << "Missing GStreamer element: " << factory << "\n";
        return nullptr;
    }
    gst_bin_add(GST_BIN(pipeline), element);
    return element;
}

static void on_demux_pad(GstElement *, GstPad *new_pad, gpointer data) {
    GstElement *queue = GST_ELEMENT(data);
    GstPad *sink_pad = gst_element_get_static_pad(queue, "sink");
    if (!gst_pad_is_linked(sink_pad)) {
        GstCaps *caps = gst_pad_get_current_caps(new_pad);
        if (!caps) caps = gst_pad_query_caps(new_pad, nullptr);
        const GstStructure *structure =
            caps && gst_caps_get_size(caps) ? gst_caps_get_structure(caps, 0) : nullptr;
        const char *name = structure ? gst_structure_get_name(structure) : "";
        if (g_str_has_prefix(name, "video/")) {
            if (gst_pad_link(new_pad, sink_pad) != GST_PAD_LINK_OK)
                std::cerr << "Failed to link video demux pad\n";
        }
        if (caps) gst_caps_unref(caps);
    }
    gst_object_unref(sink_pad);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.mp4> <output.mp4>\n";
        return 1;
    }

    gst_init(&argc, &argv);
    GstElement *pipeline = gst_pipeline_new("safety-sentinel");
    if (!pipeline) return 1;

    GstElement *source = make(pipeline, "filesrc", "source");
    GstElement *demux = make(pipeline, "qtdemux", "demux");
    GstElement *queue = make(pipeline, "queue", "video-queue");
    GstElement *parser = make(pipeline, "h264parse", "h264-parser");
    GstElement *decoder = make(pipeline, "nvv4l2decoder", "decoder");
    GstElement *mux = make(pipeline, "nvstreammux", "mux");
    GstElement *infer = make(pipeline, "nvinfer", "primary-detector");
    GstElement *tracker = make(pipeline, "nvtracker", "tracker");
    GstElement *osd = make(pipeline, "nvdsosd", "on-screen-display");
    GstElement *convert = make(pipeline, "nvvideoconvert", "converter");
    GstElement *capsfilter = make(pipeline, "capsfilter", "output-format");
    GstElement *encoder = make(pipeline, "avenc_mpeg4", "encoder");
    GstElement *mp4mux = make(pipeline, "mp4mux", "mp4-mux");
    GstElement *sink = make(pipeline, "filesink", "output");

    if (!source || !demux || !queue || !parser || !decoder || !mux ||
        !infer || !tracker || !osd || !convert || !capsfilter || !encoder ||
        !mp4mux || !sink) {
        gst_object_unref(pipeline);
        return 1;
    }

    g_object_set(source, "location", argv[1], nullptr);
    g_object_set(mux, "batch-size", 1, "width", 1920, "height", 1080,
                 "batched-push-timeout", 40000, nullptr);
    g_object_set(infer, "config-file-path", "config/config_infer_primary_batch1.txt",
                 nullptr);
    g_object_set(tracker, "ll-lib-file",
                 "/opt/nvidia/deepstream/deepstream-7.1/lib/libnvds_nvmultiobjecttracker.so",
                 "ll-config-file",
                 "/opt/nvidia/deepstream/deepstream-7.1/samples/configs/deepstream-app/config_tracker_IOU.yml",
                 "tracker-width", 640, "tracker-height", 384, nullptr);
    GstCaps *caps = gst_caps_from_string("video/x-raw,format=I420");
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);
    g_object_set(sink, "location", argv[2], nullptr);

    if (!gst_element_link(source, demux) ||
        !gst_element_link_many(queue, parser, decoder, nullptr)) {
        std::cerr << "Failed to link source/decoder elements\n";
        gst_object_unref(pipeline);
        return 1;
    }

    GstPad *decoder_src = gst_element_get_static_pad(decoder, "src");
    GstPad *mux_sink = gst_element_request_pad_simple(mux, "sink_0");
    if (!decoder_src || !mux_sink ||
        gst_pad_link(decoder_src, mux_sink) != GST_PAD_LINK_OK) {
        std::cerr << "Failed to link decoder to nvstreammux sink_0\n";
        if (decoder_src) gst_object_unref(decoder_src);
        if (mux_sink) gst_object_unref(mux_sink);
        gst_object_unref(pipeline);
        return 1;
    }
    gst_object_unref(decoder_src);
    gst_object_unref(mux_sink);

    if (!gst_element_link_many(mux, infer, tracker, osd, convert, capsfilter,
                               encoder, mp4mux, sink, nullptr)) {
        std::cerr << "Failed to link downstream elements\n";
        gst_object_unref(pipeline);
        return 1;
    }

    g_signal_connect(demux, "pad-added", G_CALLBACK(on_demux_pad), queue);

    GstPad *tracker_src_pad = gst_element_get_static_pad(tracker, "src");
    if (!tracker_src_pad) {
        std::cerr << "Could not get tracker source pad\\n";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return 1;
    }
    gst_pad_add_probe(tracker_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
                      safety_metadata_probe, nullptr, nullptr);
    gst_object_unref(tracker_src_pad);

    GstBus *bus = gst_element_get_bus(pipeline);
    std::cout << "Processing video: " << argv[1] << "\n";

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) ==
        GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Could not start pipeline\n";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return 1;
    }

    GstMessage *msg = gst_bus_timed_pop_filtered(
        bus, GST_CLOCK_TIME_NONE,
        static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    int result = 0;
    if (msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        GError *error = nullptr;
        gchar *debug = nullptr;
        gst_message_parse_error(msg, &error, &debug);
        std::cerr << "Pipeline error: "
                  << (error ? error->message : "unknown") << "\n";
        if (debug) std::cerr << debug << "\n";
        if (error) g_error_free(error);
        g_free(debug);
        result = 1;
    } else if (msg) {
        std::cout << "Video processing finished.\n";
    }

    if (msg) gst_message_unref(msg);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return result;
}
