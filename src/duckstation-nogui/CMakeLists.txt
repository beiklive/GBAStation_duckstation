add_executable(duckstation-nogui
  nogui_host.cpp
  nogui_host.h
  nogui_platform.h
)

target_precompile_headers(duckstation-nogui PRIVATE "pch.h")
target_link_libraries(duckstation-nogui PRIVATE core util common imgui scmversion)

add_core_resources(duckstation-nogui)

if(NINTENDO_SWITCH)
  enable_language(ASM)

  target_sources(duckstation-nogui PRIVATE
    switch_nogui_platform.cpp
    switch_nogui_platform.h
    switch_asm.S
    ../gbastation/GBAStationHost.cpp
    ../gbastation/GBAStationHost.h
    ../gbastation/GBAStationPaths.h
    ../tico/TicoDuckBridge.cpp
    ../tico/TicoDuckBridge.h
    ../tico/TicoOverlay.cpp
    ../tico/TicoOverlay.h
  )
  target_link_libraries(duckstation-nogui PUBLIC "${NX_LIBRARIES}")

  nx_generate_nacp (GBAStationDuckStationStub.nacp
    NAME    "GBAStation DuckStation Stub"
    AUTHOR  "GBAStation"
    VERSION "1.0.0"
  )

  # Create folder early otherwise the CMake script complains
  if (CMAKE_BUILD_TYPE STREQUAL Debug)
    nx_create_nro(duckstation-nogui
      OUTPUT ${CMAKE_BINARY_DIR}/GBAStationDuckStationStub.nro
      NACP  GBAStationDuckStationStub.nacp
      ICON  "${CMAKE_SOURCE_DIR}/extras/icons/icon-switch.jpg"
    )
  else()
    nx_create_nro(duckstation-nogui
      OUTPUT ${CMAKE_BINARY_DIR}/GBAStationDuckStationStub.nro
      NACP  GBAStationDuckStationStub.nacp
      ICON  "${CMAKE_SOURCE_DIR}/extras/icons/icon-switch.jpg"
    )
  endif()
endif()
