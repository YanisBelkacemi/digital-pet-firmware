# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c  display.c button.c pet.c  flappy_bird/animation.c flappy_bird/display_bird.c ble.c  esp_now_imp.c   # list the source files of this component
    INCLUDE_DIRS        # optional, add here public include directories
    PRIV_INCLUDE_DIRS  flappy_bird # optional, add here private include directories
    REQUIRES   esp_lcd  esp_timer esp_wifi     # optional, list the public requirements (component names)
    PRIV_REQUIRES    nvs_flash bt   # optional, list the private requirements
)
