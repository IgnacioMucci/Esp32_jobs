idf_component_register(
    SRCS "src/period_handlers.c"
         "src/deepsleep_handlers.c"
    INCLUDE_DIRS "inc"
    PRIV_REQUIRES utils)