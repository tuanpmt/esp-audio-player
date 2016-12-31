INC_DIRS += $(player_ROOT)
# args for passing into compile rule generation
player_SRC_DIR =  $(player_ROOT)
player_CFLAGS = -lm
player_LDFLAGS = -lm
player_LIBS = m
$(eval $(call component_compile_rules,player))
