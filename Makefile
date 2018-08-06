#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32_ble_example # // TODO: see it! please change the name of your project
									 
COMPONENT_ADD_INCLUDEDIRS := components/include

include $(IDF_PATH)/make/project.mk
