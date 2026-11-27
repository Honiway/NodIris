# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/graph_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/graph_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/layout_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/layout_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/ogt_core_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ogt_core_autogen.dir/ParseCache.txt"
  "CMakeFiles/ogt_snapshot_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ogt_snapshot_autogen.dir/ParseCache.txt"
  "CMakeFiles/ogt_ui_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ogt_ui_autogen.dir/ParseCache.txt"
  "CMakeFiles/opengraphtool_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/opengraphtool_autogen.dir/ParseCache.txt"
  "CMakeFiles/project_file_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/project_file_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/scene_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/scene_tests_autogen.dir/ParseCache.txt"
  "graph_tests_autogen"
  "layout_tests_autogen"
  "ogt_core_autogen"
  "ogt_snapshot_autogen"
  "ogt_ui_autogen"
  "opengraphtool_autogen"
  "project_file_tests_autogen"
  "scene_tests_autogen"
  )
endif()
