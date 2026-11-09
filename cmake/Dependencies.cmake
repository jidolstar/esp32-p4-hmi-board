include_guard(GLOBAL)

include(FetchContent)

set(HMI_LVGL_GIT_REPOSITORY "https://github.com/lvgl/lvgl.git")
set(HMI_LVGL_GIT_TAG "v8.4.0")
set(HMI_SDL_GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git")
set(HMI_SDL_GIT_TAG "release-2.30.12")

function(hmi_load_windows_ui_dependencies)
  if(TARGET lvgl::lvgl AND TARGET SDL2::SDL2-static)
    return()
  endif()

  set(LV_CONF_PATH "${PROJECT_SOURCE_DIR}/include/lv_conf.h" CACHE STRING "LVGL configuration" FORCE)

  FetchContent_Declare(lvgl
    GIT_REPOSITORY "${HMI_LVGL_GIT_REPOSITORY}"
    GIT_TAG "${HMI_LVGL_GIT_TAG}"
    GIT_SHALLOW TRUE)

  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY "${HMI_SDL_GIT_REPOSITORY}"
    GIT_TAG "${HMI_SDL_GIT_TAG}"
    GIT_SHALLOW TRUE)

  FetchContent_MakeAvailable(lvgl SDL2)

  if(TARGET lvgl_examples)
    set_target_properties(lvgl_examples PROPERTIES EXCLUDE_FROM_ALL TRUE)
  endif()
  if(TARGET lvgl_demos)
    set_target_properties(lvgl_demos PROPERTIES EXCLUDE_FROM_ALL TRUE)
  endif()
endfunction()
