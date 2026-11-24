idf_component_register(
    SRCS "src/string_check.c"
         "src/nvs_period.c"
         "src/hex_utils.c"
    INCLUDE_DIRS "inc"
    PRIV_REQUIRES nvs_flash)