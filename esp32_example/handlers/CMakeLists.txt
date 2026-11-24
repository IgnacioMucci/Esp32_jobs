idf_component_register(
    SRCS "src/period_handlers.c"
         "src/deepsleep_handlers.c"
         "src/ultralite_handlers.c"
         "src/ultralite_frame.c"
    INCLUDE_DIRS "inc"
    PRIV_REQUIRES utils poma)