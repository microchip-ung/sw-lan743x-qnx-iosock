ARTIFACT = devs-lan887x.so

#Local overrides (gitignored), e.g.: PLATFORM, BUILD_PROFILE
-include config.mk

ifndef QNX_TARGET
$(error QNX_TARGET is not set. Source qnxsdp-env.sh first)
endif
ifndef QNX_HOST
$(error QNX_HOST is not set. Source qnxsdp-env.sh first)
endif

#Build architecture/variant string, possible values: x86_64, aarch64le, armv7le, etc...
PLATFORM ?= aarch64le

#Build profile, possible values: release, debug, profile, coverage
BUILD_PROFILE ?= release

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)

#Compiler definitions
CC = qcc -Vgcc_nto$(PLATFORM)
LD = $(CC)

#Map PLATFORM to architecture-specific include directory name
ARCH_DIR_aarch64le = aarch64
ARCH_DIR_x86_64    = x86_64
ARCH_DIR = $(ARCH_DIR_$(PLATFORM))

MYSDPPATH = $(QNX_TARGET)
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/devs
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/devs/contrib/ck/include
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/devs/qnx-gen
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/devs/sys-nto
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/devs/include_$(ARCH_DIR)
SYSTEM_INCLUDES += -I$(MYSDPPATH)/usr/include/io-sock

#User defined include/preprocessor flags and libraries

#INCLUDES += -I/path/to/my/lib/include
#INCLUDES += -I../mylib/public

#LIBS += -L/path/to/my/lib/$(PLATFORM)/usr/lib -lmylib
#LIBS += -L../mylib/$(OUTPUT_DIR) -lmylib

CCFLAGS += -D_KERNEL
CCFLAGS += -DNBPFILTER=1
CCFLAGS += -DBUILDENV_qss
CCFLAGS += -DVARIANT_dll
CCFLAGS += -D_STRINGS_H_INCLUDED
CCFLAGS += -D_UNISTD_H_INCLUDED

comma := ,
SYSINCFLAGS = $(patsubst -I%,-Wp$(comma)-isystem$(comma)%,$(SYSTEM_INCLUDES))

#Compiler flags for build profiles
CCFLAGS_release += -O3 -DNDEBUG
CCFLAGS_debug += -g3 -Og -fno-builtin -DDEBUG -DVARIANT_g
CCFLAGS_coverage += -g -O0 -ftest-coverage -fprofile-arcs
LDFLAGS_coverage += -ftest-coverage -fprofile-arcs
CCFLAGS_profile += -g -O0 -finstrument-functions
LIBS_profile += -lprofilingS

#Warning flags
CWARN += -Wall
CWARN += -Wextra
CWARN += -Wpedantic
CWARN += -Wcast-align
CWARN += -Wbad-function-cast
CWARN += -Wstrict-prototypes
CWARN += -Wundef
CWARN += -Wcast-qual
CWARN += -Wshadow
CWARN += -Wunreachable-code
CWARN += -Wlogical-op
CWARN += -Wfloat-equal
CWARN += -Wstrict-aliasing
CWARN += -Wredundant-decls
CWARN += -Wold-style-definition
CWARN += -Wno-unused-variable
CWARN += -Wno-unused-parameter
CWARN += -Wno-unused-function

#Generic compiler flags (which include build type flags)
CCFLAGS_all += $(CWARN) -fmessage-length=0
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))
#Shared library has to be compiled with -fPIC
CCFLAGS_all += -fPIC
LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all += $(LIBS_$(BUILD_PROFILE))
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

#Macro to expand files recursively: parameters $1 -  directory, $2 - extension, i.e. cpp
rwildcard = $(wildcard $(addprefix $1/*.,$2)) $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2))

#Source list
SRCS = $(call rwildcard, src, c)

#Object files list
OBJS = $(addprefix $(OUTPUT_DIR)/,$(addsuffix .o, $(basename $(SRCS))))

#Compiling rule
$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(DEPS) -o $@ $(SYSINCFLAGS) $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $(EXTRA_CCFLAGS) $<

#Linking rule
$(TARGET):$(OBJS)
	$(LD) -shared -o $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(OBJS) $(LIBS_all) $(LIBS)

#Rules section for default compilation and linking
all: $(TARGET)

clean:
	rm -fr $(OUTPUT_DIR)

rebuild: clean all

#Inclusion of dependencies (object files to source and includes)
-include $(OBJS:%.o=%.d)
