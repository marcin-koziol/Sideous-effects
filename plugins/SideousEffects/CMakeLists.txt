dpf_add_plugin(sideous-effects
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      SideousEffectsPlugin.cpp
  FILES_UI
      SideousEffectsUI.cpp)

target_include_directories(sideous-effects PUBLIC ".")
