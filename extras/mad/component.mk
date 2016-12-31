INC_DIRS += $(mad_ROOT)
INC_DIRS += $(mad_ROOT)mpg12

# args for passing into compile rule generation
mad_SRC_DIR =  $(mad_ROOT)

$(eval $(call component_compile_rules,mad))
