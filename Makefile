PROJECT_NAME = $(shell basename "$(realpath ./)")

# Configurations
NRF_IC = nrf52833
SDK_VERSION = 16
SOFTDEVICE_MODEL = blank

# Source and header files
APP_HEADER_PATHS += .
APP_SOURCE_PATHS += .
APP_SOURCES = $(notdir $(wildcard ./*.c))

# Location of the nu-microbit-base checkout (override with: make NU_MICROBIT_BASE=...)
NU_MICROBIT_BASE ?= $(HOME)/nu-microbit-base/software

# Path to base of nRF52x-base repo
NRF_BASE_DIR = $(NU_MICROBIT_BASE)/nrf52x-base/

# Include board Makefile: local copy of the micro:bit v2 library in ./microbit_v2
include ./microbit_v2/Board.mk

# Include main Makefile
include $(NRF_BASE_DIR)/make/AppMakefile.mk
