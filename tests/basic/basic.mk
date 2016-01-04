#
# Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
# 
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. 
#

ifeq ($(findstring ${BUILD_ROOT},${CURDIR}),)
include ${PROJECT_ROOT}/tools/makefiles/target.mk
else

EXTRA_DEPS += ${BUILD_ROOT}/buddy-alloc/libbuddy-alloc.a

EXEC := basic

include ${PROJECT_ROOT}/tools/makefiles/project.mk

CPPFLAGS += -DSTATIC_CONFIG -I ${SOURCE_DIR}/../../include
LDFLAGS += -L ${BUILD_ROOT}/buddy-alloc
LDLIBS += -lbuddy-alloc

endif



