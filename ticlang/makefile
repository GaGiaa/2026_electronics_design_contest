MSPM0_SDK_INSTALL_DIR := D:/Software/ti/mspm0_sdk_2_05_01_00
TICLANG_ARMCOMPILER := D:/Software/ti/ccs2020/ccs/tools/compiler/ti-cgt-armllvm_4.0.3.LTS
SYSCONFIG_TOOL := D:/Software/ti/ccs2020/ccs/utils/sysconfig_1.24.0/sysconfig_cli.bat

CC = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
LNK = "$(TICLANG_ARMCOMPILER)/bin/tiarmclang"
SYSCFG_CMD_STUB = "$(SYSCONFIG_TOOL)" --compiler ticlang --product "$(MSPM0_SDK_INSTALL_DIR)/.metadata/product.json"
SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) --listGeneratedFiles --listReferencedFiles --output . ../mspm0l1306_bringup.syscfg)
SYSCFG_C_FILES = $(filter %.c,$(SYSCFG_FILES))
SYSCFG_H_FILES = $(filter %.h,$(SYSCFG_FILES))
SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

APP_SOURCES = ../app/main.c ../app/echo_queue.c ../board/board_led.c ../board/board_uart.c
OBJECTS = $(notdir $(patsubst %.c,%.obj,$(APP_SOURCES))) $(patsubst %.c,%.obj,$(notdir $(SYSCFG_C_FILES)))
NAME = mspm0l1306_bringup

.PHONY: all clean syscfg

CFLAGS = -I.. -I. -I../app -I../board $(addprefix @,$(SYSCFG_OPT_FILES)) -O2 @device.opt \
    "-I$(MSPM0_SDK_INSTALL_DIR)/source/third_party/CMSIS/Core/Include" \
    "-I$(MSPM0_SDK_INSTALL_DIR)/source" -gdwarf-3 -mcpu=cortex-m0plus \
    -march=thumbv6m -mfloat-abi=soft -mthumb
LFLAGS = -ldevice.cmd.genlibs "-L$(MSPM0_SDK_INSTALL_DIR)/source" -L.. \
    device_linker.cmd "-Wl,-m,$(NAME).map" -Wl,--rom_model -Wl,--warn_sections \
    "-L$(TICLANG_ARMCOMPILER)/lib" -llibc.a

all: $(NAME).out

syscfg: ../mspm0l1306_bringup.syscfg

	@ echo Generating SysConfig files...
	@ $(SYSCFG_CMD_STUB) --output . $<

$(SYSCFG_FILES): syscfg
	@ echo SysConfig generation complete

define C_RULE
$(basename $(notdir $(1))).obj: $(1) $(SYSCFG_H_FILES)
	@ echo Building $$@
	@ $(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach c_file,$(SYSCFG_C_FILES),$(eval $(call C_RULE,$(c_file))))
$(foreach c_file,$(APP_SOURCES),$(eval $(call C_RULE,$(c_file))))

$(NAME).out: $(OBJECTS)
	@ echo Linking $@
	@ $(LNK) -Wl,-u,_c_int00 $(OBJECTS) $(LFLAGS) -o $@

clean:
	@ cmd /c "del /Q $(OBJECTS) $(NAME).out $(NAME).map 2>NUL"
	@ cmd /c "del /Q ti_msp_dl_config.c ti_msp_dl_config.h device.opt device.cmd.genlibs device_linker.cmd Event.dot 2>NUL"
