vendor := $(srctree)/$(src)



#ifneq "$(wildcard $(vendor)/qcom)" ""
#	subdir-y += qcom
#endif

####  $(TARGET_DTS_CONFIG)  is project name, yajun.wang add
$(warning "device tree debug $(TARGET_DTS_CONFIG) dts")
ifneq "$(wildcard $(vendor)/$(TARGET_DTS_CONFIG))" ""
	subdir-y += $(TARGET_DTS_CONFIG)
endif
