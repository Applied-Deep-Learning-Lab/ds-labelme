/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

// Deepstream
#include "gstnvdsmeta.h"
#include "nvds_version.h"
#include "nvbufsurface.h"
#include "nvbufsurftransform.h"
#include "nvds_obj_encode.h"
#include "gst-nvmessage.h"

// C++
#include <cstring>
#include <string>
#include <unistd.h>
#include <termios.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <thread>
#include <mutex>
#include <chrono>

// OpenCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// Custom deepstream
#include "deepstream_app.h"
#include "deepstream_config_file_parser.h"
#include "image_meta_consumer.h"
#include "image_meta_producer.h"

// Additional modules
#include "client/client.h"
#include "base64/base64.h"

using namespace std;

// Settings
const string imageName = "tmp.jpg";
const string pathToImage = "/dev/shm/ds-labelme/image-buffer/";
const string imageFullName = pathToImage + imageName;

static const int FPS_LIMIT = 30;


static constexpr auto DELAY_FOR_LIMIT = std::chrono::milliseconds(1000 / FPS_LIMIT);

static constexpr unsigned DEFAULT_X_WINDOW_WIDTH = 640;
static constexpr unsigned DEFAULT_X_WINDOW_HEIGHT = 480;

static constexpr unsigned SEND_IMAGE_WIDTH = 640;
static constexpr unsigned SEND_IMAGE_HEIGHT = 480;

constexpr unsigned MAX_INSTANCES = 128;

#define APP_TITLE "DeepStream Transfer Learning App"


// Definitions

Client* labelSender;
Client* imageSender;
thread* recvestLoop;

static int recvCount = 0;
std::mutex recvCountLock;
std::mutex recvSendLock;
bool stop = false;

AppCtx *appCtx[MAX_INSTANCES];
static guint cintr = FALSE;
static GMainLoop *main_loop = nullptr;
static gchar **cfg_files = nullptr;
static gchar **input_files = nullptr;
static gboolean print_version = FALSE;
static gboolean show_bbox_text = FALSE;
static gboolean print_dependencies_version = FALSE;
static gboolean quit = FALSE;
static gint return_value = 0;
static guint num_instances;
static guint num_input_files;
static GMutex fps_lock;
static gdouble fps[MAX_SOURCE_BINS];
static gdouble fps_avg[MAX_SOURCE_BINS];
static guint num_fps_inst = 0;

static Display *display = nullptr;
static Window windows[MAX_INSTANCES] = {0};

static gint source_ids[MAX_INSTANCES];

static GThread *x_event_thread = nullptr;
static GMutex disp_lock;

// Object that will contain the necessary information for metadata file creation.
// It consumes the metadata created by producers and write them into files.
static ImageMetaConsumer g_img_meta_consumer;

GST_DEBUG_CATEGORY(NVDS_APP);

GOptionEntry entries[] = {
        {"version",     'v', 0, G_OPTION_ARG_NONE,           &print_version,
                "Print DeepStreamSDK version", nullptr},
        {"tiledtext",   't', 0, G_OPTION_ARG_NONE,           &show_bbox_text,
                "Display Bounding box labels in tiled mode", nullptr},
        {"version-all", 0,   0, G_OPTION_ARG_NONE,           &print_dependencies_version,
                "Print DeepStreamSDK and dependencies version", nullptr},
        {"cfg-file",    'c', 0, G_OPTION_ARG_FILENAME_ARRAY, &cfg_files,
                "Set the config file", nullptr},
        {"input-file",  'i', 0, G_OPTION_ARG_FILENAME_ARRAY, &input_files,
                "Set the input file", nullptr},
        {nullptr},
};


void test(){
    while (true)
    {
        cout << "hello from thread" << endl;
        this_thread::sleep_for(1000ms);
    }
    
}


void parseXML(string input){
    static string recvbuf = "";
    static string inTag = "";
    static bool isTagInside = false;

    cout << input << endl;

    recvbuf += input;

    while(recvbuf.length() > 0){
        if(isTagInside == false){
            auto tagStart = recvbuf.find('<');
            
            if(tagStart != string::npos){
                isTagInside = true;
                recvbuf = recvbuf.erase(0, tagStart + 1);
            } else {
                recvbuf = "";
            }
            
        }

        if(isTagInside == true){
            auto tagStart = recvbuf.find('>');

            

            if(tagStart != string::npos){
                isTagInside = false;
                inTag += recvbuf.substr(0, tagStart);
                cout << "Recvest: " << inTag << endl;
                recvbuf = recvbuf.erase(0, tagStart + 1);
                
                string test = inTag.substr(0, 4);
                if(inTag.substr(0, 4) != "head"){
                    recvCountLock.lock();
                    recvCount++;
                    recvCountLock.unlock();
                }
                inTag = "";

            } else {
                inTag += recvbuf;
                recvbuf = "";
            }
        }
    }
}

void recieveImage(Client* client){
    while (!stop)
    {
        // cout << "try_recv" << endl;
        recvSendLock.lock();
        RecvestResult recvest = client->recvest();
        recvSendLock.unlock();
        // cout << "recv: " << recv.success << endl;
        if(recvest.success){
            parseXML(recvest.message);   
        }

        this_thread::sleep_for(1000ms);
        
    }
    cout << "sender stoped" << endl;
}

// Before deepstream loop
void onStart(){
    system(((string)"mkdir -p " + pathToImage).c_str());
    labelSender = new Client(appCtx[0]->config.label_socket);
    imageSender = new Client(appCtx[0]->config.image_socket);
    labelSender->connectToHost();
    imageSender->connectToHost();
    recvestLoop = new thread(recieveImage, imageSender);
}

void onStop(){
    delete labelSender;
    delete imageSender;
    stop = true;
}

void bboxProcess(Client& client, NvDsFrameMeta *frame_meta){
    BBbox bbox;
    for (NvDsMetaList * l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
        NvDsObjectMeta *obj = (NvDsObjectMeta *) l_obj->data;
        
        bbox.left   = obj->detector_bbox_info.org_bbox_coords.left;
        bbox.top    = obj->detector_bbox_info.org_bbox_coords.top;
        bbox.width  = obj->detector_bbox_info.org_bbox_coords.width;
        bbox.height = obj->detector_bbox_info.org_bbox_coords.height;
        bbox.id     = obj->object_id;
        bbox.label  = obj->text_params.display_text;

        client.addBBox(bbox);
    }
}

void addMeta(Client& client, NvBufSurface *ip_surf, NvDsFrameMeta *frame_meta){
    client.addMeta("version", "4.5.6");
    client.addMeta("flags");
    client.addMeta("imagePath", "");
    client.addMeta("imageHeight", (int64)ip_surf->surfaceList[0].height);
    client.addMeta("imageWidth", (int64)ip_surf->surfaceList[0].width);
    client.addMeta("num", (int64)frame_meta->frame_num);
    client.addMetaTime("time", frame_meta->ntp_timestamp);
    time_t now = chrono::duration_cast<chrono::nanoseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    client.addMetaTime("sendTime", now);
    client.addMeta("deltaMilliseconds", (now - frame_meta->ntp_timestamp) / 1000000);

}

/// Will save an image cropped with the dimension specified by obj_meta
/// If the path is too long, the save will not occur and an error message will be
/// diplayed.
/// @param [in] path Where the image will be saved. If no path are specified
/// a generic one is filled. The save will be where the program was launched.
/// @param [in] ctx Object containing the saving process which is launched asynchronously.
/// @param [in] ip_surf Object containing the image to save.
/// @param [in] obj_meta Object containing information about the area to crop
/// in the full image.
/// @param [in] frame_meta Object containing information about the current frame.
/// @param [in, out] obj_counter Unsigned integer counting the number of objects saved.
/// @return true if the image was saved false otherwise.
static bool save_image(const std::string &path,
                       NvBufSurface *ip_surf, NvDsObjectMeta *obj_meta,
                       NvDsFrameMeta *frame_meta, unsigned &obj_counter) {
    
    
    // int req = imageSender->imageRequest();

    bool isCLock = recvCountLock.try_lock();
    if(!isCLock){
        return false;
    }
    if(recvCount == 0){
        recvCountLock.unlock();
        return false;
    }
    recvCount--;
    recvCountLock.unlock();
    cout << "Request image" << endl;

    int srcWidth = ip_surf->surfaceList[0].width;
    int srcHeight = ip_surf->surfaceList[0].height;

    NvBufSurfTransformRect src_rect;
    src_rect.top = 0;
    src_rect.left = 0;
    src_rect.height = srcHeight;
    src_rect.width = srcWidth;

    NvBufSurfTransformRect dst_rect;
    dst_rect.top = 0;
    dst_rect.left = 0;
    dst_rect.height = SEND_IMAGE_HEIGHT;
    dst_rect.width = SEND_IMAGE_WIDTH;
    
    // Save ratio
    if(src_rect.width > src_rect.height){;
        float scale = dst_rect.width / (float)src_rect.width;
        dst_rect.height = src_rect.height * scale;
        dst_rect.top = (SEND_IMAGE_HEIGHT - dst_rect.height) / 2;
    }else{
        float scale = dst_rect.height / (float)src_rect.height;
        dst_rect.width = src_rect.width * scale;
        dst_rect.height = (SEND_IMAGE_WIDTH - dst_rect.width) / 2;
    }




   

    NvBufSurfTransformParams transform;
    transform.dst_rect = &dst_rect;
    transform.src_rect = &src_rect;
    transform.transform_filter = NvBufSurfTransformInter_Nearest;
    transform.transform_flip = NvBufSurfTransform_None;
    transform.transform_flag = NVBUFSURF_TRANSFORM_CROP_SRC | NVBUFSURF_TRANSFORM_CROP_DST;    

    NvBufSurface* ip_surf_sys;
    NvBufSurface* ip_surf_rgb;
    NvBufSurfTransformConfigParams transform_config_params;
    NvBufSurfaceCreateParams nvbufsurface_create_params;
	
	nvbufsurface_create_params.gpuId  = ip_surf->gpuId;
	nvbufsurface_create_params.width  = (guint) SEND_IMAGE_WIDTH;
	nvbufsurface_create_params.height = (guint) SEND_IMAGE_HEIGHT;
	nvbufsurface_create_params.size = 0;
	nvbufsurface_create_params.isContiguous = true;
	nvbufsurface_create_params.colorFormat = NVBUF_COLOR_FORMAT_BGRA;
	nvbufsurface_create_params.layout = NVBUF_LAYOUT_PITCH;
    nvbufsurface_create_params.memType = NVBUF_MEM_SYSTEM;

    gint create_result = NvBufSurfaceCreate(&ip_surf_sys, ip_surf->batchSize, &nvbufsurface_create_params);
    nvbufsurface_create_params.memType = NVBUF_MEM_DEFAULT;
    create_result = NvBufSurfaceCreate(&ip_surf_rgb, ip_surf->batchSize, &nvbufsurface_create_params);
    
    auto tranformResult = NvBufSurfTransform(ip_surf, ip_surf_rgb, &transform);
        if(tranformResult != NvBufSurfTransformError_Success) {
            std::cerr << "transform failed\n";
            return false;
    }

    int result = NvBufSurfaceCopy(ip_surf_rgb, ip_surf_sys);
    
    // cv::Mat mat = cv::Mat(SEND_IMAGE_HEIGHT, SEND_IMAGE_WIDTH, CV_8UC4, (char*)ip_surf_sys->surfaceList[0].dataPtr, ip_surf_sys->surfaceList[0].pitch);
    // cv::imwrite(pathToImage +  "cv_test.jpg", mat);

    

    bboxProcess(*imageSender, frame_meta);
    addMeta(*imageSender, ip_surf_sys, frame_meta);

    NvDsObjEncUsrArgs userData = {0};
    if (path.size() >= sizeof(userData.fileNameImg)) {
        std::cerr << "Folder path too long (path: " << path
                  << ", size: " << path.size() << ") could not save image.\n"
                  << "Should be less than " << sizeof(userData.fileNameImg) << " characters.";
        return false;
    }

    int width = ip_surf_sys->surfaceList[0].width;
    int height = ip_surf_sys->surfaceList[0].height;
    int pitch = ip_surf_sys->surfaceList[0].pitch;

    uchar* data = (uchar*)ip_surf_sys->surfaceList[0].dataPtr;
    unsigned int dataSize = ip_surf_sys->surfaceList[0].dataSize;

    cv::Mat imagef = cv::Mat(height, width, CV_8UC4, data, pitch);
    cv::Mat imaget = cv::Mat(height, width, CV_8UC3);

    
    cv::cvtColor(imagef, imaget, cv::COLOR_BGRA2BGR);

    auto cdata = base64_encode(imaget.data, width * height * 3);

    
    // cv::imwrite("/dev/shm/test.jpg", imaget);

    imageSender->addMeta("imagePitch", (u_int64_t)ip_surf_sys->surfaceList[0].pitch);
    imageSender->addMeta("imageColorFormat", "BGR");
    imageSender->addMeta("imageData", cdata);

    bool isLocked = recvSendLock.try_lock();

    if(!isLocked){
        cout << "sender is busy" << endl;
        return false;
    }

    imageSender->sendMessage();
    recvSendLock.unlock();

    return true;
}

void sendSimpleJson(NvBufSurface *ip_surf, NvDsFrameMeta *frame_meta){
    bboxProcess(*labelSender, frame_meta);
    addMeta(*labelSender, ip_surf, frame_meta);
    labelSender->sendMessage();
}

void limitFps(){
    
    static auto last = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double, std::milli> processTime = now - last;

    if(processTime < DELAY_FOR_LIMIT){
        std::this_thread::sleep_for(DELAY_FOR_LIMIT - processTime);
    }
    last = std::chrono::high_resolution_clock::now();
}


/// Will fill a IPData with current frame and object information
/// @param [in] appCtx Information about the video stream paths.
/// @param [in] frame_meta Information about the current frame.
/// @param [in] obj_meta Information about the current object.
/// @return An IPData object containing the necessary information
/// for an ImageMetaProducer
static ImageMetaProducer::IPData make_ipdata(const AppCtx *appCtx,
                                         const NvDsFrameMeta *frame_meta,
                                         const NvDsObjectMeta *obj_meta) {
    ImageMetaProducer::IPData ipdata;
    ipdata.confidence = obj_meta->confidence;
    ipdata.within_confidence = ipdata.confidence > g_img_meta_consumer.get_min_confidence()
            && ipdata.confidence < g_img_meta_consumer.get_max_confidence();
    ipdata.class_id = obj_meta->class_id;
    ipdata.class_name = obj_meta->obj_label;
    ipdata.current_frame = frame_meta->frame_num;
    ipdata.img_width = obj_meta->rect_params.width;
    ipdata.img_height = obj_meta->rect_params.height;
    ipdata.img_top = obj_meta->rect_params.top;
    ipdata.img_left = obj_meta->rect_params.left;
    ipdata.video_stream_nb = frame_meta->pad_index;
    ipdata.video_path = appCtx->config.multi_source_config[ipdata.video_stream_nb].uri;
    std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%FT%T%z");
    ipdata.datetime = oss.str();
    ipdata.image_cropped_obj_path_saved =
                g_img_meta_consumer.make_img_path(ImageMetaConsumer::CROPPED_TO_OBJECT,
                                                  ipdata.video_stream_nb, ipdata.datetime);
    return ipdata;
}

static void display_bad_confidence(float confidence){
    if(confidence < 0.0 || confidence > 1.0){
        std::cerr << "Confidence ("  << confidence << ") provided by neural network output is invalid."
        << " ( 0.0 < confidence < 1.0 is required.)\n"
        << "Please verify the content of the config files.\n";
    }
}

static bool obj_meta_is_within_confidence(const NvDsObjectMeta *obj_meta){
   return obj_meta->confidence > g_img_meta_consumer.get_min_confidence()
    && obj_meta->confidence < g_img_meta_consumer.get_max_confidence();
}

static bool obj_meta_is_above_min_confidence(const NvDsObjectMeta *obj_meta){
    return obj_meta->confidence > g_img_meta_consumer.get_min_confidence();
}

static bool obj_meta_box_is_above_minimum_dimension(const NvDsObjectMeta *obj_meta){
    return obj_meta->rect_params.width > g_img_meta_consumer.get_min_box_width()
           && obj_meta->rect_params.height > g_img_meta_consumer.get_min_box_height();
}

/// Callback function that save full images, cropped images, and their related metadata
/// into path provided by config files. Here the consumer/producer design pattern is used,
/// this allows to create locally a producer that will create metadata and send them to
/// a registered consumer. The consumer will be in charge to write into files.
/// One consumer handle the content of several producers, using a thread safe queue.
/// @param [in] appCtx App context.
/// @param [in] buf Buffer containing frames.
/// @param [in] batch_meta Object containing information about neural network output.
/// @param [in] index Not used here.
///
static void
after_pgie_image_meta_save(AppCtx *appCtx, GstBuffer *buf,
                           NvDsBatchMeta *batch_meta, guint index) {
    
    if (g_img_meta_consumer.get_is_stopped()) {
        std::cerr << "Could not save image and metadata: "
                  << "Consumer is stopped.\n";
        return;
    }

    GstMapInfo inmap = GST_MAP_INFO_INIT;
    if (!gst_buffer_map(buf, &inmap, GST_MAP_READ)) {
        std::cerr << "input buffer mapinfo failed\n";
        return;
    }
    
    
    NvBufSurface *ip_surf = (NvBufSurface *) inmap.data;
    gst_buffer_unmap(buf, &inmap);

    
    /// Creating an ImageMetaProducer and registering a consumer.
    ImageMetaProducer img_producer = ImageMetaProducer(g_img_meta_consumer);

    bool at_least_one_image_saved = false;
    
    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame != nullptr;
         l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = static_cast<NvDsFrameMeta *>(l_frame->data);
        
        unsigned source_number = frame_meta->pad_index;
        
        g_img_meta_consumer.lock_source_nb(source_number);

        /// required for `get_save_full_frame_enabled()`
        // std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // std::ostringstream oss;
        // oss << std::put_time(std::localtime(&t), "%FT%T%z");
        // img_producer.generate_image_full_frame_path(frame_meta->pad_index, "tmp.jpg");
        unsigned obj_counter = 0;

        
        imageSender->reconnect();
        labelSender->reconnect();
        sendSimpleJson(ip_surf, frame_meta);


        unsigned dummy_counter = 0;
        static NvDsObjectMeta dummy_obj_meta;
        dummy_obj_meta.rect_params.width = ip_surf->surfaceList[frame_meta->batch_id].width;
        dummy_obj_meta.rect_params.height = ip_surf->surfaceList[frame_meta->batch_id].height;
        dummy_obj_meta.rect_params.top = 0;
        dummy_obj_meta.rect_params.left = 0;
        save_image(imageFullName, ip_surf, &dummy_obj_meta, frame_meta, obj_counter);
        
        g_img_meta_consumer.unlock_source_nb(source_number);
    }
    limitFps();
}

/**
 * Function to handle program interrupt signal.
 * It installs default handler after handling the interrupt.
 */
static void
_intr_handler(int signum) {
    struct sigaction action;

    NVGSTDS_ERR_MSG_V("User Interrupted.. \n");

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_DFL;

    sigaction(SIGINT, &action, nullptr);

    cintr = TRUE;
}

/**
 * callback function to print the performance numbers of each stream.
 */
static void
perf_cb(gpointer context, NvDsAppPerfStruct *str) {
    static guint header_print_cnt = 0;
    guint i;
    AppCtx *appCtx = (AppCtx *) context;
    guint numf = (num_instances == 1) ? str->num_instances : num_instances;

    g_mutex_lock(&fps_lock);
    if (num_instances > 1) {
        fps[appCtx->index] = str->fps[0];
        fps_avg[appCtx->index] = str->fps_avg[0];
    } else {
        for (i = 0; i < numf; i++) {
            fps[i] = str->fps[i];
            fps_avg[i] = str->fps_avg[i];
        }
    }

    num_fps_inst++;
    if (num_fps_inst < num_instances) {
        g_mutex_unlock(&fps_lock);
        return;
    }

    num_fps_inst = 0;

    if (header_print_cnt % 20 == 0) {
        g_print("\n**PERF: ");
        for (i = 0; i < numf; i++) {
            g_print("FPS %d (Avg)\t", i);
        }
        g_print("\n");
        header_print_cnt = 0;
    }
    header_print_cnt++;
    g_print("**PERF: ");
    for (i = 0; i < numf; i++) {
        g_print("%.2f (%.2f)\t", fps[i], fps_avg[i]);
    }
    g_print("\n");
    g_mutex_unlock(&fps_lock);
}

/**
 * Loop function to check the status of interrupts.
 * It comes out of loop if application got interrupted.
 */
static gboolean
check_for_interrupt(gpointer data) {
    if (quit) {
        return FALSE;
    }

    if (cintr) {
        cintr = FALSE;

        quit = TRUE;
        g_main_loop_quit(main_loop);

        return FALSE;
    }
    return TRUE;
}

/*
 * Function to install custom handler for program interrupt signal.
 */
static void
_intr_setup(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = _intr_handler;

    sigaction(SIGINT, &action, nullptr);
}

static gboolean
kbhit(void) {
    struct timeval tv;
    fd_set rdfs;

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    FD_ZERO(&rdfs);
    FD_SET(STDIN_FILENO, &rdfs);

    select(STDIN_FILENO + 1, &rdfs, nullptr, nullptr, &tv);
    return FD_ISSET(STDIN_FILENO, &rdfs);
}

/*
 * Function to enable / disable the canonical mode of terminal.
 * In non canonical mode input is available immediately (without the user
 * having to type a line-delimiter character).
 */
static void
changemode(int dir) {
    static struct termios oldt, newt;

    if (dir == 1) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

static void
print_runtime_commands(void) {
    g_print("\nRuntime commands:\n"
            "\th: Print this help\n"
            "\tq: Quit\n\n"
            "\tp: Pause\n"
            "\tr: Resume\n\n");

    if (appCtx[0]->config.tiled_display_config.enable) {
        g_print("NOTE: To expand a source in the 2D tiled display and view object details,"
                " left-click on the source.\n"
                "      To go back to the tiled display, right-click anywhere on the window.\n\n");
    }
}

static guint rrow, rcol;
static gboolean rrowsel = FALSE, selecting = FALSE;

/**
 * Loop function to check keyboard inputs and status of each pipeline.
 */
static gboolean
event_thread_func(gpointer arg) {
    guint i;
    gboolean ret = TRUE;

    // Check if all instances have quit
    for (i = 0; i < num_instances; i++) {
        if (!appCtx[i]->quit)
            break;
    }

    if (i == num_instances) {
        quit = TRUE;
        g_main_loop_quit(main_loop);
        return FALSE;
    }
    // Check for keyboard input
    if (!kbhit()) {
        //continue;
        return TRUE;
    }
    int c = fgetc(stdin);
    g_print("\n");

    gint source_id;
    GstElement *tiler = appCtx[0]->pipeline.tiled_display_bin.tiler;
    g_object_get(G_OBJECT(tiler), "show-source", &source_id, nullptr);

    if (selecting) {
        if (rrowsel == FALSE) {
            if (c >= '0' && c <= '9') {
                rrow = c - '0';
                if (rrow < appCtx[0]->config.tiled_display_config.rows) {
                    g_print("--selecting source  row %d--\n", rrow);
                    rrowsel = TRUE;
                } else {
                    g_print("--selected source  row %d out of bound, reenter\n", rrow);
                }
            }
        } else {
            if (c >= '0' && c <= '9') {
                unsigned int tile_num_columns = appCtx[0]->config.tiled_display_config.columns;
                rcol = c - '0';
                if (rcol < tile_num_columns) {
                    selecting = FALSE;
                    rrowsel = FALSE;
                    source_id = tile_num_columns * rrow + rcol;
                    g_print("--selecting source  col %d sou=%d--\n", rcol, source_id);
                    if (source_id >= (gint) appCtx[0]->config.num_source_sub_bins) {
                        source_id = -1;
                    } else {
                        source_ids[0] = source_id;
                        appCtx[0]->show_bbox_text = TRUE;
                        g_object_set(G_OBJECT(tiler), "show-source", source_id, nullptr);
                    }
                } else {
                    g_print("--selected source  col %d out of bound, reenter\n", rcol);
                }
            }
        }
    }
    switch (c) {
        case 'h':
            print_runtime_commands();
            break;
        case 'p':
            for (i = 0; i < num_instances; i++)
                pause_pipeline(appCtx[i]);
            break;
        case 'r':
            for (i = 0; i < num_instances; i++)
                resume_pipeline(appCtx[i]);
            break;
        case 'q':
            quit = TRUE;
            g_main_loop_quit(main_loop);
            ret = FALSE;
            break;
        case 'z':
            if (source_id == -1 && selecting == FALSE) {
                g_print("--selecting source --\n");
                selecting = TRUE;
            }
            break;
        default:
            break;
    }
    return ret;
}

static int
get_source_id_from_coordinates(float x_rel, float y_rel) {
    int tile_num_rows = appCtx[0]->config.tiled_display_config.rows;
    int tile_num_columns = appCtx[0]->config.tiled_display_config.columns;

    int source_id = (int) (x_rel * tile_num_columns);
    source_id += ((int) (y_rel * tile_num_rows)) * tile_num_columns;

    /* Don't allow clicks on empty tiles. */
    if (source_id >= (gint) appCtx[0]->config.num_source_sub_bins)
        source_id = -1;

    return source_id;
}

/**
 * Thread to monitor X window events.
 */
static gpointer
nvds_x_event_thread(gpointer data) {
    g_mutex_lock(&disp_lock);
    while (display) {
        XEvent e;
        guint index;
        while (XPending(display)) {
            XNextEvent(display, &e);
            switch (e.type) {
                case ButtonPress: {
                    XWindowAttributes win_attr;
                    XButtonEvent ev = e.xbutton;
                    gint source_id;
                    GstElement *tiler;

                    XGetWindowAttributes(display, ev.window, &win_attr);

                    for (index = 0; index < MAX_INSTANCES; index++)
                        if (ev.window == windows[index])
                            break;

                    tiler = appCtx[index]->pipeline.tiled_display_bin.tiler;
                    g_object_get(G_OBJECT(tiler), "show-source", &source_id, nullptr);

                    if (ev.button == Button1 && source_id == -1) {
                        source_id =
                                get_source_id_from_coordinates(ev.x * 1.0 / win_attr.width,
                                                               ev.y * 1.0 / win_attr.height);
                        if (source_id > -1) {
                            g_object_set(G_OBJECT(tiler), "show-source", source_id, nullptr);
                            source_ids[index] = source_id;
                            appCtx[index]->show_bbox_text = TRUE;
                        }
                    } else if (ev.button == Button3) {
                        g_object_set(G_OBJECT(tiler), "show-source", -1, nullptr);
                        source_ids[index] = -1;
                        if (!show_bbox_text)
                            appCtx[index]->show_bbox_text = FALSE;
                    }
                }
                    break;
                case KeyRelease:
                case KeyPress: {
                    KeySym p, r, q;
                    guint i;
                    p = XKeysymToKeycode(display, XK_P);
                    r = XKeysymToKeycode(display, XK_R);
                    q = XKeysymToKeycode(display, XK_Q);
                    if (e.xkey.keycode == p) {
                        for (i = 0; i < num_instances; i++)
                            pause_pipeline(appCtx[i]);
                        break;
                    }
                    if (e.xkey.keycode == r) {
                        for (i = 0; i < num_instances; i++)
                            resume_pipeline(appCtx[i]);
                        break;
                    }
                    if (e.xkey.keycode == q) {
                        quit = TRUE;
                        g_main_loop_quit(main_loop);
                    }
                }
                    break;
                case ClientMessage: {
                    Atom wm_delete;
                    for (index = 0; index < MAX_INSTANCES; index++)
                        if (e.xclient.window == windows[index])
                            break;

                    wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", 1);
                    if (wm_delete != None && wm_delete == (Atom) e.xclient.data.l[0]) {
                        quit = TRUE;
                        g_main_loop_quit(main_loop);
                    }
                }
                    break;
            }
        }
        g_mutex_unlock(&disp_lock);
        g_usleep(G_USEC_PER_SEC / 20);
        g_mutex_lock(&disp_lock);
    }
    g_mutex_unlock(&disp_lock);
    return nullptr;
}

/**
 * callback function to add application specific metadata.
 * Here it demonstrates how to display the URI of source in addition to
 * the text generated after inference.
 */
static gboolean
overlay_graphics(AppCtx *appCtx, GstBuffer *buf,
                 NvDsBatchMeta *batch_meta, guint index) {
    if (source_ids[index] == -1)
        return TRUE;

    NvDsFrameLatencyInfo *latency_info = nullptr;
    NvDsDisplayMeta *display_meta =
            nvds_acquire_display_meta_from_pool(batch_meta);

    display_meta->num_labels = 1;
    display_meta->text_params[0].display_text = g_strdup_printf("Source: %s",
         appCtx->config.multi_source_config[source_ids[index]].uri);

    display_meta->text_params[0].y_offset = 20;
    display_meta->text_params[0].x_offset = 20;
    display_meta->text_params[0].font_params.font_color = (NvOSD_ColorParams) {
            0, 1, 0, 1};
    display_meta->text_params[0].font_params.font_size =
            appCtx->config.osd_config.text_size * 1.5;
    char serif[] = "Serif";
    display_meta->text_params[0].font_params.font_name = serif;
    display_meta->text_params[0].set_bg_clr = 1;
    display_meta->text_params[0].text_bg_clr = (NvOSD_ColorParams) {
            0, 0, 0, 1.0};

    if (nvds_enable_latency_measurement) {
        g_mutex_lock(&appCtx->latency_lock);
        latency_info = &appCtx->latency_info[index];
        display_meta->num_labels++;
        display_meta->text_params[1].display_text = g_strdup_printf("Latency: %lf",
                                                                    latency_info->latency);
        g_mutex_unlock(&appCtx->latency_lock);

        display_meta->text_params[1].y_offset = (display_meta->text_params[0].y_offset * 2) +
                                                display_meta->text_params[0].font_params.font_size;
        display_meta->text_params[1].x_offset = 20;
        display_meta->text_params[1].font_params.font_color = (NvOSD_ColorParams) {
                0, 1, 0, 1};
        display_meta->text_params[1].font_params.font_size =
                appCtx->config.osd_config.text_size * 1.5;
        char arial[] = "Arial";
        display_meta->text_params[1].font_params.font_name = arial;
        display_meta->text_params[1].set_bg_clr = 1;
        display_meta->text_params[1].text_bg_clr = (NvOSD_ColorParams) {
                0, 0, 0, 1.0};
    }

    nvds_add_display_meta_to_frame(nvds_get_nth_frame_meta(batch_meta->frame_meta_list, 0), display_meta);
    return TRUE;
}

int main(int argc, char *argv[]) {
    GOptionContext *ctx = nullptr;
    GOptionGroup *group = nullptr;
    GError *error = nullptr;
    guint i;

    ctx = g_option_context_new("Nvidia DeepStream Demo");
    group = g_option_group_new("abc", nullptr, nullptr, nullptr, nullptr);
    g_option_group_add_entries(group, entries);

    g_option_context_set_main_group(ctx, group);
    g_option_context_add_group(ctx, gst_init_get_option_group());

    GST_DEBUG_CATEGORY_INIT(NVDS_APP, "NVDS_APP", 0, nullptr);

    if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
        NVGSTDS_ERR_MSG_V("%s", error->message);
        return -1;
    }

    if (print_version) {
        g_print("deepstream-app version %d.%d.%d\n",
                NVDS_APP_VERSION_MAJOR, NVDS_APP_VERSION_MINOR, NVDS_APP_VERSION_MICRO);
        nvds_version_print();
        return 0;
    }

    if (print_dependencies_version) {
        g_print("deepstream-app version %d.%d.%d\n",
                NVDS_APP_VERSION_MAJOR, NVDS_APP_VERSION_MINOR, NVDS_APP_VERSION_MICRO);
        nvds_version_print();
        nvds_dependencies_version_print();
        return 0;
    }

    if (cfg_files) {
        num_instances = g_strv_length(cfg_files);
    }
    if (input_files) {
        num_input_files = g_strv_length(input_files);
    }

    memset(source_ids, -1, sizeof(source_ids));
    do {
        if (!cfg_files || num_instances == 0) {
            NVGSTDS_ERR_MSG_V("Specify config file with -c option");
            return_value = -1;
            break;
        }

        bool should_goto_done = false;
        for (i = 0; i < num_instances; i++) {
            appCtx[i] = static_cast<AppCtx *>(g_malloc0(sizeof(AppCtx)));
            appCtx[i]->person_class_id = -1;
            appCtx[i]->car_class_id = -1;
            appCtx[i]->index = i;
            if (show_bbox_text) {
                appCtx[i]->show_bbox_text = TRUE;
            }

            if (input_files && input_files[i]) {
                appCtx[i]->config.multi_source_config[0].uri =
                        g_strdup_printf("file://%s", input_files[i]);
                g_free(input_files[i]);
            }

            if (!parse_config_file(&appCtx[i]->config, cfg_files[i])) {
                NVGSTDS_ERR_MSG_V("Failed to parse config file '%s'", cfg_files[i]);
                appCtx[i]->return_value = -1;
                should_goto_done = true;
                break;
            }
        }
        if (should_goto_done)
            break;

        for (i = 0; i < num_instances; i++) {
            if (!create_pipeline(appCtx[i], after_pgie_image_meta_save,
                                 nullptr, perf_cb, overlay_graphics)) {
                NVGSTDS_ERR_MSG_V("Failed to create pipeline");
                return_value = -1;
                should_goto_done = true;
                break;
            }
        }
        if (should_goto_done)
            break;
        NvDsImageSave nvds_imgsave = appCtx[0]->config.image_save_config;
        if (nvds_imgsave.enable) {
            bool can_start = true;
            if (!nvds_imgsave.output_folder_path) {
                std::cerr << "Consumer not started => consider adding output-folder-path=./my/path to [img-save]\n";
                can_start = false;
            }
            if (!nvds_imgsave.frame_to_skip_rules_path) {
                std::cerr
                        << "Consumer not started => consider adding frame-to-skip-rules-path=./my/path/to/file.csv to [img-save]\n";
                can_start = false;
            }
            if (can_start) {
                g_img_meta_consumer.init(nvds_imgsave.output_folder_path, nvds_imgsave.frame_to_skip_rules_path,
                                         nvds_imgsave.min_confidence, nvds_imgsave.max_confidence,
                                         nvds_imgsave.min_box_width, nvds_imgsave.min_box_height,
                                         nvds_imgsave.save_image_full_frame,
                                         nvds_imgsave.save_image_cropped_object,
                                         nvds_imgsave.second_to_skip_interval,
                                         MAX_SOURCE_BINS);
            }
            if (g_img_meta_consumer.get_is_stopped()) {
                std::cerr << "Consumer could not be started => exiting...\n\n";
                return_value = -1;
                break;
            }

        } else {
            std::cerr << "Consumer not started => consider setting enable=1 "
                      << "or adding [img-save] part in config file. (example below)\n"
                      << "[img-save]\n"
                      << "enable=1\n"
                      << "output-folder-path=./output\n"
                      << "save-img-cropped-obj=0\n"
                      << "save-img-full-frame=1\n"
                      << "frame-to-skip-rules-path=capture_time_rules.csv\n"
                      << "second-to-skip-interval=600\n"
                      << "min-confidence=0.9\n"
                      << "max-confidence=1.0\n"
                      << "min-box-width=5\n"
                      << "min-box-height=5\n\n";

            return_value = -1;
            break;
        }

        main_loop = g_main_loop_new(nullptr, FALSE);

        _intr_setup();
        g_timeout_add(400, check_for_interrupt, nullptr);

        g_mutex_init(&disp_lock);
        display = XOpenDisplay(nullptr);
        for (i = 0; i < num_instances; i++) {
            guint j;

            if (gst_element_set_state(appCtx[i]->pipeline.pipeline,
                                      GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE) {
                NVGSTDS_ERR_MSG_V("Failed to set pipeline to PAUSED");
                return_value = -1;
                should_goto_done = true;
                break;
            }


            if (!appCtx[i]->config.tiled_display_config.enable)
                continue;

            for (j = 0; j < appCtx[i]->config.num_sink_sub_bins; j++) {
                XTextProperty xproperty;
                gchar *title;
                guint width, height;

                if (!GST_IS_VIDEO_OVERLAY(appCtx[i]->pipeline.instance_bins[0].sink_bin.sub_bins[j].sink)) {
                    continue;
                }

                if (!display) {
                    NVGSTDS_ERR_MSG_V("Could not open X Display");
                    return_value = -1;
                    should_goto_done = true;
                    break;
                }

                if (appCtx[i]->config.sink_bin_sub_bin_config[j].render_config.width)
                    width =
                            appCtx[i]->config.sink_bin_sub_bin_config[j].render_config.width;
                else
                    width = appCtx[i]->config.tiled_display_config.width;

                if (appCtx[i]->config.sink_bin_sub_bin_config[j].render_config.height)
                    height =
                            appCtx[i]->config.sink_bin_sub_bin_config[j].render_config.height;
                else
                    height = appCtx[i]->config.tiled_display_config.height;

                width = (width) ? width : DEFAULT_X_WINDOW_WIDTH;
                height = (height) ? height : DEFAULT_X_WINDOW_HEIGHT;

                windows[i] =
                        XCreateSimpleWindow(display, RootWindow(display, DefaultScreen(display)), 0, 0, width, height,
                                            2, 0x00000000,
                                            0x00000000);

                if (num_instances > 1)
                    title = g_strdup_printf(APP_TITLE "-%d", i);
                else
                    title = g_strdup(APP_TITLE);
                if (XStringListToTextProperty((char **) &title, 1, &xproperty) != 0) {
                    XSetWMName(display, windows[i], &xproperty);
                    XFree(xproperty.value);
                }

                XSetWindowAttributes attr = {0};
                if ((appCtx[i]->config.tiled_display_config.enable &&
                     appCtx[i]->config.tiled_display_config.rows *
                     appCtx[i]->config.tiled_display_config.columns ==
                     1) ||
                    (appCtx[i]->config.tiled_display_config.enable == 0 &&
                     appCtx[i]->config.num_source_sub_bins == 1)) {
                    attr.event_mask = KeyPress;
                } else {
                    attr.event_mask = ButtonPress | KeyRelease;
                }
                XChangeWindowAttributes(display, windows[i], CWEventMask, &attr);

                Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", False);
                if (wmDeleteMessage != None) {
                    XSetWMProtocols(display, windows[i], &wmDeleteMessage, 1);
                }
                XMapRaised(display, windows[i]);
                XSync(display, 1); //discard the events for now
                gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(appCtx
                                                                      [i]
                                                                              ->pipeline.instance_bins[0]
                                                                              .sink_bin.sub_bins[j]
                                                                              .sink),
                                                    (gulong) windows[i]);
                gst_video_overlay_expose(
                        GST_VIDEO_OVERLAY(appCtx[i]->pipeline.instance_bins[0].sink_bin.sub_bins[j].sink));
                if (!x_event_thread)
                    x_event_thread = g_thread_new("nvds-window-event-thread",
                                                  nvds_x_event_thread, nullptr);
            }
            if (should_goto_done)
                break;
        }
        if (should_goto_done)
            break;
        /* Dont try to set playing state if error is observed */
        if (return_value != -1) {
            for (i = 0; i < num_instances; i++) {
                if (gst_element_set_state(appCtx[i]->pipeline.pipeline,
                                          GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {

                    g_print("\ncan't set pipeline to playing state.\n");
                    return_value = -1;
                    should_goto_done = true;
                    break;
                }
            }
            if (should_goto_done)
                break;
        }

        print_runtime_commands();
        changemode(1);

        // g_timeout_add(40, event_thread_func, nullptr);

        onStart();
        g_main_loop_run(main_loop);
        onStop();
        

        changemode(0);


    } while (false);

    g_print("Quitting\n");
    for (i = 0; i < num_instances; i++) {
        if (appCtx[i]->return_value == -1)
            return_value = -1;
        destroy_pipeline(appCtx[i]);

        g_mutex_lock(&disp_lock);
        if (windows[i])
            XDestroyWindow(display, windows[i]);
        windows[i] = 0;
        g_mutex_unlock(&disp_lock);

        g_free(appCtx[i]);
    }

    g_mutex_lock(&disp_lock);
    if (display)
        XCloseDisplay(display);
    display = nullptr;
    g_mutex_unlock(&disp_lock);
    g_mutex_clear(&disp_lock);

    if (main_loop) {
        g_main_loop_unref(main_loop);
    }

    if (ctx) {
        g_option_context_free(ctx);
    }

    if (return_value == 0) {
        g_print("App run successful\n");
    } else {
        g_print("App run failed\n");
    }

    gst_deinit();

    return return_value;
}
