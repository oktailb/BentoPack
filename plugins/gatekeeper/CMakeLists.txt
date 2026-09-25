# ==============================================================================
# BentoPack Proprietary Commercial Gatekeeper
# Governed by plugins/LICENSE-PLUGINS.md
# ==============================================================================

set(GATEKEEPER_SRC
    commercialgatekeeper.cpp
)

set(GATEKEEPER_HEADERS
    commercialgatekeeper.h
)

add_library(gatekeeper_commercial STATIC
    ${GATEKEEPER_SRC}
    ${GATEKEEPER_HEADERS}
)

target_include_directories(gatekeeper_commercial PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/BentoPack/include
    ${CMAKE_BINARY_DIR}/BentoPack
)

target_link_libraries(gatekeeper_commercial PUBLIC
    Qt6::Core
)
