///////////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////////
// Gstreamer
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/app/gstappsrc.h>
#define CHUNK_SIZE 320   /* Amount of bytes we are sending in each buffer */
#define SAMPLE_RATE 16000

//parsing files
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>

//network stuff
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <stdio.h>
#include <stdarg.h>
#include <thread>
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#define sockerrno errno

//math
#include <math.h>


///////////////////////////////////////////////////////////////////////////////
// Script Parameters
///////////////////////////////////////////////////////////////////////////////
// Server data interface: enp0s3
const char *server_data_channel_ip    = "192.168.100.2";
// Server control interface: enp0s8
const char *server_control_channel_ip = "192.168.200.2";

// Client data interface: enp0s3
const char *client_data_channel_ip    = "192.168.100.4";
// Client control interface: enp0s8
const char *client_control_channel_ip = "192.168.200.3";

// Ports for data and control channels
unsigned short dataPort    = 21323;
unsigned short controlPort = 8080;
	
// Data Socket
int iNetSock = INVALID_SOCKET;
struct sockaddr_in sAddr, cAddr;
socklen_t sAddrLen = sizeof(sAddr);
socklen_t cAddrLen = sizeof(cAddr);
	
// Control Socket
int iNetSockCC = INVALID_SOCKET;
struct sockaddr_in sAddrCC, cAddrCC;
socklen_t sAddrLenCC = sizeof(sAddrCC);
socklen_t cAddrLenCC = sizeof(cAddrCC);


///////////////////////////////////////////////////////////////////////////////
// Function Definitions
///////////////////////////////////////////////////////////////////////////////

gint16 byteToAudio (int b, int encodingMode) {
    float g;
    const float u = 255;
    float y, sgn;
    switch (encodingMode) {
        case 1:
            g = 128.0;
            break;
        case 2:
            g = 128.0;
            break;
        case 3:
            g = 8.0;
            break;
        default:
            std::cout << "Error: unsupported encoding mode " << encodingMode << std::endl;
            exit(EXIT_FAILURE);
     }

     y = (float) b - g;
     if (y >= 0){
         y += 1;
         sgn = 1;
     } else {
         y = abs(y);
         sgn = -1;
     }
     return (gint16) (pow(2,15)-1)*sgn*(exp((y/g)*log(1+u))-1)/u;
}

///////////////////////////////////////////////////////////////////////////////
// GStreamer
///////////////////////////////////////////////////////////////////////////////
/* Structure to contain all our information, so we can pass it to callbacks */
typedef struct _CustomData {
    GstElement *pipeline, *app_source, *audio_sink;
    guint64 num_samples;   /* Number of samples generated so far (for timestamp generation) */
    guint sourceid;        /* To control the GSource */
    GMainLoop *main_loop;  /* GLib's Main Loop */
} CustomData;

/* This method is called by the idle GSource in the mainloop, to feed CHUNK_SIZE bytes into appsrc.
 * The idle handler is added to the mainloop when appsrc requests us to start sending data (need-data signal)
 * and is removed when appsrc has enough data (enough-data signal).
 */
static gboolean push_data (CustomData *data) {

    unsigned char recBuff[2*sizeof(int)+CHUNK_SIZE];
    int nAudioBytesRead = recvfrom(iNetSock, recBuff, 2*sizeof(int)+CHUNK_SIZE, 
                                   MSG_WAITALL, (struct sockaddr *) &cAddr, &cAddrLen);
    nAudioBytesRead -= 2*sizeof(int);

    int receivedFrameNumber= -1;
    static int lastFrameNumber = 0;
    int encodingMode = -1;
    int zerosOffset = 0;
    int frameOffset = 0;

    memcpy(&receivedFrameNumber, &recBuff[0],           sizeof(int));
    memcpy(&encodingMode,        &recBuff[sizeof(int)], sizeof(int));
    std::cout << "Read " << nAudioBytesRead << " bytes, ";
    std::cout << "receivedFrameNumber: " << receivedFrameNumber << ", ";
    std::cout << "lastFrameNumber: " << lastFrameNumber << ", ";
    std::cout << "encodingMode: " << (int) encodingMode << std::endl;

    // Send received frame number back to the client program.
    sendto(iNetSockCC, recBuff, sizeof(int), 0, (struct sockaddr *)&cAddrCC, cAddrLenCC);


    GstBuffer *buffer;
    GstFlowReturn ret;
    GstMapInfo map;
    gint16 *raw = NULL;
    gint num_samples = 0;

    switch (encodingMode) {
        case 1:
            if (receivedFrameNumber == lastFrameNumber+1 || receivedFrameNumber <= lastFrameNumber || receivedFrameNumber > lastFrameNumber + 24) { // Good. Sequential frame numbers or replaying from the beginning.
                zerosOffset = 0;
                frameOffset = 0;
                num_samples = nAudioBytesRead;
                lastFrameNumber = receivedFrameNumber;

            } else { // Ok. There are missing frames that need to be filled with zeros.
                int nMissingFrames = receivedFrameNumber - lastFrameNumber - 1;
                zerosOffset = nMissingFrames*CHUNK_SIZE;
                num_samples = zerosOffset + nAudioBytesRead;
                lastFrameNumber = receivedFrameNumber;
            }
            break;

        case 2:
            if (receivedFrameNumber == lastFrameNumber) { // Good. Sequential packets.
                zerosOffset = 0;
                frameOffset = CHUNK_SIZE/2;
                num_samples = nAudioBytesRead*2/2;
                lastFrameNumber = receivedFrameNumber + 1;

            } else if (receivedFrameNumber < lastFrameNumber || receivedFrameNumber > lastFrameNumber + 24) { 
                // Either the file is being replayed or the packets are out of order.
                zerosOffset = 0;
                frameOffset = 0;
                num_samples = nAudioBytesRead*2/2;
                lastFrameNumber = receivedFrameNumber + 1;
                return TRUE;

            } else { // Ok. There are missing frames that need to be filled with zeros.
                int nMissingFrames = receivedFrameNumber - lastFrameNumber - 1;
                zerosOffset = nMissingFrames*CHUNK_SIZE/2;
                frameOffset = 0;
                num_samples = zerosOffset + nAudioBytesRead*2;
                lastFrameNumber = receivedFrameNumber + 1;
            }
            break;

        case 3:
            if (lastFrameNumber - 2 <= receivedFrameNumber && receivedFrameNumber <= lastFrameNumber+1) {
                int nOverlappingFrames = lastFrameNumber - receivedFrameNumber + 1;
                if (nOverlappingFrames == 4) {
                    // Do nothing.
                    return TRUE;
                }

                zerosOffset = 0;
                frameOffset = nOverlappingFrames*CHUNK_SIZE/4;
                num_samples = nAudioBytesRead*4*(4-nOverlappingFrames)/4;
                lastFrameNumber = receivedFrameNumber + 3;

            } else if (receivedFrameNumber < lastFrameNumber - 2 || receivedFrameNumber > lastFrameNumber + 24) { 
                // Either the file is being replayed or the packets are very out of order.
                zerosOffset = 0;
                frameOffset = 0;
                num_samples = nAudioBytesRead*4;
                lastFrameNumber = receivedFrameNumber + 3;
                return TRUE;

            } else { // Ok. There are missing frames that need to be filled with zeros.
                int nMissingFrames = receivedFrameNumber - lastFrameNumber - 1;
                zerosOffset = nMissingFrames*CHUNK_SIZE/4;
                frameOffset = 0;
                num_samples = zerosOffset + nAudioBytesRead*4;
                lastFrameNumber = receivedFrameNumber + 3; 
            }
            break;
        default:
            std::cout << "Error: unsupported encodingMode " << encodingMode << std::endl;
            exit(EXIT_FAILURE);
    }

    // Allocate the buffer used to store the decoded audio data.
    buffer = gst_buffer_new_and_alloc(num_samples*2); // *2 because samples will be converted to 16 bit from 8 bit.

    /* Set its timestamp and duration */
    GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (data->num_samples, GST_SECOND, SAMPLE_RATE);
    GST_BUFFER_DURATION (buffer)  = gst_util_uint64_scale (num_samples,       GST_SECOND, SAMPLE_RATE);
    //g_print("GST_BUFFER_TIMESTAMP = %u\n", GST_BUFFER_TIMESTAMP(buffer));
    //std::cout << "GST_BUFFER_DURATION = " << GST_BUFFER_DURATION(buffer) << std::endl;

    gst_buffer_map (buffer, &map, GST_MAP_WRITE);
    raw = (gint16 *)map.data;

    gst_buffer_unmap (buffer, &map);
    data->num_samples += num_samples;

    // Write zeros to the audio buffer for missing frames, if any.
    memset(raw, 0, zerosOffset*2);
  
    // Decode the bytes to audio samples based on the encoding type.
    switch (encodingMode) {
        case 1: // Decode mode 1 to audio.
            for (int i = 0; i < nAudioBytesRead; i++) {
                raw[i+zerosOffset] = byteToAudio(recBuff[i+2*sizeof(int)], encodingMode);
            }
            break;

        case 2: // Decode mode 2 to audio.
            for (int i = 0; i < nAudioBytesRead-frameOffset; i++) {
                raw[2*i+zerosOffset] = byteToAudio(recBuff[i+2*sizeof(int)+frameOffset], encodingMode);
            }
            // Interpolate odd numbered bytes via linear interpolation.
            for (int i = zerosOffset+1; i < num_samples; i += 2) {
                //std::cout << "Interpolating i = " << i << std::endl;
                raw[i] = 0.5*(raw[i-1] + raw[i+1]);
            }
            raw[num_samples-1] = raw[num_samples-2];
            break;

        case 3: // Decode mode 3 to audio.
            for (int i = 0; i < nAudioBytesRead-frameOffset; i++) {
                int y1 = floor(recBuff[i+2*sizeof(int)+frameOffset]/16);
                int y2 = recBuff[i+2*sizeof(int)+frameOffset]%16;
                raw[4*i+zerosOffset]   = byteToAudio(y1, encodingMode);
                raw[4*i+2+zerosOffset] = byteToAudio(y2, encodingMode);
            }

            for (int i = zerosOffset+1; i < num_samples; i += 2) {
                raw[i] =  0.5*(raw[i-1] + raw[i+1]);
            }
            raw[num_samples-1] = raw[num_samples-2];
            break;

        default:
            std::cout << "Error: Unsupported encoding mode " << encodingMode << "." << std::endl;
            exit(EXIT_FAILURE);
     }

        // Print out values.
        //for (int i = 0; i < num_samples; i++) {
        //    std::cout << "raw[" << i << "] = " << (gint16) raw[i] << std::endl;
        //}
  

    /* Push the buffer into the appsrc */
    g_signal_emit_by_name (data->app_source, "push-buffer", buffer, &ret);
    //gst_app_src_push_buffer(GST_APP_SRC(data->app_source), buffer);


    /* Free the buffer now that we are done with it */
    gst_buffer_unref (buffer);


    return TRUE;
}






///////////////////////////////////////////////////////////////////////////////
// End GStreamer
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Main
///////////////////////////////////////////////////////////////////////////////
int main(int argc, char * argv[]) {    
    //network stuff
    // Data channel
    
    //establish the IP component
    memset((char *)&sAddr, 0,sizeof(sAddr));
    sAddr.sin_family      = AF_INET;
    sAddr.sin_addr.s_addr = inet_addr(server_data_channel_ip);
    sAddr.sin_port        = htons(dataPort);
    
    struct hostent *pHost = gethostbyname(server_data_channel_ip);
    memcpy(&sAddr.sin_addr.s_addr, pHost->h_addr, pHost->h_length);
    
    memset((char *)&cAddr, 0,sizeof(cAddr));
    cAddr.sin_family      = AF_INET;
    cAddr.sin_addr.s_addr = inet_addr(client_data_channel_ip);
    cAddr.sin_port        = htons(dataPort);
    
    //creates the UDP socket to send data to
    if ((iNetSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "Error: could not create the data socket." << std::endl;
        exit(EXIT_FAILURE);
    }
        
    // Bind the socket with the server address 
    if (bind(iNetSock, (const struct sockaddr *)&sAddr, sizeof(sAddr)) < 0) { 
        std::cout << "Error: could not bind the data channel socket and address." << std::endl;
        exit(EXIT_FAILURE); 
    }
                   
    //++++++++++++++++++++++++++++++++++++++
    // second socket for CC
    // Control Channel

    //establish the IP component
    memset((char *)&sAddrCC, 0, sizeof(sAddrCC));
    sAddrCC.sin_family      = AF_INET;
    sAddrCC.sin_addr.s_addr = inet_addr(server_control_channel_ip);
    sAddrCC.sin_port        = htons(controlPort);
    
    struct hostent *pHostCC = gethostbyname(server_control_channel_ip);
    memcpy(&sAddrCC.sin_addr.s_addr, pHostCC->h_addr, pHostCC->h_length);
    
    memset((char *)&cAddrCC, 0, sizeof(cAddrCC));
    cAddrCC.sin_family      = AF_INET;
    cAddrCC.sin_addr.s_addr = inet_addr(client_control_channel_ip);
    cAddrCC.sin_port        = htons(controlPort);
    
    //creates the UDP socket to send data to
    if ((iNetSockCC = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "Error: could not create control channel socket." << std::endl;
        exit(EXIT_FAILURE);
    }
        
    // Bind the socket with the server address 
    if (bind(iNetSockCC, (const struct sockaddr *)&sAddrCC, sizeof(sAddrCC)) < 0) { 
        std::cout << "Error: could not bind the control channel socket." << std::endl;
        exit(EXIT_FAILURE);        
    }

///////////////////////////////////////////////////////////////////////////////
// GStreamer
///////////////////////////////////////////////////////////////////////////////
    CustomData data;
    GstAudioInfo info;
    GstCaps *audio_caps;

    /* Initialize cumstom data structure */
    memset (&data, 0, sizeof (data));

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    data.app_source = gst_element_factory_make ("appsrc", "audio_source");
    data.audio_sink = gst_element_factory_make ("autoaudiosink", "audio_sink");

    /* Create the empty pipeline */
    data.pipeline = gst_pipeline_new ("test-pipeline");

    if (!data.pipeline || !data.app_source || !data.audio_sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Configure appsrc */
    gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S16, SAMPLE_RATE, 1, NULL);
    audio_caps = gst_audio_info_to_caps (&info);
    g_object_set (data.app_source, "caps", audio_caps, "format", GST_FORMAT_TIME, NULL);

    /* Configure appsink */
    gst_caps_unref (audio_caps);

    /* Link all elements that can be automatically linked because they have "Always" pads */
    gst_bin_add_many (GST_BIN (data.pipeline), data.app_source, data.audio_sink, NULL);
 
    if (gst_element_link_many (data.app_source, data.audio_sink, NULL) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (data.pipeline);
        return -1;
     }

    /* Start playing the pipeline */
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

    while (TRUE) {
        push_data(&data);

    }


    /* Free resources */
    gst_element_set_state (data.pipeline, GST_STATE_NULL);
    gst_object_unref (data.pipeline);


    ///////////////////////////////////////////////////////////////////////////////
    // End GStreamer
    ///////////////////////////////////////////////////////////////////////////////

    shutdown(iNetSock,   2);
    shutdown(iNetSockCC, 2);

    return 0;
}

