################################################################################
# Copyright (c) 2019-2020, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
################################################################################

CXX=g++

APP:= ../ds_v1.out

PLATFORM_SETTINGS:=../platform/paths.sh

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)


LIB_SO+=$(shell bash $(PLATFORM_SETTINGS) ds-so)
LIB_PATHS+=$(shell bash $(PLATFORM_SETTINGS) libs)
LIB_PATHS+=-L$(LIB_SO)

# LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
# APP_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/bin/

ifeq ($(TARGET_DEVICE),aarch64)
  CFLAGS:= -DPLATFORM_TEGRA
endif


SRCS:= deepstream_transfer_learning_app_main.cpp image_meta_consumer.cpp image_meta_producer.cpp capture_time_rules.cpp
SRCS+= client/client.cpp
SRCS+= base64/base64.cpp
SRCS+= ds_base/deepstream_app.c ds_base/deepstream_app_config_parser.c
SRCS+= $(wildcard $(shell bash $(PLATFORM_SETTINGS) apps-common)*.c)


INCS:= $(wildcard *.h)

PKGS:= gstreamer-1.0 gstreamer-video-1.0 x11 json-glib-1.0 

OBJS:= $(patsubst %.c,%.o, $(patsubst %.cpp,%.o, $(SRCS)))

CFLAGS+= $(shell bash $(PLATFORM_SETTINGS) includes)
CFLAGS+= $(shell pkg-config --cflags $(PKGS))

LIBS+= $(LIB_PATHS) -lcudart -lstdc++fs -lopencv_imgcodecs -lopencv_core -lopencv_imgproc -lnvdsgst_meta -lnvds_meta -lnvds_msgbroker -lnvds_batch_jpegenc -lnvdsgst_helper -lnvdsgst_smartrecord -lnvds_utils -lm \
       -lcuda -lgstrtspserver-1.0 -lnvbufsurface -lnvbufsurftransform -lpthread -ldl -Wl,-rpath,$(LIB_SO)
LIBS+= $(shell pkg-config --libs $(PKGS))

all: $(APP)


%.o: %.c $(INCS) Makefile
	$(CC) -g -c -o $@ $(CFLAGS) $<

%.o: %.cpp $(INCS) Makefile
	$(CXX) -g -c -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CXX) -g -o $(APP) $(OBJS) $(LIBS)

install: $(APP)
	cp -rv $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(OBJS) $(APP)


