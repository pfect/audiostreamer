/*
 *  audiostreamer - audio streaming utility 
 *  Copyright (C) 2021  Pasi Patama
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * This work is based on gstreamer example applications and references
 * indicated in README.
 * 
 */
 
 /* 
  * See also:
  * 
  * https://gist.github.com/crearo/a49a8805857f1237c401be14ba6d3b03
  * https://github.com/crearo/gstreamer-cookbook/blob/master/C/tee-recording-and-display.c
  * https://gstreamer.freedesktop.org/documentation/opus/opusenc.html?gi-language=c
  * https://gist.github.com/paulbarber/7d2e43d1e3365b2042aa
  * https://gist.github.com/quasoft/6b48dd8f101955b82a55637a6ee8b3db
  */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <gst/gst.h>

#define DEFAULT_REC_FILE_SIZE 500000

int
main (int argc, char *argv[])
{
	int usetestsource=0;
	int recordlocally=0;
	int recordedfilelen=DEFAULT_REC_FILE_SIZE;
	int c, index;
	opterr = 0;
	
	while ((c = getopt (argc, argv, "tr:h")) != -1)
	switch (c)
	{
	case 't':
		usetestsource = 1;
		break;
	case 'r':
		recordlocally = 1;
		recordedfilelen = atoi(optarg);
		break;
	case 'h':
		fprintf(stderr,"\nUsage: -t             Use test source.\n       -r [file size] Record locally while streaming (/tmp/*.opus)\n\n");
		return 1;
	break;
		default:
		break;
	}
	
	/* Initialize gstreamer */
	GstElement *pipeline, *source, *dynamic, *audioconvert,*audioresample,*opusencoder,*rtppayload, *sink, *tee, *filesink,*queue_record,*queue_stream,*fileopusencoder,*fileopusmuxer,*fileaudioconvert;
	GstCaps *filtercaps;
	GstBus *bus;
	GstMessage *msg;
	GstStateChangeReturn ret;
	gst_init (&argc, &argv);

	/* Selectable source (test vs pulse)  */
	if ( usetestsource == 1 )
		source = gst_element_factory_make ("audiotestsrc", NULL); 
	if ( usetestsource == 0 ) {
		source = gst_element_factory_make ("pulsesrc", NULL);
	}
	/* audioconvert */
	audioconvert = gst_element_factory_make ("audioconvert", NULL);	
	/* caps */ 
	GstElement *capsfilter = gst_element_factory_make("capsfilter", NULL);
	GstCaps *caps = gst_caps_from_string ("audio/x-raw,channels=1,depth=16,width=16,rate=44100");
	g_object_set (capsfilter, "caps", caps, NULL);
	gst_caps_unref(caps);
	/* audioresample */
	audioresample = gst_element_factory_make ("audioresample", NULL);
	/* Opus encoder */
	opusencoder = gst_element_factory_make ("opusenc", NULL);
	g_object_set (G_OBJECT ( opusencoder ), "bitrate", 32000, NULL); 	// 6000 - 128000
	g_object_set (G_OBJECT ( opusencoder ), "audio-type", 2048, NULL);	// 2048 = voice, 2049 = Generic
	g_object_set (G_OBJECT ( opusencoder ), "bandwidth", 1103, NULL);	// 1101 narrowband 1102 medium band fullband (1105) 
	g_object_set (G_OBJECT ( opusencoder ), "dtx", FALSE, NULL);
	g_object_set (G_OBJECT ( opusencoder ), "inband-fec", TRUE, NULL);
	g_object_set (G_OBJECT ( opusencoder ), "packet-loss-percentage", 20, NULL);
	/* compressor (test) */
	dynamic = gst_element_factory_make ("audiodynamic", NULL);
	g_object_set (G_OBJECT ( dynamic ), "characteristics", 0, NULL);
	g_object_set (G_OBJECT ( dynamic ), "mode", 0, NULL); 
	g_object_set (G_OBJECT ( dynamic ), "threshold", 0.1 , NULL); 
	g_object_set (G_OBJECT ( dynamic ), "ratio",2.0 , NULL); 
	/* rtp payload */
	rtppayload = gst_element_factory_make ("rtpopuspay", NULL);
	/* tee */
	tee = gst_element_factory_make ("tee", "tee");
	
	/* filesink 
	filesink = gst_element_factory_make("filesink", NULL);
	g_object_set(filesink, "location", "/tmp/recording", NULL);*/
	
	/* multifilesink */
	filesink = gst_element_factory_make("multifilesink", NULL);
	g_object_set(filesink, "location", "/tmp/rec_%d.opus", NULL);
	g_object_set(filesink, "next-file", 4, NULL);
	g_object_set(filesink, "max-file-size", recordedfilelen, NULL);
	
	/* queue */
	queue_stream = gst_element_factory_make("queue", "queue_stream");
	queue_record = gst_element_factory_make("queue", "queue_record");
	
	/* fileaudioconvert,fileopusencoder,fileopusmuxer */
	fileaudioconvert = gst_element_factory_make ("audioconvert", NULL);
	fileopusencoder = gst_element_factory_make ("vorbisenc", NULL);
	fileopusmuxer = gst_element_factory_make ("oggmux", NULL);
	
	sink = gst_element_factory_make ("udpsink", NULL);
	if (sink == NULL)
		g_error ("Could not create udpsink");
	g_object_set(G_OBJECT(sink), "host", "0.0.0.0", NULL);
	g_object_set(G_OBJECT(sink), "port", 6000, NULL);
	
	pipeline = gst_pipeline_new ("test-pipeline");

	if (!pipeline ) {
		g_printerr ("Not all elements could be created: pipeline \n");
		return -1;
	}
	if (!source) {
		g_printerr ("Not all elements could be created: source \n");
		return -1;
	}
	if (!sink) {
		g_printerr ("Not all elements could be created: sink \n");
		return -1;
	}
 
	if ( recordlocally )
	{
		/* Build tee & queue pipelines to record locally and simultaneously stream out */
		gst_bin_add_many (GST_BIN (pipeline),  source, tee, queue_record,fileopusencoder,fileopusmuxer, filesink,queue_stream,capsfilter, audioconvert,audioresample,opusencoder, rtppayload, sink, NULL);
		if (!gst_element_link_many(source, tee, NULL) 
			|| !gst_element_link_many(tee, queue_record, fileopusencoder, fileopusmuxer, filesink, NULL)
			|| !gst_element_link_many(tee, queue_stream, capsfilter, audioconvert, audioresample, opusencoder, rtppayload, sink,NULL)) {
			g_error("Failed to link elements");
			return -2;
		} 
	}
	else
	{	
		/* Build the pipeline just for streaming */
		gst_bin_add_many (GST_BIN (pipeline), source,capsfilter,audioconvert,audioresample,opusencoder,rtppayload, sink, NULL);
		if ( gst_element_link_many (source,capsfilter,audioconvert,audioresample,opusencoder,rtppayload,sink,NULL) != TRUE) {
			g_printerr ("Elements could not be linked.\n");
			gst_object_unref (pipeline);
			return -1;
		}
	}	

	/* Start playing */
	ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		gst_object_unref (pipeline);
		return -1;
	}

	/* Wait until error or EOS */
	bus = gst_element_get_bus (pipeline);
	msg =
	  gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
	  GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

	/* Parse message */
	if (msg != NULL) {
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE (msg)) {
	  case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &err, &debug_info);
		g_printerr ("Error received from element %s: %s\n",
			GST_OBJECT_NAME (msg->src), err->message);
		g_printerr ("Debugging information: %s\n",
			debug_info ? debug_info : "none");
		g_clear_error (&err);
		g_free (debug_info);
		break;
	  case GST_MESSAGE_EOS:
		g_print ("End-Of-Stream reached.\n");
		gst_element_send_event(pipeline, gst_event_new_eos());

		break;
	  default:
		/* We should not reach here because we only asked for ERRORs and EOS */
		g_printerr ("Unexpected message received.\n");
		break;
	}
	gst_message_unref (msg);
	}

	/* Free resources */
	gst_object_unref (bus);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (pipeline);

	return 0;
}
